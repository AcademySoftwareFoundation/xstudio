# SPDX-License-Identifier: Apache-2.0
"""xStudio pipequery API."""

from xstudio.api.auxiliary import ActorConnection
from xstudio.core import get_data_atom

class PipeQuery(ActorConnection):
    def __init__(self, connection):
        """Create PipeQuery object.

        Args:
            api(API): xStudio Api
        """

        ac = connection.get_actor_from_registry("IVYDATASOURCE")

        ActorConnection.__init__(self, ac.connection, ac.remote)


    def execute_query(self, query):
        """Execute pipequery query

        Args:
            query(Json): call to execute

        Result:
            return(Json): result

        """

        return self.connection.request_receive(
                self.remote,
                get_data_atom(),
                query
            )[0]

    # def get_projects(self, fields=None):
    #     if fields is None:
    #         fields = ["id", "name"]



    #     request = """{{latest_versions(
    #         show: \"{show}\"
    #         scopes: [{{name:\"{show}\"}}]
    #         kinds: "iss"
    #         statuses: APPROVED
    #         name_tags: {{tags: [{{name: \"type\", value: \"task\"}}], match: ALL}}
    #     )
    #     {{
    #         name
    #         id
    #         files {{
    #           path
    #         }}
    #         }}
    #     }}""".format(show=show)

    #     return self.pq.execute_query(request)

