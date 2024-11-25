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
