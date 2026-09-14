#pragma once

#include "identity.h"

#include <QHash>
#include <QObject>
#include <QNetworkAccessManager>
#include <QTcpSocket>
#include <QTimer>

struct PostPeer {
  QString name;
  QString email;
  QString city;
  QString comments;
  QString host;
  QString token;
  quint16 port = 0;
  qint64 seenMs = 0;
  int ageSeconds = 0;
  bool self = false;
};

class GreetingPostClient : public QObject {
  Q_OBJECT
public:
  explicit GreetingPostClient(QObject *parent = nullptr);

  void start(const Identity &id);
  void stop();
  void refreshNow();
  QList<PostPeer> people() const { return m_people; }
  QString status() const { return m_status; }
  bool loggedOn() const { return m_loggedOn; }

signals:
  void peopleChanged();
  void statusChanged(const QString &status);

private:
  bool useMqtt() const;
  QUrl boardUrl() const;
  void heartbeat();
  void fetchOnly();
  void handleList(const QByteArray &body);
  void setStatus(const QString &s);
  void lookupPublicIp();
  void mqttConnect(const QString &spec);
  void mqttConnected();
  void mqttReadyRead();
  void mqttSendConnect();
  void mqttSubscribe();
  void mqttPublish();
  void mqttUnpublish();
  void mqttPing();
  void mqttHandlePacket(quint8 type, const QByteArray &payload);
  void ingestPeer(const QString &token, const QJsonObject &o);
  void rebuildPeople();
  QByteArray mqttCard() const;
  QString boardName() const;

  Identity m_id;
  QNetworkAccessManager m_nam;
  QTcpSocket m_mqtt;
  QByteArray m_mqttBuf;
  QTimer m_timer;
  QTimer m_ping;
  QTimer m_expire;
  QString m_token;
  QString m_publicIp;
  QString m_mqttHost;
  quint16 m_mqttPort = 1883;
  bool m_triedFallback = false;
  quint16 m_packetId = 1;
  QHash<QString, PostPeer> m_byToken;
  QList<PostPeer> m_people;
  QString m_status;
  bool m_loggedOn = false;
  bool m_running = false;
  bool m_mqttReady = false;
};
