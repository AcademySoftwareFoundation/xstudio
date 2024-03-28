// SPDX-License-Identifier: Apache-2.0
#include <utility>

#include "annotation.hpp"
#include "annotations_tool.hpp"
#include "annotation_serialiser.hpp"

using namespace xstudio::ui::viewport;
using namespace xstudio;


Annotation::Annotation() : bookmark::AnnotationBase() {}

Annotation::Annotation(const utility::JsonStore &s) : bookmark::AnnotationBase() {

    AnnotationSerialiser::deserialise(this, s);
}

utility::JsonStore Annotation::serialise(utility::Uuid &plugin_uuid) const {

    plugin_uuid = AnnotationsTool::PLUGIN_UUID;
    return AnnotationSerialiser::serialise((const Annotation *)this);
}
