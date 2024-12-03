// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQml.Models 2.15

import xStudio 1.0

Item {




    function setSelectionIndex(my_index) {

        isSelected = mediaSelectionModel.selectedIndexes.includes(my_index)
        if (mediaSelectionModel.multiSelected) {
            if (isSelected) {
                selectionIndex = mediaSelectionModel.selectedIndexes.indexOf(my_index)+1
            } else {
                selectionIndex = 0
            }
        } else {
            selectionIndex = mediaListModelData.sourceIndexToRow(my_index)+1
        }
    }



    function toggleSelection() {

        var myIdx = modelIndex()
        if (!(mediaSelectionModel.selection.count == 1 &&
            mediaSelectionModel.selection[0] == myIdx)) {
            mediaSelectionModel.select(
                myIdx,
                ItemSelectionModel.Toggle
                )
            }

    }

    function exclusiveSelect() {

        mediaSelectionModel.select(
            modelIndex(),
            ItemSelectionModel.ClearAndSelect
            | ItemSelectionModel.setCurrentIndex
            )

    }

    function inclusiveSelect() {

        // For Shift key and select find the nearest selected row,
        // select items between that row and the row of THIS item
        var row = index // row of this item
        var d = 10000
        var nearest_row = -1

        // looping over current selection
        var selection = mediaSelectionModel.selectedIndexes
        for (var i = 0; i < selection.length; ++i) {

            // convert the index of selected item (which is an index into
            // theSessionData) into the row in our filtered media list model
            var r = mediaListModelData.sourceIndexToRow(selection[i])
            if (r > -1) {
                var delta = Math.abs(r-row)
                if (delta < d) {
                    d = delta
                    nearest_row = r
                }
            }
        }

        if (nearest_row!=-1) {

            var first = Math.min(row, nearest_row)
            var last = Math.max(row, nearest_row)
            var selection = []

            for (var i = first; i <= last; ++i) {

                selection.push(helpers.makePersistent(mediaListModelData.rowToSourceIndex(i)))
            }

            mediaSelectionModel.select(
                helpers.createItemSelection(selection),
                ItemSelectionModel.Select
            )
        }
    }






}
