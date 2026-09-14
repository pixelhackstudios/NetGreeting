#pragma once

#include <QDialog>
#include <QImage>
#include <QLabel>

class ShareWindow : public QDialog {
  Q_OBJECT
public:
  explicit ShareWindow(QWidget *parent = nullptr);
  void setFrame(const QImage &img);

private:
  QLabel *m_view = nullptr;
};
