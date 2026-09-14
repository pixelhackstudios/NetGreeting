#include "mainwindow.h"

#include "audio.h"
#include "camera.h"
#include "chatwindow.h"
#include "conference.h"
#include "dialogs.h"
#include "filetransfer.h"
#include "greetingpostclient.h"
#include "landirectory.h"
#include "sharewindow.h"
#include "style.h"
#include "videopane.h"
#include "whiteboard.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QBuffer>
#include <QCloseEvent>
#include <QFileDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QScreen>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>
#include <QDir>
#include <QPixmap>
#include <algorithm>

MainWindow::MainWindow(Identity id, QWidget *parent)
    : QMainWindow(parent)
    , m_id(std::move(id))
{
  setWindowTitle(tr("NetGreeting"));
  setWindowIcon(appWindowIcon());
  resize(290, 540);
  setMinimumWidth(280);

  m_conf = new Conference(this);
  m_lan = new LanDirectory(this);
  m_post = new GreetingPostClient(this);
  m_camera = new CameraCapture(this);
  m_audio = new AudioEngine(this);
  m_chat = new ChatWindow(this);
  m_board = new WhiteboardWindow(this);
  m_files = new FileTransferWindow(this);
  m_share = new ShareWindow(this);

  m_mediaTimer = new QTimer(this);
  m_shareTimer = new QTimer(this);
  m_shareTimer->setInterval(400);

  buildUi();
  buildMenus();
  applyIdentity();

  connect(m_conf, &Conference::listeningChanged, this, [this] { updateStatus(); });
  connect(m_conf, &Conference::statusMessage, this, [this](const QString &s) {
    statusBar()->showMessage(s);
  });
  connect(m_conf, &Conference::errorOccurred, this, [this](const QString &e) {
    QMessageBox::warning(this, tr("NetGreeting"), e);
  });
  connect(m_conf, &Conference::incomingCall, this, [this](const QString &name, const QString &host, quint16) {
    if (m_autoAccept) {
      m_conf->acceptIncoming();
      return;
    }
    IncomingCallDialog dlg(name, host, this);
    if (dlg.exec() == QDialog::Accepted)
      m_conf->acceptIncoming();
    else
      m_conf->rejectIncoming();
  });
  connect(m_conf, &Conference::callStarted, this, [this](const QString &, const QString &) {
    startMedia();
    m_nameBanner->setText(m_conf->peerName());
    updateStatus();
  });
  connect(m_conf, &Conference::callEnded, this, [this](const QString &reason) {
    stopMedia();
    m_sharing = false;
    m_shareTimer->stop();
    m_remoteFrame = {};
    m_nameBanner->setText(m_id.displayName());
    statusBar()->showMessage(reason);
    updateStatus();
  });
  connect(m_conf, &Conference::participantsChanged, this, [this] {
    m_people->clear();
    m_people->addItems(m_conf->participants());
    m_chat->setParticipants(m_conf->participants());
  });
  connect(m_conf, &Conference::chatReceived, this, [this](const QString &from, const QString &text, bool w) {
    m_chat->appendMessage(from, text, w);
    m_chat->show();
    m_chat->raise();
  });
  connect(m_conf, &Conference::whiteboardOp, m_board, &WhiteboardWindow::applyRemote);
  connect(m_board, &WhiteboardWindow::opReady, m_conf, &Conference::sendWhiteboard);
  connect(m_chat, &ChatWindow::sendRequested, m_conf, &Conference::sendChat);
  connect(m_conf, &Conference::videoFrameReceived, this, [this](const QImage &img) {
    m_remoteFrame = img;
    if (!m_dataOnly)
      m_video->setFrame(img);
  });
  connect(m_conf, &Conference::shareFrameReceived, this, [this](const QImage &img) {
    m_share->setFrame(img);
    m_share->show();
    m_share->raise();
  });
  connect(m_conf, &Conference::audioFrameReceived, m_audio, &AudioEngine::playPcm);
  connect(m_conf, &Conference::fileOffered, this, [this](quint32 id, const QString &from, const QString &name, qint64 size) {
    m_files->addOffer(id, from, name, size);
    m_files->show();
    m_files->raise();
  });
  connect(m_conf, &Conference::fileProgress, m_files, &FileTransferWindow::setProgress);
  connect(m_conf, &Conference::fileFinished, m_files, &FileTransferWindow::setFinished);
  connect(m_files, &FileTransferWindow::sendFileRequested, this, [this] {
    const QString path = QFileDialog::getOpenFileName(this, tr("Send File"));
    if (!path.isEmpty())
      m_conf->offerFile(path);
  });
  connect(m_files, &FileTransferWindow::acceptRequested, m_conf, &Conference::acceptFile);
  connect(m_files, &FileTransferWindow::rejectRequested, m_conf, &Conference::rejectFile);

  connect(m_camera, &CameraCapture::frameReady, this, [this](const QImage &img) {
    m_localFrame = img;
    if (!m_conf->inCall() && !m_dataOnly && !m_showDialPad) {
      m_video->setFrame(img);
      m_video->setPipVisible(false);
    } else if (m_conf->inCall()) {
      m_video->setPipFrame(img);
      m_video->setPipVisible(!m_dataOnly);
    }
  });
  connect(m_camera, &CameraCapture::failed, this, [this](const QString &e) { statusBar()->showMessage(e, 4000); });
  connect(m_audio, &AudioEngine::captured, m_conf, &Conference::sendAudioPcm);
  connect(m_audio, &AudioEngine::failed, this, [this](const QString &e) { statusBar()->showMessage(e, 4000); });

  connect(m_mediaTimer, &QTimer::timeout, this, &MainWindow::sendMediaTick);
  connect(m_shareTimer, &QTimer::timeout, this, &MainWindow::sendShareTick);
  connect(m_lan, &LanDirectory::peersChanged, this, [this] {
    if (m_dir)
      m_dir->refreshLan(m_lan->peers());
  });
  connect(m_post, &GreetingPostClient::statusChanged, this, [this](const QString &s) {
    statusBar()->showMessage(s, 4000);
  });
  m_post->start(m_id);

  m_conf->startListening(m_id.port);
  m_lan->start(m_id.displayName(), m_id.port);
  m_camera->configure(m_id.cameraDevice, m_id.videoWidth(), m_id.videoHeight());
  m_camera->start();
  updateStatus();
}

MainWindow::~MainWindow()
{
  if (m_camera) {
    m_camera->requestStop();
    m_camera->wait(800);
  }
  m_audio->stop();
}

void MainWindow::buildUi()
{
  auto *central = new QWidget;
  setCentralWidget(central);
  auto *root = new QVBoxLayout(central);
  root->setContentsMargins(8, 8, 8, 8);
  root->setSpacing(6);

  m_video = new VideoPane;

  m_dialField = new QLineEdit;
  m_dialField->setPlaceholderText(tr("IP address"));
  auto *pad = new QWidget;
  auto *grid = new QGridLayout(pad);
  grid->setSpacing(4);
  const QString keys = QStringLiteral("123456789*0#");
  for (int i = 0; i < 12; ++i) {
    auto *b = new QPushButton(keys.mid(i, 1));
    connect(b, &QPushButton::clicked, this, [this, i, keys] { m_dialField->insert(keys.mid(i, 1)); });
    grid->addWidget(b, i / 3, i % 3);
  }
  m_dialPad = new QWidget;
  auto *dialLay = new QVBoxLayout(m_dialPad);
  dialLay->setContentsMargins(0, 0, 0, 0);
  dialLay->addWidget(m_dialField);
  dialLay->addWidget(pad);

  m_previewStack = new QStackedWidget;
  m_previewStack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  m_previewStack->setMinimumSize(160, 120);
  m_previewStack->addWidget(m_video);
  m_previewStack->addWidget(m_dialPad);

  auto *callBtn = new QPushButton(netGreetingIcon(QStringLiteral("call")), tr("Place Call"));
  auto *endBtn = new QPushButton(netGreetingIcon(QStringLiteral("hangup")), tr("End Call"));
  auto *dirBtn = new QPushButton(netGreetingIcon(QStringLiteral("directory")), tr("Directory"));
  connect(callBtn, &QPushButton::clicked, this, &MainWindow::showPlaceCall);
  connect(endBtn, &QPushButton::clicked, this, [this] { m_conf->hangup(); });
  connect(dirBtn, &QPushButton::clicked, this, &MainWindow::showDirectory);

  auto *btns = new QHBoxLayout;
  btns->addWidget(callBtn);
  btns->addWidget(endBtn);
  btns->addWidget(dirBtn);

  m_nameBanner = new QLabel(m_id.displayName());
  m_nameBanner->setAlignment(Qt::AlignCenter);
  m_nameBanner->setStyleSheet(QStringLiteral("background:#0a246a; color:white; padding:3px; font-weight:bold;"));

  m_people = new QListWidget;
  m_people->setMaximumHeight(70);
  m_people->addItem(m_id.displayName());

  auto *tools = new QHBoxLayout;
  auto addTool = [&](const QString &icon, const QString &tip, auto slot) {
    auto *b = new QPushButton(netGreetingIcon(icon), QString());
    b->setToolTip(tip);
    b->setFixedSize(36, 32);
    connect(b, &QPushButton::clicked, this, slot);
    tools->addWidget(b);
    return b;
  };
  addTool(QStringLiteral("share"), tr("Share Desktop"), [this] {
    if (!m_conf->inCall()) {
      QMessageBox::information(this, tr("NetGreeting"), tr("Place a call first."));
      return;
    }
    m_sharing = !m_sharing;
    if (m_sharing)
      m_shareTimer->start();
    else
      m_shareTimer->stop();
    statusBar()->showMessage(m_sharing ? tr("Sharing desktop...") : tr("Stopped sharing."));
  });
  addTool(QStringLiteral("chat"), tr("Chat"), [this] { m_chat->show(); m_chat->raise(); });
  addTool(QStringLiteral("whiteboard"), tr("Whiteboard"), [this] { m_board->show(); m_board->raise(); });
  addTool(QStringLiteral("file"), tr("File Transfer"), [this] { m_files->show(); m_files->raise(); });
  tools->addStretch();

  root->addWidget(m_previewStack, 1);
  root->addLayout(btns);
  root->addWidget(m_nameBanner);
  root->addWidget(new QLabel(tr("Current Call / Meeting")));
  root->addWidget(m_people);
  root->addLayout(tools);

  statusBar()->showMessage(tr("Not in a call."));
}

void MainWindow::buildMenus()
{
  auto *call = menuBar()->addMenu(tr("&Call"));
  call->addAction(tr("&New Call..."), this, &MainWindow::showPlaceCall)->setShortcut(QKeySequence::New);
  call->addAction(tr("&Directory..."), this, &MainWindow::showDirectory);
  call->addSeparator();
  call->addAction(tr("&Hang Up"), this, [this] { m_conf->hangup(); });
  call->addSeparator();
  call->addAction(tr("E&xit"), this, &QWidget::close)->setShortcut(QKeySequence::Quit);

  auto *view = menuBar()->addMenu(tr("&View"));
  auto *compact = view->addAction(tr("&Compact"));
  compact->setCheckable(true);
  connect(compact, &QAction::toggled, this, &MainWindow::setCompact);
  auto *dataOnly = view->addAction(tr("&Data Only"));
  dataOnly->setCheckable(true);
  connect(dataOnly, &QAction::toggled, this, &MainWindow::setDataOnly);
  auto *onTop = view->addAction(tr("&Always on Top"));
  onTop->setCheckable(true);
  connect(onTop, &QAction::toggled, this, [this](bool on) {
    setWindowFlag(Qt::WindowStaysOnTopHint, on);
    show();
  });
  view->addSeparator();
  auto *dial = view->addAction(tr("Dial &Pad"));
  dial->setCheckable(true);
  connect(dial, &QAction::toggled, this, [this](bool on) {
    m_showDialPad = on;
    m_previewStack->setCurrentIndex(on ? 1 : 0);
  });
  auto *myVideo = view->addAction(tr("&My Video"));
  myVideo->setCheckable(true);
  myVideo->setChecked(true);
  connect(myVideo, &QAction::toggled, this, [this](bool on) {
    if (on) {
      m_showDialPad = false;
      m_previewStack->setCurrentIndex(0);
    }
  });

  auto *tools = menuBar()->addMenu(tr("&Tools"));
  tools->addAction(tr("&Chat"), this, [this] { m_chat->show(); });
  tools->addAction(tr("&Whiteboard"), this, [this] { m_board->show(); });
  tools->addAction(tr("&File Transfer"), this, [this] { m_files->show(); });
  tools->addAction(tr("Share &Desktop"), this, [this] {
    if (m_conf->inCall()) {
      m_sharing = true;
      m_shareTimer->start();
    }
  });
  tools->addSeparator();
  tools->addAction(tr("&Options..."), this, &MainWindow::showOptions);

  auto *help = menuBar()->addMenu(tr("&Help"));
  help->addAction(tr("&About NetGreeting"), this, [this] {
    AboutDialog dlg(this);
    dlg.exec();
  });
}

void MainWindow::applyIdentity()
{
  m_conf->setIdentity(m_id);
  m_chat->setLocalName(m_id.displayName());
  m_board->setLocalName(m_id.displayName());
  m_nameBanner->setText(m_id.displayName());
  m_lan->setName(m_id.displayName());
  m_lan->setTcpPort(m_id.port);
}

void MainWindow::placeCall(const QString &host, quint16 port)
{
  m_conf->placeCall(host, port);
}

void MainWindow::showPlaceCall()
{
  PlaceCallDialog dlg(m_id, this);
  if (m_showDialPad && !m_dialField->text().trimmed().isEmpty()) {
    QString t = m_dialField->text().trimmed();
    quint16 port = m_id.port;
    if (t.contains(QLatin1Char(':'))) {
      port = t.section(QLatin1Char(':'), 1, 1).toUShort();
      t = t.section(QLatin1Char(':'), 0, 0);
    }
    dlg.setAddress(t, port);
  }
  if (dlg.exec() == QDialog::Accepted)
    placeCall(dlg.host(), dlg.port());
}

void MainWindow::showDirectory()
{
  if (!m_dir) {
    m_dir = new DirectoryWindow(&m_id, m_lan, m_post, this);
    connect(m_dir, &DirectoryWindow::callRequested, this, &MainWindow::placeCall);
  }
  m_dir->refreshLan(m_lan->peers());
  m_post->refreshNow();
  m_dir->refreshPost();
  m_dir->show();
  m_dir->raise();
}

void MainWindow::showOptions()
{
  OptionsDialog dlg(m_id, this);
  if (dlg.exec() != QDialog::Accepted)
    return;
  const quint16 oldPort = m_id.port;
  m_id = dlg.identity();
  m_id.save();
  applyIdentity();
  m_post->start(m_id);
  if (m_id.port != oldPort) {
    m_conf->startListening(m_id.port);
    m_lan->start(m_id.displayName(), m_id.port);
  }
  if (m_camera->isRunning()) {
    m_camera->requestStop();
    m_camera->wait(800);
  }
  m_camera->configure(m_id.cameraDevice, m_id.videoWidth(), m_id.videoHeight());
  m_camera->start();
  updateStatus();
}

void MainWindow::setCompact(bool on)
{
  m_people->setVisible(!on);
}

void MainWindow::setDataOnly(bool on)
{
  m_dataOnly = on;
  m_previewStack->setVisible(!on);
  m_video->setPipVisible(!on && m_conf->inCall());
}

void MainWindow::startMedia()
{
  m_mediaTimer->start(1000 / std::max(1, m_id.videoFps()));
  if (m_id.sendAudio)
    m_audio->start(true, true);
}

void MainWindow::stopMedia()
{
  m_mediaTimer->stop();
  m_audio->stop();
  m_video->setPipVisible(false);
  if (!m_localFrame.isNull())
    m_video->setFrame(m_localFrame);
}

void MainWindow::updateStatus()
{
  const QString ip = Conference::localIPv4Addresses().value(0);
  if (m_conf->inCall())
    statusBar()->showMessage(tr("In a call with %1").arg(m_conf->peerName()));
  else if (m_conf->isListening())
    statusBar()->showMessage(tr("Not in a call. %1:%2").arg(ip).arg(m_conf->listenPort()));
  else
    statusBar()->showMessage(tr("Not listening."));
}

QByteArray MainWindow::jpegOf(const QImage &img, int quality, int w, int h) const
{
  if (img.isNull())
    return {};
  const QImage scaled = img.scaled(w, h, Qt::KeepAspectRatio, Qt::SmoothTransformation);
  QByteArray ba;
  QBuffer buf(&ba);
  buf.open(QIODevice::WriteOnly);
  scaled.save(&buf, "JPEG", quality);
  return ba;
}

void MainWindow::sendMediaTick()
{
  if (!m_conf->inCall() || !m_id.sendVideo)
    return;
  const QByteArray jpeg = jpegOf(m_localFrame, m_id.jpegQuality(), m_id.videoWidth(), m_id.videoHeight());
  m_conf->sendVideoJpeg(jpeg);
}

void MainWindow::sendShareTick()
{
  if (!m_sharing || !m_conf->inCall())
    return;
  QScreen *screen = QApplication::primaryScreen();
  if (!screen)
    return;
  const QImage img = screen->grabWindow(0).toImage();
  m_conf->sendShareJpeg(jpegOf(img, 40, 800, 450));
}

void MainWindow::closeEvent(QCloseEvent *e)
{
  m_conf->hangup(QStringLiteral("Closed"));
  m_post->stop();
  QMainWindow::closeEvent(e);
}
