#pragma once

#include <QByteArray>
#include <QtEndian>
#include <QString>

namespace ng {

inline constexpr quint16 kDefaultPort = 1720;
inline constexpr quint16 kDiscoveryPort = 1729;
inline constexpr quint16 kPostPort = 1730;
inline constexpr int kPostTtlSeconds = 45;
inline constexpr char kOfficialPost[] = "mqtt://broker.hivemq.com:1883";
inline constexpr char kOfficialPostFallback[] = "mqtt://test.mosquitto.org:1883";
inline constexpr char kPostTopicRoot[] = "netgreeting/v1";
inline constexpr int kMaxMessage = 4 * 1024 * 1024;
inline constexpr char kDiscoveryMagic[] = "NGRT";
inline constexpr int kProtocolVersion = 1;

enum class Msg : quint8 {
  Hello = 1,
  HelloAck = 2,
  CallInvite = 3,
  CallAccept = 4,
  CallReject = 5,
  Hangup = 6,
  Chat = 10,
  ChatWhisper = 11,
  Whiteboard = 20,
  FileOffer = 30,
  FileAccept = 31,
  FileReject = 32,
  FileChunk = 33,
  FileComplete = 34,
  VideoFrame = 40,
  AudioFrame = 41,
  ShareFrame = 42,
  Participants = 50,
  Ping = 60,
  Pong = 61,
};

inline QByteArray frameMessage(Msg type, const QByteArray &payload)
{
  QByteArray body;
  body.reserve(1 + payload.size());
  body.append(static_cast<char>(type));
  body.append(payload);

  QByteArray out(4, Qt::Uninitialized);
  qToBigEndian(static_cast<quint32>(body.size()), reinterpret_cast<uchar *>(out.data()));
  out.append(body);
  return out;
}

inline bool decodeFrame(QByteArray &buffer, Msg &type, QByteArray &payload)
{
  if (buffer.size() < 5)
    return false;

  const quint32 len = qFromBigEndian<quint32>(reinterpret_cast<const uchar *>(buffer.constData()));
  if (len == 0 || len > static_cast<quint32>(kMaxMessage)) {
    buffer.clear();
    return false;
  }
  if (buffer.size() < 4 + static_cast<int>(len))
    return false;

  const QByteArray msg = buffer.mid(4, static_cast<int>(len));
  buffer.remove(0, 4 + static_cast<int>(len));
  type = static_cast<Msg>(static_cast<quint8>(msg.at(0)));
  payload = msg.mid(1);
  return true;
}

inline QString msgName(Msg m)
{
  switch (m) {
  case Msg::Hello: return QStringLiteral("Hello");
  case Msg::HelloAck: return QStringLiteral("HelloAck");
  case Msg::CallInvite: return QStringLiteral("CallInvite");
  case Msg::CallAccept: return QStringLiteral("CallAccept");
  case Msg::CallReject: return QStringLiteral("CallReject");
  case Msg::Hangup: return QStringLiteral("Hangup");
  case Msg::Chat: return QStringLiteral("Chat");
  case Msg::ChatWhisper: return QStringLiteral("ChatWhisper");
  case Msg::Whiteboard: return QStringLiteral("Whiteboard");
  case Msg::FileOffer: return QStringLiteral("FileOffer");
  case Msg::FileAccept: return QStringLiteral("FileAccept");
  case Msg::FileReject: return QStringLiteral("FileReject");
  case Msg::FileChunk: return QStringLiteral("FileChunk");
  case Msg::FileComplete: return QStringLiteral("FileComplete");
  case Msg::VideoFrame: return QStringLiteral("VideoFrame");
  case Msg::AudioFrame: return QStringLiteral("AudioFrame");
  case Msg::ShareFrame: return QStringLiteral("ShareFrame");
  case Msg::Participants: return QStringLiteral("Participants");
  case Msg::Ping: return QStringLiteral("Ping");
  case Msg::Pong: return QStringLiteral("Pong");
  }
  return QStringLiteral("Unknown");
}

} // namespace ng
