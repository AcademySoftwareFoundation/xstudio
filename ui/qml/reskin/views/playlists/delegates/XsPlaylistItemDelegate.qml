// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtGraphicalEffects 1.15
import QtQuick.Layouts 1.15
import QtQml.Models 2.14
import Qt.labs.qmlmodels 1.0

import xStudioReskin 1.0

Item {
    id: contentDiv
    width: parent.width;
    height: itemRowStdHeight + (isExpanded ? subItemsCount*itemRowStdHeight : 0)
    opacity: enabled ? 1.0 : 0.33

    property real itemRealHeight: XsStyleSheet.widgetStdHeight -2
    property real itemPadding: XsStyleSheet.panelPadding/2
    property real buttonWidth: XsStyleSheet.secondaryButtonStdWidth

    property color bgColorPressed: XsStyleSheet.widgetBgNormalColor
    property color bgColorNormal: "transparent"
    property color forcedBgColorNormal: bgColorNormal

    property color hintColor: XsStyleSheet.hintColor
    property color errorColor: XsStyleSheet.errorColor

    /* modelIndex should be set to point into the session data model and get
    to the playlist that we are representing */
    property var modelIndex

    /* first index in playlist is media ... */
    property var itemCount: mediaCountRole

    /* .... the third row gives us the data of the subsets/timelines etc. i.e.
    the children lists of the playlist */
    property var subItemsModelIndex: modelIndex && modelIndex.valid ? theSessionData.index(2, 0, modelIndex) : undefined
    property var subItemsCount: subItemsModel.count

    property bool isSelected: false
    property bool isMissing: false
    property bool isExpanded: false

    // Rectangle{anchors.fill: parent; color:(index%2==0)?"transparent":"yellow"; opacity:0.3}

    Button { id: visibleItemDiv

        width: parent.width
        height: itemRealHeight

        text: isMissing? "This playlist no longer exists" : nameRole
        font.pixelSize: textSize
        font.family: textFont
        hoverEnabled: true

        contentItem:
        Item{
            anchors.fill: parent

            RowLayout {

                x: spacing
                spacing: itemPadding
                width: parent.width -x -spacing -(spacing*2) //for scrollbar
                height: XsStyleSheet.widgetStdHeight
                anchors.verticalCenter: parent.verticalCenter

                Item{
                    Layout.preferredWidth: buttonWidth
                    Layout.preferredHeight: buttonWidth

                    XsSecondaryButton{

                        id: subsetBtn

                        imgSrc: "qrc:/icons/chevron_right.svg"
                        visible: subItemsCount != 0
                        anchors.fill: parent

                        isActive: isExpanded
                        scale: rotation==0 || rotation==-90? 1:0.85
                        rotation: (isExpanded)? 90:0
                        Behavior on rotation {NumberAnimation{duration: 150 }}

                        onClicked:{
                            isExpanded = !isExpanded
                        }

                    }
                }
                XsImage{
                    Layout.minimumWidth: buttonWidth
                    Layout.maximumWidth: buttonWidth
                    Layout.minimumHeight: buttonWidth
                    Layout.maximumHeight: buttonWidth
                    source: isMissing? "qrc:/icons/error.svg" : "qrc:/icons/list_default.svg"
                    // Math.floor(Math.random()*2)==0? "qrc:/icons/list_subset.svg" :
                    // Math.floor(Math.random()*2)==1? "qrc:/icons/list_shotgun.svg" :
                    imgOverlayColor: isMissing? errorColor : hintColor
                }
                XsText {
                    id: textDiv
                    text: visibleItemDiv.text //+"-"+index //#TODO
                    font: visibleItemDiv.font
                    color: isMissing? hintColor : textColorNormal
                    Layout.fillWidth: true
                    Layout.preferredHeight: buttonWidth

                    leftPadding: itemPadding
                    horizontalAlignment: Text.AlignLeft
                    tooltipText: visibleItemDiv.text
                    tooltipVisibility: visibleItemDiv.hovered && truncated
                    toolTipWidth: visibleItemDiv.width+5
                }
                XsSecondaryButton{ id: addBtn
                    imgSrc: "qrc:/icons/add.svg"
                    imgOverlayColor: hintColor
                    visible: visibleItemDiv.hovered
                    Layout.preferredWidth: buttonWidth
                    Layout.preferredHeight: buttonWidth
                }
                XsSecondaryButton{ id: moreBtn
                    imgSrc: "qrc:/icons/more_horiz.svg"
                    imgOverlayColor: hintColor
                    visible: visibleItemDiv.hovered
                    Layout.preferredWidth: buttonWidth
                    Layout.preferredHeight: buttonWidth
                }
                XsText{ id: countDiv
                    text: itemCount
                    Layout.minimumWidth: buttonWidth + 5
                    Layout.preferredHeight: buttonWidth
                    color: hintColor
                }
                Item{
                    Layout.preferredWidth: buttonWidth
                    Layout.preferredHeight: buttonWidth

                    XsSecondaryButton{ id: errorIndicator
                        anchors.fill: parent
                        visible: errorRole != 0
                        imgSrc: "qrc:/icons/error.svg"
                        imgOverlayColor: hintColor

                        toolTip.text: errorRole +" errors"
                        toolTip.visible: hovered
                    }
                }
            }
        }

        background:
        Rectangle {
            id: bgDiv
            implicitWidth: 100
            implicitHeight: 40
            border.color: visibleItemDiv.down || visibleItemDiv.hovered ? borderColorHovered: borderColorNormal
            border.width: borderWidth
            color: visibleItemDiv.down || isSelected? bgColorPressed : forcedBgColorNormal

            Rectangle {
                id: bgFocusDiv
                implicitWidth: parent.width+borderWidth
                implicitHeight: parent.height+borderWidth
                visible: visibleItemDiv.activeFocus
                color: "transparent"
                opacity: 0.33
                border.color: borderColorHovered
                border.width: borderWidth
                anchors.centerIn: parent
            }
        }

        onPressed: {
            // here we set the index of the active playlist (stored by
            // XsSessionData) to our own index on click
            selectedMediaSetIndex = modelIndex
        }

        onDoubleClicked: {
            // here we set the index of the active playlist (stored by
             // XsSessionData) to our own index on click
             viewedMediaSetIndex = modelIndex
             selectedMediaSetIndex = modelIndex
        }

    }

    /* Here we have a model to iterate over the contents of the playlist (if
        any) such as subsets, timelines, dividers etc */
    DelegateModel {
        id: subItemsModel

        // we use the main session data model
        // this is required as "model" doesn't issue notifications on change
        property var notifyModel: theSessionData

        // we use the main session data model
        model: notifyModel

        // playlists are one level in at row=0, column=0.
        rootIndex: subItemsModelIndex
        delegate: chooser
    }

    DelegateChooser {
        id: chooser
        role: "typeRole"

        DelegateChoice {

            roleValue: "Subset"
            XsSubsetItemDelegate{

                width: itemRowWidth
                height: itemRowStdHeight
                modelIndex: theSessionData.index(index, 0, subItemsModelIndex)
            }
        }

        DelegateChoice {

            roleValue: "Timeline"
            XsTimelineItemDelegate{
                width: itemRowWidth
                height: itemRowStdHeight
                modelIndex: theSessionData.index(index, 0, subItemsModelIndex)
            }
        }

        DelegateChoice {

            roleValue: "ContainerDivider"
            XsPlaylistDividerDelegate{
                isSubDivider: true
                width: itemRowWidth
                height: itemRowStdHeight
            }

        }

    }

    // The layout to show the playlist sub-items
    ColumnLayout {

        id: subItems
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: visibleItemDiv.bottom
        visible: isExpanded
        spacing: 0

        Repeater {

            model: subItemsModel

        }
    }

}