# SPDX-License-Identifier: Apache-2.0
"""xStudio Connection API.

Controls xStudio connections.
"""

from xstudio.core import get_api_mode_atom, absolute_receive_timeout
from xstudio.core import request_connection_atom, get_sync_atom, version_atom
from xstudio.core import get_application_mode_atom, authorise_connection_atom, broadcast_down_atom
from xstudio.core import join_broadcast_atom, exit_atom, api_exit_atom, leave_broadcast_atom, get_event_group_atom
from xstudio.core import error, Link
from xstudio.core import XSTUDIO_LOCAL_PLUGIN_PATH
from xstudio.api import API
from xstudio.sync_api import SyncAPI
from xstudio.core import RemoteSessionManager, remote_session_path
import uuid
import time
import os
from pathlib import Path
from threading import Thread

class Connection(object):
    """Handle connection to xstudio."""
    API_TYPE_FULL = "FULL"
    API_TYPE_SYNC = "SYNC"
    API_TYPE_GATEWAY = "GATEWAY"
    APP_TYPE_XSTUDIO = "XSTUDIO"
    APP_TYPE_XSTUDIO_GUI = "XSTUDIO_GUI"

    def __init__(self, auto_connect=False, background_processing=False, debug=False, load_python_plugins=False):
        """Create connection object.

        Kwargs:
           auto_connect (bool): Connect on creation.
           background_processing (bool): Spawn thread on connection to monitor queue..
           debug (bool): Output debug information.
        """
        self.link = Link()
        self.connected = False
        self.api_type = None
        self.app_type = None
        self.app_version = None
        self._api = None
        self.default_timeout_ms = 100000
        self.debug = debug

        self.responses = {}

        self.broadcast_handlers = {}
        self.broadcast_channel = {}
        self.keep_processing = True
        self.background_processing = background_processing

        self.plugins = {}

        if auto_connect:
            self.connect_remote_auto()

        if load_python_plugins:
            self.load_python_plugins()


    def enable_background_processing(self, state=True):
        """Enable auto background processing of queue.

        Kwargs:
           state (bool): set active or not.
        """

        self.background_processing = state
        if self.background_processing:
            self.start_background_processing()
        else:
           self.stop_background_processing()

    def api_handler(self, sender, req_id, event):
        """Handler for API exit.

        Args:
           sender (Actor): Sender of broadcast message.
           req_id (int): Message id.
           event (tuple): Message.

        """
        if len(event) == 1 and (
            (type(event[0]) == type(broadcast_down_atom())) or
            (type(event[0]) == type(api_exit_atom())) or
            (type(event[0]) == type(exit_atom()))):
            self.disconnect()


    def add_handler(self, remote, handler):
        """Add handler to remote broadcasts.

        Args:
           remote (Actor): Owner of broadcast actor.
           handler (func(sender, req_id, event)): Handler to install.

        Returns:
            handler_id(str): Handler Id.
        """
        agrp = self.get_broadcast(remote)
        self.join_broadcast(agrp)
        return self.add_broadcast_handler(agrp, remote, handler)

    # should use get_event_group...
    def get_broadcast(self, remote):
        """Request broadcast actor.

        Args:
           remote (Actor): Target actor.

        Returns:
            broadcast(Actor): Broadcast actore for remote.
        """
        return self.request_receive(remote, get_event_group_atom())[0]

    def join_broadcast(self, broadcast):
        """Join broadcast actor.

        Args:
           broadcast (Actor): Broadcast actor.

        Returns:
            success(bool): Broadcast joined.
        """
        if str(broadcast) not in self.broadcast_channel:
            if self.request_receive(broadcast, join_broadcast_atom())[0]:
                self.broadcast_channel[str(broadcast)] = {}
            else:
                return False

        return True

    def leave_broadcast(self, broadcast, send_leave=True):
        """Leave broadcast.

        Args:
           broadcast (Actor): Millisecs to process for.
           send_leave (bool): Notify actor we're leaving.
        """
        if send_leave:
            self.request_receive(broadcast, leave_broadcast_atom())

        broadcast_key = str(broadcast)
        for i in list(self.broadcast_channel[broadcast_key].keys()):
            self.remove_handler(i)
        del self.broadcast_channel[broadcast_key]

    def stop_processing_events(self):
        """Stop processing events forever."""
        self.keep_processing = False

    def process_events_forever(self, timeout=10):
        """Process pending events forever."""
        self.process_events(timeout, True)

    def process_events(self, timeout=10, forever=False):
        """Process pending events.

        Args:
           timeout (int): Millisecs to process for.
           forever (bool): Don't return until API shutsdown.
        """
        self.keep_processing = True
        while self.connected and self.keep_processing:
            self.dequeue_messages(timeout)
            if not forever:
                break

    def add_broadcast_handler(self, broadcast, sender, handler):
        """Add handler for broadcast.

        Args:
           broadcast (Actor): Handle identifier.
           sender (Actor): Handle identifier.
           handler (func(sender, req_id, event)): Handler function.

        Returns:
            handler_id(str): Id for handler.
        """
        sender = str(sender)
        broadcast = str(broadcast)

        if broadcast not in self.broadcast_channel:
            raise RuntimeError("Join broadcast first")
            # self.broadcast_channel[broadcast] = {}

        if sender not in self.broadcast_handlers:
            self.broadcast_handlers[sender] = {}

        result = str(uuid.uuid4())

        self.broadcast_handlers[sender][result] = handler
        self.broadcast_channel[broadcast][result] = handler

        return result

    def remove_handler(self, handler_id):
        """Remove event handler.

        Args:
           handler_id (str): Handle identifier.

        Returns:
            result(bool): Success.
        """
        result = False
        for i in self.broadcast_handlers.keys():
            if handler_id in self.broadcast_handlers[i]:
                del self.broadcast_handlers[i][handler_id]
                result = True
                break

        for i in self.broadcast_channel.keys():
            if handler_id in self.broadcast_channel[i]:
                del self.broadcast_channel[i][handler_id]
                result = True
                break
        return result

    # can include broadcast down..
    def handle_broadcast(self, event):
        """Handle broadcast type events.

        Args:
           event (Message tuple): The event.
        """
        req_id = event[-2]
        sender = event[-1]
        sender_key = str(sender)

        if sender_key in self.broadcast_handlers:
            for i in self.broadcast_handlers[sender_key].keys():
                self.broadcast_handlers[sender_key][i](sender, req_id, event[:len(event)-2])
        elif sender_key in self.broadcast_channel:
            for i in self.broadcast_channel[sender_key].keys():
                self.broadcast_channel[sender_key][i](sender, req_id, event[:len(event)-2])

            # remove callbacks to down broadcaster
            if len(event) == 3 and type(event[0]) == type(broadcast_down_atom()):
                self.leave_broadcast(sender, False)
        else:
            pass
            # print("ignored broadcast", sender, req_id, event[:len(event)-2])

    def get_key_from_stdin(self, lock):
        """Request lock key from user.

        Args:
           lock (str): Lock name.

        Returns:
            user_input(str): String entered by user.
        """
        return input("Enter key for lock {}: ".format(lock))

    def connect_local(self, actor):
        """Connect to in-process actor.

        Args:
           actor (actor): Actor object.
        """
        self.disconnect()
        connected = self.link.connect_local(actor)
        if connected:
            self.negotiate()
        else:
            raise RuntimeError("Failed to connect")

    def connect_remote_auto(self, session=None, sync_mode=False, sync_key_callback=None):
        """Connect to xStudio using session file.

        Kwargs:
           session (str): Session name.
           sync_mode (bool): Sync API.
           sync_key_callback (func): Callback for sync key.
        """
        r = RemoteSessionManager(remote_session_path())
        if session is None:
            if sync_mode:
                s = r.first_sync()
            else:
                s = r.first_api()
        else:
            s = m.find(session)
        self.connect_remote(s.host(), s.port(), sync_mode, sync_key_callback)

    def connect_remote(self, host, port, sync_mode=False, sync_key_callback=None):
        """Connect to xStudio using host/port.

        Args:
           host (str): Host name / IP.
           port (int): Host port number.

        Kwargs:
           sync_mode (bool): Sync API.
           sync_key_callback (func): Callback for sync key.
        """
        if sync_key_callback is None:
            sync_key_callback = self.get_key_from_stdin

        self.disconnect()
        connected = self.link.connect_remote(host, port)
        if connected:
            self.negotiate(sync_mode, sync_key_callback)
        else:
            raise RuntimeError("Failed to connect")

    def negotiate(self, sync_mode=False, sync_key_callback=None):
        """Negotiate connection, also handles sync lock/key.

        Kwargs:
           sync_mode (bool): Sync API.
           sync_key_callback (func): Callback for sync key.
        """

        self.api_type = self.request_receive(self.link.remote(), get_api_mode_atom())[0]

        if self.api_type == self.API_TYPE_GATEWAY:
            if not sync_mode:
                raise RuntimeError("Connection failed, this is the wrong port for FULL API")

            # need to authorise..
            lock = self.request_receive(self.link.remote(), request_connection_atom())[0]
            remote = self.request_receive(self.link.remote(),
                authorise_connection_atom(),
                lock,
                sync_key_callback(lock)
            )[0]

            self.link.set_remote(remote)
            self.api_type = self.request_receive(self.link.remote(), get_api_mode_atom())[0]

        elif self.api_type == self.API_TYPE_FULL and sync_mode:
            self.link.set_remote(self.request_receive(self.link.remote(), get_sync_atom())[0])
            self.api_type = self.request_receive(self.link.remote(), get_api_mode_atom())[0]

        self.app_version = self.request_receive(self.link.remote(), version_atom())[0]
        self.app_type = self.request_receive(self.link.remote(), get_application_mode_atom())[0]
        if self.api_type:
            self.connected = True

        # clear old handlers..
        self.broadcast_channel = {}
        self.broadcast_handlers = {}

        # connect to api broadcast.
        broadcast = self.request_receive(self.link.remote(), get_event_group_atom())[0]
        self.join_broadcast(broadcast)
        self.add_broadcast_handler(broadcast, self.link.remote(), self.api_handler)
        if self.background_processing:
            self.start_background_processing()

    def stop_background_processing(self):
        """Stop event processing thread."""
        try:
            self.stop_processing_events()
            self.queue_thread.join()
        except:
            pass

    def start_background_processing(self):
        """Start event processing thread."""
        self.stop_background_processing()
        self.queue_thread = Thread(target=self.process_events_forever)
        self.queue_thread.start()


    def __del__(self):
        self.disconnect()

    def remote(self):
        """Remote connection object.

        Returns:
            Connection(object): None if not connected.
        """
        if self.connected:
            return self.link.remote()

        return None

    def request_receive(self, *args):
        """Send message and return response."""
        return self.request_receive_timeout(self.default_timeout_ms, *args)

    def request_receive_timeout(self, timeout_milli, *args):
        """Send message and return response."""
        req_id = self.request(*args)
        return self.response(req_id, timeout_milli)

    def request(self, *args):
        """Send message and return promise."""
        req_id = self.link.request(*args)
        if req_id:
            self.responses[req_id] = None

        return req_id

    def response(self, req_id, timeout_milli=None):
        """"""
        if timeout_milli is not None:
            self._dequeue_messages(timeout_milli, req_id)

        return self._response(req_id)

    def _response(self, req_id):
        """"""
        result = None
        if req_id in self.responses and self.responses[req_id]  is not None:
            result = self.responses[req_id]
            del self.responses[req_id]
            if isinstance(result[0], error):
                raise RuntimeError(str(result[0]))

        return result

    def spawn(self, *args):
        """Spawn remote actor

        Args:
           args (args): Arguments to send.
        """
        result = None

        if self.connected:
            result = self.link.spawn(*args)

        return result

    def remote_spawn(self, *args):
        """Spawn remote actor

        Args:
           args (args): Arguments to send.
        """
        result = None

        if self.connected:
            result = self.link.remote_spawn(*args)

        return result

    def send(self, *args):
        """Send message, ignore reponse

        Args:
           args (args): Arguments to send.
        """
        if self.connected:
            self.link.send(*args)

    # def join(self, group):
    #     """Join message group

    #     Args:
    #        group (group): Group to join.
    #     """
    #     if self.api_type:
    #         self.link.join(group)

    # def leave(self, group):
    #     """Leave message group

    #     Args:
    #        group (group): Group to leave.
    #     """
    #     if self.api_type:
    #         self.link.leave(group)

    def dequeue_messages(self, timeout=10):
        """Pop waiting messages"""
        while True:
            try:
                self._dequeue_messages(timeout)
            except (TimeoutError, SystemError) as e:
                break
            except TimeoutError as e:
                break

    def caf_message_to_tuple(self, caf_message):
        """Decompose a CAF message object into a tuple of message params.

        Args:
           caf_message (CafMessage): A message originating in xstudio backend.
        """
        return self.link.tuple_from_message(caf_message)

    def _dequeue_messages(self, timeout_milli, watch_for = None):
        """Pop response messages off stack.

        Kwargs:
           timeout_milli (int): Milliseconds timeout.
        """
        start = time.perf_counter()

        while True:

            try:
                msg = self.link.dequeue_message_with_timeout(
                    absolute_receive_timeout(int(timeout_milli))
                )
            except Exception as e:
                if str(e) == "Dequeue timeout":
                    raise TimeoutError("Dequeue timeout")
                else:
                    raise

            if msg is None:
                raise TimeoutError("Dequeue timeout")

            # response so add to map..
            # and try for another message.
            req_id = msg[-2]
            if req_id in self.responses:
                self.responses[req_id] = msg
                if req_id == watch_for:
                    break
            else:
                # assumed a brocast msg.. May not be..
                self.handle_broadcast(msg)

            # handle non responses

            # exit loop if we've exceeded timeout
            elapsed = (time.perf_counter() - start) * 1000
            if elapsed > timeout_milli:
                break
            timeout_milli -= elapsed

    def disconnect(self):
        """Disconnect from xstudio"""
        if self.api_type == "SYNC":
            self.link.send_exit(self.link.remote())

        self.api_type = None
        self.app_type = None
        self.app_version = None
        self._api = None
        self.connected = False
        self.stop_background_processing()
        self.link.disconnect()

    @property
    def api(self):
        """Access API object.

        Returns:
            API(API or SyncAPI): API object or None
        """
        if self._api is None:
            if self.api_type == "SYNC":
                self._api = SyncAPI(self)
            elif self.api_type == "FULL":
                self._api = API(self)

        return self._api

    @property
    def host(self):
        """Remote host name.

        Returns:
            host(str): remote host name
        """
        return self.link.host()

    @property
    def port(self):
        """Remote host port.

        Returns:
            port(int): remote host port
        """
        return self.link.port()

    def load_python_plugins(self):
        """Load plugins found in paths defined by env var
        XSTUDIO_PYTHON_PLUGIN_PATH
        """
        if "XSTUDIO_PYTHON_PLUGIN_PATH" in os.environ:
            search_paths = os.environ["XSTUDIO_PYTHON_PLUGIN_PATH"].split(":")
            for search_path in search_paths:
                self.load_plugins_in_path(search_path)
        # XSTUDIO_LOCAL_PLUGIN_PATH points us at the folder where python plugins
        # installed/deployed as part of xstudio live
        self.load_plugins_in_path(XSTUDIO_LOCAL_PLUGIN_PATH)


    def load_plugins_in_path(self, path):
        """Load plugins found under the given directoy path

        Args:
            path (str): Path to a directory on filesystem
        """
        if not os.path.isdir(path):
            # silently ignore invalid paths, this is accepted behaviour for
            # search path mechanisms
            return

        for p in Path(path).iterdir():
            if p.is_dir():
                self.load_plugin_from_path(path, p.name)

    def load_plugin_from_path(self, path, plugin_name):
        """Load a plugin from given folder

        Args:
            path (Path): Path to a directory on filesystem
        """

        try:
            import importlib.util
            import sys

            sys.path.insert(0, path)
            spec = importlib.util.find_spec(plugin_name)
            if spec is not None:
                module = importlib.util.module_from_spec(spec)
                sys.modules[plugin_name] = module
                spec.loader.exec_module(module)
                self.plugins[path + plugin_name] = module.create_plugin_instance(self)
            else:
                print ("Error loading plugin \"{0}\" from \"{0}\" - not python importable.".format(
                    path))
        except Exception as e:
            print (str(e))