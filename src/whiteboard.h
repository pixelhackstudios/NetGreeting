#pragma once

#include <QColor>
#include <QDialog>
#include <QJsonObject>
#include <QPoint>
#include <QVector>
#include <QWidget>

class QButtonGroup;

struct WbItem {
  enum Type { Stroke, Line, Rect, Ellipse, Text } type = Stroke;
  QString id;
  QColor color;
  int width = 2;
  QVector<QPoint> points;
  QRect rect;
  QString text;
  bool highlighter = false;
};

class WhiteboardCanvas : public QWidget {
  Q_OBJECT
public:
  explicit WhiteboardCanvas(QWidget *parent = nullptr);
  enum Tool { Pen, Highlighter, LineTool, RectTool, EllipseTool, TextTool, Eraser, Pointer };
  void setTool(Tool t) { m_tool = t; }
  void setColor(const QColor &c) { m_color = c; }
  void setWidth(int w) { m_width = w; }
  void applyRemote(const QJsonObject &op);
  void clearAll(bool emitOp);
  QColor pointerColor() const { return m_pointerColor; }
  void setLocalName(const QString &n) { m_localName = n; }

signals:
  void opReady(const QJsonObject &op);

protected:
  void paintEvent(QPaintEvent *e) override;
  void mousePressEvent(QMouseEvent *e) override;
  void mouseMoveEvent(QMouseEvent *e) override;
  void mouseReleaseEvent(QMouseEvent *e) override;

private:
  WbItem *findItem(const QString &id);
  void addOrReplace(const WbItem &item);
  QString newId() const;
  QJsonObject itemToJson(const WbItem &it, const QString &op) const;

  Tool m_tool = Pen;
  QColor m_color{Qt::blue};
  QColor m_pointerColor{Qt::red};
  int m_width = 2;
  QVector<WbItem> m_items;
  WbItem m_draft;
  bool m_drawing = false;
  QPoint m_remotePointer;
  QString m_remotePointerName;
  bool m_hasRemotePointer = false;
  QString m_localName;
};

class WhiteboardWindow : public QDialog {
  Q_OBJECT
public:
  explicit WhiteboardWindow(QWidget *parent = nullptr);
  void applyRemote(const QJsonObject &op);
  void setLocalName(const QString &n);

signals:
  void opReady(const QJsonObject &op);

private:
  WhiteboardCanvas *m_canvas = nullptr;
};
