#pragma once

#include "greetingpostclient.h"
#include "identity.h"
#include "landirectory.h"

#include <QDialog>

class QLineEdit;
class QSpinBox;
class QComboBox;
class QCheckBox;
class QTableWidget;
class QLabel;

class PlaceCallDialog : public QDialog {
  Q_OBJECT
public:
  explicit PlaceCallDialog(const Identity &id, QWidget *parent = nullptr);
  QString host() const;
  quint16 port() const;
  void setAddress(const QString &host, quint16 port);

private:
  QComboBox *m_host = nullptr;
  QSpinBox *m_port = nullptr;
};

class IncomingCallDialog : public QDialog {
  Q_OBJECT
public:
  IncomingCallDialog(const QString &name, const QString &host, QWidget *parent = nullptr);
};

class OptionsDialog : public QDialog {
  Q_OBJECT
public:
  explicit OptionsDialog(Identity id, QWidget *parent = nullptr);
  Identity identity() const { return m_id; }

private:
  Identity m_id;
  QLineEdit *m_first = nullptr;
  QLineEdit *m_last = nullptr;
  QLineEdit *m_email = nullptr;
  QLineEdit *m_city = nullptr;
  QLineEdit *m_comments = nullptr;
  QLineEdit *m_camera = nullptr;
  QSpinBox *m_port = nullptr;
  QComboBox *m_bw = nullptr;
  QCheckBox *m_video = nullptr;
  QCheckBox *m_audio = nullptr;
  QLineEdit *m_postServer = nullptr;
  QLineEdit *m_postBoard = nullptr;
  QCheckBox *m_postEnabled = nullptr;
  QCheckBox *m_postPublish = nullptr;
};

class DirectoryWindow : public QDialog {
  Q_OBJECT
public:
  explicit DirectoryWindow(Identity *id, LanDirectory *lan, GreetingPostClient *post,
                           QWidget *parent = nullptr);
  void refreshLan(const QList<LanPeer> &peers);
  void refreshPost();

signals:
  void callRequested(const QString &host, quint16 port);

private:
  void rebuildSpeedDial();
  Identity *m_id = nullptr;
  GreetingPostClient *m_post = nullptr;
  QTableWidget *m_postTable = nullptr;
  QTableWidget *m_lan = nullptr;
  QTableWidget *m_speed = nullptr;
  QLabel *m_postStatus = nullptr;
};

class AboutDialog : public QDialog {
  Q_OBJECT
public:
  explicit AboutDialog(QWidget *parent = nullptr);
};
