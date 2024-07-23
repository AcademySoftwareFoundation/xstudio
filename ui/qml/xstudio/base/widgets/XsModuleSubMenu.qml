// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.5
import QtGraphicalEffects 1.12

import xStudio 1.0
import xstudio.qml.module 1.0

// Looking closely at the below, it looks a bit insane with the nested Instantiators!
// The idea is that this allows us to have menu/submenus going to a depth of up to 4
// levels of sub menu. I have to do it this way because we can't do recursive loading
// of a 'XsMenu' with a Loader ... it crashes QML Engine and I've not found a solution, may
// be a flaw in QML itself

// TODO: use tree model instead

XsMenu {

    id: submenu_
    title: module_menu_shim2.title
    property var root_menu_name: ""
    XsModuleMenu {
        id: module_menu_shim2
        root_menu_name: submenu_.root_menu_name ? submenu_.root_menu_name : ""

    }

    Instantiator {

        model: module_menu_shim2

        delegate: XsModuleMenuItem {}
        onObjectAdded: submenu_.addItem(object)
        onObjectRemoved: submenu_.removeItem(object)

    }

    Instantiator {

        model: module_menu_shim2.submenu_names

        delegate: XsMenu {
            id: submenu2_
            property var root_menu_name : index == -1 ? "" : module_menu_shim2.submenu_names[index]
            title: module_menu_shim3.title

            XsModuleMenu {
                id: module_menu_shim3
                root_menu_name: submenu2_.root_menu_name
        
            }

            Instantiator {

                model: module_menu_shim3
        
                delegate: XsModuleMenuItem {}
                onObjectAdded: submenu2_.addItem(object)
                onObjectRemoved: submenu2_.removeItem(object)
        
            }

            Instantiator {

                model: module_menu_shim3.submenu_names
        
                delegate: XsMenu {
                    id: submenu3_
                    property var root_menu_name : index == -1 ? "" : module_menu_shim3.submenu_names[index]
                    title: module_menu_shim4.title

                    XsModuleMenu {
                        id: module_menu_shim4
                        root_menu_name: submenu3_.root_menu_name
                
                    }

                    Instantiator {

                        model: module_menu_shim4
                
                        delegate: XsModuleMenuItem {}
                        onObjectAdded: submenu3_.addItem(object)
                        onObjectRemoved: submenu3_.removeItem(object)
                
                    }
        
                    Instantiator {

                        model: module_menu_shim4.submenu_names
                
                        delegate: XsMenu {
                            id: submenu4_
                            property var root_menu_name : index == -1 ? "" : module_menu_shim4.submenu_names[index]
                            title: module_menu_shim5.title

                            XsModuleMenu {
                                id: module_menu_shim5
                                root_menu_name: submenu4_.root_menu_name
                        
                            }

                            Instantiator {

                                model: module_menu_shim5
                        
                                delegate: XsModuleMenuItem {}
                                onObjectAdded: submenu4_.addItem(object)
                                onObjectRemoved: submenu4_.removeItem(object)
                        
                            }
        
                            Instantiator {

                                model: module_menu_shim5.submenu_names
                        
                                delegate: XsMenu {
                                    id: submenu5_
                                    property var root_menu_name : index == -1 ? "" : module_menu_shim5.submenu_names[index]
                                    title: module_menu_shim6.title

                                    XsModuleMenu {
                                        id: module_menu_shim6
                                        root_menu_name: submenu5_.root_menu_name
                                
                                    }

                                    Instantiator {

                                        model: module_menu_shim5
                                
                                        delegate: XsModuleMenuItem {}
                                        onObjectAdded: submenu4_.addItem(object)
                                        onObjectRemoved: submenu4_.removeItem(object)
                                
                                    }
                
                                    Instantiator {

                                        model: module_menu_shim6.submenu_names
                                
                                        delegate: XsMenu {
                                            id: submenu6_
                                            property var root_menu_name : index == -1 ? "" : module_menu_shim6.submenu_names[index]
                                            title: module_menu_shim7.title

                                            XsModuleMenu {
                                                id: module_menu_shim7
                                                root_menu_name: submenu6_.root_menu_name
                                        
                                            }
                                        }
                                        
                                        onObjectAdded: submenu5_.addMenu(object)
                                        onObjectRemoved: submenu5_.removeMenu(object)
                                
                                    }
                                }
                                
                                onObjectAdded: submenu4_.addMenu(object)
                                onObjectRemoved: submenu4_.removeMenu(object)
                        
                            }
                        }
                        
                        onObjectAdded: submenu3_.addMenu(object)
                        onObjectRemoved: submenu3_.removeMenu(object)
                
                    }

                }
                
                onObjectAdded: submenu2_.addMenu(object)
                onObjectRemoved: submenu2_.removeMenu(object)
        
            }
        }
        
        onObjectAdded: submenu_.addMenu(object)
        onObjectRemoved: submenu_.removeMenu(object)

    }
}