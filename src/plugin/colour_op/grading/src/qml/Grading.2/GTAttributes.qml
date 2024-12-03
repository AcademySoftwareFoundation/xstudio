// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.15

import xstudio.qml.models 1.0

import xStudio 1.0

Item {

    /* This connects to the backend model data named grading_settings to which
    many of our attributes have been added*/
    XsModuleData {
        id: grading_tool_attrs_data
        modelDataName: "grading_settings"
    }
    property alias grading_settings: grading_tool_attrs_data

    ///////////////////////////////////////////////////////////////////////
    // to DIRECTLY expose attribute role data we use XsAttributeValue and
    // give it the title (name) of the attribute. By default it will expose
    // the 'value' role data of the attribute but you can override this to
    // get to other role datas such as 'combo_box_options' (the string
    // choices in a StringChoiceAttribute) or 'default_value' etc.

    XsAttributeValue {
        id: __tool_opened_count
        attributeTitle: "tool_opened_count"
        model: grading_tool_attrs_data
    }
    property alias tool_opened_count: __tool_opened_count.value

    XsAttributeValue {
        id: __grading_action
        attributeTitle: "grading_action"
        model: grading_tool_attrs_data
    }
    property alias grading_action: __grading_action.value

    XsAttributeValue {
        id: __media_colour_managed
        attributeTitle: "media_colour_managed"
        model: grading_tool_attrs_data
    }
    property alias media_colour_managed: __media_colour_managed.value

    XsAttributeValue {
        id: __grading_bookmark
        attributeTitle: "grading_bookmark"
        model: grading_tool_attrs_data
    }
    property alias grading_bookmark: __grading_bookmark.value

    XsAttributeValue {
        id: __grading_bypass
        attributeTitle: "grading_bypass"
        model: grading_tool_attrs_data
    }
    property alias grading_bypass: __grading_bypass.value

    XsAttributeValue {
        id: __working_space
        attributeTitle: "working_space"
        model: grading_tool_attrs_data
    }
    property alias working_space: __working_space.value

    XsAttributeValue {
        id: __colour_space
        attributeTitle: "colour_space"
        model: grading_tool_attrs_data
    }
    property alias colour_space: __colour_space.value

    XsAttributeValue {
        id: __grade_in
        attributeTitle: "grade_in"
        model: grading_tool_attrs_data
    }
    property alias grade_in: __grade_in.value

    XsAttributeValue {
        id: __grade_out
        attributeTitle: "grade_out"
        model: grading_tool_attrs_data
    }
    property alias grade_out: __grade_out.value

    XsAttributeValue {
        id: __mask_tool_active
        attributeTitle: "mask_tool_active"
        model: grading_tool_attrs_data
    }
    property alias mask_tool_active: __mask_tool_active.value

    ///////////////////////////////////////////////////////////////////////
    // Get access to all 'role' data items of the actual grading attributes
    // The 'values' object has properties that map to the names of attribute
    // role data. E.g. 'value' 'default_value' 'float_scrub_min' 'combo_box_options'
    // etc.

    XsAttributeFullData {
        id: __slope
        attributeTitle: "Slope"
        model: grading_tool_attrs_data
    }
    property alias slope: __slope.values

    XsAttributeFullData {
        id: __offset
        attributeTitle: "Offset"
        model: grading_tool_attrs_data
    }
    property alias offset: __offset.values

    XsAttributeFullData {
        id: __power
        attributeTitle: "Power"
        model: grading_tool_attrs_data
    }
    property alias power: __power.values

    XsAttributeFullData {
        id: __saturation
        attributeTitle: "Saturation"
        model: grading_tool_attrs_data
    }
    property alias saturation: __saturation.values

    XsAttributeFullData {
        id: __exposure
        attributeTitle: "Exposure"
        model: grading_tool_attrs_data
    }
    property alias exposure: __exposure.values

    XsAttributeFullData {
        id: __contrast
        attributeTitle: "Contrast"
        model: grading_tool_attrs_data
    }
    property alias contrast: __contrast.values

    ///////////////////////////////////////////////////////////////////////
    // this provides access to the attributes in the "grading_sliders" group
    XsModuleData {
        id: grading_wheels_model
        modelDataName: "grading_wheels"
    }
    property alias grading_wheels_model: grading_wheels_model

    XsModuleData {
        id: grading_sliders_model
        modelDataName: "grading_sliders"
    }
    property alias grading_sliders_model: grading_sliders_model

    ///////////////////////////////////////////////////////////////////////
    // helpers
    function getAttrValue(attr_name) {
        var idx = grading_wheels_model.searchRecursive(attr_name,"title")
        if (idx.valid) {
            return grading_wheels_model.get(idx, "value")
        }

        var idx = grading_sliders_model.searchRecursive(attr_name,"title")
        if (idx.valid) {
            return grading_sliders_model.get(idx, "value")
        }

        console.log("GradingAttrs: attribute named", attr_name, "not found.")
        return undefined
    }

}