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

#ifndef READER_H
#define READER_H

#include <QCoreApplication>
#include <QNetworkAccessManager>

#ifdef Q_OS_UNIX
    #include <QSocketNotifier>
#endif

#include "console/console_input.h"
#include "console/push_manager.h"

class ExtraChainNode;
class AccountController;
class NetworkManager;

class ConsoleManager : public QObject {
    Q_OBJECT

public:
    explicit ConsoleManager(QObject *parent = nullptr);
    ~ConsoleManager();

    PushManager *pushManager() const;

    void setExtraChainNode(ExtraChainNode *node);
    void startInput();
    void dfsStart();

    static QString getSomething(const QString &name);

signals:
    void textReceived(QString message);

public slots:
    void commandReceiver(QString command);
    void saveNotificationToken(QByteArray os, ActorId actorId, ActorId token);

private:
#ifdef Q_OS_UNIX
    QSocketNotifier notifier;
    QTextStream     notifierInput;
#endif
#ifdef Q_OS_WIN
    ConsoleInput consoleInput;
#endif

    ExtraChainNode *node;
    PushManager    *m_pushManager;
};

#endif // READER_H
