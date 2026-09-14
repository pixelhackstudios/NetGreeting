#include "audio.h"

#ifdef HAVE_PULSE
#include <QMutex>
#include <QWaitCondition>
#include <QQueue>

#include <pulse/error.h>
#include <pulse/simple.h>
#endif

#ifdef HAVE_PULSE
class AudioEngine::CaptureThread : public QThread {
public:
  std::atomic<bool> stop{false};
  AudioEngine *engine = nullptr;
  void run() override
  {
    pa_sample_spec ss;
    ss.format = PA_SAMPLE_S16LE;
    ss.rate = AudioEngine::kRate;
    ss.channels = AudioEngine::kChannels;
    int err = 0;
    pa_simple *s = pa_simple_new(nullptr, "NetGreeting", PA_STREAM_RECORD, nullptr,
                                 "capture", &ss, nullptr, nullptr, &err);
    if (!s) {
      emit engine->failed(QStringLiteral("Microphone: %1").arg(pa_strerror(err)));
      return;
    }
    QByteArray buf(AudioEngine::kFrameBytes, Qt::Uninitialized);
    while (!stop.load()) {
      if (pa_simple_read(s, buf.data(), buf.size(), &err) < 0)
        break;
      if (engine->captureEnabled())
        emit engine->captured(buf);
    }
    pa_simple_free(s);
  }
};

class AudioEngine::PlaybackThread : public QThread {
public:
  std::atomic<bool> stop{false};
  QMutex mutex;
  QWaitCondition cv;
  QQueue<QByteArray> queue;
  void play(const QByteArray &pcm)
  {
    QMutexLocker lock(&mutex);
    if (queue.size() > 20)
      queue.dequeue();
    queue.enqueue(pcm);
    cv.wakeOne();
  }
  void run() override
  {
    pa_sample_spec ss;
    ss.format = PA_SAMPLE_S16LE;
    ss.rate = AudioEngine::kRate;
    ss.channels = AudioEngine::kChannels;
    int err = 0;
    pa_simple *s = pa_simple_new(nullptr, "NetGreeting", PA_STREAM_PLAYBACK, nullptr,
                                 "playback", &ss, nullptr, nullptr, &err);
    if (!s)
      return;
    while (!stop.load()) {
      QByteArray pcm;
      {
        QMutexLocker lock(&mutex);
        if (queue.isEmpty()) {
          cv.wait(&mutex, 100);
          if (queue.isEmpty())
            continue;
        }
        pcm = queue.dequeue();
      }
      pa_simple_write(s, pcm.constData(), size_t(pcm.size()), &err);
    }
    pa_simple_drain(s, &err);
    pa_simple_free(s);
  }
};

#endif // HAVE_PULSE

AudioEngine::AudioEngine(QObject *parent)
    : QObject(parent)
{
}

AudioEngine::~AudioEngine()
{
#ifdef HAVE_PULSE
  if (m_capture) {
    m_capture->stop.store(true);
    m_capture->wait(300);
    if (m_capture->isRunning())
      m_capture->terminate();
    m_capture->wait(300);
    delete m_capture;
    m_capture = nullptr;
  }
  if (m_play) {
    m_play->stop.store(true);
    m_play->cv.wakeAll();
    m_play->wait(300);
    if (m_play->isRunning())
      m_play->terminate();
    m_play->wait(300);
    delete m_play;
    m_play = nullptr;
  }
#endif
}

bool AudioEngine::start(bool capture, bool playback)
{
  m_captureOn = capture;
  m_playOn = playback;
#ifdef HAVE_PULSE
  if (capture && !m_capture) {
    m_capture = new CaptureThread;
    m_capture->setObjectName(QStringLiteral("ng-audio-capture"));
    m_capture->engine = this;
    m_capture->start();
  }
  if (playback && !m_play) {
    m_play = new PlaybackThread;
    m_play->setObjectName(QStringLiteral("ng-audio-play"));
    m_play->start();
  }
#endif
  return true;
}

void AudioEngine::stop()
{
  // Leave Pulse threads alive until destruction; pa_simple_read cannot be
  // interrupted, and deleting the QThread here aborts the process.
  m_captureOn = false;
  m_playOn = false;
}

void AudioEngine::playPcm(const QByteArray &pcm)
{
#ifdef HAVE_PULSE
  if (m_play && m_playOn)
    m_play->play(pcm);
#else
  Q_UNUSED(pcm);
#endif
}
