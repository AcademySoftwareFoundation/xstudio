# SPDX-License-Identifier: Apache-2.0
"""xStudio pipequery API."""

from xstudio.api.auxiliary import ActorConnection
from xstudio.core import get_data_atom, JsonStore, shotgun_update_entity_atom, VectorString

class ShotBrowser(ActorConnection):
    def __init__(self, connection):
        """Create PipeQuery object.

        Args:
            api(API): xStudio Api
        """

        ac = connection.get_actor_from_registry("SHOTBROWSER")

        ActorConnection.__init__(self, ac.connection, ac.remote)

    def get_data(self, data_type, project_id=-1):
        request = {"operation": "GetData", "type": data_type, "project_id": project_id}
        return self.connection.request_receive(
        self.remote,
        get_data_atom(),
        JsonStore(request)
    )[0]

    def get_projects(self):
        request = {"operation": "GetData", "type": "project", "project_id": 0}
        return self.connection.request_receive(
        self.remote,
        get_data_atom(),
        JsonStore(request)
    )[0]

    def get_project_sequence(self, project_id):
        request = {"operation": "GetData", "type": "sequence", "project_id": project_id}
        return self.connection.request_receive(
        self.remote,
        get_data_atom(),
        JsonStore(request)
    )[0]

    def get_project_episode(self, project_id):
        request = {"operation": "GetData", "type": "episode", "project_id": project_id}
        return self.connection.request_receive(
        self.remote,
        get_data_atom(),
        JsonStore(request)
    )[0]

    def get_project_shot(self, project_id):
        request = {"operation": "GetData", "type": "sequence_shot", "project_id": project_id}
        return self.connection.request_receive(
        self.remote,
        get_data_atom(),
        JsonStore(request)
    )[0]

    def get_project_scope(self, project_id):
        seq = self.get_project_sequence(project_id)
        shot = self.get_project_shot(project_id)
        return seq.get() + shot.get()


    def update_entity(self, entity, record_id, body, fields=None):
        if fields is None:
            fields = []
        return self.connection.request_receive(
            self.remote,
            shotgun_update_entity_atom(),
            entity,
            record_id,
            JsonStore(body),
            VectorString(fields))[0]
