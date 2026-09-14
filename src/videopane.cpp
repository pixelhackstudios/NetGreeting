#include "videopane.h"

#include <QPainter>
#include <QPaintEvent>

VideoPane::VideoPane(QWidget *parent)
    : QWidget(parent)
{
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  setMinimumSize(160, 120);
  setAttribute(Qt::WA_OpaquePaintEvent);
}

void VideoPane::setFrame(const QImage &img)
{
  m_frame = img;
  update();
}

void VideoPane::setPipFrame(const QImage &img)
{
  m_pip = img;
  if (m_pipOn)
    update();
}

void VideoPane::setPipVisible(bool on)
{
  if (m_pipOn == on)
    return;
  m_pipOn = on;
  update();
}

QSize VideoPane::sizeHint() const
{
  return {256, 192};
}

QSize VideoPane::minimumSizeHint() const
{
  return {160, 120};
}

void VideoPane::paintEvent(QPaintEvent *)
{
  QPainter p(this);
  p.fillRect(rect(), Qt::black);

  const QRect inner = rect().adjusted(2, 2, -2, -2);
  auto blit = [&](const QImage &img, const QRect &box) {
    if (img.isNull() || box.isEmpty())
      return;
    const QImage scaled = img.scaled(box.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    const QPoint pos(box.x() + (box.width() - scaled.width()) / 2,
                     box.y() + (box.height() - scaled.height()) / 2);
    p.drawImage(pos, scaled);
  };

  blit(m_frame, inner);

  if (m_pipOn && !m_pip.isNull()) {
    const int pw = qBound(64, width() / 4, 160);
    const int ph = pw * 3 / 4;
    const QRect pip(inner.left() + 6, inner.bottom() - ph - 6, pw, ph);
    p.fillRect(pip.adjusted(-1, -1, 1, 1), Qt::white);
    p.fillRect(pip, Qt::black);
    blit(m_pip, pip);
  }

  p.setPen(QColor(128, 128, 128));
  p.drawLine(0, 0, width() - 1, 0);
  p.drawLine(0, 0, 0, height() - 1);
  p.setPen(QColor(255, 255, 255));
  p.drawLine(width() - 1, 0, width() - 1, height() - 1);
  p.drawLine(0, height() - 1, width() - 1, height() - 1);
}
