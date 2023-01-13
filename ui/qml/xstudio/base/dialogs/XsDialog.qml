// SPDX-License-Identifier: Apache-2.0
import QtQml 2.15
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15

import xStudio 1.0

XsWindow {
	asDialog: true

	Shortcut {
	    sequence: "Esc"
	    onActivated: reject()
	}

	signal accepted()
	signal rejected()

	function accept() {
		accepted()
		close()
	}

	function reject() {
		rejected()
		close()
	}

	function open() {
		show()
	}
}