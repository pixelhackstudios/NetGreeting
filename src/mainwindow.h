#pragma once

#include "identity.h"

#include <QImage>
#include <QMainWindow>

class Conference;
class LanDirectory;
class CameraCapture;
class AudioEngine;
class ChatWindow;
class WhiteboardWindow;
class FileTransferWindow;
class ShareWindow;
class DirectoryWindow;
class GreetingPostClient;
class VideoPane;
class QLabel;
class QListWidget;
class QStackedWidget;
class QLineEdit;
class QTimer;

class MainWindow : public QMainWindow {
  Q_OBJECT
public:
  explicit MainWindow(Identity id, QWidget *parent = nullptr);
  ~MainWindow() override;

  void placeCall(const QString &host, quint16 port);
  void setAutoAccept(bool on) { m_autoAccept = on; }

protected:
  void closeEvent(QCloseEvent *e) override;

private:
  void buildUi();
  void buildMenus();
  void applyIdentity();
  void startMedia();
  void stopMedia();
  void updateStatus();
  void showPlaceCall();
  void showDirectory();
  void showOptions();
  void setCompact(bool on);
  void setDataOnly(bool on);
  QByteArray jpegOf(const QImage &img, int quality, int w, int h) const;
  void sendMediaTick();
  void sendShareTick();

  Identity m_id;
  Conference *m_conf = nullptr;
  LanDirectory *m_lan = nullptr;
  CameraCapture *m_camera = nullptr;
  AudioEngine *m_audio = nullptr;
  ChatWindow *m_chat = nullptr;
  WhiteboardWindow *m_board = nullptr;
  FileTransferWindow *m_files = nullptr;
  ShareWindow *m_share = nullptr;
  DirectoryWindow *m_dir = nullptr;
  GreetingPostClient *m_post = nullptr;

  VideoPane *m_video = nullptr;
  QLabel *m_nameBanner = nullptr;
  QListWidget *m_people = nullptr;
  QStackedWidget *m_previewStack = nullptr;
  QWidget *m_dialPad = nullptr;
  QLineEdit *m_dialField = nullptr;
  QLabel *m_status = nullptr;
  QTimer *m_mediaTimer = nullptr;
  QTimer *m_shareTimer = nullptr;
  QImage m_localFrame;
  QImage m_remoteFrame;
  bool m_sharing = false;
  bool m_dataOnly = false;
  bool m_showDialPad = false;
  bool m_autoAccept = false;
};
