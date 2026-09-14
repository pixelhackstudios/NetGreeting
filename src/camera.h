#pragma once

#include <QImage>
#include <QMutex>
#include <QThread>
#include <atomic>

class CameraCapture : public QThread {
  Q_OBJECT
public:
  explicit CameraCapture(QObject *parent = nullptr);
  ~CameraCapture() override;

  void configure(const QString &device, int width, int height);
  void requestStop();

signals:
  void frameReady(const QImage &frame);
  void failed(const QString &error);

protected:
  void run() override;

private:
  QImage testCard(int frameIndex) const;
  bool captureV4L2();

  QString m_device;
  int m_width = 320;
  int m_height = 240;
  std::atomic<bool> m_stop{false};
};
