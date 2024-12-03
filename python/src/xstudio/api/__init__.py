# SPDX-License-Identifier: Apache-2.0
from xstudio.core import get_studio_atom, get_global_image_cache_atom, get_global_audio_cache_atom, get_global_thumbnail_atom
from xstudio.core import get_global_store_atom, get_plugin_manager_atom, get_scanner_atom, exit_atom, get_python_atom
from xstudio.common_api import CommonAPI
from xstudio.api.studio import Studio
from xstudio.api.intrinsic import GlobalStore
from xstudio.api.intrinsic import Thumbnail
from xstudio.api.intrinsic import MediaCache
from xstudio.api.intrinsic import PluginManager
from xstudio.api.intrinsic import Scanner
from xstudio.api.auxiliary.helpers import Filesize

class API(CommonAPI):
    """API connection.

    Subclass connection for full API.
    """
    def __init__(self, connection):
        """Create API object.

        Args:
           connection (object): Connection object.
        """
        super(API, self).__init__(connection)
        self._app = None
        self._image_cache = None
        self._audio_cache = None
        self._global_store = None
        self._thumbnail = None
        self._scanner = None
        self._plugin_manager = None

    @property
    def app(self):
        """Studio object.

        Returns:
            Studio(object): If connected, `None` otherwise

        """
        if self._app is None:
            if self.connection.app_type in [
                self.connection.APP_TYPE_XSTUDIO,
                self.connection.APP_TYPE_XSTUDIO_GUI
            ]:
                self._app = Studio(
                    self.connection,
                    self.connection.request_receive(
                        self.connection.remote(),
                        get_studio_atom()
                    )[0]
                )

        return self._app

    def shutdown(self):
        """Shutdown xStudio app.

        Returns:
            Success(bool): Shutting down.

        """
        return self.connection.request_receive(
                        self.connection.remote(),
                        exit_atom()
                    )[0]

    @property
    def host(self):
        """Current connection host.

        Returns
            host(str): host address

        """

        return self.connection.link.host()

    @property
    def port(self):
        """Current connection port.

        Returns
            port(int): host port
        """
        return self.connection.link.port()


    @property
    def session(self):
        """Session object.

        Returns:
            Session(object): If connected, `None` otherwise
        """
        return self.app.session

    @property
    def global_store(self):
        """Global JSON store object.

        Returns:
            GlobalStore(object): If connected, `None` otherwise
        """
        if self._global_store is None:
            self._global_store = GlobalStore(
                self.connection,
                self.connection.request_receive(
                    self.connection.remote(),
                    get_global_store_atom()
                )[0]
            )

        return self._global_store

    @property
    def thumbnail(self):
        """Global thumbnail object.

        Returns:
            Thumbnail(object): If connected, `None` otherwise
        """
        if self._thumbnail is None:
            self._thumbnail = Thumbnail(
                self.connection,
                self.connection.request_receive(
                    self.connection.remote(),
                    get_global_thumbnail_atom()
                )[0]
            )

        return self._thumbnail

    @property
    def scanner(self):
        """Global scanner object.

        Returns:
            Scanner(object): If connected, `None` otherwise
        """
        if self._scanner is None:
            self._scanner = Scanner(
                self.connection,
                self.connection.request_receive(
                    self.connection.remote(),
                    get_scanner_atom()
                )[0]
            )

        return self._scanner

    @property
    def plugin_manager(self):
        """Global thumbnail object.

        Returns:
            PluginManager(object): If connected, `None` otherwise
        """
        if self._plugin_manager is None:

            self._plugin_manager = PluginManager(
                self.connection,
                self.connection.request_receive(
                    self.connection.remote(),
                    get_plugin_manager_atom()
                )[0]
            )

        return self._plugin_manager

    @property
    def image_cache(self):
        """Global image cache object.

        Returns:
            MediaCache(object): If connected, `None` otherwise
        """
        if self._image_cache is None:
            self._image_cache = MediaCache(
                self.connection,
                self.connection.request_receive(
                    self.connection.remote(),
                    get_global_image_cache_atom()
                )[0]
            )

        return self._image_cache

    @property
    def audio_cache(self):
        """Global audio cache object.

        Returns:
            MediaCache(object): If connected, `None` otherwise
        """
        if self._audio_cache is None:
            self._audio_cache = MediaCache(
                self.connection,
                self.connection.request_receive(
                    self.connection.remote(),
                    get_global_audio_cache_atom()
                )[0]
            )

        return self._audio_cache

    def status(self):
        """Return status of application

        Returns:
            status(dict): Dict with status.
        """
        stat = {}

        stat["cache"] = {"image": {}, "audio": {}, "thumbnail": {"disk": {}, "memory": {}}, }
        stat["session"] = {"playlist": {}, "media": {}}
        stat["connection"] = {}

        try:
            if self.connection.app_version is None:
                stat["connection"]["connected"] = False
            else:
                # connection stats
                stat["connection"]["connected"] = True
                stat["connection"]["type"] = self.connection.api_type
                stat["connection"]["version"] = self.connection.app_version
                stat["connection"]["application"] = self.connection.app_type
                stat["connection"]["host"] = self.connection.host
                stat["connection"]["port"] = self.connection.port

                stat["cache"]["image"]["count"] = self.image_cache.count
                stat["cache"]["image"]["size"] = self.image_cache.size
                stat["cache"]["audio"]["count"] = self.audio_cache.count
                stat["cache"]["audio"]["size"] = self.audio_cache.size
                stat["cache"]["thumbnail"]["disk"]["count"] = self.thumbnail.count_disk
                stat["cache"]["thumbnail"]["disk"]["size"] = self.thumbnail.size_disk
                stat["cache"]["thumbnail"]["memory"]["count"] = self.thumbnail.count_memory
                stat["cache"]["thumbnail"]["memory"]["size"] = self.thumbnail.size_memory

                stat["session"]["path"] = self.app.session.path
                stat["session"]["playlist"]["count"] = len(self.app.session.playlists)
                stat["session"]["media"]["count"] = len(self.app.session.get_media())

        except Exception as err:
            print (err)


        return stat


    def print_status(self):
        """Print status nicely."""
        s = self.status()

        print("xStudio status")

        if s["connection"]["connected"]:
            print("Connection:")
            print("  Remote:", s["connection"]["host"] + ":" + str(s["connection"]["port"]))
            print("  Detail:", s["connection"]["application"], s["connection"]["type"], s["connection"]["version"])
            print("Cache:")
            print("  Image:", s["cache"]["image"]["count"], str(Filesize(s["cache"]["image"]["size"])),)
            print("  Audio:", s["cache"]["audio"]["count"], str(Filesize(s["cache"]["audio"]["size"])),)
            print("  Thumbnail Dsk:", s["cache"]["thumbnail"]["disk"]["count"], str(Filesize(s["cache"]["thumbnail"]["disk"]["size"])),)
            print("  Thumbnail Mem:", s["cache"]["thumbnail"]["memory"]["count"], str(Filesize(s["cache"]["thumbnail"]["memory"]["size"])),)
            print("Session:")
            print("  Path:", s["session"]["path"])
            print("  Playlists:", s["session"]["playlist"]["count"])
            print("  Media:", s["session"]["media"]["count"])
        else:
            print("Not connected.")