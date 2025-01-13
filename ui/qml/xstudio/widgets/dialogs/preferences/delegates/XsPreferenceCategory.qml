// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.15
import QtQml.Models 2.15
import QtQuick.Layouts 1.3
import Qt.labs.qmlmodels 1.0

import xstudio.qml.helpers 1.0
import xstudio.qml.models 1.0
import xstudio.qml.global_store_model 1.0
import xStudio 1.0

import "."

XsListView {

    property var category
    property var prefsLabelWidth: 200
    property var theIndex

    spacing: 8
    model: delegateModel

    DelegateModel {
        id: delegateModel
        model: preferencesModel
        delegate: chooser
        rootIndex: theIndex
    }

    DelegateChooser {
        id: chooser
        role: "datatypeRole"

        DelegateChoice {
            roleValue: "int"
            XsIntegerPreference {
            }
        }
        DelegateChoice {
            roleValue: "string"
            XsStringPreference {
            }
        }
        DelegateChoice {
            roleValue: "bool"
            XsTogglePreference {
            }
        }
        DelegateChoice {
            roleValue: "double"
            XsFloatPreference {
            }
        }
        DelegateChoice {
            roleValue: "multichoice string"
            XsStringMultichoicePreference {
            }
        }
        DelegateChoice {
            roleValue: "colour"
            XsColourPreference {
            }
        }
        DelegateChoice {
            roleValue: "json"
            Item {

                id: parent_item
                width: parent.width
                height: XsStyleSheet.widgetStdHeight            
                property var dynamic_widget
                property var qml_code_: optionsRole
                onQml_code_Changed: {
                    dynamic_widget = Qt.createQmlObject(qml_code_, parent_item)
                }
            }
        }

    }
}

