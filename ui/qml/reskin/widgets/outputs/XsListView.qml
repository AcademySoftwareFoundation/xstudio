// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtGraphicalEffects 1.15
import QtQml.Models 2.14

import xStudioReskin 1.0


ListView { id: widget

    property bool isScrollbarVisibile : true
    
    clip: true
    spacing: 0

    width: contentWidth<parent.width-x? contentWidth : parent.width-x
    height: contentHeight<parent.height-y? contentHeight : parent.height-y

    contentHeight: contentItem.childrenRect.height
    contentWidth: contentItem.childrenRect.width

    orientation: ListView.Vertical
    snapMode: ListView.SnapToItem //SnapToItem
    // highlightRangeMode: ListView.StrictlyEnforceRange

    ScrollBar.vertical: XsScrollBar {id: scrollBar; x:width; visible: isScrollbarVisibile && widget.height < widget.contentHeight;}
    // ScrollBar.horizontal: XsScrollBar { y:height; }//visible: isScrollbarVisibile && widget.width < widget.contentWidth}

    // model: delegateModel
    // delegate: 

    
    // DelegateModel { 
    //     id: delegateModel    
    //     rootIndex: 0
    //     model: dataModel
    //     delegate: chooser 
    // }

    // DelegateChooser {
    //     id: delegateChooser
    //     // role: "_type"
    // }
 
    




    // property var searchResultsViewModel
    
    // property bool isSelected: selectionModel.isSelected(searchResultsViewModel.index(index, 0))
    
    // property int checkedCount: sourceSelectionModel.selectedIndexes.length
    
    // property alias checkedIndexes: sourceSelectionModel.selectedIndexes

    // ItemSelectionModel {
    //     id: selectionModel

    //     model: searchResultsViewModel
        
    //     property int selectedCount: 0
    //     property int prevSelectedIndex: 0

    //     property int displayedItemCount: searchResultsViewModel.sourceModel.count

    //     onDisplayedItemCountChanged: {
    //         prevSelectedIndex=0
    //         resetSelection()
    //     }

    //     property int shotsSelectedCount: 0
    //     onShotsSelectedCountChanged: selectedCount = shotsSelectedCount;
    // }
}