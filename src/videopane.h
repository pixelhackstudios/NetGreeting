#pragma once

#include <QImage>
#include <QWidget>

class VideoPane : public QWidget {
  Q_OBJECT
public:
  explicit VideoPane(QWidget *parent = nullptr);

  void setFrame(const QImage &img);
  void setPipFrame(const QImage &img);
  void setPipVisible(bool on);

  QSize sizeHint() const override;
  QSize minimumSizeHint() const override;

protected:
  void paintEvent(QPaintEvent *event) override;

private:
  QImage m_frame;
  QImage m_pip;
  bool m_pipOn = false;
};
