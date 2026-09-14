#include "style.h"

#include <QApplication>
#include <QFont>
#include <QIcon>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QWidget>

void applyNetGreetingStyle(QApplication &app)
{
  app.setStyle(QStringLiteral("Fusion"));

  QPalette pal;
  const QColor window(212, 208, 200);
  const QColor highlight(10, 36, 106);
  pal.setColor(QPalette::Window, window);
  pal.setColor(QPalette::WindowText, Qt::black);
  pal.setColor(QPalette::Base, Qt::white);
  pal.setColor(QPalette::AlternateBase, QColor(230, 226, 218));
  pal.setColor(QPalette::Text, Qt::black);
  pal.setColor(QPalette::Button, window);
  pal.setColor(QPalette::ButtonText, Qt::black);
  pal.setColor(QPalette::Highlight, highlight);
  pal.setColor(QPalette::HighlightedText, Qt::white);
  pal.setColor(QPalette::Light, QColor(255, 255, 255));
  pal.setColor(QPalette::Midlight, QColor(233, 231, 227));
  pal.setColor(QPalette::Dark, QColor(128, 128, 128));
  pal.setColor(QPalette::Mid, QColor(160, 160, 160));
  pal.setColor(QPalette::Shadow, QColor(64, 64, 64));
  pal.setColor(QPalette::ToolTipBase, QColor(255, 255, 225));
  pal.setColor(QPalette::ToolTipText, Qt::black);
  pal.setColor(QPalette::Link, highlight);
  pal.setColor(QPalette::PlaceholderText, QColor(120, 120, 120));
  app.setPalette(pal);

  QFont f = app.font();
  f.setPointSize(9);
  app.setFont(f);

  app.setStyleSheet(QStringLiteral(
      "QMainWindow, QDialog, QWidget { background-color: #d4d0c8; }"
      "QMenuBar { background: #d4d0c8; }"
      "QMenuBar::item:selected { background: #0a246a; color: white; }"
      "QStatusBar { background: #d4d0c8; border-top: 1px solid #808080; }"
      "QLineEdit, QPlainTextEdit, QTextEdit, QListWidget, QTableWidget, QComboBox {"
      "  background: white; border: 1px solid #808080; border-top-color: #404040;"
      "  border-left-color: #404040; border-right-color: #ffffff; border-bottom-color: #ffffff;"
      "}"
      "QPushButton, QToolButton {"
      "  background: #d4d0c8; border: 2px solid #ffffff; border-right-color: #404040;"
      "  border-bottom-color: #404040; padding: 3px 10px; min-height: 18px;"
      "}"
      "QPushButton:pressed, QToolButton:pressed {"
      "  border: 2px solid #404040; border-right-color: #ffffff; border-bottom-color: #ffffff;"
      "}"
      "QPushButton:default { font-weight: bold; }"
      "QGroupBox { border: 2px groove #808080; margin-top: 0.6em; padding: 6px; }"
      "QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 4px; }"
      "QHeaderView::section { background: #d4d0c8; border: 1px solid #808080; padding: 3px; }"));
}

void bevelPanel(QWidget *w)
{
  w->setStyleSheet(QStringLiteral(
      "background: #d4d0c8; border: 2px solid #808080;"
      "border-top-color: #ffffff; border-left-color: #ffffff;"
      "border-right-color: #404040; border-bottom-color: #404040;"));
}

QIcon appWindowIcon()
{
  return QIcon(QStringLiteral(":/icons/netgreeting.png"));
}

QIcon netGreetingIcon(const QString &name)
{
  QPixmap pm(24, 24);
  pm.fill(Qt::transparent);
  QPainter p(&pm);
  p.setRenderHint(QPainter::Antialiasing, true);

  auto phone = [&] {
    p.setBrush(QColor(32, 96, 48));
    p.setPen(QPen(QColor(16, 48, 24), 1));
    p.drawRoundedRect(QRectF(5, 3, 14, 18), 3, 3);
    p.setPen(QPen(Qt::white, 2));
    p.drawArc(QRectF(8, 6, 8, 8), 30 * 16, 120 * 16);
  };

  if (name == QLatin1String("call")) {
    phone();
  } else if (name == QLatin1String("hangup")) {
    p.setBrush(QColor(160, 32, 32));
    p.setPen(QPen(QColor(80, 0, 0), 1));
    p.drawRoundedRect(QRectF(5, 7, 14, 10), 3, 3);
    p.setPen(QPen(Qt::white, 2));
    p.drawLine(QPointF(8, 12), QPointF(16, 12));
  } else if (name == QLatin1String("directory")) {
    p.setBrush(QColor(200, 180, 80));
    p.setPen(QPen(QColor(80, 60, 20), 1));
    p.drawRoundedRect(QRectF(4, 4, 16, 16), 2, 2);
    p.setPen(QPen(QColor(80, 60, 20), 1));
    p.drawLine(8, 8, 16, 8);
    p.drawLine(8, 12, 16, 12);
    p.drawLine(8, 16, 14, 16);
  } else if (name == QLatin1String("chat")) {
    p.setBrush(QColor(255, 255, 220));
    p.setPen(QPen(QColor(10, 36, 106), 1.4));
    p.drawRoundedRect(QRectF(3, 3, 16, 12), 2, 2);
    QPolygonF tail;
    tail << QPointF(7, 15) << QPointF(7, 21) << QPointF(12, 15);
    p.drawPolygon(tail);
  } else if (name == QLatin1String("whiteboard")) {
    p.setBrush(Qt::white);
    p.setPen(QPen(QColor(80, 80, 80), 1));
    p.drawRect(QRectF(4, 4, 16, 16));
    p.setPen(QPen(QColor(0, 80, 180), 2));
    p.drawLine(7, 16, 11, 8);
    p.setPen(QPen(QColor(180, 0, 0), 2));
    p.drawEllipse(QRectF(12, 10, 5, 5));
  } else if (name == QLatin1String("file")) {
    p.setBrush(QColor(250, 250, 240));
    p.setPen(QPen(QColor(80, 80, 80), 1));
    p.drawRect(QRectF(6, 3, 12, 16));
    p.drawLine(6, 3, 12, 3);
    p.drawLine(12, 3, 12, 8);
    p.drawLine(12, 8, 6, 8);
    p.setPen(QPen(QColor(0, 100, 0), 2));
    p.drawLine(9, 14, 15, 18);
    p.drawLine(15, 18, 12, 18);
    p.drawLine(15, 18, 15, 15);
  } else if (name == QLatin1String("share")) {
    p.setBrush(QColor(40, 40, 40));
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(QRectF(3, 4, 18, 13), 1, 1);
    p.setBrush(QColor(80, 160, 220));
    p.drawRect(QRectF(5, 6, 14, 9));
    p.setBrush(QColor(80, 80, 80));
    p.drawRect(QRectF(9, 17, 6, 2));
    p.drawRect(QRectF(6, 19, 12, 2));
  } else if (name == QLatin1String("pip")) {
    p.setBrush(Qt::black);
    p.drawRect(QRectF(3, 4, 18, 16));
    p.setBrush(QColor(0, 80, 160));
    p.drawRect(QRectF(12, 12, 8, 6));
  } else if (name == QLatin1String("hold")) {
    p.setBrush(QColor(160, 120, 20));
    p.setPen(QPen(QColor(80, 60, 0), 1));
    p.drawRoundedRect(QRectF(5, 3, 14, 18), 3, 3);
    p.setBrush(Qt::white);
    p.setPen(Qt::NoPen);
    p.drawRect(QRectF(9, 7, 3, 10));
    p.drawRect(QRectF(13, 7, 3, 10));
  } else if (name == QLatin1String("resume")) {
    p.setBrush(QColor(32, 96, 48));
    p.setPen(QPen(QColor(16, 48, 24), 1));
    p.drawRoundedRect(QRectF(5, 3, 14, 18), 3, 3);
    QPolygonF tri;
    tri << QPointF(10, 7) << QPointF(10, 17) << QPointF(16, 12);
    p.setBrush(Qt::white);
    p.setPen(Qt::NoPen);
    p.drawPolygon(tri);
  } else {
    p.setBrush(QColor(10, 36, 106));
    p.drawEllipse(QRectF(4, 4, 16, 16));
  }
  p.end();
  return QIcon(pm);
}
