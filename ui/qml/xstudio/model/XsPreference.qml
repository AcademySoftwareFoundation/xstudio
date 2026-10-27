// SPDX-License-Identifier: Apache-2.0
import xstudio.qml.helpers 1.0

XsPreferenceMap {
	id: control
	property string path: ""
	property string dpath: globalStoreModel ? path : ""

	onDpathChanged: {
		if(globalStoreModel)
			index = helpers.makePersistent(globalStoreModel.searchRecursive(path, "pathRole"))
	}
}

