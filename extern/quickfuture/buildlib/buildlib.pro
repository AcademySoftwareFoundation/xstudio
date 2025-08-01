TARGET = quickfuture
TEMPLATE = lib
CONFIG += plugin

isEmpty(SHARED): SHARED = "false"
isEmpty(PLUGIN): PLUGIN = "true" #Install as a QML PLugin

DEFAULT_INSTALL_ROOT = $$[QT_INSTALL_LIBS]

isEmpty(INSTALL_ROOT) {
    INSTALL_ROOT = $${DEFAULT_INSTALL_ROOT}
}

include(../quickfuture.pri)

INSTALLS += target
DEFINES += QUICK_FUTURE_BUILD_PLUGIN

equals(PLUGIN, "true") {
    SHARED = "true"
    TARGET = quickfutureqmlplugin
    isEmpty(INSTALL_ROOT): INSTALL_ROOT=$$[QT_INSTALL_QML]

    target.path = $${INSTALL_ROOT}/QuickFuture

    QML.files = $$PWD/qmldir $$PWD/quickfuture.qmltypes
    QML.path = $${INSTALL_ROOT}/QuickFuture

    INSTALLS += QML

} else {
    isEmpty(INSTALL_ROOT): INSTALL_ROOT=$$[QT_INSTALL_LIBS]

    headers.files += \
        $$PWD/qfvariantwrapper.h \
        $$PWD/qffuture.h \
        $$PWD/QuickFuture \
        $$PWD/quickfuture.h

    target.path = $${INSTALL_ROOT}/lib
    INSTALLS += headers

    headers.path = $${INSTALL_ROOT}/include
}

equals(SHARED, "false") {
    CONFIG += staticlib
}

DISTFILES += \
    quickfuture.qmltypes \
    qmldir
