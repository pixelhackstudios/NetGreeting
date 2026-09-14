#include "greetingpostclient.h"
#include "protocol.h"


#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QtEndian>

#include <algorithm>
#include <cstring>

namespace {

QByteArray mqttStr(const QString &s)
{
  const QByteArray u = s.toUtf8();
  QByteArray out(2 + u.size(), Qt::Uninitialized);
  qToBigEndian(quint16(u.size()), reinterpret_cast<uchar *>(out.data()));
  memcpy(out.data() + 2, u.constData(), size_t(u.size()));
  return out;
}

QByteArray mqttRemaining(int n)
{
  QByteArray b;
  do {
    quint8 d = quint8(n % 128);
    n /= 128;
    if (n > 0)
      d |= 0x80;
    b.append(char(d));
  } while (n > 0);
  return b;
}

QByteArray mqttPacket(quint8 typeFlags, const QByteArray &payload)
{
  return QByteArray(1, char(typeFlags)) + mqttRemaining(payload.size()) + payload;
}

bool mqttDecodeRemaining(const QByteArray &buf, int offset, int &value, int &lenBytes)
{
  value = 0;
  lenBytes = 0;
  int mul = 1;
  while (true) {
    if (offset + lenBytes >= buf.size())
      return false;
    const quint8 d = quint8(buf.at(offset + lenBytes));
    ++lenBytes;
    value += (d & 127) * mul;
    if ((d & 128) == 0)
      return true;
    mul *= 128;
    if (lenBytes > 4) {
      value = -1;
      return true;
    }
  }
}

} // namespace

GreetingPostClient::GreetingPostClient(QObject *parent)
    : QObject(parent)
{
  m_timer.setInterval(15000);
  connect(&m_timer, &QTimer::timeout, this, &GreetingPostClient::heartbeat);
  m_ping.setInterval(20000);
  connect(&m_ping, &QTimer::timeout, this, &GreetingPostClient::mqttPing);
  m_expire.setInterval(5000);
  connect(&m_expire, &QTimer::timeout, this, [this] {
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    bool changed = false;
    for (auto it = m_byToken.begin(); it != m_byToken.end();) {
      if (!it->self && now - it->seenMs > ng::kPostTtlSeconds * 1000) {
        it = m_byToken.erase(it);
        changed = true;
      } else {
        ++it;
      }
    }
    if (changed)
      rebuildPeople();
  });
  connect(&m_mqtt, &QTcpSocket::connected, this, &GreetingPostClient::mqttConnected);
  connect(&m_mqtt, &QTcpSocket::readyRead, this, &GreetingPostClient::mqttReadyRead);
  connect(&m_mqtt, &QAbstractSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
    if (!m_running || !useMqtt())
      return;
    m_mqttReady = false;
    m_loggedOn = false;
    if (!m_triedFallback) {
      m_triedFallback = true;
      setStatus(tr("Directory broker retrying..."));
      mqttConnect(QString::fromUtf8(ng::kOfficialPostFallback));
      return;
    }
    setStatus(tr("Greeting Post unreachable (%1).").arg(m_mqtt.errorString()));
  });
}

void GreetingPostClient::start(const Identity &id)
{
  stop();
  m_id = id;
  if (m_id.postToken.isEmpty()) {
    m_id.postToken = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_id.save();
  }
  m_token = m_id.postToken;
  m_triedFallback = false;
  m_mqttReady = false;
  m_byToken.clear();
  m_people.clear();
  emit peopleChanged();

  if (!m_id.postEnabled) {
    setStatus(tr("Directory is off."));
    return;
  }
  m_running = true;
  m_timer.start();
  m_expire.start();
  lookupPublicIp();
  if (useMqtt())
    QTimer::singleShot(0, this, [this] { mqttConnect(m_id.postServer); });
  else
    heartbeat();
}

void GreetingPostClient::stop()
{
  m_timer.stop();
  m_ping.stop();
  m_expire.stop();
  if (m_running && m_id.postPublish) {
    if (useMqtt() && m_mqttReady)
      mqttUnpublish();
    else if (!useMqtt() && !m_token.isEmpty()) {
      QNetworkRequest req(boardUrl());
      req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
      const QByteArray body =
          QJsonDocument(QJsonObject{{QStringLiteral("token"), m_token}}).toJson(QJsonDocument::Compact);
      auto *reply = m_nam.sendCustomRequest(req, "DELETE", body);
      connect(reply, &QNetworkReply::finished, reply, &QObject::deleteLater);
    }
  }
  m_mqtt.disconnectFromHost();
  m_mqttBuf.clear();
  m_running = false;
  m_loggedOn = false;
  m_mqttReady = false;
  m_byToken.clear();
  if (!m_people.isEmpty()) {
    m_people.clear();
    emit peopleChanged();
  }
}

void GreetingPostClient::refreshNow()
{
  if (!m_running)
    return;
  heartbeat();
}

bool GreetingPostClient::useMqtt() const
{
  const QString s = m_id.postServer.trimmed().toLower();
  return s.startsWith(QLatin1String("mqtt://")) || s.startsWith(QLatin1String("mqtts://")) ||
         s.contains(QLatin1String("hivemq")) || s.contains(QLatin1String("mosquitto"));
}

QString GreetingPostClient::boardName() const
{
  QString board = m_id.postBoard.trimmed();
  if (board.isEmpty())
    board = QStringLiteral("lobby");
  return board;
}

QUrl GreetingPostClient::boardUrl() const
{
  const QString raw = m_id.postServer.trimmed();
  const bool hadScheme = raw.startsWith(QLatin1String("http://")) || raw.startsWith(QLatin1String("https://"));
  QString host = raw;
  if (!hadScheme)
    host.prepend(QStringLiteral("http://"));
  QUrl url(host);
  if (!hadScheme && url.port() < 0)
    url.setPort(ng::kPostPort);
  url.setPath(QStringLiteral("/v1/boards/%1/here").arg(boardName()));
  return url;
}

void GreetingPostClient::lookupPublicIp()
{
  auto *reply = m_nam.get(QNetworkRequest(QUrl(QStringLiteral("https://api.ipify.org"))));
  connect(reply, &QNetworkReply::finished, this, [this, reply] {
    reply->deleteLater();
    if (reply->error() == QNetworkReply::NoError) {
      m_publicIp = QString::fromUtf8(reply->readAll()).trimmed();
      if (m_mqttReady && m_id.postPublish)
        mqttPublish();
    }
  });
}

void GreetingPostClient::mqttConnect(const QString &spec)
{
  QString s = spec.trimmed();
  s.replace(QStringLiteral("mqtt://"), QString());
  s.replace(QStringLiteral("mqtts://"), QString());
  m_mqttHost = s.section(QLatin1Char(':'), 0, 0);
  m_mqttPort = quint16(s.section(QLatin1Char(':'), 1, 1).toUShort());
  if (m_mqttPort == 0)
    m_mqttPort = 1883;
  setStatus(tr("Connecting to Greeting Post..."));
  m_mqtt.abort();
  m_mqttBuf.clear();
  m_mqtt.connectToHost(m_mqttHost, m_mqttPort, QIODevice::ReadWrite, QAbstractSocket::IPv4Protocol);
}

void GreetingPostClient::mqttConnected()
{
  mqttSendConnect();
}

void GreetingPostClient::mqttSendConnect()
{
  const QString clientId = QStringLiteral("ng-") + m_token.left(12);
  QByteArray payload;
  payload += mqttStr(QStringLiteral("MQTT"));
  payload.append(char(4));
  payload.append(char(0x02)); // clean session
  payload.append(char(0x00));
  payload.append(char(30)); // keepalive
  payload += mqttStr(clientId);
  m_mqtt.write(mqttPacket(0x10, payload));
}

void GreetingPostClient::mqttSubscribe()
{
  const QString topic = QStringLiteral("%1/%2/#").arg(QString::fromUtf8(ng::kPostTopicRoot), boardName());
  QByteArray payload;
  const quint16 id = m_packetId++;
  payload.resize(2);
  qToBigEndian(id, reinterpret_cast<uchar *>(payload.data()));
  payload += mqttStr(topic);
  payload.append(char(0)); // QoS 0
  m_mqtt.write(mqttPacket(0x82, payload));
}

void GreetingPostClient::mqttPublish()
{
  if (!m_mqttReady || !m_id.postPublish)
    return;
  const QString topic =
      QStringLiteral("%1/%2/%3").arg(QString::fromUtf8(ng::kPostTopicRoot), boardName(), m_token);
  QByteArray payload = mqttStr(topic);
  payload += mqttCard();
  m_mqtt.write(mqttPacket(0x31, payload));
}

void GreetingPostClient::mqttUnpublish()
{
  const QString topic =
      QStringLiteral("%1/%2/%3").arg(QString::fromUtf8(ng::kPostTopicRoot), boardName(), m_token);
  QByteArray payload = mqttStr(topic);
  m_mqtt.write(mqttPacket(0x31, payload)); // retained empty = delete
}

void GreetingPostClient::mqttPing()
{
  if (m_mqttReady)
    m_mqtt.write(QByteArray("\xc0\x00", 2));
}

void GreetingPostClient::mqttReadyRead()
{
  m_mqttBuf += m_mqtt.readAll();
  while (m_mqttBuf.size() >= 2) {
    int remaining = 0;
    int lenBytes = 0;
    if (!mqttDecodeRemaining(m_mqttBuf, 1, remaining, lenBytes))
      return;
    if (remaining < 0) {
      m_mqttBuf.clear();
      return;
    }
    const int total = 1 + lenBytes + remaining;
    if (m_mqttBuf.size() < total)
      return;
    const quint8 type = quint8(m_mqttBuf.at(0)) >> 4;
    const QByteArray payload = m_mqttBuf.mid(1 + lenBytes, remaining);
    m_mqttBuf.remove(0, total);
    mqttHandlePacket(type, payload);
  }
}

void GreetingPostClient::mqttHandlePacket(quint8 type, const QByteArray &payload)
{
  switch (type) {
  case 2: { // CONNACK
    const quint8 rc = payload.size() >= 2 ? quint8(payload.at(1)) : 1;
    if (rc != 0) {
      setStatus(tr("Directory refused the logon."));
      return;
    }
    m_mqttReady = true;
    mqttSubscribe();
    m_ping.start();
    break;
  }
  case 9: { // SUBACK
    if (m_id.postPublish)
      mqttPublish();
    m_loggedOn = true;
    if (m_id.postPublish)
      setStatus(tr("Logged on to Greeting Post as %1.").arg(m_id.displayName()));
    else
      setStatus(tr("Browsing the Greeting Post."));
    break;
  }
  case 3: { // PUBLISH qos0
    if (payload.size() < 2)
      break;
    const quint16 tlen = qFromBigEndian<quint16>(reinterpret_cast<const uchar *>(payload.constData()));
    if (payload.size() < 2 + tlen)
      break;
    const QString topic = QString::fromUtf8(payload.mid(2, tlen));
    const QByteArray body = payload.mid(2 + tlen);
    const QString token = topic.section(QLatin1Char('/'), -1);
    if (body.isEmpty()) {
      if (m_byToken.remove(token))
        rebuildPeople();
      break;
    }
    const auto o = QJsonDocument::fromJson(body).object();
    ingestPeer(token, o);
    break;
  }
  default:
    break;
  }
}

void GreetingPostClient::ingestPeer(const QString &token, const QJsonObject &o)
{
  PostPeer p;
  p.token = token;
  p.name = o.value(QStringLiteral("name")).toString();
  p.email = o.value(QStringLiteral("email")).toString();
  p.city = o.value(QStringLiteral("city")).toString();
  p.comments = o.value(QStringLiteral("comments")).toString();
  p.host = o.value(QStringLiteral("host")).toString();
  p.port = quint16(o.value(QStringLiteral("port")).toInt());
  p.seenMs = o.value(QStringLiteral("ts")).toInteger(QDateTime::currentMSecsSinceEpoch());
  p.self = (token == m_token);
  if (p.name.isEmpty())
    return;
  m_byToken.insert(token, p);
  rebuildPeople();
}

void GreetingPostClient::rebuildPeople()
{
  const qint64 now = QDateTime::currentMSecsSinceEpoch();
  QList<PostPeer> next;
  for (PostPeer p : m_byToken) {
    p.ageSeconds = int(qMax(qint64(0), (now - p.seenMs) / 1000));
    next.append(p);
  }
  std::sort(next.begin(), next.end(), [](const PostPeer &a, const PostPeer &b) {
    if (a.self != b.self)
      return a.self;
    return a.name.localeAwareCompare(b.name) < 0;
  });
  m_people = next;
  emit peopleChanged();
}

QByteArray GreetingPostClient::mqttCard() const
{
  QJsonObject o{
      {QStringLiteral("name"), m_id.displayName()},
      {QStringLiteral("email"), m_id.email},
      {QStringLiteral("city"), m_id.city},
      {QStringLiteral("comments"), m_id.comments},
      {QStringLiteral("port"), m_id.port},
      {QStringLiteral("host"), m_publicIp},
      {QStringLiteral("ts"), QDateTime::currentMSecsSinceEpoch()},
  };
  return QJsonDocument(o).toJson(QJsonDocument::Compact);
}

void GreetingPostClient::heartbeat()
{
  if (!m_running)
    return;
  if (useMqtt()) {
    if (m_mqttReady)
      mqttPublish();
    else if (m_mqtt.state() == QAbstractSocket::UnconnectedState)
      mqttConnect(m_id.postServer);
    return;
  }
  if (!m_id.postPublish) {
    fetchOnly();
    return;
  }

  QJsonObject o{
      {QStringLiteral("name"), m_id.displayName()},
      {QStringLiteral("email"), m_id.email},
      {QStringLiteral("city"), m_id.city},
      {QStringLiteral("comments"), m_id.comments},
      {QStringLiteral("port"), m_id.port},
      {QStringLiteral("host"), m_publicIp},
  };
  if (!m_token.isEmpty())
    o.insert(QStringLiteral("token"), m_token);

  QNetworkRequest req(boardUrl());
  req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
  auto *reply = m_nam.put(req, QJsonDocument(o).toJson(QJsonDocument::Compact));
  connect(reply, &QNetworkReply::finished, this, [this, reply] {
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
      m_loggedOn = false;
      setStatus(tr("Greeting Post unreachable (%1).").arg(reply->errorString()));
      return;
    }
    const auto doc = QJsonDocument::fromJson(reply->readAll());
    const auto obj = doc.object();
    m_token = obj.value(QStringLiteral("token")).toString(m_token);
    m_loggedOn = true;
    const QString seen = obj.value(QStringLiteral("host")).toString();
    setStatus(tr("Logged on to Greeting Post as %1 (%2).").arg(m_id.displayName(), seen));
    handleList(QJsonDocument(obj).toJson());
  });
}

void GreetingPostClient::fetchOnly()
{
  auto *reply = m_nam.get(QNetworkRequest(boardUrl()));
  connect(reply, &QNetworkReply::finished, this, [this, reply] {
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
      m_loggedOn = false;
      setStatus(tr("Greeting Post unreachable (%1).").arg(reply->errorString()));
      return;
    }
    m_loggedOn = true;
    setStatus(tr("Browsing Greeting Post (not published)."));
    handleList(reply->readAll());
  });
}

void GreetingPostClient::handleList(const QByteArray &body)
{
  const auto obj = QJsonDocument::fromJson(body).object();
  const auto arr = obj.value(QStringLiteral("people")).toArray();
  m_byToken.clear();
  for (const auto &v : arr) {
    const auto o = v.toObject();
    QString key = o.value(QStringLiteral("token")).toString();
    if (key.isEmpty())
      key = QStringLiteral("%1|%2|%3")
                .arg(o.value(QStringLiteral("name")).toString(), o.value(QStringLiteral("host")).toString())
                .arg(o.value(QStringLiteral("port")).toInt());
    ingestPeer(key, o);
  }
  if (arr.isEmpty())
    rebuildPeople();
}

void GreetingPostClient::setStatus(const QString &s)
{
  if (m_status == s)
    return;
  m_status = s;
  emit statusChanged(s);
}
