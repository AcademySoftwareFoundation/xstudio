// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQml 2.14

import xStudio 1.0

XsMenu {
    id: flagMenu
    hasCheckable: true
    property color flag: "#00000000"
    property string flagHex: "#00000000"
    property string flagText: "Clear"
    property bool showChecked: false
    property var colours_model: default_model

    signal flagSet(string hex, string text)

    function colour2Hex(c) {
        var r = (c.r*255).toString(16).toUpperCase()
        var g = (c.g*255).toString(16).toUpperCase()
        var b = (c.b*255).toString(16).toUpperCase()
        var a = (c.a*255).toString(16).toUpperCase()

        if (r.length == 1)
            r = "0" + r
        if (g.length == 1)
            g = "0" + g
        if (b.length == 1)
            b = "0" + b
        if (a.length == 1)
            a = "0" + a

        return "#" + a + r + g + b
    }

    function hexToRGBA(hex, alpha) {
        var r = parseInt(hex.slice(3, 5), 16),
            g = parseInt(hex.slice(5, 7), 16),
            b = parseInt(hex.slice(7, 9), 16);

        return Qt.rgba(r/255.0,g/255.0,b/255.0, alpha)
    }

    ListModel {
        id: default_model

        ListElement {
            name: "Remove Flag"
            colour: "#00000000"
        }
        ListElement {
            name: "Red"
            colour: "#FFFF0000"
        }
        ListElement {
            name: "Green"
            colour: "#FF00FF00"
        }
        ListElement {
            name: "Blue"
            colour: "#FF0000FF"
        }
        ListElement {
            name: "Yellow"
            colour: "#FFFFFF00"
        }
        ListElement {
            name: "Orange"
            colour: "#FFFFA500"
        }
        ListElement {
            name: "Purple"
            colour: "#FF800080"
        }
        ListElement {
            name: "Black"
            colour: "#FF000000"
        }
        ListElement {
            name: "White"
            colour: "#FFFFFFFF"
        }
    }

    function elementFromName(model, name) {
        if(model.count !== undefined) {
            for (var i = 0; i < model.count; i++) {
                if(model.get(i).name === name)
                    return model.get(i)
            }
        } else {
            for (var i = 0; i < model.length; i++) {
                if(model[i].name === name)
                    return model[i]
            }
        }
        return null
    }

    function elementFromColour(model, colour) {
        if(model.count !== undefined) {
            for (var i = 0; i < model.count; i++) {
                if(model.get(i).colour === colour)
                    return model.get(i)
            }
            return model.get(model.count-1)
        }
        for (var i = 0; i < model.length; i++) {
            if(model[i].colour === colour)
                return model[i]
        }
        return model[model.length-1]
    }

    ActionGroup{
        id: actgrp
        onTriggered: {
            flag = elementFromName(colours_model, action.text).colour
            flagHex = colour2Hex(flag)
            flagText = elementFromName(colours_model,action.text).name

            flagMenu.flagSet(flagHex, flagText)
            if(!showChecked) {
                actgrp.checkedAction = null
                for (var i = 0; i < actgrp.actions.length; i++) {
                    actgrp.actions[i].checked = false
                }
            }
        }
    }

    Connections {
        target: flagMenu
        function onFlagChanged() {
            var check = null
            var txt = elementFromColour(colours_model, flagMenu.flagHex).name
            for (var i = 0; i < actgrp.actions.length; i++) {
                if(actgrp.actions[i].text === txt) {
                    if(showChecked) {
                        check = actgrp.actions[i]
                    } else {
                        actgrp.actions[i].checked = false
                    }

                    break
                }
            }
            if(check && showChecked) {
                actgrp.checkedAction = check
            }
        }
    }

    title: qsTr("Flag Selected Items")

    Repeater {
        model: colours_model
        XsMenuItem {
            mytext: name
            iconbg: hexToRGBA(colour, 1.0)
            mycheckable: mytext != "Remove Flag"
            actiongroup: actgrp
        }
    }
}
