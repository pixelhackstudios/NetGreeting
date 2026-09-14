#include "protocol.h"

#include <cstdio>

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QHostAddress>
#include <QDateTime>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QUuid>

#include <algorithm>

static QString escapeHtml(QString s)
{
  s.replace(QLatin1Char('&'), QStringLiteral("&amp;"));
  s.replace(QLatin1Char('<'), QStringLiteral("&lt;"));
  s.replace(QLatin1Char('>'), QStringLiteral("&gt;"));
  s.replace(QLatin1Char('"'), QStringLiteral("&quot;"));
  return s;
}

static QString peerIpv4(const QHostAddress &addr)
{
  QHostAddress a = addr;
  if (a.protocol() == QAbstractSocket::IPv6Protocol) {
    bool ok = false;
    const quint32 v4 = a.toIPv4Address(&ok);
    if (ok)
      a = QHostAddress(v4);
  }
  return a.toString();
}

static QByteArray http(int code, const QByteArray &ctype, const QByteArray &body)
{
  const char *reason = "OK";
  if (code == 400)
    reason = "Bad Request";
  else if (code == 404)
    reason = "Not Found";
  else if (code == 413)
    reason = "Payload Too Large";
  QByteArray out;
  out += "HTTP/1.0 ";
  out += QByteArray::number(code);
  out += ' ';
  out += reason;
  out += "\r\nContent-Type: ";
  out += ctype;
  out += "\r\nContent-Length: ";
  out += QByteArray::number(body.size());
  out += "\r\nAccess-Control-Allow-Origin: *";
  out += "\r\nAccess-Control-Allow-Methods: GET, PUT, POST, DELETE, OPTIONS";
  out += "\r\nConnection: close\r\n\r\n";
  out += body;
  return out;
}

struct Card {
  QString token;
  QString name;
  QString email;
  QString city;
  QString comments;
  QString host;
  quint16 port = 0;
  qint64 lastSeen = 0;
};

class GreetingPost : public QObject {
  Q_OBJECT
public:
  explicit GreetingPost(int ttlSeconds, QObject *parent = nullptr)
      : QObject(parent)
      , m_ttlMs(ttlSeconds * 1000)
  {
    connect(&m_server, &QTcpServer::newConnection, this, &GreetingPost::onNew);
    m_expire.setInterval(5000);
    connect(&m_expire, &QTimer::timeout, this, &GreetingPost::expire);
  }

  bool listen(quint16 port)
  {
    if (!m_server.listen(QHostAddress::Any, port))
      return false;
    m_expire.start();
    return true;
  }

  QString errorString() const { return m_server.errorString(); }
  quint16 port() const { return m_server.serverPort(); }

private:
  struct Conn {
    QByteArray buf;
  };

  void onNew()
  {
    while (m_server.hasPendingConnections()) {
      QTcpSocket *sock = m_server.nextPendingConnection();
      sock->setProperty("buf", QByteArray());
      connect(sock, &QTcpSocket::readyRead, this, &GreetingPost::onRead);
      connect(sock, &QTcpSocket::disconnected, sock, &QObject::deleteLater);
    }
  }

  void onRead()
  {
    auto *sock = qobject_cast<QTcpSocket *>(sender());
    if (!sock)
      return;
    QByteArray buf = sock->property("buf").toByteArray();
    buf += sock->readAll();
    const int hdrEnd = buf.indexOf("\r\n\r\n");
    if (hdrEnd < 0) {
      if (buf.size() > 16 * 1024) {
        sock->write(http(413, "text/plain", "too large"));
        sock->disconnectFromHost();
      } else {
        sock->setProperty("buf", buf);
      }
      return;
    }
    const QByteArray header = buf.left(hdrEnd);
    const QList<QByteArray> lines = header.split('\n');
    if (lines.isEmpty()) {
      sock->write(http(400, "text/plain", "bad request"));
      sock->disconnectFromHost();
      return;
    }
    const QByteArray req = lines.first().trimmed();
    const QList<QByteArray> parts = req.split(' ');
    if (parts.size() < 2) {
      sock->write(http(400, "text/plain", "bad request"));
      sock->disconnectFromHost();
      return;
    }
    const QByteArray method = parts.at(0).toUpper();
    QByteArray path = parts.at(1);
    const int qpos = path.indexOf('?');
    if (qpos >= 0)
      path = path.left(qpos);

    int contentLength = 0;
    QByteArray forwarded;
    for (int i = 1; i < lines.size(); ++i) {
      const QByteArray line = lines.at(i).trimmed();
      const int colon = line.indexOf(':');
      if (colon < 0)
        continue;
      const QByteArray key = line.left(colon).trimmed().toLower();
      const QByteArray val = line.mid(colon + 1).trimmed();
      if (key == "content-length")
        contentLength = val.toInt();
      if (key == "x-forwarded-for")
        forwarded = val.split(',').first().trimmed();
    }
    if (contentLength < 0 || contentLength > 8 * 1024) {
      sock->write(http(413, "text/plain", "too large"));
      sock->disconnectFromHost();
      return;
    }
    const QByteArray body = buf.mid(hdrEnd + 4);
    if (body.size() < contentLength) {
      sock->setProperty("buf", buf);
      return;
    }

    const QString seen = forwarded.isEmpty() ? peerIpv4(sock->peerAddress())
                                             : QString::fromLatin1(forwarded);
    sock->write(handle(method, path, body.left(contentLength), seen));
    sock->disconnectFromHost();
  }

  QByteArray handle(const QByteArray &method, const QByteArray &path, const QByteArray &body,
                    const QString &seenHost)
  {
    if (method == "OPTIONS")
      return http(200, "text/plain", {});

    if (method == "GET" && (path == "/" || path == "/health")) {
      if (path == "/health")
        return jsonOk(QJsonObject{{QStringLiteral("ok"), true},
                                  {QStringLiteral("name"), QStringLiteral("Greeting Post")},
                                  {QStringLiteral("boards"), int(m_boards.size())}});
      return http(200, "text/html; charset=utf-8", htmlIndex());
    }

    static const QRegularExpression re(QStringLiteral("^/v1/boards/([A-Za-z0-9_-]{1,32})/here$"));
    const auto m = re.match(QString::fromUtf8(path));
    if (!m.hasMatch())
      return http(404, "application/json", "{\"error\":\"not found\"}");
    const QString board = m.captured(1);

    if (method == "GET")
      return jsonOk(listObject(board));

    if (method == "PUT" || method == "POST") {
      const auto o = QJsonDocument::fromJson(body).object();
      if (o.isEmpty() && !body.isEmpty())
        return http(400, "application/json", "{\"error\":\"invalid json\"}");
      Card c;
      c.token = o.value(QStringLiteral("token")).toString();
      if (c.token.isEmpty())
        c.token = QUuid::createUuid().toString(QUuid::WithoutBraces);
      c.name = o.value(QStringLiteral("name")).toString().left(80);
      if (c.name.trimmed().isEmpty())
        c.name = QStringLiteral("NetGreeting User");
      c.email = o.value(QStringLiteral("email")).toString().left(120);
      c.city = o.value(QStringLiteral("city")).toString().left(80);
      c.comments = o.value(QStringLiteral("comments")).toString().left(160);
      c.port = static_cast<quint16>(o.value(QStringLiteral("port")).toInt());
      if (c.port == 0)
        c.port = ng::kDefaultPort;
      const QString advertised = o.value(QStringLiteral("host")).toString().trimmed();
      c.host = advertised.isEmpty() ? seenHost : advertised.left(64);
      c.lastSeen = QDateTime::currentMSecsSinceEpoch();
      upsert(board, c);
      QJsonObject out = listObject(board);
      out.insert(QStringLiteral("ok"), true);
      out.insert(QStringLiteral("token"), c.token);
      out.insert(QStringLiteral("host"), c.host);
      return jsonOk(out);
    }

    if (method == "DELETE") {
      const auto o = QJsonDocument::fromJson(body).object();
      const QString token = o.value(QStringLiteral("token")).toString();
      auto &cards = m_boards[board];
      cards.erase(std::remove_if(cards.begin(), cards.end(),
                                 [&](const Card &c) { return c.token == token; }),
                  cards.end());
      return jsonOk({{QStringLiteral("ok"), true}});
    }

    return http(400, "application/json", "{\"error\":\"unsupported method\"}");
  }

  void upsert(const QString &board, const Card &c)
  {
    auto &cards = m_boards[board];
    for (Card &e : cards) {
      if (e.token == c.token || (e.host == c.host && e.port == c.port && e.name == c.name)) {
        e = c;
        return;
      }
    }
    if (cards.size() >= 500)
      cards.removeFirst();
    cards.append(c);
  }

  QJsonObject listObject(const QString &board) const
  {
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    QJsonArray people;
    for (const Card &c : m_boards.value(board)) {
      const int age = int((now - c.lastSeen) / 1000);
      people.append(QJsonObject{
          {QStringLiteral("name"), c.name},
          {QStringLiteral("email"), c.email},
          {QStringLiteral("city"), c.city},
          {QStringLiteral("comments"), c.comments},
          {QStringLiteral("host"), c.host},
          {QStringLiteral("port"), c.port},
          {QStringLiteral("age"), age},
      });
    }
    return {{QStringLiteral("board"), board},
            {QStringLiteral("ttl"), m_ttlMs / 1000},
            {QStringLiteral("people"), people}};
  }

  QByteArray jsonOk(const QJsonObject &o) const
  {
    return http(200, "application/json", QJsonDocument(o).toJson(QJsonDocument::Compact));
  }

  QByteArray htmlIndex() const
  {
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    QString html;
    html += QStringLiteral(
        "<!DOCTYPE html><html><head><meta charset=utf-8><title>Greeting Post</title>"
        "<style>body{font:14px sans-serif;background:#d4d0c8;color:#000;margin:24px}"
        "h1{font-size:20px}table{border-collapse:collapse;background:#fff}"
        "th,td{border:1px solid #808080;padding:4px 8px;text-align:left}"
        "th{background:#0a246a;color:#fff}</style></head><body>"
        "<h1>Greeting Post</h1>"
        "<p>A bulletin board, not a switchboard. Pin your card, pick a name, call them yourself.</p>");
    QStringList boards = m_boards.keys();
    if (boards.isEmpty())
      boards << QStringLiteral("lobby");
    for (const QString &board : boards) {
      html += QStringLiteral("<h2>Board: %1</h2><table><tr>"
                             "<th>Name</th><th>City</th><th>Address</th><th>Port</th>"
                             "<th>Comments</th><th>Age</th></tr>")
                  .arg(escapeHtml(board));
      const auto cards = m_boards.value(board);
      if (cards.isEmpty())
        html += QStringLiteral("<tr><td colspan=6><i>Nobody is posted here.</i></td></tr>");
      for (const Card &c : cards) {
        html += QStringLiteral("<tr><td>%1</td><td>%2</td><td>%3</td><td>%4</td><td>%5</td><td>%6s</td></tr>")
                    .arg(escapeHtml(c.name), escapeHtml(c.city), escapeHtml(c.host),
                         QString::number(c.port), escapeHtml(c.comments),
                         QString::number(int((now - c.lastSeen) / 1000)));
      }
      html += QStringLiteral("</table>");
    }
    html += QStringLiteral("</body></html>");
    return html.toUtf8();
  }

  void expire()
  {
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    for (auto it = m_boards.begin(); it != m_boards.end();) {
      auto &cards = it.value();
      cards.erase(std::remove_if(cards.begin(), cards.end(),
                                 [&](const Card &c) { return now - c.lastSeen > m_ttlMs; }),
                  cards.end());
      if (cards.isEmpty())
        it = m_boards.erase(it);
      else
        ++it;
    }
  }

  QTcpServer m_server;
  QTimer m_expire;
  QHash<QString, QList<Card>> m_boards;
  qint64 m_ttlMs = 45000;
};

int main(int argc, char *argv[])
{
  QCoreApplication app(argc, argv);
  app.setApplicationName(QStringLiteral("GreetingPost"));
  app.setApplicationVersion(QStringLiteral(NETGREETING_VERSION));

  QCommandLineParser parser;
  parser.setApplicationDescription(
      QStringLiteral("Greeting Post — a tiny ILS-style bulletin board for NetGreeting"));
  parser.addHelpOption();
  parser.addVersionOption();
  QCommandLineOption portOpt({QStringLiteral("p"), QStringLiteral("port")},
                             QStringLiteral("Listen port (default 1730)."), QStringLiteral("port"),
                             QStringLiteral("1730"));
  QCommandLineOption ttlOpt(QStringLiteral("ttl"),
                            QStringLiteral("Seconds before a silent card falls off."),
                            QStringLiteral("seconds"), QStringLiteral("45"));
  parser.addOption(portOpt);
  parser.addOption(ttlOpt);
  parser.process(app);

  const quint16 port = static_cast<quint16>(parser.value(portOpt).toUShort());
  const int ttl = parser.value(ttlOpt).toInt();
  GreetingPost post(ttl < 5 ? 5 : ttl);
  if (!post.listen(port)) {
    fprintf(stderr, "Greeting Post could not listen on %u: %s\n", port,
            qPrintable(post.errorString()));
    return 1;
  }
  fprintf(stdout, "Greeting Post is up on http://0.0.0.0:%u/  (boards at /v1/boards/<name>/here)\n",
          post.port());
  fflush(stdout);
  return app.exec();
}

#include "greetingpost.moc"
