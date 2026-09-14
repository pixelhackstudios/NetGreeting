#include "conference.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkInterface>

namespace {

QByteArray jsonBytes(const QJsonObject &o)
{
  return QJsonDocument(o).toJson(QJsonDocument::Compact);
}

QJsonObject jsonParse(const QByteArray &b)
{
  const auto doc = QJsonDocument::fromJson(b);
  return doc.isObject() ? doc.object() : QJsonObject{};
}

} // namespace

Conference::Conference(QObject *parent)
    : QObject(parent)
{
  connect(&m_server, &QTcpServer::newConnection, this, &Conference::onNewConnection);

  m_ping.setInterval(8000);
  connect(&m_ping, &QTimer::timeout, this, [this] {
    if (!m_inCall)
      return;
    for (Peer *p : m_peers) {
      if (p->inConference)
        sendTo(p, ng::Msg::Ping, {});
    }
  });

  m_fileTimer.setInterval(20);
  connect(&m_fileTimer, &QTimer::timeout, this, &Conference::pumpOutboundFile);
}

void Conference::setIdentity(const Identity &id)
{
  m_id = id;
}

QStringList Conference::localIPv4Addresses()
{
  QStringList out;
  for (const QNetworkInterface &iface : QNetworkInterface::allInterfaces()) {
    if (!(iface.flags() & QNetworkInterface::IsUp) ||
        !(iface.flags() & QNetworkInterface::IsRunning) ||
        (iface.flags() & QNetworkInterface::IsLoopBack))
      continue;
    for (const QNetworkAddressEntry &e : iface.addressEntries()) {
      if (e.ip().protocol() == QAbstractSocket::IPv4Protocol)
        out << e.ip().toString();
    }
  }
  if (out.isEmpty())
    out << QStringLiteral("127.0.0.1");
  return out;
}

bool Conference::startListening(quint16 port)
{
  stopListening();
  if (!m_server.listen(QHostAddress::AnyIPv4, port)) {
    emit errorOccurred(tr("Could not listen on port %1: %2").arg(port).arg(m_server.errorString()));
    emit listeningChanged(false, 0);
    return false;
  }
  m_listenPort = port;
  emit listeningChanged(true, port);
  emit statusMessage(tr("Waiting for a call on port %1.").arg(port));
  return true;
}

void Conference::stopListening()
{
  hangup(QStringLiteral("Listener stopped"));
  m_server.close();
  emit listeningChanged(false, 0);
}

void Conference::placeCall(const QString &host, quint16 port)
{
  if (m_inCall || m_placing) {
    emit errorOccurred(tr("Already in a call."));
    return;
  }
  auto *sock = new QTcpSocket(this);
  m_outgoing = sock;
  m_placing = true;
  emit statusMessage(tr("Calling %1:%2...").arg(host).arg(port));

  auto *p = new Peer;
  p->sock = sock;
  p->addr = QHostAddress(host);
  m_peers.append(p);

  connect(sock, &QTcpSocket::connected, this, [this, p] {
    sendHello(p, false);
    sendTo(p, ng::Msg::CallInvite, helloPayload(true));
  });
  connect(sock, &QTcpSocket::readyRead, this, &Conference::onReadyRead);
  connect(sock, &QTcpSocket::disconnected, this, &Conference::onDisconnected);
  connect(sock, &QTcpSocket::errorOccurred, this, &Conference::onSocketError);
  sock->connectToHost(host, port);
}

void Conference::acceptIncoming()
{
  if (!m_pendingPeer)
    return;
  sendHello(m_pendingPeer, true);
  sendTo(m_pendingPeer, ng::Msg::CallAccept, helloPayload(false));
  beginConference(m_pendingPeer);
  m_pendingPeer = nullptr;
}

void Conference::rejectIncoming()
{
  if (!m_pendingPeer)
    return;
  sendTo(m_pendingPeer, ng::Msg::CallReject, QByteArray("busy"));
  dropPeer(m_pendingPeer, QStringLiteral("Call declined"));
  m_pendingPeer = nullptr;
}

void Conference::hangup(const QString &reason)
{
  for (Peer *p : std::as_const(m_peers)) {
    if (p->sock && (p->inConference || m_placing))
      sendTo(p, ng::Msg::Hangup, reason.toUtf8());
  }
  while (!m_peers.isEmpty())
    dropPeer(m_peers.first(), reason);
  m_placing = false;
  m_pendingPeer = nullptr;
  m_outgoing = nullptr;
}

QStringList Conference::participants() const
{
  QStringList names;
  names << m_id.displayName();
  for (Peer *p : m_peers) {
    if (p->inConference && !p->name.isEmpty())
      names << p->name;
  }
  return names;
}

void Conference::sendChat(const QString &text, const QString &whisperTo)
{
  QJsonObject o{{QStringLiteral("from"), m_id.displayName()},
                {QStringLiteral("text"), text},
                {QStringLiteral("whisperTo"), whisperTo}};
  const auto type = whisperTo.isEmpty() ? ng::Msg::Chat : ng::Msg::ChatWhisper;
  broadcast(type, jsonBytes(o));
}

void Conference::sendWhiteboard(const QJsonObject &op)
{
  broadcast(ng::Msg::Whiteboard, jsonBytes(op));
}

void Conference::sendVideoJpeg(const QByteArray &jpeg)
{
  if (jpeg.isEmpty())
    return;
  if (Peer *p = primaryPeer())
    sendTo(p, ng::Msg::VideoFrame, jpeg);
}

void Conference::sendAudioPcm(const QByteArray &pcm)
{
  if (pcm.isEmpty())
    return;
  if (Peer *p = primaryPeer())
    sendTo(p, ng::Msg::AudioFrame, pcm);
}

void Conference::sendShareJpeg(const QByteArray &jpeg)
{
  if (jpeg.isEmpty())
    return;
  broadcast(ng::Msg::ShareFrame, jpeg);
}

quint32 Conference::offerFile(const QString &path)
{
  QFileInfo fi(path);
  if (!fi.exists() || !fi.isFile()) {
    emit errorOccurred(tr("File not found: %1").arg(path));
    return 0;
  }
  Peer *p = primaryPeer();
  if (!p) {
    emit errorOccurred(tr("Not in a call."));
    return 0;
  }
  OutboundFile of;
  of.id = m_nextFileId++;
  of.path = path;
  of.name = fi.fileName();
  of.size = fi.size();
  of.to = p;
  m_outbound.insert(of.id, of);
  QJsonObject o{{QStringLiteral("id"), static_cast<qint64>(of.id)},
                {QStringLiteral("name"), of.name},
                {QStringLiteral("size"), of.size}};
  sendTo(p, ng::Msg::FileOffer, jsonBytes(o));
  emit fileProgress(of.id, of.name, 0, of.size, true);
  return of.id;
}

void Conference::acceptFile(quint32 transferId, const QString &savePath)
{
  auto it = m_inbound.find(transferId);
  if (it == m_inbound.end())
    return;
  it->savePath = savePath;
  it->file = new QFile(savePath, this);
  if (!it->file->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    emit fileFinished(transferId, it->name, savePath, false, it->file->errorString());
    delete it->file;
    it->file = nullptr;
    m_inbound.erase(it);
    return;
  }
  it->accepted = true;
  if (it->fromPeer)
    sendTo(it->fromPeer, ng::Msg::FileAccept, jsonBytes({{QStringLiteral("id"), static_cast<qint64>(transferId)}}));
}

void Conference::rejectFile(quint32 transferId)
{
  auto it = m_inbound.find(transferId);
  if (it == m_inbound.end())
    return;
  if (it->fromPeer)
    sendTo(it->fromPeer, ng::Msg::FileReject, jsonBytes({{QStringLiteral("id"), static_cast<qint64>(transferId)}}));
  m_inbound.erase(it);
}

Conference::Peer *Conference::peerFor(QTcpSocket *sock)
{
  for (Peer *p : m_peers) {
    if (p->sock == sock)
      return p;
  }
  return nullptr;
}

Conference::Peer *Conference::primaryPeer()
{
  for (Peer *p : m_peers) {
    if (p->inConference)
      return p;
  }
  return nullptr;
}

void Conference::sendTo(Peer *p, ng::Msg type, const QByteArray &payload)
{
  if (!p || !p->sock || p->sock->state() != QAbstractSocket::ConnectedState)
    return;
  p->sock->write(ng::frameMessage(type, payload));
}

void Conference::broadcast(ng::Msg type, const QByteArray &payload, Peer *except)
{
  for (Peer *p : m_peers) {
    if (p != except && p->inConference)
      sendTo(p, type, payload);
  }
}

void Conference::onNewConnection()
{
  while (m_server.hasPendingConnections()) {
    QTcpSocket *sock = m_server.nextPendingConnection();
    auto *p = new Peer;
    p->sock = sock;
    p->addr = sock->peerAddress();
    m_peers.append(p);
    connect(sock, &QTcpSocket::readyRead, this, &Conference::onReadyRead);
    connect(sock, &QTcpSocket::disconnected, this, &Conference::onDisconnected);
    connect(sock, &QTcpSocket::errorOccurred, this, &Conference::onSocketError);
  }
}

void Conference::onReadyRead()
{
  auto *sock = qobject_cast<QTcpSocket *>(sender());
  Peer *p = peerFor(sock);
  if (!p)
    return;
  p->buf.append(sock->readAll());
  ng::Msg type{};
  QByteArray payload;
  while (ng::decodeFrame(p->buf, type, payload))
    handleMessage(p, type, payload);
}

void Conference::onDisconnected()
{
  auto *sock = qobject_cast<QTcpSocket *>(sender());
  if (Peer *p = peerFor(sock))
    dropPeer(p, tr("%1 disconnected").arg(p->name.isEmpty() ? tr("Peer") : p->name));
}

void Conference::onSocketError()
{
  auto *sock = qobject_cast<QTcpSocket *>(sender());
  if (!sock)
    return;
  if (sock->error() == QAbstractSocket::RemoteHostClosedError)
    return;
  emit errorOccurred(tr("Network error: %1").arg(sock->errorString()));
  if (Peer *p = peerFor(sock))
    dropPeer(p, sock->errorString());
}

void Conference::dropPeer(Peer *p, const QString &reason)
{
  if (!p)
    return;
  const bool wasIn = p->inConference;
  if (p->sock) {
    p->sock->disconnect(this);
    p->sock->deleteLater();
  }
  m_peers.removeOne(p);
  if (m_pendingPeer == p)
    m_pendingPeer = nullptr;
  delete p;

  if (wasIn) {
    sendParticipantList();
    emit participantsChanged();
    if (!primaryPeer()) {
      m_inCall = false;
      m_placing = false;
      m_primaryName.clear();
      m_ping.stop();
      m_fileTimer.stop();
      clearFiles();
      emit callEnded(reason);
    }
  } else if (m_placing && m_peers.isEmpty()) {
    m_placing = false;
    emit callEnded(reason);
  }
}

void Conference::beginConference(Peer *p)
{
  p->inConference = true;
  m_inCall = true;
  m_placing = false;
  m_primaryName = p->name;
  m_ping.start();
  m_fileTimer.start();
  sendParticipantList();
  emit participantsChanged();
  emit callStarted(p->name, p->addr.toString());
  emit statusMessage(tr("In a call with %1.").arg(p->name));
}

void Conference::sendHello(Peer *p, bool ack)
{
  sendTo(p, ack ? ng::Msg::HelloAck : ng::Msg::Hello, helloPayload(false));
}

QByteArray Conference::helloPayload(bool invite) const
{
  QJsonObject o{{QStringLiteral("name"), m_id.displayName()},
                {QStringLiteral("email"), m_id.email},
                {QStringLiteral("city"), m_id.city},
                {QStringLiteral("comments"), m_id.comments},
                {QStringLiteral("version"), ng::kProtocolVersion},
                {QStringLiteral("listenPort"), m_listenPort},
                {QStringLiteral("invite"), invite}};
  return jsonBytes(o);
}

void Conference::sendParticipantList()
{
  QJsonArray arr;
  for (const QString &n : participants())
    arr.append(n);
  broadcast(ng::Msg::Participants, jsonBytes({{QStringLiteral("names"), arr}}));
}

void Conference::handleMessage(Peer *p, ng::Msg type, const QByteArray &payload)
{
  switch (type) {
  case ng::Msg::Hello:
  case ng::Msg::HelloAck: {
    const auto o = jsonParse(payload);
    p->name = o.value(QStringLiteral("name")).toString(tr("Unknown"));
    p->email = o.value(QStringLiteral("email")).toString();
    p->peerListenPort = static_cast<quint16>(o.value(QStringLiteral("listenPort")).toInt());
    p->helloed = true;
    if (type == ng::Msg::Hello)
      sendHello(p, true);
    break;
  }
  case ng::Msg::CallInvite: {
    const auto o = jsonParse(payload);
    if (!p->helloed) {
      p->name = o.value(QStringLiteral("name")).toString(tr("Unknown"));
      p->helloed = true;
    }
    if (m_inCall) {
      sendTo(p, ng::Msg::CallReject, QByteArray("busy"));
      return;
    }
    m_pendingPeer = p;
    emit incomingCall(p->name, p->addr.toString(), static_cast<quint16>(p->sock ? p->sock->peerPort() : 0));
    break;
  }
  case ng::Msg::CallAccept:
    if (!p->helloed) {
      const auto o = jsonParse(payload);
      p->name = o.value(QStringLiteral("name")).toString(p->name);
    }
    beginConference(p);
    break;
  case ng::Msg::CallReject:
    emit statusMessage(tr("Call declined."));
    dropPeer(p, tr("Call declined"));
    break;
  case ng::Msg::Hangup:
    dropPeer(p, payload.isEmpty() ? tr("Remote hang up") : QString::fromUtf8(payload));
    break;
  case ng::Msg::Chat:
  case ng::Msg::ChatWhisper: {
    const auto o = jsonParse(payload);
    const QString from = o.value(QStringLiteral("from")).toString(p->name);
    const QString text = o.value(QStringLiteral("text")).toString();
    const QString whisperTo = o.value(QStringLiteral("whisperTo")).toString();
    const bool whisper = type == ng::Msg::ChatWhisper;
    if (whisper && whisperTo != m_id.displayName() && from != m_id.displayName())
      break;
    emit chatReceived(from, text, whisper);
    broadcast(type, payload, p);
    break;
  }
  case ng::Msg::Whiteboard:
    emit whiteboardOp(jsonParse(payload));
    broadcast(ng::Msg::Whiteboard, payload, p);
    break;
  case ng::Msg::VideoFrame: {
    QImage img;
    if (img.loadFromData(payload, "JPEG"))
      emit videoFrameReceived(img);
    break;
  }
  case ng::Msg::AudioFrame:
    emit audioFrameReceived(payload);
    break;
  case ng::Msg::ShareFrame: {
    QImage img;
    if (img.loadFromData(payload, "JPEG"))
      emit shareFrameReceived(img);
    break;
  }
  case ng::Msg::FileOffer: {
    const auto o = jsonParse(payload);
    InboundFile inf;
    inf.id = static_cast<quint32>(o.value(QStringLiteral("id")).toInteger());
    inf.name = o.value(QStringLiteral("name")).toString();
    inf.size = o.value(QStringLiteral("size")).toInteger();
    inf.from = p->name;
    inf.fromPeer = p;
    m_inbound.insert(inf.id, inf);
    emit fileOffered(inf.id, inf.from, inf.name, inf.size);
    break;
  }
  case ng::Msg::FileAccept: {
    const auto o = jsonParse(payload);
    const quint32 id = static_cast<quint32>(o.value(QStringLiteral("id")).toInteger());
    auto it = m_outbound.find(id);
    if (it != m_outbound.end())
      it->accepted = true;
    emit statusMessage(tr("File transfer accepted."));
    break;
  }
  case ng::Msg::FileReject: {
    const auto o = jsonParse(payload);
    const quint32 id = static_cast<quint32>(o.value(QStringLiteral("id")).toInteger());
    auto it = m_outbound.find(id);
    if (it != m_outbound.end()) {
      emit fileFinished(id, it->name, it->path, false, tr("Remote declined the file."));
      m_outbound.erase(it);
    }
    break;
  }
  case ng::Msg::FileChunk: {
    if (payload.size() < 12)
      break;
    const quint32 id = qFromBigEndian<quint32>(reinterpret_cast<const uchar *>(payload.constData()));
    const auto it = m_inbound.find(id);
    if (it == m_inbound.end() || !it->accepted || !it->file)
      break;
    const QByteArray chunk = payload.mid(12);
    it->file->write(chunk);
    it->received += chunk.size();
    emit fileProgress(id, it->name, it->received, it->size, false);
    break;
  }
  case ng::Msg::FileComplete: {
    const auto o = jsonParse(payload);
    const quint32 id = static_cast<quint32>(o.value(QStringLiteral("id")).toInteger());
    auto it = m_inbound.find(id);
    if (it == m_inbound.end())
      break;
    if (it->file) {
      it->file->close();
      it->file->deleteLater();
    }
    emit fileFinished(id, it->name, it->savePath, true, {});
    m_inbound.erase(it);
    break;
  }
  case ng::Msg::Participants: {
    emit participantsChanged();
    break;
  }
  case ng::Msg::Ping:
    sendTo(p, ng::Msg::Pong, {});
    break;
  case ng::Msg::Pong:
    break;
  }
}

void Conference::pumpOutboundFile()
{
  if (m_outbound.isEmpty())
    return;
  auto it = m_outbound.begin();
  while (it != m_outbound.end() && !it->accepted)
    ++it;
  if (it == m_outbound.end())
    return;
  OutboundFile &of = it.value();
  QFile f(of.path);
  if (!f.open(QIODevice::ReadOnly)) {
    emit fileFinished(of.id, of.name, of.path, false, f.errorString());
    m_outbound.erase(it);
    return;
  }
  if (!f.seek(of.sent)) {
    emit fileFinished(of.id, of.name, of.path, false, tr("Seek failed"));
    m_outbound.erase(it);
    return;
  }
  const QByteArray chunk = f.read(32 * 1024);
  if (chunk.isEmpty()) {
    sendTo(of.to, ng::Msg::FileComplete, jsonBytes({{QStringLiteral("id"), static_cast<qint64>(of.id)}}));
    emit fileFinished(of.id, of.name, of.path, true, {});
    m_outbound.erase(it);
    return;
  }
  QByteArray payload(12, Qt::Uninitialized);
  qToBigEndian(of.id, reinterpret_cast<uchar *>(payload.data()));
  qToBigEndian(static_cast<quint32>(of.sent >> 32), reinterpret_cast<uchar *>(payload.data() + 4));
  qToBigEndian(static_cast<quint32>(of.sent & 0xffffffffu), reinterpret_cast<uchar *>(payload.data() + 8));
  payload.append(chunk);
  sendTo(of.to, ng::Msg::FileChunk, payload);
  of.sent += chunk.size();
  emit fileProgress(of.id, of.name, of.sent, of.size, true);
}

void Conference::clearFiles()
{
  for (auto &inf : m_inbound) {
    if (inf.file) {
      inf.file->close();
      inf.file->deleteLater();
    }
  }
  m_inbound.clear();
  m_outbound.clear();
}
