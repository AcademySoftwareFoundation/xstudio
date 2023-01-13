#ifndef FILEACTOR_H
#define FILEACTOR_H

#include <QObject>
#include <QFuture>
#include <QSize>

class Actor : public QObject
{
    Q_OBJECT
public:
    class Reply {
    public:
        int code;
        QString message;
    };

    explicit Actor(QObject *parent = 0);

signals:

public slots:

    QFuture<QString> read(const QString& fileName);

    QFuture<int> delayMapped(int count);

    QFuture<void> dummy();

    QFuture<void> alreadyFinished();

    QFuture<void> canceled();

    QFuture<int> canceledInt();

    QFuture<bool> delayReturnBool(bool value);

    QFuture<QSize> delayReturnQSize(QSize value);

    QFuture<Actor::Reply> delayReturnReply();

};

#endif // FILEACTOR_H
