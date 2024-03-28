// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtQuick.Controls.Styles 1.4
import QtQml.Models 2.14
import Qt.labs.qmlmodels 1.0
import QtQuick.Layouts 1.15

import xStudioReskin 1.0

XsGridView{ id: editToolsView

    property alias toolsModel: delegateModel.model
    property real btnWidth: XsStyleSheet.primaryButtonStdWidth
    property real btnHeight: XsStyleSheet.widgetStdHeight+4
    property real spacing: 1

    currentIndex: toolsModel? 0:-1
    isScrollbarVisibile: false
    flow: GridView.FlowTopToBottom

    cellWidth: btnWidth + spacing
    cellHeight: btnHeight + spacing

    model: delegateModel
    
    DelegateModel { id: delegateModel
        rootIndex: 0
        model: null
        delegate: chooser 
    }


    DelegateChooser {
        id: chooser
        role: "_type"
    
        DelegateChoice {
            roleValue: "basic";
        
            XsTimelineEditToolItems{ 
                width: btnWidth 
                height: btnHeight
                
                imgSrc: _icon
                text: _name

                isActive: editToolsView.currentIndex == index
                onClicked:{
                    editToolsView.currentIndex = index
                }

            } 
        }

    }
    
}