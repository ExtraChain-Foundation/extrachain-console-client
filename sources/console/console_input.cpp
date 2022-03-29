#include "console/console_input.h"

#include <QCoreApplication>
#include <QIODevice>
#include <QtDebug>

ConsoleInput::ConsoleInput(QObject *parent)
    : QObject(parent) {
}

ConsoleInput::~ConsoleInput() {
    if (notifierInput != nullptr) {
        delete notifierInput;
    }
}

void ConsoleInput::process() {
    notifierInput = new QTextStream(stdin, QIODevice::ReadOnly);

    while (true) {
        QString line = notifierInput->readLine();
        if (!line.isEmpty())
            emit this->input(line);
    }
}
