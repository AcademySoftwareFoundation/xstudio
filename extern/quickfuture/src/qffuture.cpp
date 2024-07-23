#include <QGuiApplication>
#include <QtQml>
#include <QQmlComponent>
#include <QUrl>
#include <QUuid>

#include "qffuture.h"
#include "quickfuture.h"

Q_DECLARE_METATYPE(QFuture<QString>)
Q_DECLARE_METATYPE(QFuture<int>)
Q_DECLARE_METATYPE(QFuture<void>)
Q_DECLARE_METATYPE(QFuture<bool>)
Q_DECLARE_METATYPE(QFuture<qreal>)
Q_DECLARE_METATYPE(QFuture<QByteArray>)
Q_DECLARE_METATYPE(QFuture<QVariant>)
Q_DECLARE_METATYPE(QFuture<QVariantMap>)
Q_DECLARE_METATYPE(QFuture<QSize>)
Q_DECLARE_METATYPE(QUrl)
Q_DECLARE_METATYPE(QList<QUuid>)
Q_DECLARE_METATYPE(QFuture<QUuid>)
Q_DECLARE_METATYPE(QFuture<QList<QUuid>>)
Q_DECLARE_METATYPE(QFuture<QUrl>)

namespace QuickFuture {

static QMap<int, VariantWrapperBase*> m_wrappers;

static int typeId(const QVariant& v) {
    return v.userType();
}

Future::Future(QObject *parent) : QObject(parent)
{
}

void Future::registerType(int typeId, VariantWrapperBase* wrapper)
{
    if (m_wrappers.contains(typeId)) {
        qWarning() << QString("QuickFuture::registerType:It is already registered:%1").arg(QMetaType::typeName(typeId));
        return;
    }

    m_wrappers[typeId] = wrapper;
}

QJSEngine *Future::engine() const
{
    return m_engine;
}

void Future::setEngine(QQmlEngine *engine)
{
    m_engine = engine;
    if (m_engine.isNull()) {
        return;
    }

    QString qml = "import QtQuick 2.0\n"
                  "import QuickPromise 1.0\n"
                  "import QuickFuture 1.0\n"
                  "QtObject { \n"
                  "function create(future) {\n"
                  "    var promise = Q.promise();\n"
                  "    Future.onFinished(future, function(value) {\n"
                  "        if (Future.isCanceled(future)) {\n"
                  "            promise.reject();\n"
                  "        } else {\n"
                  "            promise.resolve(value);\n"
                  "        }\n"
                  "    });\n"
                  "    return promise;\n"
                  "}\n"
                  "}\n";

    QQmlComponent comp (engine);
    comp.setData(qml.toUtf8(), QUrl());
    QObject* holder = comp.create();
    if (holder == nullptr) {
        return;
    }

    promiseCreator = engine->newQObject(holder);
}

bool Future::isFinished(const QVariant &future)
{
    if (!m_wrappers.contains(typeId(future))) {
        qWarning() << QString("Future: Can not handle input data type: %1").arg(QMetaType::typeName(future.type()));
        return false;
    }

    VariantWrapperBase* wrapper = m_wrappers[typeId(future)];
    return wrapper->isFinished(future);
}

bool Future::isRunning(const QVariant &future)
{
    if (!m_wrappers.contains(typeId(future))) {
        qWarning() << QString("Future: Can not handle input data type: %1").arg(QMetaType::typeName(future.type()));
        return false;
    }

    VariantWrapperBase* wrapper = m_wrappers[typeId(future)];
    return wrapper->isRunning(future);
}

bool Future::isCanceled(const QVariant &future)
{
    if (!m_wrappers.contains(typeId(future))) {
        qWarning() << QString("Future.isCanceled: Can not handle input data type: %1").arg(QMetaType::typeName(future.type()));
        return false;
    }

    VariantWrapperBase* wrapper = m_wrappers[typeId(future)];
    return wrapper->isCanceled(future);
}

int Future::progressValue(const QVariant &future)
{
    if (!m_wrappers.contains(typeId(future))) {
        qWarning() << QString("Future.progressValue: Can not handle input data type: %1").arg(QMetaType::typeName(future.type()));
        return false;
    }

    VariantWrapperBase* wrapper = m_wrappers[typeId(future)];
    return wrapper->progressValue(future);
}

int Future::progressMinimum(const QVariant &future)
{
    if (!m_wrappers.contains(typeId(future))) {
        qWarning() << QString("Future.progressMinimum: Can not handle input data type: %1").arg(QMetaType::typeName(future.type()));
        return false;
    }

    VariantWrapperBase* wrapper = m_wrappers[typeId(future)];
    return wrapper->progressMinimum(future);
}

int Future::progressMaximum(const QVariant &future)
{
    if (!m_wrappers.contains(typeId(future))) {
        qWarning() << QString("Future.progressMaximum: Can not handle input data type: %1").arg(QMetaType::typeName(future.type()));
        return false;
    }

    VariantWrapperBase* wrapper = m_wrappers[typeId(future)];
    return wrapper->progressMaximum(future);
}

void Future::onFinished(const QVariant &future, QJSValue func, QJSValue owner)
{
    Q_UNUSED(owner);

    if (!m_wrappers.contains(typeId(future))) {
        qWarning() << QString("Future: Can not handle input data type: %1").arg(QMetaType::typeName(future.type()));
        return;
    }
    VariantWrapperBase* wrapper = m_wrappers[typeId(future)];
    wrapper->onFinished(m_engine, future, func, owner.toQObject());
}

void Future::onCanceled(const QVariant &future, QJSValue func, QJSValue owner)
{
    if (!m_wrappers.contains(typeId(future))) {
        qWarning() << QString("Future: Can not handle input data type: %1").arg(QMetaType::typeName(static_cast<int>(future.type())));
        return;
    }
    VariantWrapperBase* wrapper = m_wrappers[typeId(future)];
    wrapper->onCanceled(m_engine, future, func, owner.toQObject());
}

void Future::onProgressValueChanged(const QVariant &future, QJSValue func)
{
    if (!m_wrappers.contains(typeId(future))) {
        qWarning() << QString("Future.onProgressValueChanged: Can not handle input data type: %1").arg(QMetaType::typeName(future.type()));
        return;
    }
    VariantWrapperBase* wrapper = m_wrappers[typeId(future)];
    wrapper->onProgressValueChanged(m_engine, future, func);
}

QVariant Future::result(const QVariant &future)
{
    QVariant res;
    if (!m_wrappers.contains(typeId(future))) {
        qWarning() << QString("Future: Can not handle input data type: %1").arg(QMetaType::typeName(future.type()));
        return res;
    }

    VariantWrapperBase* wrapper = m_wrappers[typeId(future)];
    return wrapper->result(future);
}

QVariant Future::results(const QVariant &future)
{
    QVariant res;
    if (!m_wrappers.contains(typeId(future))) {
        qWarning() << QString("Future: Can not handle input data type: %1").arg(QMetaType::typeName(future.type()));
        return res;
    }

    VariantWrapperBase* wrapper = m_wrappers[typeId(future)];
    return wrapper->results(future);
}

QJSValue Future::promise(QJSValue future)
{
    QJSValue create = promiseCreator.property("create");
    QJSValueList args;
    args << future;

    QJSValue result = create.call(args);
    if (result.isError() || result.isUndefined()) {
        qWarning() << "Future.promise: QuickPromise is not installed or setup properly";
        result = QJSValue();
    }

    return result;
}

void Future::sync(const QVariant &future, const QString &propertyInFuture, QObject *target, const QString &propertyInTarget)
{
    if (!m_wrappers.contains(typeId(future))) {
        qWarning() << QString("Future: Can not handle input data type: %1").arg(QMetaType::typeName(future.type()));
        return;
    }


    VariantWrapperBase* wrapper = m_wrappers[typeId(future)];
    wrapper->sync(future, propertyInFuture, target, propertyInTarget);
}

static QObject *provider(QQmlEngine *engine, QJSEngine *scriptEngine) {
    Q_UNUSED(scriptEngine);

    auto object = new Future();
    object->setEngine(engine);

    return object;
}

static void init() {
    bool called = false;
    if (called) {
        return;
    }
    called = true;

    QCoreApplication* app = QCoreApplication::instance();
    auto tmp = new QObject(app);

    QObject::connect(tmp,&QObject::destroyed,[=]() {
        auto iter = m_wrappers.begin();
        while (iter != m_wrappers.end()) {
            delete iter.value();
            iter++;
        }
    });

    qmlRegisterSingletonType<Future>("QuickFuture", 1, 0, "Future", provider);

    Future::registerType<QString>();
    Future::registerType<int>();
    Future::registerType<void>();
    Future::registerType<bool>();
    Future::registerType<qreal>();
    Future::registerType<QByteArray>();
    Future::registerType<QVariant>();
    Future::registerType<QVariantMap>();
    Future::registerType<QSize>();

    Future::registerType<QUrl>();
    Future::registerType<QUuid>();
    Future::registerType<QList<QUuid>>();

}

#ifndef QUICK_FUTURE_BUILD_PLUGIN
Q_COREAPP_STARTUP_FUNCTION(init)
#endif
} // End of namespace

#ifdef QUICK_FUTURE_BUILD_PLUGIN
void QuickFutureQmlPlugin::registerTypes(const char *uri) {
    Q_ASSERT(QString("QuickFuture") == uri);
    QuickFuture::init();
}
#endif
