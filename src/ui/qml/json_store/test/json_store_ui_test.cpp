// SPDX-License-Identifier: Apache-2.0

#include "xstudio/atoms.hpp"
#include "xstudio/json_store/json_store_actor.hpp"
#include "xstudio/ui/qml/json_store_ui.hpp"

#include <gtest/gtest.h>

CAF_PUSH_WARNINGS
#include <QDebug>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
CAF_POP_WARNINGS

using namespace caf;

using namespace xstudio::json_store;
using namespace xstudio::utility;
using namespace xstudio::ui::qml;
#include "xstudio/utility/serialise_headers.hpp"


ACTOR_TEST_SETUP()


void dump_children(const QObject *obj) {
    for (auto i : obj->children()) {
        qDebug() << "child name " << i->objectName() << ", class "
                 << i->metaObject()->className();
        dump_children(i);
    }
}

TEST(JsonStoreUI, Test) {
    int argc    = 0;
    char **argv = nullptr;

    fixture f;

    auto store1   = f.self->spawn<JsonStoreActor>();
    auto store1_f = make_function_view(store1);

    auto store2   = f.self->spawn<JsonStoreActor>();
    auto store2_f = make_function_view(store2);

    auto store3   = f.self->spawn<JsonStoreActor>();
    auto store3_f = make_function_view(store3);


    JsonStore json_data1(nlohmann::json::parse(
        R"({ "happy": true, "sub": {}, "pi": 3.141, "arr": [0, 2, 4] })"));
    JsonStore json_data2(nlohmann::json::parse(
        R"({ "happy": false, "sub": {}, "pi": 3.141, "arr": [0, 2, 4] })"));
    JsonStore json_data3(
        nlohmann::json::parse(R"({ "happy": false, "pi": 3.141, "arr": [0, 2, 4] })"));
    store1_f(set_json_atom_v, json_data1);
    store2_f(set_json_atom_v, json_data2);
    store3_f(set_json_atom_v, json_data3);
    auto a = store1_f(xstudio::json_store::subscribe_atom_v, "/sub", store2);
    a      = store2_f(xstudio::json_store::subscribe_atom_v, "/sub", store3);

    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QGuiApplication app(argc, argv);

    qmlRegisterType<JsonStoreUI>("xstudio.qml.json_store", 1, 0, "JsonStore");

    QQmlApplicationEngine engine;
    const QUrl url(QStringLiteral("qrc:/main.qml"));
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreated,
        &app,
        [url](QObject *obj, const QUrl &objUrl) {
            if (!obj && url == objUrl)
                QCoreApplication::exit(-1);
        },
        Qt::QueuedConnection);

    engine.load(url);


    for (auto obj : engine.rootObjects()) {
        // dump_children(obj);
        auto js = obj->findChild<JsonStoreUI *>("json_store1");
        if (js) {
            js->init(f.system);
            js->subscribe(store1);
        }

        js = obj->findChild<JsonStoreUI *>("json_store2");
        if (js) {
            js->init(f.system);
            js->subscribe(store2);
        }

        js = obj->findChild<JsonStoreUI *>("json_store3");
        if (js) {
            js->init(f.system);
            js->subscribe(store3);
        }
    }

    app.exec();

    store1_f(xstudio::json_store::unsubscribe_atom_v, store2);
    store2_f(xstudio::json_store::unsubscribe_atom_v, store3);
    f.self->send_exit(store1, exit_reason::user_shutdown);
    f.self->send_exit(store2, exit_reason::user_shutdown);
    f.self->send_exit(store3, exit_reason::user_shutdown);
}
