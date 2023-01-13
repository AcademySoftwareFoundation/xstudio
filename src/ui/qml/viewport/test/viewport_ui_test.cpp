// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include "xstudio/atoms.hpp"

CAF_PUSH_WARNINGS
#include <QDebug>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
CAF_POP_WARNINGS

using namespace caf;

using namespace xstudio::utility;
using namespace xstudio::ui::qml;

#include "xstudio/utility/serialise_headers.hpp"


ACTOR_TEST_SETUP()


TEST(ViewportUI, Test) { fixture f; }
