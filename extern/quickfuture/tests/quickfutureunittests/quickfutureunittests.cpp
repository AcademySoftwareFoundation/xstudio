#include <QQmlApplicationEngine>
#include <QTest>
#include <Automator>
#include <QQmlContext>
#include <QtShell>
#include <QuickFuture>
#include <QtConcurrent>
#include "actor.h"
#include "quickfutureunittests.h"

Q_DECLARE_METATYPE(QFuture<QString>)
using namespace QuickFuture;

template <typename F>
static bool waitUntil(F f, int timeout = -1) {
    QTime time;
    time.start();

    while (!f()) {
        Automator::wait(10);
        if (timeout > 0 && time.elapsed() > timeout) {
            return false;
        }
    }
    return true;
}

QuickFutureUnitTests::QuickFutureUnitTests(QObject *parent) : QObject(parent)
{
    auto ref = [=]() {
        QTest::qExec(this, 0, 0); // Autotest detect available test cases of a QObject by looking for "QTest::qExec" in source code
    };
    Q_UNUSED(ref);
}

void QuickFutureUnitTests::test_QFVariantWrapper()
{
    QFuture<QString> future;
    QVariant v = QVariant::fromValue<QFuture<QString> >(future);

    QVERIFY(future.isFinished());

    VariantWrapper<QString> wrapper;
    QVERIFY(wrapper.isFinished(v));
}

void QuickFutureUnitTests::test_QFFuture()
{
    Future wrapper;
    QFuture<QString> future;
    QVariant v = QVariant::fromValue<QFuture<QString> >(future);

    QVERIFY(future.isFinished());
    QVERIFY(wrapper.isFinished(v));
}

void QuickFutureUnitTests::PromiseIsNotInstalled()
{
    QQmlApplicationEngine engine;

    qDebug() << "Excepted error:";

    engine.rootContext()->setContextProperty("Actor", new Actor());

    engine.load(QString(SRCDIR) + "/qmltests/PromiseIsNotInstalled.qml");

    QObject* object = engine.rootObjects().first();
    QVERIFY(object);

    QMetaObject::invokeMethod(object, "test",Qt::DirectConnection);

}
