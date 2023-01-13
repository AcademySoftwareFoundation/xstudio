// SPDX-License-Identifier: Apache-2.0
import QtQml 2.14
import QtQuick 2.14
import QtQuick.Controls 2.14

import xstudio.qml.embedded_python 1.0
import xStudio 1.0

XsWindow {
	id: python_dialog
    width:  800
    height: 600
    title: "Python Console"
    property var session_uuid: null
	property var history: []

    EmbeddedPython {
    	id: python
    }

    onVisibleChanged: {
    	if(visible && !python_dialog.session_uuid) {
	    	python_dialog.session_uuid = python.createSession()
			output.forceActiveFocus()
			history = preferences.python_history.value
		}
    }

	ScrollView {
	    id: view_output
	    anchors.fill: parent

	    TextArea {
	    	id: output
	    	//readOnly: true
	        text: ""
            // textFormat: TextArea.RichText
            font.pixelSize: 14
            font.family: XsStyle.fontFamily
            font.hintingPreference: Font.PreferNoHinting
            color:"#909090"
			selectionColor: XsStyle.highlightColor
			wrapMode: TextEdit.WordWrap
			selectByMouse: true
			property int history_index: 0
            // add history, ctrl-d. ctrl-c
			focus: true

	        Keys.onReturnPressed: {
	        	// get last line.
	        	// only capute after first 4 chars
				if(event.modifiers == Qt.ControlModifier) {
					var input = selectedText
		        	text += selectedText + "\n"
					python.sendInput(input)
					if(input.length && history[history.length-1] != input) {
						history.push(selectedText)
						preferences.python_history.value = history
					}
					history_index = history.length
				} else {
		        	var input = text.substring(text.lastIndexOf("\n")+5)
		        	text += "\n"
		        	python.sendInput(input)
					if(input.length && history[history.length-1] != input) {
						history.push(input)
						preferences.python_history.value = history
					}
					history_index = history.length
		        }
	        }

		    Keys.onUpPressed: {
		    	if(history.length) {
			    	if(history_index != 0) {
				    	history_index--
			    	}
			    	var input = history[history_index]
			    	text = text.substring(0, text.lastIndexOf("\n")+5) + input
			    	cursorPosition = text.length
				}
	    	}

		    Keys.onDownPressed: {
		    	if(history.length) {
			    	var input = ""

			    	if(history_index >= history.length-1) {
			    		//blank..
			    	} else {
				    	history_index++
				    	input = history[history_index]
			    	}
			    	text = text.substring(0, text.lastIndexOf("\n")+5) + input
			    	cursorPosition = text.length
			    }
	    	}

	        Keys.onPressed: {
 			    if ((event.key == Qt.Key_C) && (event.modifiers & Qt.ControlModifier)) {
		            python.sendInterrupt()
		            event.accepted = true;
		        } else if(event.key == Qt.Key_Copy) {
		            event.accepted = true;
		            output.copy()
		        } else if(event.key == Qt.Key_Paste) {
		            event.accepted = true;
		            output.paste()
		        } else if(event.key == Qt.Key_Home) {
   			    	cursorPosition = text.lastIndexOf("\n")+5
		            event.accepted = true;
		        } else if(event.key == Qt.Key_Backspace || event.key == Qt.Key_Left) {
   			    	if(cursorPosition <= text.lastIndexOf("\n")+5){
			            event.accepted = true;
   			    	}
		        } else if(cursorPosition < text.lastIndexOf("\n")+5) {
		        	event.accepted = true;
		        }
		    }

		    Connections {
		        target: python
		        function onStdoutEvent(str) {
		    		output.text = output.text + str
		    		output.cursorPosition = output.length
		    		// output.text = output.text + str.replace(/(?:\r\n|\r|\n)/g, '<br>')
		        }
		        function onStderrEvent(str) {
		    		output.text = output.text + str
		    		output.cursorPosition = output.length
		    		// output.text = output.text + "<font color=\"#999999\">"+ str.replace(/(?:\r\n|\r|\n)/g, '<br>')+"</font>"
		    		// output.append("<font color=\"#999999\">"+str+"</font>")
		        }
		    }
	    }
	}
}
