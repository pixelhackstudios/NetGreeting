#pragma once

#include "protocol.h"

#include <QSettings>
#include <QString>
#include <QStringList>
#include <QUuid>

struct Identity {
  QString firstName{QStringLiteral("User")};
  QString lastName;
  QString email;
  QString comments;
  QString city;
  quint16 port{1720};
  QString cameraDevice{QStringLiteral("/dev/video0")};
  bool sendVideo{true};
  bool sendAudio{true};
  int bandwidthIndex{0}; // 0 LAN, 1 Cable/DSL, 2 56k
  QStringList speedDial; // "Name|host|port"
  QString postServer{QString::fromUtf8(ng::kOfficialPost)};
  QString postBoard{QStringLiteral("lobby")};
  bool postEnabled{true};
  bool postPublish{true};
  QString postToken;

  QString displayName() const
  {
    const QString n = (firstName + QLatin1Char(' ') + lastName).trimmed();
    return n.isEmpty() ? QStringLiteral("NetGreeting User") : n;
  }

  int jpegQuality() const
  {
    switch (bandwidthIndex) {
    case 1: return 50;
    case 2: return 28;
    default: return 70;
    }
  }

  int videoFps() const
  {
    switch (bandwidthIndex) {
    case 1: return 6;
    case 2: return 3;
    default: return 10;
    }
  }

  int videoWidth() const { return bandwidthIndex >= 2 ? 176 : 320; }
  int videoHeight() const { return bandwidthIndex >= 2 ? 144 : 240; }

  static Identity load()
  {
    QSettings s(QStringLiteral("NetGreeting"), QStringLiteral("NetGreeting"));
    Identity i;
    i.firstName = s.value(QStringLiteral("firstName"), QStringLiteral("User")).toString();
    i.lastName = s.value(QStringLiteral("lastName")).toString();
    i.email = s.value(QStringLiteral("email")).toString();
    i.comments = s.value(QStringLiteral("comments")).toString();
    i.city = s.value(QStringLiteral("city")).toString();
    i.port = static_cast<quint16>(s.value(QStringLiteral("port"), 1720).toUInt());
    i.cameraDevice = s.value(QStringLiteral("cameraDevice"), QStringLiteral("/dev/video0")).toString();
    i.sendVideo = s.value(QStringLiteral("sendVideo"), true).toBool();
    i.sendAudio = s.value(QStringLiteral("sendAudio"), true).toBool();
    i.bandwidthIndex = s.value(QStringLiteral("bandwidthIndex"), 0).toInt();
    i.speedDial = s.value(QStringLiteral("speedDial")).toStringList();
    i.postServer = s.value(QStringLiteral("postServer"), QString::fromUtf8(ng::kOfficialPost)).toString();
    i.postBoard = s.value(QStringLiteral("postBoard"), QStringLiteral("lobby")).toString();
    i.postEnabled = s.value(QStringLiteral("postEnabled"), true).toBool();
    i.postPublish = s.value(QStringLiteral("postPublish"), true).toBool();
    i.postToken = s.value(QStringLiteral("postToken")).toString();
    bool persist = false;
    if (i.postServer == QLatin1String("127.0.0.1:1730") || i.postServer.trimmed().isEmpty()) {
      i.postServer = QString::fromUtf8(ng::kOfficialPost);
      persist = true;
    }
    if (i.postToken.isEmpty()) {
      i.postToken = QUuid::createUuid().toString(QUuid::WithoutBraces);
      persist = true;
    }
    if (persist)
      i.save();
    return i;
  }

  void save() const
  {
    QSettings s(QStringLiteral("NetGreeting"), QStringLiteral("NetGreeting"));
    s.setValue(QStringLiteral("firstName"), firstName);
    s.setValue(QStringLiteral("lastName"), lastName);
    s.setValue(QStringLiteral("email"), email);
    s.setValue(QStringLiteral("comments"), comments);
    s.setValue(QStringLiteral("city"), city);
    s.setValue(QStringLiteral("port"), port);
    s.setValue(QStringLiteral("cameraDevice"), cameraDevice);
    s.setValue(QStringLiteral("sendVideo"), sendVideo);
    s.setValue(QStringLiteral("sendAudio"), sendAudio);
    s.setValue(QStringLiteral("bandwidthIndex"), bandwidthIndex);
    s.setValue(QStringLiteral("speedDial"), speedDial);
    s.setValue(QStringLiteral("postServer"), postServer);
    s.setValue(QStringLiteral("postBoard"), postBoard);
    s.setValue(QStringLiteral("postEnabled"), postEnabled);
    s.setValue(QStringLiteral("postPublish"), postPublish);
    s.setValue(QStringLiteral("postToken"), postToken);
  }
};
