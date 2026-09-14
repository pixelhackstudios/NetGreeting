#include "dialogs.h"
#include "protocol.h"
#include "style.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPair>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QTabWidget>
#include <QVBoxLayout>

PlaceCallDialog::PlaceCallDialog(const Identity &id, QWidget *parent)
    : QDialog(parent)
{
  setWindowTitle(tr("Place a Call"));
  setModal(true);
  m_host = new QComboBox;
  m_host->setEditable(true);
  m_host->addItem(QStringLiteral("127.0.0.1"));
  for (const QString &entry : id.speedDial) {
    const auto parts = entry.split(QLatin1Char('|'));
    if (parts.size() >= 2)
      m_host->addItem(QStringLiteral("%1 (%2)").arg(parts.at(0), parts.at(1)), parts.at(1));
  }
  m_port = new QSpinBox;
  m_port->setRange(1, 65535);
  m_port->setValue(1720);

  auto *form = new QFormLayout;
  form->addRow(tr("To:"), m_host);
  form->addRow(tr("Port:"), m_port);

  auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
  buttons->button(QDialogButtonBox::Ok)->setText(tr("Call"));
  connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

  auto *lay = new QVBoxLayout(this);
  lay->addWidget(new QLabel(tr("Type an IP address or computer name, then click Call.")));
  lay->addLayout(form);
  lay->addWidget(buttons);
}

QString PlaceCallDialog::host() const
{
  const QVariant d = m_host->currentData();
  if (d.isValid() && !d.toString().isEmpty() && m_host->currentText().contains(QLatin1Char('(')))
    return d.toString();
  QString t = m_host->currentText().trimmed();
  const int paren = t.indexOf(QLatin1Char('('));
  if (paren > 0) {
    t = t.mid(paren + 1);
    t.chop(t.endsWith(QLatin1Char(')')) ? 1 : 0);
  }
  if (t.contains(QLatin1Char(':')))
    t = t.section(QLatin1Char(':'), 0, 0);
  return t;
}

quint16 PlaceCallDialog::port() const
{
  return static_cast<quint16>(m_port->value());
}

void PlaceCallDialog::setAddress(const QString &host, quint16 port)
{
  m_host->setCurrentText(host);
  m_port->setValue(port);
}

IncomingCallDialog::IncomingCallDialog(const QString &name, const QString &host, QWidget *parent)
    : QDialog(parent)
{
  setWindowTitle(tr("Incoming Call"));
  setModal(true);
  auto *lay = new QVBoxLayout(this);
  auto *icon = new QLabel;
  icon->setPixmap(appWindowIcon().pixmap(48, 48));
  auto *msg = new QLabel(tr("%1 is calling you from %2.\nWould you like to accept the call?").arg(name, host));
  msg->setWordWrap(true);
  auto *row = new QHBoxLayout;
  row->addWidget(icon);
  row->addWidget(msg, 1);
  auto *buttons = new QDialogButtonBox;
  auto *accept = buttons->addButton(tr("Accept"), QDialogButtonBox::AcceptRole);
  auto *ignore = buttons->addButton(tr("Ignore"), QDialogButtonBox::RejectRole);
  connect(accept, &QPushButton::clicked, this, &QDialog::accept);
  connect(ignore, &QPushButton::clicked, this, &QDialog::reject);
  lay->addLayout(row);
  lay->addWidget(buttons);
}

OptionsDialog::OptionsDialog(Identity id, QWidget *parent)
    : QDialog(parent)
    , m_id(std::move(id))
{
  setWindowTitle(tr("Options"));
  resize(440, 520);

  m_first = new QLineEdit(m_id.firstName);
  m_last = new QLineEdit(m_id.lastName);
  m_email = new QLineEdit(m_id.email);
  m_city = new QLineEdit(m_id.city);
  m_comments = new QLineEdit(m_id.comments);
  m_camera = new QLineEdit(m_id.cameraDevice);
  m_port = new QSpinBox;
  m_port->setRange(1, 65535);
  m_port->setValue(m_id.port);
  m_bw = new QComboBox;
  m_bw->addItems({tr("Local Area Network"), tr("Cable, xDSL, or ISDN"), tr("14400 bps or faster modem")});
  m_bw->setCurrentIndex(m_id.bandwidthIndex);
  m_video = new QCheckBox(tr("Automatically send video at the start of a call"));
  m_video->setChecked(m_id.sendVideo);
  m_audio = new QCheckBox(tr("Enable audio"));
  m_audio->setChecked(m_id.sendAudio);

  auto *myInfo = new QGroupBox(tr("My Information"));
  auto *info = new QFormLayout(myInfo);
  info->addRow(tr("First name:"), m_first);
  info->addRow(tr("Last name:"), m_last);
  info->addRow(tr("E-mail address:"), m_email);
  info->addRow(tr("City/State:"), m_city);
  info->addRow(tr("Comments:"), m_comments);

  auto *net = new QGroupBox(tr("Calling"));
  auto *nf = new QFormLayout(net);
  nf->addRow(tr("Network bandwidth:"), m_bw);
  nf->addRow(tr("Listen port:"), m_port);
  nf->addRow(tr("Camera device:"), m_camera);
  nf->addRow(m_video);
  nf->addRow(m_audio);

  m_postServer = new QLineEdit(m_id.postServer);
  m_postServer->setPlaceholderText(QString::fromUtf8(ng::kOfficialPost));
  m_postBoard = new QLineEdit(m_id.postBoard);
  m_postEnabled = new QCheckBox(tr("Log on to the directory when NetGreeting starts"));
  m_postEnabled->setChecked(m_id.postEnabled);
  m_postPublish = new QCheckBox(tr("List my name so other people can call me"));
  m_postPublish->setChecked(m_id.postPublish);
  auto *post = new QGroupBox(tr("Greeting Post (shared directory)"));
  auto *pf = new QFormLayout(post);
  pf->addRow(new QLabel(tr("Everyone using the official directory sees the same list. Leave the server as-is unless you run your own board.")));
  pf->addRow(tr("Server:"), m_postServer);
  pf->addRow(tr("Board:"), m_postBoard);
  pf->addRow(m_postEnabled);
  pf->addRow(m_postPublish);

  auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
  connect(buttons, &QDialogButtonBox::accepted, this, [this] {
    m_id.firstName = m_first->text().trimmed();
    m_id.lastName = m_last->text().trimmed();
    m_id.email = m_email->text().trimmed();
    m_id.city = m_city->text().trimmed();
    m_id.comments = m_comments->text().trimmed();
    m_id.cameraDevice = m_camera->text().trimmed();
    m_id.port = static_cast<quint16>(m_port->value());
    m_id.bandwidthIndex = m_bw->currentIndex();
    m_id.sendVideo = m_video->isChecked();
    m_id.sendAudio = m_audio->isChecked();
    m_id.postServer = m_postServer->text().trimmed();
    m_id.postBoard = m_postBoard->text().trimmed();
    if (m_id.postBoard.isEmpty())
      m_id.postBoard = QStringLiteral("lobby");
    m_id.postEnabled = m_postEnabled->isChecked();
    m_id.postPublish = m_postPublish->isChecked();
    accept();
  });
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

  auto *lay = new QVBoxLayout(this);
  lay->addWidget(myInfo);
  lay->addWidget(net);
  lay->addWidget(post);
  lay->addStretch();
  lay->addWidget(buttons);
}

DirectoryWindow::DirectoryWindow(Identity *id, LanDirectory *lan, GreetingPostClient *post,
                                 QWidget *parent)
    : QDialog(parent)
    , m_id(id)
    , m_post(post)
{
  setWindowTitle(tr("NetGreeting - Directory"));
  resize(620, 440);

  auto makeTable = [](const QStringList &headers) {
    auto *t = new QTableWidget(0, headers.size());
    t->setHorizontalHeaderLabels(headers);
    t->horizontalHeader()->setStretchLastSection(true);
    t->setSelectionBehavior(QAbstractItemView::SelectRows);
    t->setSelectionMode(QAbstractItemView::SingleSelection);
    t->verticalHeader()->setVisible(false);
    return t;
  };

  m_postTable = makeTable({tr("Name"), tr("City"), tr("Address"), tr("Port"), tr("Comments")});
  m_postTable->setProperty("emptyText", tr("Waiting for people to log on..."));
  m_lan = makeTable({tr("Name"), tr("Address"), tr("Port")});
  m_speed = makeTable({tr("Name"), tr("Address"), tr("Port")});
  m_postStatus = new QLabel;

  auto *callPost = new QPushButton(tr("Call"));
  auto *addPost = new QPushButton(tr("Add to SpeedDial"));
  auto *callLan = new QPushButton(tr("Call"));
  auto *addLan = new QPushButton(tr("Add to SpeedDial"));
  auto *callSd = new QPushButton(tr("Call SpeedDial"));
  auto *del = new QPushButton(tr("Remove"));

  auto *postPage = new QWidget;
  auto *postLay = new QVBoxLayout(postPage);
  postLay->addWidget(new QLabel(
      tr("This is the shared NetGreeting directory. Anyone who is running the app and listed here can be called.")));
  postLay->addWidget(m_postStatus);
  postLay->addWidget(m_postTable, 1);
  auto *pr = new QHBoxLayout;
  pr->addWidget(callPost);
  pr->addWidget(addPost);
  pr->addStretch();
  postLay->addLayout(pr);

  auto *lanPage = new QWidget;
  auto *lanLay = new QVBoxLayout(lanPage);
  lanLay->addWidget(new QLabel(tr("People nearby on this LAN (multicast, no server).")));
  lanLay->addWidget(m_lan, 1);
  auto *lr = new QHBoxLayout;
  lr->addWidget(callLan);
  lr->addWidget(addLan);
  lr->addStretch();
  lanLay->addLayout(lr);

  auto *sdPage = new QWidget;
  auto *sdLay = new QVBoxLayout(sdPage);
  sdLay->addWidget(m_speed, 1);
  auto *sr = new QHBoxLayout;
  sr->addWidget(callSd);
  sr->addWidget(del);
  sr->addStretch();
  sdLay->addLayout(sr);

  auto *tabs = new QTabWidget;
  tabs->addTab(postPage, tr("Greeting Post"));
  tabs->addTab(lanPage, tr("Nearby"));
  tabs->addTab(sdPage, tr("SpeedDial"));

  auto *lay = new QVBoxLayout(this);
  lay->addWidget(tabs);

  rebuildSpeedDial();
  if (lan)
    refreshLan(lan->peers());
  refreshPost();

  auto selectedCall = [](QTableWidget *t, int hostCol, int portCol) {
    const int r = t->currentRow();
    if (r < 0)
      return QPair<QString, quint16>{};
    return QPair<QString, quint16>{t->item(r, hostCol)->text(), t->item(r, portCol)->text().toUShort()};
  };
  auto addSpeed = [this](QTableWidget *t, int nameCol, int hostCol, int portCol) {
    const int r = t->currentRow();
    if (r < 0 || !m_id)
      return;
    const QString entry = QStringLiteral("%1|%2|%3")
                              .arg(t->item(r, nameCol)->text(), t->item(r, hostCol)->text(),
                                   t->item(r, portCol)->text());
    if (!m_id->speedDial.contains(entry))
      m_id->speedDial.append(entry);
    m_id->save();
    rebuildSpeedDial();
  };

  connect(callPost, &QPushButton::clicked, this, [this, selectedCall] {
    const auto p = selectedCall(m_postTable, 2, 3);
    if (!p.first.isEmpty())
      emit callRequested(p.first, p.second);
  });
  connect(addPost, &QPushButton::clicked, this, [this, addSpeed] { addSpeed(m_postTable, 0, 2, 3); });
  connect(callLan, &QPushButton::clicked, this, [this, selectedCall] {
    const auto p = selectedCall(m_lan, 1, 2);
    if (!p.first.isEmpty())
      emit callRequested(p.first, p.second);
  });
  connect(addLan, &QPushButton::clicked, this, [this, addSpeed] { addSpeed(m_lan, 0, 1, 2); });
  connect(callSd, &QPushButton::clicked, this, [this, selectedCall] {
    const auto p = selectedCall(m_speed, 1, 2);
    if (!p.first.isEmpty())
      emit callRequested(p.first, p.second);
  });
  connect(del, &QPushButton::clicked, this, [this] {
    const int r = m_speed->currentRow();
    if (r < 0 || !m_id)
      return;
    m_id->speedDial.removeAt(r);
    m_id->save();
    rebuildSpeedDial();
  });
  if (m_post) {
    connect(m_post, &GreetingPostClient::peopleChanged, this, &DirectoryWindow::refreshPost);
    connect(m_post, &GreetingPostClient::statusChanged, this, [this](const QString &s) {
      m_postStatus->setText(s);
    });
  }
}

void DirectoryWindow::refreshLan(const QList<LanPeer> &peers)
{
  m_lan->setRowCount(0);
  for (const LanPeer &p : peers) {
    const int r = m_lan->rowCount();
    m_lan->insertRow(r);
    m_lan->setItem(r, 0, new QTableWidgetItem(p.name));
    m_lan->setItem(r, 1, new QTableWidgetItem(p.host));
    m_lan->setItem(r, 2, new QTableWidgetItem(QString::number(p.port)));
  }
}

void DirectoryWindow::refreshPost()
{
  m_postTable->setRowCount(0);
  if (m_post)
    m_postStatus->setText(m_post->status());
  if (!m_post)
    return;
  for (const PostPeer &p : m_post->people()) {
    const int r = m_postTable->rowCount();
    m_postTable->insertRow(r);
    const QString name = p.self ? tr("%1 (you)").arg(p.name) : p.name;
    m_postTable->setItem(r, 0, new QTableWidgetItem(name));
    m_postTable->setItem(r, 1, new QTableWidgetItem(p.city));
    m_postTable->setItem(r, 2, new QTableWidgetItem(p.host));
    m_postTable->setItem(r, 3, new QTableWidgetItem(QString::number(p.port)));
    m_postTable->setItem(r, 4, new QTableWidgetItem(p.comments));
  }
  if (m_postTable->rowCount() == 0 && m_postStatus->text().isEmpty())
    m_postStatus->setText(tr("Waiting for people to log on to the directory..."));
}

void DirectoryWindow::rebuildSpeedDial()
{
  m_speed->setRowCount(0);
  if (!m_id)
    return;
  for (const QString &entry : m_id->speedDial) {
    const auto parts = entry.split(QLatin1Char('|'));
    const int r = m_speed->rowCount();
    m_speed->insertRow(r);
    m_speed->setItem(r, 0, new QTableWidgetItem(parts.value(0)));
    m_speed->setItem(r, 1, new QTableWidgetItem(parts.value(1)));
    m_speed->setItem(r, 2, new QTableWidgetItem(parts.value(2, QStringLiteral("1720"))));
  }
}

AboutDialog::AboutDialog(QWidget *parent)
    : QDialog(parent)
{
  setWindowTitle(tr("About NetGreeting"));
  auto *lay = new QVBoxLayout(this);
  auto *icon = new QLabel;
  icon->setPixmap(appWindowIcon().pixmap(64, 64));
  icon->setAlignment(Qt::AlignCenter);
  auto *title = new QLabel(QStringLiteral("<b>NetGreeting %1</b>").arg(QStringLiteral(NETGREETING_VERSION)));
  title->setAlignment(Qt::AlignCenter);
  auto *body = new QLabel(tr(
      "A C++ homage to Microsoft NetMeeting.\n"
      "Place a call by IP, chat, sketch on the whiteboard,\n"
      "send files, share a desktop, and wave at the test card.\n\n"
      "H.323 is left in the 1990s on purpose.\n"
      "Greeting Post is the shared directory: log on, see who is online, call them.\n"
      "Calls still go machine to machine — the directory is only a phone book."));
  body->setAlignment(Qt::AlignCenter);
  auto *ok = new QPushButton(tr("OK"));
  connect(ok, &QPushButton::clicked, this, &QDialog::accept);
  lay->addWidget(icon);
  lay->addWidget(title);
  lay->addWidget(body);
  lay->addWidget(ok, 0, Qt::AlignCenter);
}
