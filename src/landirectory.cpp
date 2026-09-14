#include "landirectory.h"
#include "protocol.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkInterface>

#include <algorithm>

namespace {
const QHostAddress kGroup(QStringLiteral("239.255.17.20"));
}

LanDirectory::LanDirectory(QObject *parent)
    : QObject(parent)
{
  m_beacon.setInterval(3000);
  m_expire.setInterval(2000);
  connect(&m_beacon, &QTimer::timeout, this, &LanDirectory::beacon);
  connect(&m_expire, &QTimer::timeout, this, &LanDirectory::expire);
  connect(&m_sock, &QUdpSocket::readyRead, this, &LanDirectory::onReadyRead);
}

void LanDirectory::start(const QString &name, quint16 tcpPort)
{
  stop();
  m_name = name;
  m_tcpPort = tcpPort;
  const auto mode = QAbstractSocket::ShareAddress | QAbstractSocket::ReuseAddressHint;
  if (!m_sock.bind(QHostAddress::AnyIPv4, ng::kDiscoveryPort, mode)) {
    return;
  }
  for (const QNetworkInterface &iface : QNetworkInterface::allInterfaces()) {
    if ((iface.flags() & QNetworkInterface::IsUp) &&
        (iface.flags() & QNetworkInterface::IsRunning) &&
        !(iface.flags() & QNetworkInterface::IsLoopBack))
      m_sock.joinMulticastGroup(kGroup, iface);
  }
  m_sock.joinMulticastGroup(kGroup);
  m_beacon.start();
  m_expire.start();
  beacon();
}

void LanDirectory::stop()
{
  m_beacon.stop();
  m_expire.stop();
  m_sock.close();
  if (!m_peers.isEmpty()) {
    m_peers.clear();
    emit peersChanged();
  }
}

void LanDirectory::setName(const QString &name)
{
  m_name = name;
}

void LanDirectory::setTcpPort(quint16 port)
{
  m_tcpPort = port;
}

void LanDirectory::beacon()
{
  if (m_sock.state() != QAbstractSocket::BoundState)
    return;
  QJsonObject o{{QStringLiteral("name"), m_name},
                {QStringLiteral("port"), m_tcpPort}};
  QByteArray pkt = QByteArray(ng::kDiscoveryMagic);
  pkt += QJsonDocument(o).toJson(QJsonDocument::Compact);
  m_sock.writeDatagram(pkt, kGroup, ng::kDiscoveryPort);
}

void LanDirectory::onReadyRead()
{
  while (m_sock.hasPendingDatagrams()) {
    QByteArray data;
    data.resize(int(m_sock.pendingDatagramSize()));
    QHostAddress from;
    quint16 fromPort = 0;
    m_sock.readDatagram(data.data(), data.size(), &from, &fromPort);
    Q_UNUSED(fromPort);
    if (!data.startsWith(ng::kDiscoveryMagic))
      continue;
    const auto doc = QJsonDocument::fromJson(data.mid(int(sizeof(ng::kDiscoveryMagic) - 1)));
    if (!doc.isObject())
      continue;
    const auto o = doc.object();
    const quint16 port = static_cast<quint16>(o.value(QStringLiteral("port")).toInt());
    const QString name = o.value(QStringLiteral("name")).toString();
    const QString host = from.toString();
    if (port == m_tcpPort) {
      bool self = false;
      if (from.isLoopback())
        self = true;
      // Same-machine different port is not self; same port on any of our IPs is.
      Q_UNUSED(self);
    }
    if (name == m_name && port == m_tcpPort)
      continue;

    bool found = false;
    for (LanPeer &p : m_peers) {
      if (p.host == host && p.port == port) {
        p.name = name;
        p.lastSeen = QDateTime::currentDateTime();
        found = true;
        break;
      }
    }
    if (!found) {
      LanPeer p;
      p.name = name;
      p.host = host;
      p.port = port;
      p.lastSeen = QDateTime::currentDateTime();
      m_peers.append(p);
      emit peersChanged();
    }
  }
}

void LanDirectory::expire()
{
  const auto now = QDateTime::currentDateTime();
  const int before = m_peers.size();
  m_peers.erase(std::remove_if(m_peers.begin(), m_peers.end(),
                               [&](const LanPeer &p) {
                                 return p.lastSeen.msecsTo(now) > 10000;
                               }),
                m_peers.end());
  if (m_peers.size() != before)
    emit peersChanged();
}
