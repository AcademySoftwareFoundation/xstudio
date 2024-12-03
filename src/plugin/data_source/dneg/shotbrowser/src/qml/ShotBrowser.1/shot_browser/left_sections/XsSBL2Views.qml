// SPDX-License-Identifier: Apache-2.0

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

import xStudio 1.0
import ShotBrowser 1.0
import xstudio.qml.helpers 1.0


XsSplitView { id: viewDiv
    QTreeModelToTableModel {
        id: treePresetsTreeModel
        model: treeModel
    }

    QTreeModelToTableModel {
        id: recentPresetsTreeModel
        model: recentModel
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
            else if(currentCategory == "Recent") return recentPresetsTreeModel
            else return menuPresetsTreeModel
        }
    }

    spacing: currentCategory == "Tree"? panelPadding : 0
    thumbWidth: currentCategory == "Tree"? panelPadding / 2 : 0

    XsSBL2V1Tree{ id: treeView
        SplitView.minimumWidth: main_split.minimumTreeWidth
        SplitView.preferredWidth: prefs.treePanelWidth
        SplitView.fillHeight: true

        visible: currentCategory == "Tree"

        onWidthChanged: {
            if(SplitView.view.resizing) {
                prefs.treePanelWidth = width
            }
        }
    }

    XsSBL2V2Presets{ id: presetsView
        SplitView.fillWidth: true
        SplitView.fillHeight: true
        SplitView.minimumWidth: 140

        visible: currentCategory != "Tree" || sequenceTreeShowPresets
    }
}
