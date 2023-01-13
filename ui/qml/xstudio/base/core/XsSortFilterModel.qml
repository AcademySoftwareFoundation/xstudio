// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.9
import QtQml.Models 2.14

import xStudio 1.0

DelegateModel {
    id: delegateModel

    property var srcModel: null
    property var lessThan: function(left, right) { return true; }
    property var filterAcceptsItem: function(item) { return true; }

    onSrcModelChanged: model = srcModel

    function update() {
        if (items.count > 0) {
            items.setGroups(0, items.count, "items");
        }

        // Step 1: Filter items
        var ivisible = [];
        for (var i = 0; i < items.count; ++i) {
            var item = items.get(i);
            if (filterAcceptsItem(item.model)) {
                ivisible.push(item);
            }
        }

        // Step 2: Sort the list of visible items
        ivisible.sort(function(a, b) {
            return lessThan(a.model, b.model) ? -1 : 1;
        });

        // Step 3: Add all items to the visible group:
        for (i = 0; i < ivisible.length; ++i) {
            item = ivisible[i];
            item.inIvisible = true;
            if (item.ivisibleIndex !== i) {
                visibleItems.move(item.ivisibleIndex, i, 1);
            }
        }
    }

    items.onChanged: update()
    onLessThanChanged: update()
    onFilterAcceptsItemChanged: update()

    groups: DelegateModelGroup {
        id: visibleItems

        name: "ivisible"
        includeByDefault: false
    }

    filterOnGroup: "ivisible"
}