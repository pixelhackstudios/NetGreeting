#include "sharewindow.h"

#include <QVBoxLayout>
#include <QScrollArea>
#include <QPixmap>

ShareWindow::ShareWindow(QWidget *parent)
    : QDialog(parent)
{
  setWindowTitle(tr("NetGreeting - Desktop Sharing"));
  resize(720, 480);
  m_view = new QLabel(tr("Waiting for shared desktop..."));
  m_view->setAlignment(Qt::AlignCenter);
  m_view->setMinimumSize(320, 240);
  auto *scroll = new QScrollArea;
  scroll->setWidget(m_view);
  scroll->setWidgetResizable(true);
  auto *lay = new QVBoxLayout(this);
  lay->addWidget(scroll);
}

void ShareWindow::setFrame(const QImage &img)
{
  m_view->setPixmap(QPixmap::fromImage(img));
  m_view->setMinimumSize(img.size());
}
