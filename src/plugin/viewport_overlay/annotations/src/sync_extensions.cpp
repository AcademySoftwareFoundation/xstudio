// SPDX-License-Identifier: Apache-2.0

#include "xstudio/utility/helpers.hpp"
#include "annotations_tool.hpp"

using namespace xstudio;
using namespace xstudio::ui::viewport;

namespace {

    std::string timestmp() {
        std::time_t time = std::time({});
        std::array<char, 1024> timeString;
        std::strftime(timeString.data(), timeString.size(), "%Y-%m-%dT%H:%M:%S.%fZ", std::gmtime(&time));
        return std::string(timeString.data());
    }

    static const nlohmann::json paintStartTemplate = 
        nlohmann::json::parse(
        R"(
{
    "schema": "SYNC_REVIEW_1.0",
    "payload": {
        "command_schema": "Annotation.1",
        "command": {
            "event": "PaintStart",
            "payload": {
                "timestamp": "",
                "source_index": 0,
                "paint": {
                    "OTIO_SCHEMA": "Paint.1",
                    "uuid": "",
                    "friendly_name": "",
                    "participant_hash": "",
                    "point": {},
                    "rgba": [ 1.0, 1.0, 0.0, 1.0 ],
                    "type": "COLOR",
                    "brush": "circle",
                    "visible": true,
                    "name": "Paint",
                    "effect_name": "Paint",
                    "layer_range": {
                        "OTIO_SCHEMA": "TimeRange.1",
                        "start_time": {
                            "OTIO_SCHEMA": "RationalTime.1",
                            "value": 0,
                            "rate": 24.0
                        },
                        "duration": {
                            "OTIO_SCHEMA": "RationalTime.1",
                            "value": 1,
                            "rate": 24.0
                        }
                    },
                    "hold": false,
                    "ghost": false,
                    "ghost_before": 3,
                    "ghost_after": 3
                }
            }
        }
    }
})");

    static const nlohmann::json paintPointTemplate = 
        nlohmann::json::parse(
        R"(
{
    "schema": "SYNC_REVIEW_1.0",
    "payload": {
        "command_schema": "Annotation.1",
        "command": {
            "event": "PaintPoint",
            "payload": {
                "creation_timestamp": "",
                "point_target": {
                    "source_index": 0,
                    "uuid": "",
                    "range": {
                        "OTIO_SCHEMA": "TimeRange.1",
                        "start_time": {
                            "OTIO_SCHEMA": "RationalTime.1",
                            "value": 0,
                            "rate": 24.0
                        },
                        "duration": {
                            "OTIO_SCHEMA": "RationalTime.1",
                            "value": 1,
                            "rate": 24.0
                        }
                    }
                },
                "point": {
                    "OTIO_SCHEMA": "Point.1",
                    "x": 5.669550344154889,                
                    "y": 2.0246346746398025,
                    "size": 0.05   
                }         
            }
        }
    }
})");

    static const nlohmann::json paintEndTemplate = 
        nlohmann::json::parse(
        R"(
{
    "schema": "SYNC_REVIEW_1.0",
    "payload": {
        "command_schema": "Annotation.1",
        "command": {
            "event": "PaintEnd",
            "payload": { 
                "uuid": "",
                "timestamp": ""        
            }
        }
    }
})");

    static const nlohmann::json paintClearTemplate = 
        nlohmann::json::parse(
        R"(
{
    "schema": "SYNC_REVIEW_1.0",
    "payload": {
        "command_schema": "Annotation.1",
        "command": { 
            "event": "Clear",
            "payload": {
                "timestamp": "2025-01-31T16:14:00Z",
                "clear_all": false,
                "clear_target": {
                    "source_index": 0,
                    "range": {
                        "OTIO_SCHEMA": "TimeRange.1",
                        "start_time": {
                            "OTIO_SCHEMA": "RationalTime.1",
                            "value": 0.0,
                            "rate": 24.0
                        },
                        "duration": {
                            "OTIO_SCHEMA": "RationalTime.1",
                            "value": 1.0,
                            "rate": 24.0
                        }
                    }
                }     
            } 
        }
    }
})");

const static auto EVENT_JPOINTER  = nlohmann::json::json_pointer("/event");
const static auto RGBA_JPOINTER  = nlohmann::json::json_pointer("/payload/paint/rgba");
const static auto X_JPOINTER  = nlohmann::json::json_pointer("/payload/point/x");
const static auto Y_JPOINTER  = nlohmann::json::json_pointer("/payload/point/y");
const static auto SIZE_JPOINTER  = nlohmann::json::json_pointer("/payload/point/size");

}

void AnnotationsTool::paint_start_event(const Imath::V2f &point) {

    paint_stroke_id_ = utility::Uuid::generate();

    utility::JsonStore paintStartData = paintStartTemplate;
    paintStartData["payload"]["command"]["payload"]["timestamp"] = timestmp();
    paintStartData["payload"]["command"]["payload"]["paint"]["uuid"] = paint_stroke_id_;

    auto rgb = pen_colour_->value();
    auto a = float(pen_opacity_->value())/100.0f;
    paintStartData["payload"]["command"]["payload"]["paint"]["rgba"][0] = rgb.red();
    paintStartData["payload"]["command"]["payload"]["paint"]["rgba"][1] = rgb.green();
    paintStartData["payload"]["command"]["payload"]["paint"]["rgba"][2] = rgb.blue();
    paintStartData["payload"]["command"]["payload"]["paint"]["rgba"][3] = a;

    paintStartData["payload"]["command"]["payload"]["paint"]["layer_range"]["start_time"]["value"] = int(image_being_annotated_.playhead_logical_frame());
    paintStartData["payload"]["command"]["payload"]["paint"]["layer_range"]["start_time"]["rate"] = 1.0/timebase::to_seconds(image_being_annotated_.frame_id().rate());

    paintStartData["payload"]["command"]["payload"]["paint"]["layer_range"]["duration"]["value"] = 1;
    paintStartData["payload"]["command"]["payload"]["paint"]["layer_range"]["duration"]["rate"] = 1.0/timebase::to_seconds(image_being_annotated_.frame_id().rate());
    
    mail(utility::event_atom_v, ui::viewport::annotation_atom_v, paintStartData).send(plugin_events_group());
}

void AnnotationsTool::paint_point_event(const Imath::V2f &point) {

    if (paint_stroke_id_.is_null()) return;

    std::time_t time = std::time({});
    std::array<char, 1024> timeString;
    std::strftime(timeString.data(), timeString.size(), "%Y-%m-%dT%H:%M:%S.%fZ", std::gmtime(&time));

    utility::JsonStore paintPointData = paintPointTemplate;
    paintPointData["payload"]["command"]["payload"]["creation_timestamp"] = timestmp();
    paintPointData["payload"]["command"]["payload"]["point_target"]["uuid"] = paint_stroke_id_;

    paintPointData["payload"]["command"]["payload"]["point_target"]["range"]["start_time"]["value"] = int(image_being_annotated_.playhead_logical_frame());
    paintPointData["payload"]["command"]["payload"]["point_target"]["range"]["start_time"]["rate"] = 1.0/timebase::to_seconds(image_being_annotated_.frame_id().rate());

    paintPointData["payload"]["command"]["payload"]["point_target"]["range"]["duration"]["rate"] = 1.0/timebase::to_seconds(image_being_annotated_.frame_id().rate());
    
    paintPointData["payload"]["command"]["payload"]["point"]["x"] = point.x;
    paintPointData["payload"]["command"]["payload"]["point"]["y"] = -point.y;
    paintPointData["payload"]["command"]["payload"]["point"]["size"] = draw_pen_size_->value() / PEN_STROKE_THICKNESS_SCALE;

    mail(utility::event_atom_v, ui::viewport::annotation_atom_v, paintPointData).send(plugin_events_group());

}

void AnnotationsTool::paint_end_event() {

    if (paint_stroke_id_.is_null()) return;

    std::time_t time = std::time({});
    std::array<char, 1024> timeString;
    std::strftime(timeString.data(), timeString.size(), "%Y-%m-%dT%H:%M:%S.%fZ", std::gmtime(&time));

    utility::JsonStore paintEndData = paintEndTemplate;
    paintEndData["payload"]["command"]["payload"]["timestamp"] = timestamp();
    paintEndData["payload"]["command"]["payload"]["uuid"] = paint_stroke_id_;

    mail(utility::event_atom_v, ui::viewport::annotation_atom_v, paintEndData).send(plugin_events_group());

    paint_stroke_id_ = utility::Uuid();

}

void AnnotationsTool::incoming_paint_event(const utility::JsonStore &command_data) {

    try {

        if (!command_data.contains(EVENT_JPOINTER)) {
            throw std::runtime_error("event field.");
        }
        const auto event = command_data.at(EVENT_JPOINTER).get<std::string>();

        if (event == "PaintStart") {

            if (command_data.contains(RGBA_JPOINTER)) {
                const auto &rgba = command_data.at(RGBA_JPOINTER);
                std::cerr << "rgba " << rgba.dump() << "\n";
                if (rgba.is_array() && rgba.size() == 4) {

                    start_editing(active_viewport_name());

                    utility::ColourTriplet rgb(rgba[0].get<float>(), rgba[1].get<float>(), rgba[2].get<float>());
                    float opacity = rgba[3].get<float>();

                    interaction_canvas_.start_stroke(
                        rgb,
                        0.1, // dummy size value until we get first point
                        0.0f,
                        opacity);

                } else {
                    throw std::runtime_error("colour field.");
                }
            } else {
                throw std::runtime_error("colour field.");
            }

        } else if (event == "PaintPoint") {

            Imath::V2f pt;
            // Annotation spec normalises to the HEIGHT of the image.
            const float scale_factor = 1.0f/(image_aspect(image_being_annotated_)*0.5f);
            float size = 0;
            if (command_data.contains(X_JPOINTER)) {
                pt.x = command_data.at(X_JPOINTER).get<float>()*scale_factor;
            } else std::runtime_error("x point field.");
            if (command_data.contains(Y_JPOINTER)) {
                pt.y = -command_data.at(Y_JPOINTER).get<float>()*scale_factor;
            } else std::runtime_error("y point field.");
            if (command_data.contains(SIZE_JPOINTER)) {
                size = command_data.at(SIZE_JPOINTER).get<float>();
            } else std::runtime_error("size field.");

            interaction_canvas_.update_stroke(pt, size);

        } else if (event == "PaintEnd") {

            interaction_canvas_.end_draw();
            if (is_laser_mode()) {
                // start up the laser fade timer loop - see the event handler
                // in the constructor here to see how this works
                anon_mail(utility::event_atom_v, true).send(caf::actor_cast<caf::actor>(this));

            } else {
                update_bookmark_annotation_data();
            }

        }
    } catch (std::exception &e) {

        spdlog::warn("{} Failed to porcess event payload relating to: {}", __PRETTY_FUNCTION__, e.what());

    }
    do_redraw();
}
