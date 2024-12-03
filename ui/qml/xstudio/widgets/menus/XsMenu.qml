import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQml.Models 2.14
import Qt.labs.qmlmodels 1.0
import QtQuick.Layouts 1.15

import xStudio 1.0
import xstudio.qml.models 1.0

XsPopup {

    id: the_popup
    // x: 30
    height: view.height+ (topPadding+bottomPadding)
    width: view.width
    objectName: "XsPopup"

    onHeightChanged: {
        if (visible) {
            var yshift = appWindow.checkMenuYPosition(
                the_popup.parent,
                height,
                x,
                y);
            y = y - yshift
        }
    }

    property var menu_model
    property var menu_model_index

    property alias menuWidth: view.width

    property var menuContextData

    property var parent_menu

    property var panelContext: helpers.contextPanel(the_popup)
    property var panelContextAddress: helpers.contextPanelAddress(the_popup)

    property var hotkey_sequence: undefined

    function closeAll() {
        visible = false
        if (parent_menu) parent_menu.closeAll()
    }

    function hideOtherSubMenus(widget) {

        for (var i = 0; i < view.count; ++i) {
            let item = view.itemAtIndex(i)
            if (item != null && item != widget && typeof item.hideSubMenus != "undefined") {
                item.hideSubMenus()
            }
        }
    }

    onVisibleChanged: {
        if (typeof watch_visibility !== "undefined" && typeof menu_model.menuItemVisibilityChanged !== "undefined") {
            menu_model.menuItemVisibilityChanged(menu_model_index, visible, helpers.contextPanel(the_popup))
        }

        if (typeof parent_menu !== "undefined" && typeof parent_menu.submenu_visibility_changed == "function") {
            parent_menu.submenu_visibility_changed(visible)
        }

    }

    ListView {

        id: view
        orientation: ListView.Vertical
        spacing: 0
        width: minWidth
        height: maxMenuHeight(contentHeight)
        contentHeight: contentItem.childrenRect.height
        contentWidth: minWidth
        snapMode: ListView.SnapToItem
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

        model: DelegateModel {

            // setting up the model and rootIndex like this causes us to
            // iterate over the children of the node in the 'menu_model' that
            // is found at 'menu_model_index'. Note that the delegate will
            // be assigned a value 'index' which is its index in the list of
            // children.
            model: the_popup.menu_model
            rootIndex: the_popup.menu_model_index
            delegate: chooser

            DelegateChooser {
                id: chooser
                role: "menu_item_type"

                DelegateChoice {
                    roleValue: "button"

                    XsMenuItemNew {
                        // again, we pass in the model to the menu item and
                        // step one level deeper into the tree by useing row=index
                        menu_model: the_popup.menu_model
                        menu_model_index: the_popup.menu_model.index(
                            index, // row = child index
                            0, // column = 0 (always, we don't use columns)
                            the_popup.menu_model_index // the parent index into the model
                        )
                        parent_menu: the_popup
                        width: view.width
                        indent: view.indent
                        enabled: menu_item_enabled
                        onMinWidthChanged: {
                            view.setMinWidth(minWidth)
                        }
                        onLeftIconSizeChanged: {
                            view.setIndent(leftIconSize)
                        }
                    }

                }

                DelegateChoice {
                    roleValue: "menu"

                    XsMenuItemNew {
                        // again, we pass in the model to the menu item and
                        // step one level deeper into the tree by useing row=index
                        menu_model: the_popup.menu_model
                        menu_model_index: the_popup.menu_model.index(
                            index, // row = child index
                            0, // column = 0 (always, we don't use columns)
                            the_popup.menu_model_index // the parent index into the model
                        )
                        parent_menu: the_popup
                        width: view.width
                        indent: view.indent
                        onMinWidthChanged: {
                            view.setMinWidth(minWidth)
                        }
                        onLeftIconSizeChanged: {
                            view.setIndent(leftIconSize)
                        }
                    }
                }

                DelegateChoice {
                    roleValue: "divider"
                    XsMenuDivider {
                        width: view.width
                        onMinWidthChanged: {
                            view.setMinWidth(minWidth)
                        }
                    }

                }

                DelegateChoice {
                    roleValue: "multichoice"

                    XsMenuItemNew {
                        menu_model: the_popup.menu_model
                        menu_model_index: the_popup.menu_model.index(index, 0, the_popup.menu_model_index)

                        parent_menu: the_popup
                        width: view.width
                        indent: view.indent
                        onMinWidthChanged: {
                            view.setMinWidth(minWidth)
                        }
                        onLeftIconSizeChanged: {
                            view.setIndent(leftIconSize)
                        }

                    }

                }

                DelegateChoice {

                    roleValue: "radiogroup"

                    ColumnLayout {

                        width: view.width
                        spacing: 0
                        id: layout
                        property var idx: index
                        Repeater {

                            // we can optionally drive the menu selection with
                            // 'choice_ids' - this allows us to handle the case
                            // where there are duplicate names in 'choices' but
                            // they really mean different
                            model: choices_ids ? choices_ids : choices
                            XsMenuItemToggle {
                                actualValue: choices_ids ? choices_ids[index] : choices[index]
                                label: choices[index]
                                isRadioButton: true
                                radioSelectedChoice: current_choice
                                menu_model: the_popup.menu_model
                                menu_model_index: the_popup.menu_model.index(layout.idx, 0, the_popup.menu_model_index)
                                parent_menu: the_popup
                                onClicked: {
                                    current_choice = actualValue
                                    the_popup.closeAll()
                                }
                                width: view.width
                                onMinWidthChanged: {
                                    view.setMinWidth(minWidth)
                                }
                                onLeftIconSizeChanged: {
                                    view.setIndent(leftIconSize)
                                }

                            }
                        }
                    }

                }

                DelegateChoice {
                    roleValue: "toggle"

                    XsMenuItemToggle {
                        menu_model: the_popup.menu_model
                        menu_model_index: the_popup.menu_model.index(index, 0, the_popup.menu_model_index)
                        parent_menu: the_popup

                        onClicked: {
                            menu_model.nodeActivated(menu_model_index, "menu", helpers.contextPanel(the_popup))
                            the_popup.closeAll()
                        }

                        width: view.width
                        onMinWidthChanged: {
                            view.setMinWidth(minWidth)
                        }
                        onLeftIconSizeChanged: {
                            view.setIndent(leftIconSize)
                        }

                    }

                }

                DelegateChoice {
                    roleValue: "toggle_settings"

                    XsMenuItemToggleWithSettings {
                        menu_model: the_popup.menu_model
                        menu_model_index: the_popup.menu_model.index(index, 0, the_popup.menu_model_index)

                        parent_menu: the_popup
                        width: view.width
                        isChecked: is_checked
                        onClicked:{
                            is_checked = !is_checked
                            the_popup.closeAll()
                        }
                        onMinWidthChanged: {
                            view.setMinWidth(minWidth)
                        }
                        onLeftIconSizeChanged: {
                            view.setIndent(leftIconSize)
                        }

                    }

                }

                DelegateChoice {
                    roleValue: "toggle_checkbox"

                    XsMenuItemToggle {
                        menu_model: the_popup.menu_model
                        menu_model_index: the_popup.menu_model.index(index, 0, the_popup.menu_model_index)

                        isRadioButton: true
                        parent_menu: the_popup

                        onClicked: {
                            // note 'is_checked' data in this context is provided
                            // by the menu_model
                            is_checked = !is_checked
                        }
                        onMinWidthChanged: {
                            view.setMinWidth(minWidth)
                        }
                        onLeftIconSizeChanged: {
                            view.setIndent(leftIconSize)
                        }

                    }

                }

                DelegateChoice {
                    roleValue: "custom"

                    XsMenuItemCustom {
                        menu_model: the_popup.menu_model
                        menu_model_index: the_popup.menu_model.index(index, 0, the_popup.menu_model_index)
                        parent_menu: the_popup
                        width: view.width
                        onMinWidthChanged: {
                            view.setMinWidth(minWidth)
                        }
                        onLeftIconSizeChanged: {
                            view.setIndent(leftIconSize)
                        }

                    }

                }

            }
        }

    }
}


