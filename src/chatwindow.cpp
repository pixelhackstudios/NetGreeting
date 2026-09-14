#include "chatwindow.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QDateTime>

ChatWindow::ChatWindow(QWidget *parent)
    : QDialog(parent)
{
  setWindowTitle(tr("NetGreeting - Chat"));
  resize(420, 320);

  m_log = new QPlainTextEdit;
  m_log->setReadOnly(true);
  m_input = new QLineEdit;
  m_to = new QComboBox;
  m_to->addItem(tr("Everyone in this meeting"), QString());

  auto *send = new QPushButton(tr("Send"));
  send->setDefault(true);

  auto *row = new QHBoxLayout;
  row->addWidget(new QLabel(tr("Send to:")));
  row->addWidget(m_to, 1);

  auto *inRow = new QHBoxLayout;
  inRow->addWidget(m_input, 1);
  inRow->addWidget(send);

  auto *lay = new QVBoxLayout(this);
  lay->addWidget(m_log, 1);
  lay->addLayout(row);
  lay->addLayout(inRow);

  auto fire = [this] {
    const QString text = m_input->text().trimmed();
    if (text.isEmpty())
      return;
    const QString to = m_to->currentData().toString();
    emit sendRequested(text, to);
    appendMessage(m_localName, text, !to.isEmpty());
    m_input->clear();
  };
  connect(send, &QPushButton::clicked, this, fire);
  connect(m_input, &QLineEdit::returnPressed, this, fire);
}

void ChatWindow::appendMessage(const QString &from, const QString &text, bool whisper)
{
  const QString ts = QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss"));
  if (whisper)
    m_log->appendPlainText(tr("<%1> %2 (whisper): %3").arg(ts, from, text));
  else
    m_log->appendPlainText(tr("<%1> %2: %3").arg(ts, from, text));
}

void ChatWindow::setParticipants(const QStringList &names)
{
  const QString current = m_to->currentData().toString();
  m_to->clear();
  m_to->addItem(tr("Everyone in this meeting"), QString());
  for (const QString &n : names) {
    if (n != m_localName)
      m_to->addItem(n, n);
  }
  const int idx = m_to->findData(current);
  if (idx >= 0)
    m_to->setCurrentIndex(idx);
}

void ChatWindow::setLocalName(const QString &name)
{
  m_localName = name;
}
