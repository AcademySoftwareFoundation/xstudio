// SPDX-License-Identifier: Apache-2.0
#include "xstudio/global/xstudio_actor_system.hpp" //NOLINT
#include "xstudio/ui/qml/helper_ui.hpp"            //NOLINT

CAF_PUSH_WARNINGS
#include "xstudio_app.hpp"
#include <QVBoxLayout>
CAF_POP_WARNINGS

using namespace xstudio;

XstudioPyApp::XstudioPyApp(QWidget *parent) : QObject(parent) {

    // make sure our logger is started
    utility::start_logger(spdlog::level::info);

    // this call sets up the CAF actor system, if it hasn't already happened
    CafActorSystem::instance();

    // this call sets up all the xstudio core components
    CafActorSystem::global_actor(false);
}

XstudioPyApp::~XstudioPyApp() { CafActorSystem::exit(); }
