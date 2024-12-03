// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.15

import xstudio.qml.models 1.0

import xStudio 1.0

Item {

    /* This connects to the backend model data named grading_settings to which
    many of our attributes have been added*/
    XsModuleData {
        id: mask_tool_attrs_data
        modelDataName: "mask_tool_settings"
    }
    property alias model: mask_tool_attrs_data

    // to DIRECTLY expose attribute role data we use XsAttributeValue and give it the
    // title (name) of the attribute. By default it will expose the 'value'
    // role data of the attribute but you can override this to get to other
    // role datas such as 'combo_box_options' (the string choices in a
    // StringChoiceAttribute) or 'default_value' etc.

    XsAttributeValue {
        id: __draw_pen_size
        attributeTitle: "Draw Pen Size"
        model: mask_tool_attrs_data
    }
    property alias draw_pen_size: __draw_pen_size.value

    XsAttributeValue {
        id: __erase_pen_size
        attributeTitle: "Erase Pen Size"
        model: mask_tool_attrs_data
    }
    property alias erase_pen_size: __erase_pen_size.value

    XsAttributeValue {
        id: __pen_colour
        attributeTitle: "Pen Colour"
        model: mask_tool_attrs_data
    }
    property alias pen_colour: __pen_colour.value

    XsAttributeValue {
        id: __pen_opacity
        attributeTitle: "Pen Opacity"
        model: mask_tool_attrs_data
    }
    property alias pen_opacity: __pen_opacity.value

    XsAttributeValue {
        id: __pen_softness
        attributeTitle: "Pen Softness"
        model: mask_tool_attrs_data
    }
    property alias pen_softness: __pen_softness.value

    XsAttributeValue {
        id: __drawing_tool
        attributeTitle: "drawing_tool"
        model: mask_tool_attrs_data
    }
    property alias drawing_tool: __drawing_tool.value

    XsAttributeValue {
        id: __drawing_action
        attributeTitle: "drawing_action"
        model: mask_tool_attrs_data
    }
    property alias drawing_action: __drawing_action.value

    XsAttributeValue {
        id: __display_mode
        attributeTitle: "display_mode"
        model: mask_tool_attrs_data
    }
    property alias display_mode: __display_mode.value

    XsAttributeValue {
        id: __mask_selected_shape
        attributeTitle: "mask_selected_shape"
        model: mask_tool_attrs_data
    }
    property alias mask_selected_shape: __mask_selected_shape.value

    XsAttributeValue {
        id: __mask_shapes_visible
        attributeTitle: "mask_shapes_visible"
        model: mask_tool_attrs_data
    }
    property alias mask_shapes_visible: __mask_shapes_visible.value

    XsAttributeValue {
        id: __shape_invert
        attributeTitle: "shape_invert"
        model: mask_tool_attrs_data
    }
    property alias shape_invert: __shape_invert.value

    XsAttributeValue {
        id: __polygon_init
        attributeTitle: "polygon_init"
        model: mask_tool_attrs_data
    }
    property alias polygon_init: __polygon_init.value

}