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
