#pragma once

#include <QByteArray>
#include <QThread>
#include <atomic>

class AudioEngine : public QObject {
  Q_OBJECT
public:
  explicit AudioEngine(QObject *parent = nullptr);
  ~AudioEngine() override;

  bool start(bool capture, bool playback);
  void stop();
  void setCaptureEnabled(bool on);
  void setPlaybackEnabled(bool on);
  void playPcm(const QByteArray &pcm);
  bool captureEnabled() const { return m_captureOn; }
  bool playbackEnabled() const { return m_playOn; }

  static constexpr int kRate = 16000;
  static constexpr int kChannels = 1;
  static constexpr int kFrameBytes = 640; // 20ms s16le mono

signals:
  void captured(const QByteArray &pcm);
  void failed(const QString &error);

private:
#ifdef HAVE_PULSE
  class CaptureThread;
  class PlaybackThread;
  CaptureThread *m_capture = nullptr;
  PlaybackThread *m_play = nullptr;
#endif
  bool m_captureOn = false;
  bool m_playOn = false;
};
