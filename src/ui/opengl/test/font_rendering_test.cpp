// SPDX-License-Identifier: Apache-2.0

#include <caf/all.hpp>
#include <gtest/gtest.h>
#include "xstudio/utility/helpers.hpp"
#include "xstudio/ui/opengl/opengl_text_rendering.hpp"

using namespace xstudio::ui::opengl;
using namespace xstudio;

TEST(LoadFontTest, Test) {

    OpenGLTextRendererBitmap(utility::xstudio_root("/fonts/Overpass-Regular.ttf"), 48);
}