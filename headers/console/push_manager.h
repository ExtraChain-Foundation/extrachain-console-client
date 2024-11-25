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

#ifndef PUSHMANAGER_H
#define PUSHMANAGER_H

#include <QNetworkAccessManager>
#include <QNetworkReply>

#include "managers/account_controller.h"
#include "utils/db_connector.h"
#include "utils/exc_utils.h"

class ExtraChainNode;

class PushManager : public QObject {
    Q_OBJECT

public:
    explicit PushManager(ExtraChainNode *node, QObject *parent = nullptr);

    void setAccController(ExtraChainNode *node);

public slots:
    void pushNotification(QString actorId, Notification notification);
    void saveNotificationToken(QByteArray os, ActorId actorId, ActorId token);
    void responseResolver(QNetworkReply *reply);
    void chatMessage(QString sender, QString msgPath);
    //    void fileAdded(QString path, QString original, DfsStruct::Type type, QString actorId);

private:
    void    sendNotification(const QString &token, const QString &os, const Notification &notification);
    QString notificationToMessage(const Notification &notification);

    const QString          pushServerUrl = "http://127.0.0.1:8000/";
    QNetworkAccessManager *manager;
    ExtraChainNode        *node;
};

#endif // PUSHMANAGER_H
