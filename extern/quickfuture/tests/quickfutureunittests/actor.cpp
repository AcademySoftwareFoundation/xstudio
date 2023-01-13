#include <QtConcurrent>
#include <QuickFuture>
#include <Automator>
#include <QtQml>
#include <asyncfuture.h>
#include "actor.h"

using namespace AsyncFuture;

static int delayWorker(int value) {
    Automator::wait(100);
    return value * value;
}

Actor::Actor(QObject *parent) : QObject(parent)
{

}

QFuture<QString> Actor::read(const QString &fileName)
{
    return QtConcurrent::run([fileName]() -> QString {

        QFile file(fileName);
        if (!file.open(QIODevice::ReadOnly)) {
            return "";
        }

        return file.readAll();
    });
}

QFuture<int> Actor::delayMapped(int count)
{
    QList<int> list;
    for (int i = 0 ; i < count ;i++) {
        list << i;
    }

    return QtConcurrent::mapped(list, delayWorker);
}

QFuture<void> Actor::dummy()
{
    return QtConcurrent::run([]() -> void {
        Automator::wait(100);
        return;
    });
}

QFuture<void> Actor::alreadyFinished()
{
    auto defer = deferred<void>();
    defer.complete();

    return defer.future();
}

QFuture<void> Actor::canceled()
{
    auto defer = deferred<void>();

    defer.cancel();

    return defer.future();
}

QFuture<int> Actor::canceledInt()
{
    auto defer = deferred<int>();

    defer.cancel();

    return defer.future();
}

QFuture<bool> Actor::delayReturnBool(bool value)
{
    auto defer = deferred<bool>();

    QTimer::singleShot(50,[=]() {
        auto d = defer;
        d.complete(value);
    });

    return defer.future();
}

QFuture<QSize> Actor::delayReturnQSize(QSize value)
{
    auto defer = deferred<QSize>();

    QTimer::singleShot(50,[=]() {
        auto d = defer;
        d.complete(value);
    });

    return defer.future();
}

QFuture<Actor::Reply> Actor::delayReturnReply()
{
    auto defer = deferred<Reply>();

    QTimer::singleShot(50,[=]() {
        auto d = defer;
        Reply reply;
        reply.code = -1;
        reply.message = "reply";
        d.complete(reply);
    });

    return defer.future();
}

// First, define the singleton type provider function (callback).
static QObject* provider(QQmlEngine *engine, QJSEngine *scriptEngine)
{
    Q_UNUSED(engine)
    Q_UNUSED(scriptEngine)

    Actor* actor = new Actor();

    return actor;
}

Q_DECLARE_METATYPE(Actor::Reply)
Q_DECLARE_METATYPE(QFuture<Actor::Reply>)

static void init() {
    qmlRegisterSingletonType<Actor>("FutureTests", 1, 0, "Actor", provider);
    QuickFuture::registerType<Actor::Reply>([](Actor::Reply reply) -> QVariant {
        QVariantMap map;
        map["code"] = reply.code;
        map["message"] = reply.message;
        return map;
    });
}

Q_COREAPP_STARTUP_FUNCTION(init)
