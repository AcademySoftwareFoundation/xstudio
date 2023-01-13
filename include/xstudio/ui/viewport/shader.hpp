// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "xstudio/utility/uuid.hpp"
#include <memory>
#include <string>

namespace xstudio {
namespace ui {
    namespace viewport {

        struct GPUShader {
            GPUShader(utility::Uuid id, std::string code)
                : shader_id_(id), shader_code_(std::move(code)) {}
            utility::Uuid shader_id_;
            const std::string shader_code_;
        };

        typedef std::shared_ptr<const GPUShader> GPUShaderPtr;

    } // namespace viewport
} // namespace ui
} // namespace xstudio