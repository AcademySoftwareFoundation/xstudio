// SPDX-License-Identifier: Apache-2.0
import QtQml.Models 2.14

DelegateModel {
	property var baseModel: null

	onBaseModelChanged: model = baseModel

	function remove(row) {
		model.removeRows(row, 1, rootIndex)
	}

	function move(src_row, dst_row) {
		// console.log("TreeDelegateModel.qml", src_row, "->", dst_row, rootIndex)
		// invert logic if moving down
		if(dst_row > src_row) {
			model.moveRows(rootIndex, dst_row, 1, rootIndex, src_row)
		} else {
			model.moveRows(rootIndex, src_row, 1, rootIndex, dst_row)
		}
	}

	function insert(row, data) {
		model.insert(row, rootIndex, data)
	}

	function append(data, root=rootIndex) {
		model.insert(rowCount(root), root, data)
	}

	function rowCount(root=rootIndex) {
		return model.rowCount(root)
	}

	function get(row, role) {
		return model.get(row, rootIndex, role)
	}

	function set(row, value, role) {
		return model.set(row, value, role, rootIndex)
	}
}
