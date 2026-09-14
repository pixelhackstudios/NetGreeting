#include "filetransfer.h"

#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

FileTransferWindow::FileTransferWindow(QWidget *parent)
    : QDialog(parent)
{
  setWindowTitle(tr("NetGreeting - File Transfer"));
  resize(520, 240);

  m_table = new QTableWidget(0, 5);
  m_table->setHorizontalHeaderLabels({tr("ID"), tr("File"), tr("Direction"), tr("Progress"), tr("Status")});
  m_table->horizontalHeader()->setStretchLastSection(true);
  m_table->verticalHeader()->setVisible(false);
  m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_table->setColumnHidden(0, true);

  auto *send = new QPushButton(tr("Send File..."));
  auto *accept = new QPushButton(tr("Accept"));
  auto *reject = new QPushButton(tr("Decline"));

  auto *row = new QHBoxLayout;
  row->addWidget(send);
  row->addStretch();
  row->addWidget(accept);
  row->addWidget(reject);

  auto *lay = new QVBoxLayout(this);
  lay->addWidget(m_table, 1);
  lay->addLayout(row);

  connect(send, &QPushButton::clicked, this, &FileTransferWindow::sendFileRequested);
  connect(accept, &QPushButton::clicked, this, [this] {
    const int r = m_table->currentRow();
    if (r < 0)
      return;
    const quint32 id = m_table->item(r, 0)->text().toUInt();
    const QString name = m_table->item(r, 1)->text();
    const QString path = QFileDialog::getSaveFileName(this, tr("Save incoming file"), name);
    if (!path.isEmpty())
      emit acceptRequested(id, path);
  });
  connect(reject, &QPushButton::clicked, this, [this] {
    const int r = m_table->currentRow();
    if (r < 0)
      return;
    emit rejectRequested(m_table->item(r, 0)->text().toUInt());
  });
}

int FileTransferWindow::rowForId(quint32 id) const
{
  for (int r = 0; r < m_table->rowCount(); ++r) {
    if (m_table->item(r, 0) && m_table->item(r, 0)->text().toUInt() == id)
      return r;
  }
  return -1;
}

void FileTransferWindow::addOffer(quint32 id, const QString &from, const QString &name, qint64 size)
{
  int r = rowForId(id);
  if (r < 0) {
    r = m_table->rowCount();
    m_table->insertRow(r);
    m_table->setItem(r, 0, new QTableWidgetItem(QString::number(id)));
  }
  m_table->setItem(r, 1, new QTableWidgetItem(name));
  m_table->setItem(r, 2, new QTableWidgetItem(tr("From %1").arg(from)));
  m_table->setItem(r, 3, new QTableWidgetItem(tr("0 / %1").arg(size)));
  m_table->setItem(r, 4, new QTableWidgetItem(tr("Offered")));
}

void FileTransferWindow::setProgress(quint32 id, const QString &name, qint64 done, qint64 total, bool outbound)
{
  int r = rowForId(id);
  if (r < 0) {
    r = m_table->rowCount();
    m_table->insertRow(r);
    m_table->setItem(r, 0, new QTableWidgetItem(QString::number(id)));
  }
  m_table->setItem(r, 1, new QTableWidgetItem(name));
  m_table->setItem(r, 2, new QTableWidgetItem(outbound ? tr("Sending") : tr("Receiving")));
  const int pct = total > 0 ? int(done * 100 / total) : 0;
  m_table->setItem(r, 3, new QTableWidgetItem(tr("%1%").arg(pct)));
  m_table->setItem(r, 4, new QTableWidgetItem(tr("In progress")));
}

void FileTransferWindow::setFinished(quint32 id, const QString &name, const QString &path, bool ok, const QString &error)
{
  int r = rowForId(id);
  if (r < 0)
    return;
  m_table->setItem(r, 1, new QTableWidgetItem(name));
  m_table->setItem(r, 4, new QTableWidgetItem(ok ? tr("Complete: %1").arg(path) : error));
}
