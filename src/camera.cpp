#include "camera.h"

#include <QDateTime>
#include <QPainter>
#include <QElapsedTimer>
#include <QtGlobal>

#include <cmath>
#include <cstring>
#include <vector>

#ifdef Q_OS_LINUX
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <linux/videodev2.h>
#endif

CameraCapture::CameraCapture(QObject *parent)
    : QThread(parent)
{
  setObjectName(QStringLiteral("ng-camera"));
}

CameraCapture::~CameraCapture()
{
  requestStop();
  wait(1500);
}

void CameraCapture::configure(const QString &device, int width, int height)
{
  m_device = device;
  m_width = width;
  m_height = height;
}

void CameraCapture::requestStop()
{
  m_stop.store(true);
}

#ifdef Q_OS_LINUX
static QImage yuyvToImage(const uchar *data, int width, int height, int bytesused)
{
  QImage img(width, height, QImage::Format_RGB32);
  const int stride = width * 2;
  if (bytesused < stride * height)
    return {};
  for (int y = 0; y < height; ++y) {
    const uchar *row = data + y * stride;
    auto *dst = reinterpret_cast<QRgb *>(img.scanLine(y));
    for (int x = 0; x < width; x += 2) {
      const int y0 = row[x * 2 + 0];
      const int u = row[x * 2 + 1];
      const int y1 = row[x * 2 + 2];
      const int v = row[x * 2 + 3];
      auto conv = [&](int Y) {
        const int c = Y - 16;
        const int d = u - 128;
        const int e = v - 128;
        const int r = qBound(0, (298 * c + 409 * e + 128) >> 8, 255);
        const int g = qBound(0, (298 * c - 100 * d - 208 * e + 128) >> 8, 255);
        const int b = qBound(0, (298 * c + 516 * d + 128) >> 8, 255);
        return qRgb(r, g, b);
      };
      dst[x] = conv(y0);
      if (x + 1 < width)
        dst[x + 1] = conv(y1);
    }
  }
  return img;
}
#endif // Q_OS_LINUX

QImage CameraCapture::testCard(int frameIndex) const
{
  QImage img(m_width, m_height, QImage::Format_RGB32);
  QPainter p(&img);
  p.fillRect(img.rect(), QColor(12, 24, 64));
  const int bars = 7;
  const QColor colors[] = {
      QColor(192, 192, 192), QColor(192, 192, 0), QColor(0, 192, 192), QColor(0, 192, 0),
      QColor(192, 0, 192), QColor(192, 0, 0), QColor(0, 0, 192)};
  const int bh = m_height / 3;
  for (int i = 0; i < bars; ++i) {
    p.fillRect(QRect(i * m_width / bars, 0, m_width / bars + 1, bh), colors[i]);
  }
  p.setPen(Qt::white);
  p.setFont(QFont(QStringLiteral("Sans Serif"), 11, QFont::Bold));
  p.drawText(img.rect().adjusted(8, bh + 8, -8, -8), Qt::AlignLeft | Qt::AlignTop,
             QStringLiteral("NetGreeting\nTest Card"));
  p.drawText(img.rect().adjusted(8, 0, -8, -8), Qt::AlignRight | Qt::AlignBottom,
             QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss")));
  const int r = 10;
  const int cx = m_width / 2 + int(60 * std::cos(frameIndex / 12.0));
  const int cy = bh + m_height / 3 + int(20 * std::sin(frameIndex / 9.0));
  p.setBrush(QColor(255, 200, 40));
  p.setPen(Qt::NoPen);
  p.drawEllipse(QPoint(cx, cy), r, r);
  p.end();
  return img;
}

bool CameraCapture::captureV4L2()
{
#ifndef Q_OS_LINUX
  Q_UNUSED(this);
  return false;
#else
  const QByteArray dev = m_device.toLocal8Bit();
  int fd = ::open(dev.constData(), O_RDWR | O_NONBLOCK);
  if (fd < 0)
    return false;

  v4l2_capability cap{};
  if (ioctl(fd, VIDIOC_QUERYCAP, &cap) < 0) {
    ::close(fd);
    return false;
  }

  auto tryFmt = [&](quint32 fourcc) {
    v4l2_format fmt{};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = m_width;
    fmt.fmt.pix.height = m_height;
    fmt.fmt.pix.pixelformat = fourcc;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;
    return ioctl(fd, VIDIOC_S_FMT, &fmt) == 0 ? fmt : v4l2_format{};
  };

  v4l2_format fmt = tryFmt(V4L2_PIX_FMT_MJPEG);
  bool mjpeg = fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_MJPEG;
  if (!mjpeg) {
    fmt = tryFmt(V4L2_PIX_FMT_YUYV);
    if (fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_YUYV) {
      ::close(fd);
      return false;
    }
  }

  const int width = int(fmt.fmt.pix.width);
  const int height = int(fmt.fmt.pix.height);

  v4l2_requestbuffers req{};
  req.count = 4;
  req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  req.memory = V4L2_MEMORY_MMAP;
  if (ioctl(fd, VIDIOC_REQBUFS, &req) < 0) {
    ::close(fd);
    return false;
  }

  struct Map {
    void *ptr = MAP_FAILED;
    size_t len = 0;
  };
  std::vector<Map> maps(req.count);
  for (unsigned i = 0; i < req.count; ++i) {
    v4l2_buffer buf{};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = i;
    if (ioctl(fd, VIDIOC_QUERYBUF, &buf) < 0) {
      ::close(fd);
      return false;
    }
    maps[i].len = buf.length;
    maps[i].ptr = mmap(nullptr, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
    if (maps[i].ptr == MAP_FAILED) {
      ::close(fd);
      return false;
    }
    if (ioctl(fd, VIDIOC_QBUF, &buf) < 0) {
      ::close(fd);
      return false;
    }
  }

  v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (ioctl(fd, VIDIOC_STREAMON, &type) < 0) {
    ::close(fd);
    return false;
  }

  while (!m_stop.load()) {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    timeval tv{0, 200000};
    const int r = select(fd + 1, &fds, nullptr, nullptr, &tv);
    if (r <= 0)
      continue;
    v4l2_buffer buf{};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    if (ioctl(fd, VIDIOC_DQBUF, &buf) < 0)
      continue;
    const uchar *data = static_cast<uchar *>(maps[buf.index].ptr);
    QImage img;
    if (mjpeg)
      img.loadFromData(data, int(buf.bytesused), "JPEG");
    else
      img = yuyvToImage(data, width, height, int(buf.bytesused));
    ioctl(fd, VIDIOC_QBUF, &buf);
    if (!img.isNull())
      emit frameReady(img.copy());
  }

  ioctl(fd, VIDIOC_STREAMOFF, &type);
  for (auto &m : maps) {
    if (m.ptr != MAP_FAILED)
      munmap(m.ptr, m.len);
  }
  ::close(fd);
  return true;
#endif
}

void CameraCapture::run()
{
  m_stop.store(false);
  if (!m_device.isEmpty() && captureV4L2())
    return;

  emit failed(tr("Camera unavailable — using test card."));
  int i = 0;
  QElapsedTimer t;
  t.start();
  while (!m_stop.load()) {
    emit frameReady(testCard(i++));
    msleep(100);
  }
}
