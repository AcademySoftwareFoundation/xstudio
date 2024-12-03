import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQml.Models 2.14
import Qt.labs.qmlmodels 1.0

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

    ListView {

        id: view
        orientation: ListView.Vertical
        spacing: 0
        width: contentWidth
        height: maxMenuHeight(contentHeight)
        contentHeight: contentItem.childrenRect.height
        contentWidth: minWidth
        snapMode: ListView.SnapToItem
        // currentIndex: -1
        property real minWidth: 20
        clip: true

        ScrollBar.vertical: ScrollBar {
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
        }

        // awkward solution to make all items in the list view the
        // same width ... the width of the widest item in the view!
        function setMinWidth(mw) {
            if (mw > minWidth) {
                minWidth = mw
            }
        }

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
                width: view.width
                onMinWidthChanged: {
                    view.setMinWidth(minWidth)
                }
                enabled: is_enabled ? _choices_enabled.length > index ? _choices_enabled[index] : true : false

                property var name: _choices[index]

            }

        }

    }
}


