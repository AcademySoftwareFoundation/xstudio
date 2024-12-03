// SPDX-License-Identifier: Apache-2.0

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import QtQml 2.15
import QtQml.Models 2.14
import xStudio 1.0

Control { id: wheel

    property real wheelSize: 0 //135
    onWheelSizeChanged:{
        update_frontend()
    }
    property int radius: wheelSize / 2
    onRadiusChanged:{
        update_frontend()
    }
    property int center: wheelSize / 2
    onCenterChanged:{
        update_frontend()
    }
    property real ring_rel_size: 0.1
    property real cursor_width: 17 //wheelSize/8 > 17 ? 17 : wheelSize/8 
    property bool interacting: cArea.pressed || mArea.pressed

    property real local_value: 1.0
    property real saturation: 1.0
    readonly property vector4d default_color: Qt.vector4d(1.0, 1.0, 1.0, 1.0)

    // "Front end" color value
    property vector4d color: default_color
    property var backend_color: value

    onBackend_colorChanged: {
        if (!interacting) {
            update_frontend()
        }
    }

    function update_frontend() {
        color = val_to_pos_color(backend_color)
        var pos = rgb_to_pos(color)
        cDrag.x = center + pos.x * radius
        cDrag.y = center - pos.y * radius
    }

    function update_backend() {
        var color_out = pos_to_val_color(color)
        var v = value
        v[0] = color_out.x
        v[1] = color_out.y
        v[2] = color_out.z
        value = v
    }

    function update_backend_from_cursor() {
        var cursor_pos = Qt.vector2d(cursor.x, cursor.y)
        var offset = Qt.vector2d(cursor.width / 2, cursor.height / 2)
        var pos = cursor_pos.plus(offset)

        // Hue angle normalised [0,1]
        var hue = Math.atan2(
            pos.x - center,
            pos.y - center)
        hue = hue / (2 * Math.PI) + 0.5
        // Distance from center normalised [0,1]
        var dist = Math.hypot(
            pos.x - center,
            pos.y - center)
        dist /= radius

        var hsv = Qt.vector3d(hue, 1.0, dist)
        var rgb = hsv_to_rgb(hsv)
        color = Qt.vector4d(rgb.x, rgb.y, rgb.z, 1.0)

        update_backend()
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

    function pos_to_val(v, idx) {
        var min = default_value[idx]
        var max = float_scrub_max[idx]
        var steepness = 4

        function lin_to_log(v) {
            var log = Math.log
            var antilog = Math.exp
            return (antilog(v * steepness) - antilog(0.0)) / (antilog(1.0 * steepness) - antilog(0.0))
        }

        return lin_to_log(v) * (max - min) + min
    }

    function val_to_pos(v, idx) {
        var min = default_value[idx]
        var max = float_scrub_max[idx]
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
            pos_to_val(color.x, 0),
            pos_to_val(color.y, 1),
            pos_to_val(color.z, 2),
            1.0
        );
    }

    function val_to_pos_color(color) {
        return Qt.vector4d(
            val_to_pos(color[0],0),
            val_to_pos(color[1],1),
            val_to_pos(color[2],2),
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

    contentItem: Item {
        implicitWidth: wheelSize
        implicitHeight: implicitWidth

        ShaderEffect {
            id: shadereffect
            width: parent.width
            height: parent.height

            readonly property real radius: 0.5
            readonly property real ring_radius: radius - radius * wheel.ring_rel_size
            readonly property real saturation: wheel.saturation
            readonly property real local_value: wheel.local_value

            fragmentShader: "
                #version 330

                #define M_PI 3.1415926535897932384626433832795
                #define M_PI_2 (2.0 * M_PI)

                varying highp vec2 qt_TexCoord0;

                uniform highp float qt_Opacity;
                uniform highp float radius;
                uniform highp float ring_radius;
                uniform highp float saturation;
                uniform highp float local_value;

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
                    highp float v = r <= ring_radius ? local_value * 0.35 : local_value;

                    if (r <= radius) {
                        vec3 rgb = hsv_to_rgb( vec3((h + M_PI) / M_PI_2, s, v) );
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

        MouseArea {
            id: mArea
            anchors.fill: parent
            propagateComposedEvents: true

            onPressed: {
                cDrag.x = mouse.x
                cDrag.y = mouse.y
                update_backend_from_cursor()
            }
        }

        // Cursor
        Rectangle {
            id: cursor

            width: wheel.cursor_width
            height: width
            radius: width/2

            x: (cDrag.radius <= wheel.radius ? cDrag.x : wheel.center + (cDrag.x - wheel.center) * (wheel.radius / cDrag.radius)) - (width / 2)
            y: (cDrag.radius <= wheel.radius ? cDrag.y : wheel.center + (cDrag.y - wheel.center) * (wheel.radius / cDrag.radius)) - (height / 2)

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
                id: cArea
                anchors.fill: parent
                propagateComposedEvents: true

                drag.filterChildren: true
                drag.threshold: 0
                drag.target: Item {
                    id: cDrag
                    readonly property real radius: Math.hypot(x - wheel.center, y - wheel.center)

                    x: wheel.center
                    y: wheel.center
                }

                onDoubleClicked: {
                    cDrag.x = wheel.center
                    cDrag.y = wheel.center
                    update_backend_from_cursor()
                }

                onPositionChanged: {
                    update_backend_from_cursor()
                }
            }
        }

    }
}