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

#include "console/push_manager.h"

#include <QJsonObject>

#include "chain/actor_index.h"

PushManager::PushManager(ExtraChainNode *node, QObject *parent)
    : QObject(parent) {
    manager = new QNetworkAccessManager(this);
    connect(manager, &QNetworkAccessManager::finished, this, &PushManager::responseResolver);
}

void PushManager::pushNotification(QString actorId, Notification notification) {
    if (!QFile::exists("notification"))
        return;

    auto main = node->accountController()->system_actor();
    if (main.id() != node->actorIndex()->network_id())
        return;
    auto &key = main.key();

    auto        encrypt_res      = key.encrypt_self(ByteArray(actorId).toBytes());
    QByteArray  actorIdEncrypted = ByteArray(encrypt_res.value()).toQByteArray();
    DbConnector db("notification");
    db.open();
    auto res = actorId == "all" ? db.select("SELECT * FROM Notification")
                                : db.select("SELECT * FROM Notification WHERE actorId = ?;",
                                            "Notification",
                                            { { "actorId", actorIdEncrypted.toStdString() } });

    if (res.size() == 0) {
        eLog("[Push] Actor with id {} has not configured any push", actorId);
        return;
    }

    for (auto &&el : res) {
        // QString token = ByteArray(key.decrypt_self(ByteArray(el["token"]).toBytes())).toQString();
        // QString os    = ByteArray(key.decrypt_self(ByteArray(el["os"]).toBytes())).toQString();

        // if (os == "ios" || os == "android") {
        //     eLog("[Push] New notification: {} {}", actorId, notification.data);
        //     sendNotification(token, os, notification);
        // }
    }
}

void PushManager::saveNotificationToken(QByteArray os, ActorId actorId, ActorId token) {
    static const std::string notificationTableCreation = //
        "CREATE TABLE IF NOT EXISTS Notification ("
        "token    BLOB PRIMARY KEY NOT NULL, "
        "actorId  BLOB             NOT NULL, "
        "os       BLOB             NOT NULL);";

    auto main = node->accountController()->system_actor();
    if (main.id() != node->actorIndex()->network_id())
        return;
    auto &key = main.key();

    const std::string &osActorId = actorId.to_string();
    // auto               apk         = node->actorIndex()->getActor(actorId).key().public_key();
    // std::string        osDecrypted = ByteArray(key.decrypt(ByteArray(os).toBytes(), apk)).toString();
    // std::string        osToken = ByteArray(key.decrypt(ByteArray(token.toQString()).toBytes(), apk)).toString();

    // if (osDecrypted != "ios" && osDecrypted != "android") {
    //     eLog("[Push] Try save, but wrong os: {}", osDecrypted);
    //     return;
    // }

    // DbConnector db("notification");
    // db.open();
    // db.create_table(notificationTableCreation);
    // db.delete_row("Notification", { { "token", osToken } });
    // db.insert("Notification", { { "actorId", osActorId }, { "token", osToken }, { "os", osDecrypted } });

    // eLog("[Push] Saved {} {} {}",
    //      QByteArray::fromStdString(osDecrypted),
    //      QByteArray::fromStdString(osActorId),
    //      QByteArray::fromStdString(osToken));
}

void PushManager::responseResolver(QNetworkReply *reply) {
    // eLog("[Push] Result from url {}", reply->url().toString());

    if (reply->error()) {
        eLog("[Push] Error: {}", reply->errorString());
        return;
    }

    QByteArray answer = reply->readAll();
    auto       json   = QJsonDocument::fromJson(answer);
    eLog("[Push] Result: {}", json.toJson(QJsonDocument::Compact));

    // QString errorType = json["errorType"].toString();
    // if (errorType == "None") {
    //     eLog("[Push] Successfully sent for {}", json["errorText"].toString());
    // } else if (errorType == "BadDeviceToken" || errorType == "Unregistered") {
    //     QString token = json["errorText"].toString();
    //     eLog("[Push] Remove token {}", token);

    //     auto &main = node->accountController()->mainActor();
    //     auto &key  = main->key();

    //     DbConnector db("notification");
    //     db.open();
    //     db.delete_row("Notification",
    //                   { { "token",
    //                       ByteArray(key.encrypt_self(ByteArray(token.toStdString()).toBytes())).toString() } });
    // } else {
    //     eLog("[Push] Error: {} {}", errorType, json["errorText"].toString());
    //     // TODO: repeat request, ConnectionError
    // }
}

void PushManager::chatMessage(QString sender, QString msgPath) {
    //    QString userId = msgPath.mid(DfsStruct::ROOT_FOOLDER_NAME_MID, 40);
    //    QString chatId = msgPath.mid(32, 64);

    //    std::string usersPath = CardManager::buildPathForFile(
    //        userId.toStdString(), chatId.toStdString() + "/users", DfsStruct::Type(104));
    //    DBConnector db(usersPath);
    //    db.open();
    //    auto res = db.select("SELECT * FROM Users");

    //    if (res.size() == 2) {
    //        QString users[2] = { QString::fromStdString(res[0]["userId"]),
    //                             QString::fromStdString(res[1]["userId"]) };

    //        pushNotification(
    //            users[0] == sender ? users[1] : users[0],
    //            Notification { .time = 100,
    //                           .type = Notification::NotifyType::ChatMsg,
    //                           .data = (users[0] == sender ? users[0] : users[1]).toLatin1() + " " });
    //    };
}

// void PushManager::fileAdded(QString path, QString original, DfsStruct::Type type, QString actorId) {
//     Q_UNUSED(original)

//    if (type == DfsStruct::Type::Post || type == DfsStruct::Type::Event) {
//        if (path.contains("."))
//            return;

//        Notification notify { .time = 100,
//                              .type = type == DfsStruct::Type::Post ? Notification::NewPost
//                                                                    : Notification::NewEvent,
//                              .data = actorId.toLatin1() + " " };
//        pushNotification("all", notify);
//    }
//}

void PushManager::sendNotification(const QString &token, const QString &os, const Notification &notification) {
    QJsonObject json;
    json["os"]      = os;
    json["token"]   = token;
    json["message"] = notificationToMessage(notification);

    QJsonObject data;
    data["notifyType"] = notification.type;
    data["data"]       = QString(notification.data);
    json["data"]       = data;

    QString jsonStr = QJsonDocument(json).toJson();
    eLog("[Push] Send {}", QJsonDocument(json).toJson(QJsonDocument::Compact));
    manager->get(QNetworkRequest(QUrl(pushServerUrl + jsonStr)));
}

QString PushManager::notificationToMessage(const Notification &notification) {
    QString    message;
    QByteArray userId;

    // switch (notification.type) {
    // case (Notification::NotifyType::TxToUser):
    //     message = "Transaction to *" + notification.data.right(5) + " completed";
    //     userId  = "";
    //     break;
    // case (Notification::NotifyType::TxToMe):
    //     message = "New transaction from *" + notification.data.right(5);
    //     userId  = "";
    //     break;
    // case (Notification::NotifyType::ChatMsg): {
    //     QByteArray chatId = notification.data.split(' ').at(0);
    //     message           = "New message from ";
    //     userId            = chatId;
    //     break;
    // }
    // case (Notification::NotifyType::ChatInvite): {
    //     QByteArray user = notification.data.split(' ').at(0);
    //     message         = "New chat from ";
    //     userId          = user;
    //     break;
    // }
    // case (Notification::NotifyType::NewPost): {
    //     QByteArray user = notification.data.split(' ').at(0);
    //     message         = "New post from ";
    //     userId          = user;
    //     break;
    // }
    // case (Notification::NotifyType::NewEvent): {
    //     QByteArray user = notification.data.split(' ').at(0);
    //     message         = "New event from ";
    //     userId          = user;
    //     break;
    // }
    // case (Notification::NotifyType::NewFollower):
    //     message = "New follower ";
    //     userId  = notification.data;
    //     break;
    // }

    if (userId.isEmpty())
        return message;

    return "";

    //    if (actorIndex == nullptr || !actorIndex)
    //    {
    //        eFatal("[Push] No actor index access");
    //    }
    /*
    QByteArrayList profile = node->actorIndex()->getProfile(userId);
    if (profile.isEmpty())
        return message;
    else {
        QString name = profile.at(3);
        QString secName = profile.at(4);
        return message + name + " " + secName;
    }
    */
}
