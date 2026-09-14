#include "whiteboard.h"

#include <QButtonGroup>
#include <QInputDialog>
#include <QJsonArray>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QToolButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QUuid>
#include <QLabel>

#include <algorithm>

WhiteboardCanvas::WhiteboardCanvas(QWidget *parent)
    : QWidget(parent)
{
  setMinimumSize(480, 360);
  setBackgroundRole(QPalette::Base);
  setAutoFillBackground(true);
  setMouseTracking(true);
  QPalette pal = palette();
  pal.setColor(QPalette::Window, Qt::white);
  pal.setColor(QPalette::Base, Qt::white);
  setPalette(pal);
}

WbItem *WhiteboardCanvas::findItem(const QString &id)
{
  for (WbItem &it : m_items) {
    if (it.id == id)
      return &it;
  }
  return nullptr;
}

void WhiteboardCanvas::addOrReplace(const WbItem &item)
{
  if (WbItem *e = findItem(item.id))
    *e = item;
  else
    m_items.append(item);
}

QString WhiteboardCanvas::newId() const
{
  return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QJsonObject WhiteboardCanvas::itemToJson(const WbItem &it, const QString &op) const
{
  QJsonArray pts;
  for (const QPoint &pt : it.points)
    pts.append(QJsonArray{pt.x(), pt.y()});
  return QJsonObject{
      {QStringLiteral("op"), op},
      {QStringLiteral("id"), it.id},
      {QStringLiteral("type"), int(it.type)},
      {QStringLiteral("color"), it.color.name(QColor::HexRgb)},
      {QStringLiteral("width"), it.width},
      {QStringLiteral("highlighter"), it.highlighter},
      {QStringLiteral("points"), pts},
      {QStringLiteral("x"), it.rect.x()},
      {QStringLiteral("y"), it.rect.y()},
      {QStringLiteral("w"), it.rect.width()},
      {QStringLiteral("h"), it.rect.height()},
      {QStringLiteral("text"), it.text},
  };
}

static WbItem itemFromJson(const QJsonObject &o)
{
  WbItem it;
  it.id = o.value(QStringLiteral("id")).toString();
  it.type = static_cast<WbItem::Type>(o.value(QStringLiteral("type")).toInt());
  it.color = QColor(o.value(QStringLiteral("color")).toString());
  it.width = o.value(QStringLiteral("width")).toInt(2);
  it.highlighter = o.value(QStringLiteral("highlighter")).toBool();
  it.rect = QRect(o.value(QStringLiteral("x")).toInt(), o.value(QStringLiteral("y")).toInt(),
                  o.value(QStringLiteral("w")).toInt(), o.value(QStringLiteral("h")).toInt());
  it.text = o.value(QStringLiteral("text")).toString();
  const auto pts = o.value(QStringLiteral("points")).toArray();
  for (const auto &v : pts) {
    const auto a = v.toArray();
    if (a.size() >= 2)
      it.points.append(QPoint(a.at(0).toInt(), a.at(1).toInt()));
  }
  return it;
}

void WhiteboardCanvas::applyRemote(const QJsonObject &op)
{
  const QString kind = op.value(QStringLiteral("op")).toString();
  if (kind == QLatin1String("clear")) {
    m_items.clear();
  } else if (kind == QLatin1String("pointer")) {
    m_remotePointer = QPoint(op.value(QStringLiteral("x")).toInt(), op.value(QStringLiteral("y")).toInt());
    m_remotePointerName = op.value(QStringLiteral("name")).toString();
    m_hasRemotePointer = true;
  } else if (kind == QLatin1String("remove")) {
    const QString id = op.value(QStringLiteral("id")).toString();
    m_items.erase(std::remove_if(m_items.begin(), m_items.end(),
                                 [&](const WbItem &it) { return it.id == id; }),
                  m_items.end());
  } else {
    addOrReplace(itemFromJson(op));
  }
  update();
}

void WhiteboardCanvas::clearAll(bool emitOp)
{
  m_items.clear();
  update();
  if (emitOp)
    emit opReady({{QStringLiteral("op"), QStringLiteral("clear")}});
}

void WhiteboardCanvas::paintEvent(QPaintEvent *)
{
  QPainter p(this);
  p.fillRect(rect(), Qt::white);
  p.setRenderHint(QPainter::Antialiasing, true);

  auto drawItem = [&](const WbItem &it) {
    QColor c = it.color;
    if (it.highlighter)
      c.setAlpha(90);
    QPen pen(c, it.highlighter ? 12 : it.width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    switch (it.type) {
    case WbItem::Stroke:
      if (it.points.size() == 1)
        p.drawPoint(it.points.first());
      else
        p.drawPolyline(it.points.constData(), it.points.size());
      break;
    case WbItem::Line:
      p.drawLine(it.rect.topLeft(), it.rect.bottomRight());
      break;
    case WbItem::Rect:
      p.drawRect(it.rect.normalized());
      break;
    case WbItem::Ellipse:
      p.drawEllipse(it.rect.normalized());
      break;
    case WbItem::Text:
      p.setPen(it.color);
      p.drawText(it.rect.topLeft(), it.text);
      break;
    }
  };

  for (const WbItem &it : m_items)
    drawItem(it);
  if (m_drawing)
    drawItem(m_draft);

  if (m_hasRemotePointer) {
    p.setBrush(QColor(255, 80, 80));
    p.setPen(Qt::black);
    QPolygon poly;
    poly << m_remotePointer << m_remotePointer + QPoint(12, 4) << m_remotePointer + QPoint(4, 12);
    p.drawPolygon(poly);
    p.drawText(m_remotePointer + QPoint(14, 12), m_remotePointerName);
  }
}

void WhiteboardCanvas::mousePressEvent(QMouseEvent *e)
{
  if (e->button() != Qt::LeftButton)
    return;
  m_drawing = true;
  m_draft = {};
  m_draft.id = newId();
  m_draft.color = m_color;
  m_draft.width = m_width;
  m_draft.points << e->pos();
  m_draft.rect = QRect(e->pos(), e->pos());
  switch (m_tool) {
  case Pen:
    m_draft.type = WbItem::Stroke;
    break;
  case Highlighter:
    m_draft.type = WbItem::Stroke;
    m_draft.highlighter = true;
    m_draft.width = 12;
    break;
  case LineTool:
    m_draft.type = WbItem::Line;
    break;
  case RectTool:
    m_draft.type = WbItem::Rect;
    break;
  case EllipseTool:
    m_draft.type = WbItem::Ellipse;
    break;
  case TextTool: {
    m_drawing = false;
    const QString t = QInputDialog::getText(this, tr("Whiteboard text"), tr("Text:"));
    if (t.isEmpty())
      return;
    m_draft.type = WbItem::Text;
    m_draft.text = t;
    m_draft.rect = QRect(e->pos(), QSize(200, 24));
    addOrReplace(m_draft);
    emit opReady(itemToJson(m_draft, QStringLiteral("item")));
    update();
    return;
  }
  case Eraser: {
    m_drawing = false;
    for (int i = m_items.size() - 1; i >= 0; --i) {
      const WbItem &it = m_items.at(i);
      bool hit = it.rect.normalized().adjusted(-6, -6, 6, 6).contains(e->pos());
      for (const QPoint &pt : it.points)
        hit = hit || (pt - e->pos()).manhattanLength() < 12;
      if (hit) {
        const QString id = it.id;
        m_items.removeAt(i);
        emit opReady({{QStringLiteral("op"), QStringLiteral("remove")}, {QStringLiteral("id"), id}});
        update();
        return;
      }
    }
    return;
  }
  case Pointer:
    m_drawing = false;
    emit opReady({{QStringLiteral("op"), QStringLiteral("pointer")},
                  {QStringLiteral("x"), e->pos().x()},
                  {QStringLiteral("y"), e->pos().y()},
                  {QStringLiteral("name"), m_localName}});
    return;
  }
  update();
}

void WhiteboardCanvas::mouseMoveEvent(QMouseEvent *e)
{
  if (m_tool == Pointer) {
    emit opReady({{QStringLiteral("op"), QStringLiteral("pointer")},
                  {QStringLiteral("x"), e->pos().x()},
                  {QStringLiteral("y"), e->pos().y()},
                  {QStringLiteral("name"), m_localName}});
  }
  if (!m_drawing)
    return;
  if (m_draft.type == WbItem::Stroke)
    m_draft.points.append(e->pos());
  else
    m_draft.rect.setBottomRight(e->pos());
  update();
}

void WhiteboardCanvas::mouseReleaseEvent(QMouseEvent *e)
{
  if (!m_drawing || e->button() != Qt::LeftButton)
    return;
  if (m_draft.type != WbItem::Stroke)
    m_draft.rect.setBottomRight(e->pos());
  else if (!m_draft.points.isEmpty())
    m_draft.points.append(e->pos());
  addOrReplace(m_draft);
  emit opReady(itemToJson(m_draft, QStringLiteral("item")));
  m_drawing = false;
  update();
}

WhiteboardWindow::WhiteboardWindow(QWidget *parent)
    : QDialog(parent)
{
  setWindowTitle(tr("NetGreeting - Whiteboard"));
  resize(640, 480);
  m_canvas = new WhiteboardCanvas;

  auto *tools = new QHBoxLayout;
  auto *group = new QButtonGroup(this);
  group->setExclusive(true);

  auto addTool = [&](const QString &label, WhiteboardCanvas::Tool t) {
    auto *b = new QToolButton;
    b->setText(label);
    b->setCheckable(true);
    b->setAutoRaise(false);
    group->addButton(b);
    tools->addWidget(b);
    connect(b, &QToolButton::clicked, this, [this, t] { m_canvas->setTool(t); });
    return b;
  };
  addTool(tr("Pen"), WhiteboardCanvas::Pen)->setChecked(true);
  addTool(tr("Highlight"), WhiteboardCanvas::Highlighter);
  addTool(tr("Line"), WhiteboardCanvas::LineTool);
  addTool(tr("Rect"), WhiteboardCanvas::RectTool);
  addTool(tr("Ellipse"), WhiteboardCanvas::EllipseTool);
  addTool(tr("Text"), WhiteboardCanvas::TextTool);
  addTool(tr("Eraser"), WhiteboardCanvas::Eraser);
  addTool(tr("Pointer"), WhiteboardCanvas::Pointer);

  const QList<QColor> palette = {Qt::black, Qt::blue, Qt::red, Qt::darkGreen,
                                 Qt::magenta, QColor(128, 64, 0), Qt::darkCyan, Qt::gray};
  for (const QColor &c : palette) {
    auto *b = new QPushButton;
    b->setFixedSize(18, 18);
    b->setStyleSheet(QStringLiteral("background:%1; border:1px solid #404040;").arg(c.name()));
    connect(b, &QPushButton::clicked, this, [this, c] { m_canvas->setColor(c); });
    tools->addWidget(b);
  }

  auto *clear = new QPushButton(tr("Clear"));
  tools->addStretch();
  tools->addWidget(clear);

  auto *lay = new QVBoxLayout(this);
  lay->addLayout(tools);
  lay->addWidget(m_canvas, 1);

  connect(clear, &QPushButton::clicked, this, [this] { m_canvas->clearAll(true); });
  connect(m_canvas, &WhiteboardCanvas::opReady, this, &WhiteboardWindow::opReady);
}

void WhiteboardWindow::applyRemote(const QJsonObject &op)
{
  m_canvas->applyRemote(op);
}

void WhiteboardWindow::setLocalName(const QString &n)
{
  m_canvas->setLocalName(n);
}
