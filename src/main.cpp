#include "identity.h"
#include "mainwindow.h"
#include "style.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QTimer>

int main(int argc, char *argv[])
{
  QApplication app(argc, argv);
  app.setApplicationName(QStringLiteral("NetGreeting"));
  app.setApplicationVersion(QStringLiteral(NETGREETING_VERSION));
  app.setOrganizationName(QStringLiteral("NetGreeting"));
  applyNetGreetingStyle(app);
  app.setWindowIcon(appWindowIcon());

  QCommandLineParser parser;
  parser.setApplicationDescription(QStringLiteral("A C++ homage to Microsoft NetMeeting"));
  parser.addHelpOption();
  parser.addVersionOption();
  QCommandLineOption portOpt({QStringLiteral("p"), QStringLiteral("port")},
                             QStringLiteral("TCP listen port (default 1720)."),
                             QStringLiteral("port"));
  QCommandLineOption nameOpt({QStringLiteral("n"), QStringLiteral("name")},
                             QStringLiteral("Display first name."),
                             QStringLiteral("name"));
  QCommandLineOption callOpt({QStringLiteral("c"), QStringLiteral("call")},
                             QStringLiteral("Place a call on startup (host[:port])."),
                             QStringLiteral("address"));
  QCommandLineOption autoAcceptOpt(QStringLiteral("auto-accept"),
                                   QStringLiteral("Accept incoming calls without a prompt."));
  parser.addOption(portOpt);
  parser.addOption(nameOpt);
  parser.addOption(callOpt);
  parser.addOption(autoAcceptOpt);
  parser.process(app);

  Identity id = Identity::load();
  if (parser.isSet(portOpt))
    id.port = static_cast<quint16>(parser.value(portOpt).toUShort());
  if (parser.isSet(nameOpt))
    id.firstName = parser.value(nameOpt);

  MainWindow w(id);
  w.setAutoAccept(parser.isSet(autoAcceptOpt));
  w.show();

  if (parser.isSet(callOpt)) {
    const QString addr = parser.value(callOpt);
    QString host = addr;
    quint16 port = id.port;
    if (addr.contains(QLatin1Char(':'))) {
      host = addr.section(QLatin1Char(':'), 0, 0);
      port = addr.section(QLatin1Char(':'), 1, 1).toUShort();
    }
    QTimer::singleShot(400, &w, [&w, host, port] { w.placeCall(host, port); });
  }

  return app.exec();
}
