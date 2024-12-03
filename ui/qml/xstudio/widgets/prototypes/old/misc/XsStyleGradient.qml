// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12

import xStudio 1.0

Item {

	property alias accent_gradient: accent_gradient

    property string highlightColorString: XsStyle.highlightColor+""
    onHighlightColorStringChanged:{
        assignAccentGradient()
    }

    Gradient {
        id:accent_gradient
        orientation: Gradient.Horizontal
        GradientStop { position: 0.0; color: XsStyle.highlightColor }
    }
    Component
    {
        id: gradiantStopComponent
        GradientStop { }
    }

    function assignAccentGradient()
    {
        var value = highlightColorString
        assignColorStringToGradient(value, accent_gradient)

        // if (value.indexOf(', ') === -1) XsStyle.highlightColor = value// solid color
        // else {
        //     XsStyle.highlightColor = "#666666"// gradient color
        // }
        // if (value.indexOf(', ') !== -1) XsStyle.highlightColor = "#666666"// gradient color

        if (value.indexOf(',') === -1) {
            // solid color
            XsStyle.highlightColor = value
        } else {
            // gradient color
            XsStyle.highlightColor = "#666666"
        }
    }

    function assignColorStringToGradient(colorstring, gradobj)
    {
        var spl
        if (colorstring.indexOf(',') === -1) spl = [colorstring] // solid color
        else {
            spl = colorstring.split(',') // gradient color
        }
        var stops = []
        var interval = 1.0
        if (spl.length > 1)
        {
            interval = 1.00 / (spl.length - 1)
        }
        var currStop = 0.0
        var stop
        for (var index in spl)
        {
            // console.debug("index: "+index+"-"+spl[index])
            stop = gradiantStopComponent.createObject(appWindow, {"position": currStop, "color": spl[index]});
            stops.push(stop)
            currStop = currStop + interval
        }
        gradobj.stops = stops
    }
}
