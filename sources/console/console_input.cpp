/*
 * ExtraChain Console Client
 * Copyright (C) 2025 ExtraChain Foundation <official@extrachain.io>
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

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
