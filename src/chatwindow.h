#pragma once

#include <QDialog>

class QComboBox;
class QPlainTextEdit;
class QLineEdit;

class ChatWindow : public QDialog {
  Q_OBJECT
public:
  explicit ChatWindow(QWidget *parent = nullptr);
  void appendMessage(const QString &from, const QString &text, bool whisper);
  void setParticipants(const QStringList &names);
  void setLocalName(const QString &name);

signals:
  void sendRequested(const QString &text, const QString &whisperTo);

private:
  QPlainTextEdit *m_log = nullptr;
  QLineEdit *m_input = nullptr;
  QComboBox *m_to = nullptr;
  QString m_localName;
};
