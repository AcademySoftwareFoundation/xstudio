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

    signal updated()

    function update() {
        hiddenItems.setGroups(0, hiddenItems.count, "unsorted")
        items.setGroups(0, items.count, "unsorted")
    }

    function insertPosition(lessThan, item) {
        let lower = 0
        let upper = items.count
        while (lower < upper) {
            const middle = Math.floor(lower + (upper - lower) / 2)
            const result = lessThan(item.model,
                items.get(middle).model)
            if (result) {
                upper = middle
            } else {
                lower = middle + 1
            }
        }
        return lower
    }

    function sort(lessThan) {
        while (unsortedItems.count > 0) {
            const item = unsortedItems.get(0)

            if(!filterAcceptsItem(item.model)) {
                item.groups = "hidden"
            } else {
                const index = insertPosition(lessThan, item)
                item.groups = "items"
                items.move(item.itemsIndex, index)
            }
        }
    }

    items.includeByDefault: false
    groups: [
        DelegateModelGroup {
            id: unsortedItems
            name: "unsorted"

            includeByDefault: true

            onChanged: {
                delegateModel.sort(delegateModel.lessThan)
                updated()
            }
        },
        DelegateModelGroup {
            id: hiddenItems
            name: "hidden"

            includeByDefault: false
        }
    ]
}


// // SPDX-License-Identifier: Apache-2.0
// import QtQuick 2.9
// import QtQml.Models 2.14

// import xStudio 1.0

// DelegateModel {
//     id: delegateModel

//     property var srcModel: null
//     property var lessThan: function(left, right) { return true; }
//     property var filterAcceptsItem: function(item) { return true; }

//     onSrcModelChanged: model = srcModel

//     signal updated()

//     function update() {
//         if (items.count > 0) {
//             items.setGroups(0, items.count, "items");
//         }

//         // Step 1: Filter items
//         var ivisible = [];
//         for (var i = 0; i < items.count; ++i) {
//             var item = items.get(i);
//             if (filterAcceptsItem(item.model)) {
//                 ivisible.push(item);
//             }
//         }

//         // Step 2: Sort the list of visible items
//         ivisible.sort(function(a, b) {
//             return lessThan(a.model, b.model) ? -1 : 1;
//         });


//         // Step 3: Add all items to the visible group:
//         for (i = 0; i < ivisible.length; ++i) {
//             item = ivisible[i];
//             item.inIvisible = true;
//             if (item.ivisibleIndex !== i) {
//                 visibleItems.move(item.ivisibleIndex, i, 1);
//             }
//         }
//         updated()
//     }

//     items.onChanged: update()
//     onLessThanChanged: update()
//     onFilterAcceptsItemChanged: update()

//     groups: DelegateModelGroup {
//         id: visibleItems

//         name: "ivisible"
//         includeByDefault: false
//     }

//     filterOnGroup: "ivisible"
// }