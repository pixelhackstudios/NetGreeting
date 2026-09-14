#pragma once

#include <QDateTime>
#include <QHostAddress>
#include <QObject>
#include <QTimer>
#include <QUdpSocket>

struct LanPeer {
  QString name;
  QString host;
  quint16 port = 0;
  QDateTime lastSeen;
};

class LanDirectory : public QObject {
  Q_OBJECT
public:
  explicit LanDirectory(QObject *parent = nullptr);

  void start(const QString &name, quint16 tcpPort);
  void stop();
  void setName(const QString &name);
  void setTcpPort(quint16 port);
  QList<LanPeer> peers() const { return m_peers; }

signals:
  void peersChanged();

private:
  void beacon();
  void onReadyRead();
  void expire();

  QUdpSocket m_sock;
  QTimer m_beacon;
  QTimer m_expire;
  QString m_name;
  quint16 m_tcpPort = 0;
  QList<LanPeer> m_peers;
};
