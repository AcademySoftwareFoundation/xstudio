// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <caf/all.hpp>

CAF_PUSH_WARNINGS
#include <pybind11/pybind11.h>
CAF_POP_WARNINGS

#include "xstudio/utility/uuid.hpp"

PYBIND11_MAKE_OPAQUE(std::vector<xstudio::utility::Uuid>)
PYBIND11_MAKE_OPAQUE(std::vector<xstudio::utility::UuidActor>)
