#pragma once

#include <QIcon>
#include <QString>

class QApplication;
class QWidget;

void applyNetGreetingStyle(QApplication &app);
QIcon netGreetingIcon(const QString &name);
QIcon appWindowIcon();
void bevelPanel(QWidget *w);
