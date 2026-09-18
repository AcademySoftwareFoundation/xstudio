# SPDX-License-Identifier: Apache-2.0
from xstudio.api.module import ModuleBase

class ColourPipeline(ModuleBase):
    """ColourPipeline object, represents the colour pipeline plugin
    instance that interacts with the viewport to provide colour management
    data for image display in the viewport."""

    def __init__(self, connection, remote):
        """Create Viewport object.

        Args:
            connection(Connection): Connection object
            remote(Actor): The colour pipeline actor
        """
        ModuleBase.__init__(
            self,
            connection,
            remote
            )

    @property
    def view(self):
        """Access the ModuleAttribute object that controls the OCIO View
        """
        return self.get_attribute("View")

    @property
    def display(self):
        """Access the ModuleAttribute object that controls the OCIO Display
        """
        return self.get_attribute("Display")

    @property
    def exposure(self):
        """Access the ModuleAttribute object that controls the viewer Exposure
        """
        return self.get_attribute("Exposure (E)")

    @property
    def gamma(self):
        """Access the ModuleAttribute object that controls the viewer Gamma
        """
        return self.get_attribute("Gamma")

    @property
    def saturation(self):
        """Access the ModuleAttribute object that controls the viewer Saturation
        """
        return self.get_attribute("Saturation")

    @property
    def channel(self):
        """Access the ModuleAttribute object that controls the viewer Channel
        """
        return self.get_attribute("Channel")
