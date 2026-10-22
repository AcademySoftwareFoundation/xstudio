import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

import xStudio 1.0
import xstudio.qml.models 1.0

XsPopup {

    // Note that the model gives use a string 'current_choice', plus
    // a list of strings 'choices'

    id: the_popup
    height: view.height+ (topPadding+bottomPadding)
    width: view.width

    property var menu_model
    property var menu_model_index
    property var parent_menu

    function closeAll() {
        visible = false
        if (parent_menu) parent_menu.closeAll()
    }

    function hideSubMenus() {}

    // N.B. the XsMenuModel exposes role data called 'choices' which lists the
    // items in the multi choice. The active option is exposed via 'current_choice'
    // role data
    // Multi-choice menus can also be made via the XsModuleData model. This
    // exposes a role data 'combo_box_options' fir the items in the multi-choice.
    // The active option is exposed via 'value' role data
    property var _choices: typeof choices !== "undefined" ? choices : typeof combo_box_options !== "undefined" ? combo_box_options : []
    property var _currentChoice: typeof current_choice !== "undefined" ? current_choice : typeof value !== "undefined" ? value : ""
    property var _choices_enabled: typeof combo_box_options_enabled !== "undefined" ? combo_box_options_enabled : []

    property real minWidth: 20

    // awkward solution to make all items in the list view the
    // same width ... the width of the widest item in the view!
    function setMinWidth(mw) {
        if (mw > minWidth) {
            minWidth = mw
        }
    }

    Flickable {

        id: view
        width: minWidth
        height: maxMenuHeight(layout.height)
        contentHeight: layout.height
        contentWidth: minWidth
        clip: true

        ScrollBar.vertical: XsScrollBar {
            parent: view.parent
            id: scrollBar;
            width: 10
            anchors.top: view.top
            anchors.bottom: view.bottom
            anchors.right: view.left
            visible: view.height < view.contentHeight
            policy: ScrollBar.AlwaysOn
            palette.mid: XsStyleSheet.scrollbarBaseColor
            palette.dark: XsStyleSheet.scrollbarActiveColor
            animatedGlow: visible
        }

        property real minWidth: 20
        property real indent: 0

        // awkward solution to make all items in the list view the
        // same width ... the width of the widest item in the view!
        function setMinWidth(mw) {
            if (mw > minWidth) {
                minWidth = mw
            }
        }

        // awkward solution to make all items indent to the maximum indent
        function setIndent(i) {
            if (i > indent) {
                indent = i
            }
        }

        property alias repeater: repeater

        ColumnLayout {

            id: layout
            spacing: 0
            Repeater {

                id: repeater
                model: DelegateModel {

                    model: _choices

                    delegate: XsMenuItemToggle{

                        // isRadioButton: true
                        // radioSelectedChoice: current_choice
                        // label: choices[index]

                        // onChecked: {
                        //     label= choices[index]
                        //     current_choice = label
                        //     radioSelectedChoice = current_choice
                        // }

                        isRadioButton: true
                        actualValue: _choices[index]
                        radioSelectedChoice: _currentChoice
                        onClicked:{
                            if (typeof current_choice!== "undefined") {
                                current_choice = name
                            } else if (value) {
                                value = name
                            }
                            the_popup.closeAll()
                        }

                        parent_menu: the_popup
                        width: view.minWidth
                        onMinWidthChanged: {
                            view.setMinWidth(minWidth)
                        }
                        enabled: (is_enabled && index >= 0) ?_choices_enabled.length > index ? _choices_enabled[index] : true : false

                        property var name: _choices[index]
                    }
                }

            }

        }

    }
}


