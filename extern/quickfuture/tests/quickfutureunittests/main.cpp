#include <QString>
#include <QtTest>
#include <TestRunner>
#include <QFuture>
#include <QSize>
#include <QuickFuture>
#include <XBacktrace.h>
#include <QtQuickTest/quicktest.h>
#include "quickfutureunittests.h"
#include <QQmlExtensionPlugin>

Q_DECLARE_METATYPE(QFuture<QSize>)

namespace AutoTestRegister {
    QUICK_TEST_MAIN(QuickTests)
}

int main(int argc, char *argv[])
{
    XBacktrace::enableBacktraceLogOnUnhandledException();

    QGuiApplication app(argc, argv);
    Q_IMPORT_PLUGIN(QuickFutureQmlPlugin);

    foreach(QObject* plugin, QPluginLoader::staticInstances()) {
        if (plugin->metaObject()->className() == QString("QuickFutureQmlPlugin")) {
            QQmlExtensionPlugin* extend = qobject_cast<QQmlExtensionPlugin*>(plugin);
            if (extend) {
                extend->registerTypes("QuickFuture");
            }
        }
    }

    TestRunner runner;
    runner.addImportPath("qrc:///");
    runner.add<QuickFutureUnitTests>();
    runner.add(QString(SRCDIR) + "/qmltests");

    int waitTime = 100;
    if (app.arguments().size() != 1) {
        waitTime = 60000;
    }

    QVariantMap config;
    config["waitTime"] = waitTime;
    runner.setConfig(config);

    bool error = runner.exec(app.arguments());

    if (!error) {
        qDebug() << "All test cases passed!";
    }

    return error;
}
