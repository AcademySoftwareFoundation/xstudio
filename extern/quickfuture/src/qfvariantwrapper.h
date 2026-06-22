#ifndef QFVARIANTWRAPPER_H
#define QFVARIANTWRAPPER_H

//NOLINTBEGIN

#include <QVariant>
#include <QFuture>
#include <QJSValue>
#include <QFutureWatcher>
#include <QPointer>
#include <QJSEngine>
#include <QCoreApplication>
#include <QQmlEngine>
#include <functional>

namespace QuickFuture {

    typedef std::function<QVariant(void*)> Converter;

    template <typename T>
    inline QJSValueList valueList(const QPointer<QQmlEngine>& engine, const QFuture<T>& future) {
        QJSValue value;
        if (future.resultCount() > 0)
            value = engine->toScriptValue<T>(future.result());
        return QJSValueList() << value;
    }

    template <>
    inline QJSValueList valueList<void>(const QPointer<QQmlEngine>& engine, const QFuture<void>& future) {
        Q_UNUSED(engine);
        Q_UNUSED(future);
        return QJSValueList();
    }

    template <typename F>
    inline void nextTick(F func) {
        QObject tmp;
        QObject::connect(&tmp, &QObject::destroyed, QCoreApplication::instance(), func, Qt::QueuedConnection);
    }

    template <typename T>
    inline QVariant toVariant(const QFuture<T> &future, Converter converter) {
        if (!future.isResultReadyAt(0)) {
            qWarning() << "Future.result(): The result is not ready!";
            return QVariant();
        }

        QVariant ret;

        if (converter != nullptr) {
            T t = future.result();
            ret = converter(&t);
        } else {
            ret =  QVariant::fromValue<T>(future.result());
        }

        return ret;
    }

    template <>
    inline QVariant toVariant<void>(const QFuture<void> &future, Converter converter) {
        Q_UNUSED(converter);
        Q_UNUSED(future);
        return QVariant();
    }

    template <typename T>
    inline QVariant toVariantList(const QFuture<T> &future, Converter converter) {
        if (future.resultCount() == 0) {
            qWarning() << "Future.results(): The result is not ready!";
            return QVariant();
        }

        QVariantList ret;

        QList<T> results = future.results();

        if (converter != nullptr) {

            for (int i = 0 ; i < results.size() ;i++) {
                T t = future.resultAt(i);
                ret.append(converter(&t));
            }

        } else {

            for (int i = 0 ; i < results.size() ;i++) {
                ret.append(QVariant::fromValue<T>(future.resultAt(i)));
            }

        }

        return ret;
    }

    template <>
    inline QVariant toVariantList<void>(const QFuture<void> &future, Converter converter) {
        Q_UNUSED(converter);
        Q_UNUSED(future);
        return QVariant();
    }

    inline void printException(QJSValue value) {
        QString message = QString("%1:%2: %3: %4")
                          .arg(value.property("fileName").toString())
                          .arg(value.property("lineNumber").toString())
                          .arg(value.property("name").toString())
                          .arg(value.property("message").toString());
        qWarning() << message;
    }

class VariantWrapperBase {
public:
    VariantWrapperBase() {
    }

    virtual inline ~VariantWrapperBase() {
    }

    virtual bool isPaused(const QVariant& v) = 0;
    virtual bool isFinished(const QVariant& v) = 0;
    virtual bool isRunning(const QVariant& v) = 0;
    virtual bool isCanceled(const QVariant& v) = 0;

    virtual int progressValue(const QVariant& v) = 0;

    virtual int progressMinimum(const QVariant& v) = 0;

    virtual int progressMaximum(const QVariant& v) = 0;

    virtual QVariant result(const QVariant& v) = 0;

    virtual QVariant results(const QVariant& v) = 0;

    virtual void onFinished(QPointer<QQmlEngine> engine, const QVariant& v, const QJSValue& func, QObject* owner) = 0;

    virtual void onCanceled(QPointer<QQmlEngine> engine, const QVariant& v, const QJSValue& func, QObject* owner) = 0;

    virtual void onProgressValueChanged(QPointer<QQmlEngine> engine, const QVariant& v, const QJSValue& func) = 0;

    virtual void sync(const QVariant &v, const QString &propertyInFuture, QObject *target, const QString &propertyInTarget) = 0;

    // Obtain the value of property by name
    bool property(const QVariant& v, const QString& name) {
        bool res = false;
        if (name == "isFinished") {
            res = isFinished(v);
        } else if (name == "isRunning") {
            res = isRunning(v);
        } else if (name == "isPaused") {
            res = isPaused(v);
        } else {
            qWarning().noquote() << QString("Future: Unknown property: %1").arg(name);
        }
        return res;
    }

    Converter converter;
};

#define QF_WRAPPER_DECL_READ(type, method) \
    virtual type method(const QVariant& v) { \
        QFuture<T> future = v.value<QFuture<T> >();\
        return future.method(); \
    }

#define QF_WRAPPER_CHECK_CALLABLE(method, func) \
    if (!func.isCallable()) { \
        qWarning() << "Future." #method ": Callback is not callable"; \
        return; \
    }

#define QF_WRAPPER_CONNECT(method, checker, watcherSignal) \
        virtual void method(QPointer<QQmlEngine> engine, const QVariant& v, const QJSValue& func, QObject* owner) { \
        QPointer<QObject> context = owner; \
        if (!func.isCallable()) { \
            qWarning() << "Future." #method ": Callback is not callable"; \
            return; \
        } \
        QFuture<T> future = v.value<QFuture<T>>(); \
        auto listener = [=]() { \
            if (!engine.isNull()) { \
                QJSValue callback = func; \
                QJSValue ret = callback.call(QuickFuture::valueList<T>(engine, future)); \
                if (ret.isError()) { \
                    printException(ret); \
                } \
            } \
        };\
        if (future.checker()) { \
            QuickFuture::nextTick([=]() { \
                if (owner && context.isNull()) { \
                    return;\
                } \
                listener(); \
            }); \
        } else { \
            QFutureWatcher<T> *watcher = new QFutureWatcher<T>(); \
            QObject::connect(watcher, &QFutureWatcherBase::watcherSignal, [=]() { \
                listener(); \
                delete watcher; \
            }); \
            watcher->setParent(owner); \
            watcher->setFuture(future); \
        } \
    }

template <typename T>
class VariantWrapper : public VariantWrapperBase {
public:

    QF_WRAPPER_DECL_READ(bool, isFinished)

    QF_WRAPPER_DECL_READ(bool, isRunning)

    QF_WRAPPER_DECL_READ(bool, isPaused)

    QF_WRAPPER_DECL_READ(bool, isCanceled)

    QF_WRAPPER_DECL_READ(int, progressValue)

    QF_WRAPPER_DECL_READ(int, progressMinimum)

    QF_WRAPPER_DECL_READ(int, progressMaximum)

    QF_WRAPPER_CONNECT(onFinished, isFinished, finished)

    QF_WRAPPER_CONNECT(onCanceled, isCanceled, canceled)

    QVariant result(const QVariant &future) {
        QFuture<T> f = future.value<QFuture<T>>();
        return QuickFuture::toVariant(f, converter);
    }

    QVariant results(const QVariant &future) {
        QFuture<T> f = future.value<QFuture<T>>();
        return QuickFuture::toVariantList(f, converter);
    }

    void onProgressValueChanged(QPointer<QQmlEngine> engine, const QVariant &v, const QJSValue &func) {
        if (!func.isCallable()) {
            qWarning() << "Future.onProgressValueChanged: Callback is not callable";
            return;
        }

        QFuture<T> future = v.value<QFuture<T>>();
        QFutureWatcher<T> *watcher = 0;
        auto listener = [=](int value) {
            if (!engine.isNull()) {
                QJSValue callback = func;
                QJSValueList args;
                args << engine->toScriptValue<int>(value);
                QJSValue ret = callback.call(args);
                if (ret.isError()) {
                    printException(ret);
                }
            }
        };
        watcher = new QFutureWatcher<T>();
        QObject::connect(watcher, &QFutureWatcherBase::progressValueChanged, listener);
        QObject::connect(watcher, &QFutureWatcherBase::finished, [=](){
            watcher->disconnect();
            watcher->deleteLater();
        });
        watcher->setFuture(future);
    }

    void sync(const QVariant &future, const QString &propertyInFuture, QObject *target, const QString &propertyInTarget) {
        QPointer<QObject> object = target;
        QString pt = propertyInTarget;
        if (pt.isEmpty()) {
            pt = propertyInFuture;
        }

        auto setProperty = [=]() {
            if (object.isNull()) {
                return;
            }
            bool value = property(future, propertyInFuture);
            object->setProperty( pt.toUtf8().constData(), value);
        };

        setProperty();
        QFuture<T> f = future.value<QFuture<T> >();

        if (f.isFinished()) {
            // No need to listen on an already finished future
            return;
        }

        QFutureWatcher<T> *watcher = new QFutureWatcher<T>();

        QObject::connect(watcher, &QFutureWatcherBase::canceled, setProperty);
        QObject::connect(watcher, &QFutureWatcherBase::paused, setProperty);
        QObject::connect(watcher, &QFutureWatcherBase::resumed, setProperty);
        QObject::connect(watcher, &QFutureWatcherBase::started, setProperty);

        QObject::connect(watcher,  &QFutureWatcherBase::finished, [=]() {
            setProperty();
            watcher->deleteLater();
        });

        watcher->setFuture(f);
    }
};

} // End of namespace

//NOLINTEND
#endif // QFVARIANTWRAPPER_H
