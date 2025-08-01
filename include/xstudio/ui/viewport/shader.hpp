// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "xstudio/utility/uuid.hpp"
#include "xstudio/ui/viewport/enums.hpp"
#include <memory>
#include <string>

namespace xstudio {
namespace ui {
    namespace viewport {

        /* Base class for shader data. Subclass for OpenGL, Metal,
        Vulkan, DirectX etc. */

        class GPUShader {
          public:
            GPUShader(utility::Uuid id, GraphicsAPI api) : shader_id_(id), graphics_api_(api) {}

            [[nodiscard]] utility::Uuid shader_id() const { return shader_id_; }
            [[nodiscard]] GraphicsAPI graphics_api() const { return graphics_api_; }

          private:
            utility::Uuid shader_id_;
            const GraphicsAPI graphics_api_;
        };

        typedef std::shared_ptr<const GPUShader> GPUShaderPtr;

    } // namespace viewport
} // namespace ui
} // namespace xstudio