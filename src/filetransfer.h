#pragma once

#include <QDialog>

class QTableWidget;

class FileTransferWindow : public QDialog {
  Q_OBJECT
public:
  explicit FileTransferWindow(QWidget *parent = nullptr);

  void addOffer(quint32 id, const QString &from, const QString &name, qint64 size);
  void setProgress(quint32 id, const QString &name, qint64 done, qint64 total, bool outbound);
  void setFinished(quint32 id, const QString &name, const QString &path, bool ok, const QString &error);

signals:
  void sendFileRequested();
  void acceptRequested(quint32 id, const QString &savePath);
  void rejectRequested(quint32 id);

private:
  int rowForId(quint32 id) const;
  QTableWidget *m_table = nullptr;
};
