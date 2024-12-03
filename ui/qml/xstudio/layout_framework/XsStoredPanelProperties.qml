// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12

import xstudio.qml.helpers 1.0

// This utility allows us to have a property be stored in the 'user_data'
// roleData item in the node in the panels model. It takes care of storing 
// the property when it changes, and retrieves the stored value from user_data
// on construction.
// This lets us easily have persistent properties from one session to the next
Item {
    
    id: ddHandler
    property var propertyNames
    property var numInitialised: 0
    onNumInitialisedChanged: {
        if (numInitialised == propertyNames.length) propertiesInitialised()
    }

    signal propertiesInitialised()

    Repeater {
        model: propertyNames
        Item {
            id: ff
            property bool initialised: false
            property var thePropertyName: propertyNames[index]
            XsPropertyFollower {
                id: clipboard
                propertyName: thePropertyName
                target: ddHandler.parent
                onPropertyValueChanged: {
                    // We don't get notification on user_data unless we do it 
                    // this long winded way
                    var v = {}
                    if (typeof user_data == "object" && !Array.isArray(user_data)) {
                        // deep copy
                        v = JSON.parse(JSON.stringify(user_data))
                    }
                    v[propertyName] = propertyValue
                    user_data = v
                }
            }
        
            Binding {
                target: ddHandler.parent
                property: thePropertyName
                value: foo
                when: foo != undefined
            }

            property var userData: user_data
            property var foo
            onUserDataChanged: {
                if (initialised) return
                if (typeof user_data == "object" && 
                    user_data[thePropertyName] != undefined) {
                    foo = user_data[thePropertyName]
                }
                initialised = true
                numInitialised = numInitialised+1
            }
        
        }
    }


}
