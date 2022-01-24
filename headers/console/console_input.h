#ifndef WINDOWSINPUT_H
#define WINDOWSINPUT_H

#include <QObject>
#include <QTextStream>

class ConsoleInput : public QObject {
    Q_OBJECT
public:
    explicit ConsoleInput(QObject *parent = nullptr);
    ~ConsoleInput();

signals:
    void input(QString text);
    void finished();

public slots:
    void process();

private:
    QTextStream *notifierInput;
};

#endif // WINDOWSINPUT_H
