// SPDX-License-Identifier: Apache-2.0

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import QtQml 2.15
import xstudio.qml.bookmarks 1.0
import QtQml.Models 2.14
import QtQuick.Dialogs 1.3 //for ColorDialog
import QtGraphicalEffects 1.15 //for RadialGradient

import xStudio 1.1
import xstudio.qml.module 1.0


Item {
    id: root

    width: wheel.width + 25
    height: titleRow.height + wheel.height + wheelInputCol.height

    property real size: 135

    property string title
    property var value
    property real default_value
    property real from
    property real to
    property string attr_group
    property string attr_suffix

    onValueChanged: {
        if (!carea.pressed) {
            wheel.color = val_to_pos_color(value)
        }
    }

    XsModuleAttributes {
        id: attr
        attributesGroupNames: root.attr_group

        onAttrAdded: {
            // For undetermined reasons, directly binding the value property
            // to the attribute with "attr.red_slope ? attr.red_slope : 0"
            // kind of syntax doesn't work, so we instead use this hack until
            // we understand how to make it work directly.
            // We know blue is added last so now create the full binding..
            if (attr_name.includes("blue")) {
                root.value = Qt.binding(function() {
                    return Qt.vector4d(
                        attr["red_"    + root.attr_suffix],
                        attr["green_"  + root.attr_suffix],
                        attr["blue_"   + root.attr_suffix],
                        attr["master_" + root.attr_suffix])
                })
                if (typeof val_to_pos_color === "function") {
                    wheel.color = val_to_pos_color(root.value)
                }
            }
        }
    }
    XsModuleAttributes {
        id: attr_default_value
        attributesGroupNames: root.attr_group
        roleName: "default_value"

        onAttrAdded: {
            if (attr_name.includes("blue")) {
                root.default_value = Qt.binding(function() { return attr_default_value["red_"   + root.attr_suffix] })
            }
        }
    }
    XsModuleAttributes {
        id: attr_float_scrub_min
        attributesGroupNames: root.attr_group
        roleName: "float_scrub_min"

        onAttrAdded: {
            if (attr_name.includes("master")) {
                redInput.validator.bottom    = Qt.binding(function() { return attr_float_scrub_min["red_"    + root.attr_suffix] })
                greenInput.validator.bottom  = Qt.binding(function() { return attr_float_scrub_min["green_"  + root.attr_suffix] })
                blueInput.validator.bottom   = Qt.binding(function() { return attr_float_scrub_min["blue_"   + root.attr_suffix] })
                masterInput.validator.bottom = Qt.binding(function() { return attr_float_scrub_min["master_" + root.attr_suffix] })
                root.from = Qt.binding(function() { return attr_float_scrub_min["red_"   + root.attr_suffix] })
            }
        }
    }
    XsModuleAttributes {
        id: attr_float_scrub_max
        attributesGroupNames: root.attr_group
        roleName: "float_scrub_max"

        onAttrAdded: {
            if (attr_name.includes("master")) {
                redInput.validator.top   = Qt.binding(function() { return attr_float_scrub_max["red_"    + root.attr_suffix] })
                greenInput.validator.top = Qt.binding(function() { return attr_float_scrub_max["green_"  + root.attr_suffix] })
                blueInput.validator.top  = Qt.binding(function() { return attr_float_scrub_max["blue_"   + root.attr_suffix] })
                masterInput.validator.top  = Qt.binding(function() { return attr_float_scrub_max["master_" + root.attr_suffix] })
                root.to = Qt.binding(function() { return attr_float_scrub_max["red_"   + root.attr_suffix] })
            }
        }
    }

    function clamp(number, min, max) {
        return Math.max(min, Math.min(number, max))
    }

    function clamp_v4d(v) {
        return Qt.vector4d(
            clamp(v.x, 0.0, 1.0),
            clamp(v.y, 0.0, 1.0),
            clamp(v.z, 0.0, 1.0),
            clamp(v.w, 0.0, 1.0)
        )
    }

    function v4d_to_color(v) {
        return Qt.rgba(v.x, v.y, v.z, v.w)
    }

    function color_to_v4d(c) {
        return Qt.vector4d(c.r, c.g, c.b, c.a)
    }

    // Note this is a naive log scale, in case the min and max are not
    // mirrored around mid, the derivate will not be continous at the
    // mid point.

    // Colour wheels only support adding / scaling up values.

    function pos_to_val(v) {
        var min = root.default_value
        var max = root.to
        var steepness = 4

        function lin_to_log(v) {
            var log = Math.log
            var antilog = Math.exp
            return (antilog(v * steepness) - antilog(0.0)) / (antilog(1.0 * steepness) - antilog(0.0))
        }

        return lin_to_log(v) * (max - min) + min
    }

    function val_to_pos(v) {
        var min = root.default_value
        var max = root.to
        var steepness = 4

        function log_to_lin(v) {
            var log = Math.log
            var antilog = Math.exp
            return log(v * (antilog(1.0 * steepness) - antilog(0.0)) + antilog(0.0)) / steepness
        }

        if (v < min)
            v = min
        else if (v > max)
            v = max

        return log_to_lin((v - min) / (max - min))
    }

    function pos_to_val_color(color) {
        return Qt.vector4d(
            pos_to_val(color.x),
            pos_to_val(color.y),
            pos_to_val(color.z),
            1.0
        );
    }

    function val_to_pos_color(color) {
        return Qt.vector4d(
            val_to_pos(color.x),
            val_to_pos(color.y),
            val_to_pos(color.z),
            1.0
        );
    }

    function rgb_to_hsv(color) {

        var h, s, v = 0.0
        var r = color.x
        var g = color.y
        var b = color.z

        var max = Math.max(r, g, b)
        var min = Math.min(r, g, b)
        var delta = max - min

        v = max
        s = max === 0 ? 0 : delta / max

        if (max === min) {
            h = 0
        } else if (r === max) {
            h = (g - b) / delta
        } else if (g === max) {
            h = 2 + (b - r) / delta
        } else if (b === max) {
            h = 4 + (r - g) / delta
        }

        h = h < 0 ? h + 6 : h
        h /= 6

        // Handle extended range inputs (from OpenColorIO RGB_TO_HSV builtin)
        if (min < 0) {
            v += min
        }
        if (-min > max) {
            s = delta / -min
        }

        return Qt.vector3d(h, s, v)
    }

    function hsv_to_rgb(color) {

        var MAX_SAT = 1.999

        var r, g, b = 0.0
        var h = color.x
        var s = color.y
        var v = color.z

        h = ( h - Math.floor( h ) ) * 6.0
        s = clamp( s, 0.0, MAX_SAT )
        v = v

        r = clamp( Math.abs(h - 3.0) - 1.0, 0.0, 1.0 )
        g = clamp( 2.0 - Math.abs(h - 2.0), 0.0, 1.0 )
        b = clamp( 2.0 - Math.abs(h - 4.0), 0.0, 1.0 )

        var max = v
        var min = v * (1.0 - s)

        // Handle extended range inputs (from OpenColorIO HSV_TO_RGB builtin)
        if (s > 1.0)
        {
            min = v * (1.0 - s) / (2.0 - s)
            max = v - min
        }
        if (v < 0.0)
        {
            min = v / (2.0 - s)
            max = v - min
        }

        var delta = max - min
        r = r * delta + min
        g = g * delta + min
        b = b * delta + min

        return Qt.vector3d(r, g, b)
    }

    function rgb_to_pos(color) {

        var hsv = rgb_to_hsv(color)
        hsv = Qt.vector3d(hsv.x, hsv.z, hsv.y)

        var angle = (1 - hsv.x) * (2 * Math.PI)
        var dist = Math.abs(hsv.y)
        return Qt.vector2d(
            Math.sin(angle) * dist,
            Math.cos(angle) * dist
        )
    }

    Column {
        anchors.topMargin: 5
        anchors.fill: parent
        spacing: 10

        Row {
            id: titleRow
            spacing: 10
            anchors.horizontalCenter: parent.horizontalCenter
            height: 30

            Text {
                text: root.title
                font.pixelSize: 20
                color: "white"
            }

            XsButton {
                id: reloadButton
                width: 20; height: 20
                bgColorNormal: "transparent"
                borderWidth: 0

                onClicked: {
                    attr["red_"    + root.attr_suffix] = attr_default_value["red_"    + root.attr_suffix]
                    attr["green_"  + root.attr_suffix] = attr_default_value["green_"  + root.attr_suffix]
                    attr["blue_"   + root.attr_suffix] = attr_default_value["blue_"   + root.attr_suffix]
                    attr["master_" + root.attr_suffix] = attr_default_value["master_" + root.attr_suffix]
                }

                Image {
                    source: "qrc:/feather_icons/rotate-ccw.svg"

                    layer {
                        enabled: true
                        effect: ColorOverlay {
                            color: reloadButton.down || reloadButton.hovered ? "white" : XsStyle.controlTitleColor
                        }
                    }
                }
            }
        }

        Control {
            id: wheel
            anchors.horizontalCenter: parent.horizontalCenter

            property int radius: root.size / 2
            property int center: root.size / 2
            property real ring_rel_size: 0.1
            property real cursor_width: 17

            property real value: 1.0
            property real saturation: 1.0
            property vector4d color: Qt.vector4d(1.0, 1.0, 1.0, 1.0)

            onColorChanged: {

                if (carea.pressed) {
                    var color_out = pos_to_val_color(color)
                    attr["red_"   + root.attr_suffix] = color_out.x
                    attr["green_" + root.attr_suffix] = color_out.y
                    attr["blue_"  + root.attr_suffix] = color_out.z
                } else {
                    var pos = rgb_to_pos(color)
                    cdrag.x = center + pos.x * radius
                    cdrag.y = center - pos.y * radius
                }
            }

            contentItem: Item {
                implicitWidth: root.size
                implicitHeight: width

                ShaderEffect {
                    id: shadereffect
                    width: parent.width
                    height: parent.height

                    readonly property real radius: 0.5
                    readonly property real ring_radius: radius - radius * wheel.ring_rel_size
                    readonly property real saturation: wheel.saturation
                    readonly property real value: wheel.value

                    fragmentShader: "
                        #version 330

                        #define M_PI 3.1415926535897932384626433832795
                        #define M_PI_2 (2.0 * M_PI)

                        varying highp vec2 qt_TexCoord0;

                        uniform highp float qt_Opacity;
                        uniform highp float radius;
                        uniform highp float ring_radius;
                        uniform highp float saturation;
                        uniform highp float value;

                        vec3 hsv_to_rgb(vec3 c) {
                            vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
                            vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
                            return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
                        }

                        void main() {
                            highp vec2 coord = qt_TexCoord0 - vec2(0.5);
                            highp float r = length(coord);
                            highp float h = atan(coord.x, coord.y);
                            highp float s = r <= ring_radius ? saturation * 0.5 : saturation;
                            highp float v = r <= ring_radius ? value * 0.35 : value;

                            if (r <= radius) {
                                vec3 rgb = hsv_to_rgb( vec3(h / M_PI_2 + 0.5, s, v) );
                                gl_FragColor = vec4(rgb, 1.0);
                            } else {
                                gl_FragColor = vec4(0.0);
                            }
                        }
                    "
                }

                // Cross in the center
                Rectangle {
                    color: "grey"
                    width: wheel.width - wheel.ring_rel_size * wheel.width
                    height: 1
                    x: wheel.ring_rel_size * wheel.width / 2
                    y: wheel.center
                }
                Rectangle {
                    color: "grey"
                    width: 1
                    height: wheel.height - wheel.ring_rel_size * wheel.height
                    x: wheel.center
                    y: wheel.ring_rel_size * wheel.height / 2
                }

                // Cursor
                Rectangle {
                    id: cursor

                    width: wheel.cursor_width
                    height: width
                    radius: width/2

                    x: (cdrag.radius <= wheel.radius ? cdrag.x : wheel.center + (cdrag.x - wheel.center) * (wheel.radius / cdrag.radius)) - (width / 2)
                    y: (cdrag.radius <= wheel.radius ? cdrag.y : wheel.center + (cdrag.y - wheel.center) * (wheel.radius / cdrag.radius)) - (height / 2)

                    color: Qt.darker(cursor_color(wheel.color), 1.25)
                    border.color: Qt.darker(color)
                    border.width: 0.75

                    function cursor_color(color) {
                        var rgb_norm = clamp_v4d(color)
                        var hsv = rgb_to_hsv(Qt.vector3d(rgb_norm.x, rgb_norm.y, rgb_norm.z))
                        var rgb = hsv_to_rgb(Qt.vector3d(hsv.x, hsv.z, 1.0))
                        return Qt.rgba(rgb.x, rgb.y, rgb.z, 1.0)
                    }

                    MouseArea {
                        id: carea
                        anchors.fill: parent
                        drag.threshold: 0
                        drag.target: Item {
                            id: cdrag

                            readonly property real radius: Math.hypot(x - wheel.center, y - wheel.center)

                            x: wheel.center
                            y: wheel.center
                        }

                        onPositionChanged: {

                            var cursor_pos = Qt.vector2d(cursor.x, cursor.y)
                            var offset = Qt.vector2d(cursor.width / 2, cursor.height / 2)
                            var pos = cursor_pos.plus(offset)

                            // Hue angle normalised [0,1]
                            var hue = Math.atan2(
                                pos.x - wheel.center,
                                pos.y - wheel.center)
                            hue = hue / (2 * Math.PI) + 0.5
                            // Distance from center normalised [0,1]
                            var dist = Math.hypot(
                                pos.x - wheel.center,
                                pos.y - wheel.center)
                            dist /= wheel.radius

                            var hsv = Qt.vector3d(hue, 1.0, dist)
                            var rgb = hsv_to_rgb(hsv)
                            wheel.color = Qt.vector4d(rgb.x, rgb.y, rgb.z, 1.0)
                        }
                    }
                }
            }
        }

        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            leftPadding: 20

            Column {
                id: wheelInputCol

                XsTextField {
                    id: redInput
                    width: 60
                    bgColorNormal: "transparent"
                    borderColor: bgColorNormal
                    text: root.value ? root.value.x.toFixed(5) : ""
                    validator: DoubleValidator {}

                    onEditingFinished: {
                        attr["red_" + root.attr_suffix] = parseFloat(text)
                    }
                }
                XsTextField {
                    id: greenInput
                    width: 60
                    bgColorNormal: "transparent"
                    borderColor: bgColorNormal
                    text: root.value ? root.value.y.toFixed(5) : ""
                    validator: DoubleValidator {}

                    onEditingFinished: {
                        attr["green_" + root.attr_suffix] = parseFloat(text)
                    }
                }
                XsTextField {
                    id: blueInput
                    width: 60
                    bgColorNormal: "transparent"
                    borderColor: bgColorNormal
                    text: root.value ? root.value.z.toFixed(5) : ""
                    validator: DoubleValidator {}

                    onEditingFinished: {
                        attr["blue_" + root.attr_suffix] = parseFloat(text)
                    }
                }
            }

            Column {
                anchors.verticalCenter: parent.verticalCenter
                leftPadding: 5

                Label {
                    visible: root.title != "Sat"
                    text: root.title == "Offset" ? "+" : "x"
                }
            }

            Column {
                anchors.verticalCenter: parent.verticalCenter

                XsTextField {
                    id: masterInput
                    width: 60
                    bgColorNormal: "transparent"
                    borderColor: bgColorNormal
                    text: root.value ? root.value.w.toFixed(5) : ""
                    validator: DoubleValidator {}

                    onEditingFinished: {
                        attr["master_" + root.attr_suffix] = parseFloat(text)
                    }
                }
            }
        }
    }
}