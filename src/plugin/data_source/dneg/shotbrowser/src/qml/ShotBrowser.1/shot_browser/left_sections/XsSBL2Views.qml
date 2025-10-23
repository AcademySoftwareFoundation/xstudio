// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic

import xStudio 1.0
import ShotBrowser 1.0
import xstudio.qml.helpers 1.0


XsSplitView { id: viewDiv


    QTreeModelToTableModel {
        id: treePresetsTreeModel
        model: treeModel
    }

    QTreeModelToTableModel {
        id: menuPresetsTreeModel
        model: menuModel
    }

    property var presetsTreeModel: {
        if(!ShotBrowserEngine.ready)
            return null
        else {
            if(currentCategory == "Tree") return treePresetsTreeModel
            else return menuPresetsTreeModel
        }
    }

    // spacing: currentCategory == "Tree"? panelPadding : 0
    // thumbWidth: currentCategory == "Tree"? panelPadding / 2 : 0
    spacing: panelPadding
    thumbWidth: panelPadding / 2

    XsSBL2V1Tree{ id: treeView
        SplitView.minimumWidth: main_split.minimumTreeWidth
        SplitView.preferredWidth: prefs.treePanelWidth
        SplitView.fillHeight: true
        setupMode: currentCategory != "Tree"
        // opacity: currentCategory == "Tree" ? 1.0 : 0.2

        // visible: currentCategory == "Tree"

        XsPreference {
              id: hideTree
              path: "/plugin/data_source/shotbrowser/browser/hide_tree"
        }

        onWidthChanged: {
            if(SplitView.view.resizing) {
                prefs.treePanelWidth = width
            }
        }
        visible: !hideTree.value
    }

    XsSBL2V2Presets{ id: presetsView
        SplitView.fillWidth: true
        SplitView.fillHeight: true
        SplitView.minimumWidth: 140

        // visible: currentCategory != "Tree" || sequenceTreeShowPresets
    }
}
