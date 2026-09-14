#pragma once

#include "identity.h"
#include "protocol.h"

#include <QFile>
#include <QHash>
#include <QHostAddress>
#include <QImage>
#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>

class Conference : public QObject {
  Q_OBJECT
public:
  explicit Conference(QObject *parent = nullptr);

  void setIdentity(const Identity &id);
  Identity identity() const { return m_id; }

  bool startListening(quint16 port);
  void stopListening();
  quint16 listenPort() const { return m_listenPort; }
  bool isListening() const { return m_server.isListening(); }

  void placeCall(const QString &host, quint16 port);
  void acceptIncoming();
  void rejectIncoming();
  void hangup(const QString &reason = QStringLiteral("Hang up"));

  bool inCall() const { return m_inCall; }
  QString peerName() const { return m_primaryName; }
  QStringList participants() const;

  void sendChat(const QString &text, const QString &whisperTo = {});
  void sendWhiteboard(const QJsonObject &op);
  void sendVideoJpeg(const QByteArray &jpeg);
  void sendAudioPcm(const QByteArray &pcm);
  void sendShareJpeg(const QByteArray &jpeg);
  void sendHold(bool on);

  quint32 offerFile(const QString &path);
  void acceptFile(quint32 transferId, const QString &savePath);
  void rejectFile(quint32 transferId);

  static QStringList localIPv4Addresses();

signals:
  void listeningChanged(bool on, quint16 port);
  void incomingCall(const QString &name, const QString &host, quint16 port);
  void callStarted(const QString &peerName, const QString &peerHost);
  void callEnded(const QString &reason);
  void statusMessage(const QString &msg);
  void chatReceived(const QString &from, const QString &text, bool whisper);
  void whiteboardOp(const QJsonObject &op);
  void videoFrameReceived(const QImage &img);
  void shareFrameReceived(const QImage &img);
  void audioFrameReceived(const QByteArray &pcm);
  void participantsChanged();
  void peerHoldChanged(bool onHold, const QString &from);
  void fileOffered(quint32 id, const QString &from, const QString &name, qint64 size);
  void fileProgress(quint32 id, const QString &name, qint64 received, qint64 total, bool outbound);
  void fileFinished(quint32 id, const QString &name, const QString &path, bool ok, const QString &error);
  void errorOccurred(const QString &err);

private:
  struct Peer {
    QTcpSocket *sock = nullptr;
    QByteArray buf;
    QString name;
    QString email;
    QHostAddress addr;
    quint16 peerListenPort = 0;
    bool helloed = false;
    bool inConference = false;
  };

  struct InboundFile {
    quint32 id = 0;
    QString from;
    QString name;
    qint64 size = 0;
    qint64 received = 0;
    QString savePath;
    QFile *file = nullptr;
    bool accepted = false;
    Peer *fromPeer = nullptr;
  };

  struct OutboundFile {
    quint32 id = 0;
    QString path;
    QString name;
    qint64 size = 0;
    qint64 sent = 0;
    Peer *to = nullptr;
    bool accepted = false;
  };

  Peer *peerFor(QTcpSocket *sock);
  Peer *primaryPeer();
  void sendTo(Peer *p, ng::Msg type, const QByteArray &payload);
  void broadcast(ng::Msg type, const QByteArray &payload, Peer *except = nullptr);
  void handleMessage(Peer *p, ng::Msg type, const QByteArray &payload);
  void onNewConnection();
  void onReadyRead();
  void onDisconnected();
  void onSocketError();
  void dropPeer(Peer *p, const QString &reason);
  void beginConference(Peer *p);
  void sendHello(Peer *p, bool ack);
  QByteArray helloPayload(bool invite) const;
  void sendParticipantList();
  void pumpOutboundFile();
  void clearFiles();

  Identity m_id;
  QTcpServer m_server;
  quint16 m_listenPort = ng::kDefaultPort;
  QList<Peer *> m_peers;
  QPointer<QTcpSocket> m_outgoing;
  bool m_inCall = false;
  bool m_placing = false;
  bool m_peerOnHold = false;
  QString m_primaryName;
  QString m_pendingName;
  QString m_pendingHost;
  Peer *m_pendingPeer = nullptr;
  QTimer m_ping;
  QTimer m_fileTimer;
  quint32 m_nextFileId = 1;
  QHash<quint32, InboundFile> m_inbound;
  QHash<quint32, OutboundFile> m_outbound;
};
