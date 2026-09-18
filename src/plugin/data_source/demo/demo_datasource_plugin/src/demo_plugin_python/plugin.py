#!/bin/env python
# SPDX-License-Identifier: Apache-2.0

from xstudio.plugin import PluginBase
from xstudio.core import JsonStore, Uuid, put_data_atom, event_atom
from . import database_data
import json
from .jsonpointer import resolve_pointer
from .generate_timeline import make_otio_timeline

custom_qml = """
import QtQuick
import QtQuick.Controls

Popup {
    id: popup
    x: 100
    y: 100
    width: 200
    height: 300
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent
    contentItem: Text {
        text: "Demo Plugin - Popup Window"
    }

}"""

# Declare our plugin class - we're using the PluginBase base class here which
# is based off the StandardPlugin/Module C++ base classes and exposes many of
# the same API features. We are able to build custom menus for our UI panel or
# 
class DemoPluginPython(PluginBase):

    def __init__(self, connection):

        PluginBase.__init__(
            self,
            connection,
            name="DemoPluginPython"
            )

        self.menu_id = self.insert_menu_item(
            menu_model_name="main menu bar",
            menu_text="Show Pop-Up",
            menu_path="Demo Plugin|First Submenu|Sub-Sub Menu",
            menu_item_position=1.0,
            callback=self.cb)

        # add a toggle attribute
        self.toggle_attribute = self.add_attribute(
            "Toggle Attribute",
            False,
            preference_path="/plugin/demo_plugin/menu_item_hidden")

        # add a toggle menu item that is linked to the toggle attribute
        self.menu_id2 = self.insert_menu_item(
            menu_model_name="main menu bar",
            menu_path="Demo Plugin|Second Submenu",
            menu_text="Other Sub-menu is Hidden",
            menu_item_position=1.0,
            attr_id=self.toggle_attribute.uuid,
            callback=self.cb2)

        # tell xstudio how to order the sub-menus
        self.set_submenu_position(
            menu_model_name="main menu bar",
            submenu_path="Demo Plugin|First Submenu",
            menu_item_position=1.0)

        # this makes 'Demo Plugin' appear before the Colour menu
        # (whose position is set to 30.0)
        self.set_submenu_position(
            menu_model_name="main menu bar",
            submenu_path="Demo Plugin",
            menu_item_position=10.0)

        self.set_submenu_position(
            menu_model_name="main menu bar",
            submenu_path="Demo Plugin|Second Submenu",
            menu_item_position=2.0)

        self.insert_menu_item(
            menu_model_name="main menu bar",
            menu_path="Demo Plugin",
            menu_text="divider",
            menu_item_position=1.5,
            divider=True)

        # expose our attributes in the UI layer
        self.connect_to_ui()

        self.database_tables = database_data.generate_tables()
        self.jobs_table = self.database_tables[0]
        self.versions_table = {}
        # version table is one flat list!
        self.versions_table = self.database_tables[1]

        self.cpp_plugin_actor = None

    def attribute_changed(self, attribute, role):

        if attribute == self.toggle_attribute:
            print ("ATTRIBUTE CHANGE", attribute.as_json())

    def cb(self):
        self.create_qml_item(custom_qml)

    def cb2(self):
        if self.toggle_attribute.value() and self.menu_id:
            self.remove_menu_item(self.menu_id)
            self.menu_id = None
        elif not self.toggle_attribute.value() and not self.menu_id:
            self.menu_id = self.insert_menu_item(
                menu_model_name="main menu bar",
                menu_text="Show Pop-Up",
                menu_path="Demo Plugin|First Submenu|Sub-Sub Menu",
                menu_item_position=1.0,
                callback=self.cb)
            self.set_submenu_position(
                menu_model_name="main menu bar",
                submenu_path="Demo Plugin|First Submenu",
                menu_item_position=1.0)


    def get_list_of_productions(self):
        """
        Return the 'job' data value from the first set of
        rows in our job table. This will give us a list 
        of productions (jobs) that can be used to populate
        the drop-down menu to select current production

        returns:
            jobs (list(str)): list of production code names
        """

        # Now is a good time to get a handle on the C++ actor component
        # of our plugin, because we know that it's the C++ plugin that
        # has called 'get_list_of_productions' so it has definintely
        # been created.
        self.cpp_plugin_actor = self.connection.api.plugin_manager.get_plugin_instance(
            Uuid("28813519-aa6e-42a5-a201-a55f07136565")
            )

        # return type here must be JsonStore
        return [j['job'] for j in self.jobs_table['rows']]

    def set_version_data(self, version_uuid, role_name, role_value):
        """
        Set a particualr value (for a given role_name/key) in the versions table
        identifying the table entry with the version uuid

        Args:
            version_uuid(str): uuid as a string of the version entry
            role_name(str): the key (or role name) for the value to change
            role_value(json): the value to set
        """
        for v in self.versions_table:
            if v['uuid'] == version_uuid:
                if v[role_name] != role_value:
                    v[role_name] = role_value
                    # now we send a message to the C++ plugin that data has changed, so it can update the UI
                    j = JsonStore()
                    j.parse_string(json.dumps(role_value))
                    self.connection.send(self.cpp_plugin_actor, event_atom(), put_data_atom(), version_uuid, role_name, j)

    def get_data(self, versions_list, json_ptr):
        """
        Get the database entry at some given node-key using json_pointer

        Args:
            versions_list(bool): lookup from the versions table (or jobs table if false)
            json_ptr(str): A json pointer into the table. For example /rows/1/rows/3/shot
            will return the 'shot' value at the 4th row of the 2nd row from the root of
            the dataset. (note the jobs_table has a tree structure where child nodes are
            located in an array at 'rows' of the parent)

        Returns:
            value(JsonStore): the database value @ json_ptr
        """
        return resolve_pointer(self.versions_table if versions_list else self.jobs_table, json_ptr)

    def get_row_count(self, versions_list, rows_ptr):
        """
        Get the database child row count at a given node accessed with a json_pointer

        Args:
            versions_list(bool): lookup from the versions table (or jobs table if false)
            json_ptr(str): A json pointer into the table.

        Returns:
            row_count(int): the number of rows @ json_ptr
        """

        return len(resolve_pointer(self.versions_table if versions_list else self.jobs_table, rows_ptr))

    def select_versions(self, selection_rows):
        """
        Get an array of versions (assets) published under the shots or sequences indicated
        by the incoming list of rows in the jobs table

        Args:
            selection_rows(list(str)): A list of json pointers into the jobs data tree

        Returns:
            versions_data(list(JsonStore)): All the versions that belong to the sequences/shots in
                                            the selection set
        """

        selected_uuids = []
        result = []
        for json_ptr in selection_rows:
            obj = resolve_pointer(self.jobs_table, json_ptr)
            selected_uuids.append(obj['uuid'])
        for v in self.versions_table:
            if v['shot_id'] in selected_uuids:
                result.append(v)
        return result

    def get_version_by_uuid(self, uuid):
        for v in self.versions_table:
            if v['uuid'] == uuid:
                return v
        raise Exception("No such uuid {} in versions table.".format(uuid))

    def search_jobs_records(self, match_fields, level, branch=None, result=None):

        if not branch:
            branch = self.jobs_table
        if result == None:
            result = []
        for row in branch['rows']:
            match_count = sum(
                int(
                    field in row and row[field] == value
                    ) for (field, value) in match_fields)
            if 'level' in row and row['level'] == level:
                result.append(row)
            elif match_count:
                self.search_jobs_records(match_fields, level, row, result)
        return result

    def loadSequences(self, sequence_uuids):
        """
        For a given list of sequence IDs, build an OTIO from the
        shot frame ranges using the shot records under the given
        sequence (timeline version) ID.
        """
        result = []
        session = self.connection.api.session
        for v in self.versions_table:
            if v['uuid'] not in sequence_uuids:
                continue
            # Get the shot records underneath the sequence
            shot_sets = self.search_jobs_records(
                [
                    ('uuid', v['job_id']),
                    ('uuid', v['sequence_id']),
                ],
                level='shot'
            )
            otio_json = make_otio_timeline(shot_sets)
            playlist = session.create_playlist(name=v['version_name'])[1]
            timeline = playlist.create_timeline(name="Sequence")[1]
            timeline.load_otio(otio_json)
            result.append(str(timeline.uuid))

            # Let's say we wanted to put the timeline on the screen. We 
            # can do it like this (we'll actually do it on the QML side instead)
            # session.viewed_container = timeline
            # session.inspected_container = timeline            

        return result


# This method is required by xSTUDIO
def create_plugin_instance(connection):
    return DemoPluginPython(connection)
