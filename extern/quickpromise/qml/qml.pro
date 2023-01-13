TARGET = quickpromise

isEmpty(SHARED) {
    SHARED = "false"
}

isEmpty(INSTALL_ROOT) {
    INSTALL_ROOT = $$[QT_INSTALL_LIBS]
}

target.path = $${INSTALL_ROOT}

qml.path = $${INSTALL_ROOT}/QuickPromise
qml.files = \
    QuickPromise/promise.js \
    QuickPromise/qmldir \
    QuickPromise/Promise.qml \
    QuickPromise/PromiseTimer.qml

equals(SHARED, "true") {
    TEMPLATE = aux
    INSTALLS += qml
} else {
    TEMPLATE = lib
    CONFIG += staticlib
    include(../quickpromise.pri)
    INSTALLS += target
}
