// Author:  Ben Lau (https://github.com/benlau)

#include <QApplication>
#include <QStringList>
#include <QtQuickTest/quicktest.h>
#include <QtCore>

namespace AutoTestRegister {
    QUICK_TEST_MAIN(QuickTests)
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    Q_INIT_RESOURCE(quickpromise);

    QString importPath = "qrc:///";
    QStringList args = a.arguments();
    QString executable = args.takeAt(0);

    char **s = (char**) malloc(sizeof(char*) * 10 );
    int idx = 0;
    s[idx++] = executable.toUtf8().data();
    s[idx++] = strdup("-import");
    s[idx++] = strdup(SRCDIR);
    s[idx++] = strdup("-import");
    s[idx++] = strdup(importPath.toLocal8Bit().data());

    foreach( QString arg,args) {
        s[idx++] = strdup(arg.toUtf8().data());
    }

    s[idx++] = 0;

    const char *name = "QuickPromise";
    const char *source = SRCDIR;

    bool error = quick_test_main( idx-1,s,name,source);

    if (error){
        qWarning() << "Error found!";
        return -1;
    }

    return 0;
}
