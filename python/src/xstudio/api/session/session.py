# SPDX-License-Identifier: Apache-2.0
from xstudio.core import get_playlists_atom, get_playlist_atom, add_playlist_atom, move_container_atom
from xstudio.core import get_container_atom, create_group_atom, remove_container_atom, path_atom, get_media_atom
from xstudio.core import rename_container_atom, create_divider_atom, media_rate_atom, playhead_rate_atom
from xstudio.core import reflag_container_atom, merge_playlist_atom, copy_container_to_atom
from xstudio.core import get_bookmark_atom, save_atom, active_media_container_atom, name_atom
from xstudio.core import viewport_active_media_container_atom
from xstudio.core import URI, Uuid, VectorUuid, item_selection_atom, type_atom
from xstudio.core import render_to_video_atom, get_event_group_atom, event_atom

from xstudio.api.session.container import Container, PlaylistTree, PlaylistItem
from xstudio.api.session.playlist import Playlist
from xstudio.api.session.media.media import Media
from xstudio.api.session.bookmark import Bookmarks
from xstudio.api.session.playlist.subset import Subset
from xstudio.api.session.playlist.contact_sheet import ContactSheet
from xstudio.api.session.playlist.timeline import Timeline
from xstudio.api.auxiliary import NotificationHandler

class Session(Container, NotificationHandler):
    """Session object."""

    def __init__(self, connection, remote, uuid=None):
        """Create Session object.

        Derived from Container.

        Args:
            connection(Connection): Connection object
            remote(object): Remote actor object

        Kwargs:
            uuid(Uuid): Uuid of remote actor.
        """
        Container.__init__(self, connection, remote, uuid)
        NotificationHandler.__init__(self, self)

        # for video renderer interaction
        self.vid_render_job_callbacks = {}
        self.subscribed_to_vid_renderer = False

    @property
    def selected_media(self):
        """Get currently selected media.

        Returns:
            Media(): Media.
        """

        if self.inspected_container:
            return self.inspected_container.playhead_selection.selected_sources
        return []

    @property
    def selected_containers(self):
        """Get currently selected containers.

        Returns:
            container(Playlist,Subset,Timelime,ContactSheet): Container.
        """

        items = self.connection.request_receive(self.remote, item_selection_atom())[0]
        result = []

        for i in items:
            item_type = self.connection.request_receive(i.actor, type_atom())[0]
            if item_type == "Timeline":
                result.append(
                    Timeline(self.connection, i.actor, i.uuid)
                )
            elif item_type == "Subset":
                result.append(
                    Subset(self.connection, i.actor, i.uuid)
                )
            elif item_type == "ContactSheet":
                result.append(
                    ContactSheet(self.connection, i.actor, i.uuid)
                )
            elif item_type == "Playlist":
                result.append(
                    Playlist(self.connection, i.actor, i.uuid)
                )


        return result

    @property
    def viewed_playlist(self):
        """Get currently viewed (Playlist,Subset,Timelime,ContactSheet).

        Returns:
            Playlist(): Playlist.
        """
        return self.viewed_container

    @property
    def viewed_container(self):
        """Get currently viewed container.

        Returns:
            container(Playlist,Subset,Timelime,ContactSheet): Container.
        """

        result = self.connection.request_receive(self.remote, viewport_active_media_container_atom())[0]
        c = Container(self.connection, result.actor)

        if c.type == "Timeline":
            return Timeline(self.connection, result.actor, result.uuid)
        elif c.type == "Subset":
            return Subset(self.connection, result.actor, result.uuid)
        elif c.type == "ContactSheet":
            return ContactSheet(self.connection, result.actor, result.uuid)

        return Playlist(self.connection, result.actor, result.uuid)

    @viewed_container.setter
    def viewed_container(self, container):
        """Set the current, on-screen playlist

        Args:
            container(Playlist,Subset,Timelime,ContactSheet): playlist, subset or timeline(sequence)
        """
        self.connection.send(self.remote, viewport_active_media_container_atom(), container.uuid)

    @property
    def inspected_playlist(self):
        """Get currently inspected playlist/subset/timeline etc.

        Returns:
            container(Playlist,Subset,Timelime,ContactSheet): Container.
        """
        return self.inspected_container


    @property
    def inspected_container(self):
        """Get currently inspected container.

        Returns:
            container(Playlist,Subset,Timelime,ContactSheet): Container.
        """

        result = self.connection.request_receive(self.remote, active_media_container_atom())[0]
        c = Container(self.connection, result.actor)

        if c.type == "Timeline":
            return Timeline(self.connection, result.actor, result.uuid)
        elif c.type == "Subset":
            return Subset(self.connection, result.actor, result.uuid)
        elif c.type == "ContactSheet":
            return ContactSheet(self.connection, result.actor, result.uuid)

        return Playlist(self.connection, result.actor, result.uuid)

    @inspected_container.setter
    def inspected_container(self, container):
        """Set the current, *inspected* playlist

        Args:
            container(Playlist,Subset,Timelime,ContactSheet): playlist, subset or timeline(sequence)
        """
        self.connection.send(self.remote, active_media_container_atom(), container.uuid)

    @property
    def path(self):
        """Get session file path.

        Returns:
            path(str): Session path.
        """

        return str(self.connection.request_receive(self.remote, path_atom())[0][0])

    @property
    def media_rate(self):
        """Get new media rate.

        Returns:
            media_rate(core.FrameRate): Media rate to use for rateless media.
        """
        return self.connection.request_receive(self.remote, media_rate_atom())[0]

    @property
    def bookmarks(self):
        """Get bookmarks.

        Returns:
            Bookmarks(): Session bookmarks.
        """
        return Bookmarks(
            self.connection,
            self.connection.request_receive(self.remote, get_bookmark_atom())[0],
            Uuid()
        )

    @media_rate.setter
    def media_rate(self, x):
        """Set media_rate.

        Args:
            media_rate(core.FrameRate): Set media_rate.
        """
        self.connection.send(self.remote, media_rate_atom(), x)

    @property
    def playhead_rate(self):
        """Get playhead rate.

        Returns:
            playhead_rate(core.FrameRate): Rate for new playheads.
        """
        return self.connection.request_receive(self.remote, playhead_rate_atom())[0]

    @playhead_rate.setter
    def playhead_rate(self, x):
        """Set playhead_rate.

        Args:
            playhead_rate(core.FrameRate): Set playhead_rate.
        """
        self.connection.send(self.remote, playhead_rate_atom(), x)

    @property
    def playlists(self):
        """Get playlists.

        Returns:
            playlists(list[Playlist]): Playlists.
        """
        result = self.connection.request_receive(self.remote, get_playlists_atom())[0]
        return [Playlist(self.connection, i.actor, i.uuid) for i in result]


    def create_playlist(self, name="New Playlist", before=Uuid(), into=False):
        """Create new playlist.

        Kwargs:
            name(str): Name of new playlist
            before(Uuid): Insert before this item.
            into(bool): Insert into this container (not used).

        Returns:
            playlist(Uuid,Playlist): Returns container Uuid and Playlist
        """

        if not isinstance(before, Uuid):
            before = before.uuid

        result = self.connection.request_receive(self.remote, add_playlist_atom(), name, before, into)[0]
        return (result[0], Playlist(self.connection, result[1].actor, result[1].uuid))

    def merge_playlist(self, playlists, name="Combined Playlist", before=Uuid(), into=False):
        """Create new playlist.

        Kwargs:
            playlists(list): Playlists to merge
            name(str): Name of new playlist
            before(Uuid): Insert before this item.
            into(bool): Insert into this container (not used).

        Returns:
            playlist(Uuid,Playlist): Returns container Uuid and Playlist
        """
        uuids = []
        for i in playlists:
            if not isinstance(i, Uuid):
                i = i.uuid
            uuids.append(i)

        if not isinstance(before, Uuid):
            before = before.uuid

        result = self.connection.request_receive(self.remote, merge_playlist_atom(), name, before, VectorUuid(uuids))[0]
        return (result[0], Playlist(self.connection, result[1].actor, result[1].uuid))

        # return (result[0], Playlist(self.connection, result[1][1], result[1][0]))

    def create_divider(self, name="New Divider", before=Uuid(), into=False):
        """Create new divider.

        Kwargs:
            name(str): Name of new divider
            before(Uuid): Insert before this item.
            into(bool): Insert into this container (not used).

        Returns:
            divider(PlaylistTree): Returns container.
        """

        if not isinstance(before, Uuid):
            before = before.uuid

        uuid = self.connection.request_receive(self.remote, create_divider_atom(), name, before, into)[0]
        result = self.connection.request_receive(self.remote, get_container_atom(), uuid)[0]
        return PlaylistTree(self.connection, self.remote, result)

    def move_container(self, src, before=Uuid(), into=False):
        """Move container in tree.

        Args:
            src(Container/Uuid): Item to move.

        Kwargs:
            before(Uuid): Insert before this item.
            into(bool): Insert into this container (not used).

        Returns:
            success(bool): Returns result.
        """
        if not isinstance(src, Uuid):
            src = src.uuid
        if not isinstance(before, Uuid):
            before = before.uuid

        return self.connection.request_receive(self.remote, move_container_atom(), src, before, into)[0]

    def remove_container(self, src):
        """Remove container from tree.

        Args:
            src(Container/Uuid): Item to remove.

        Returns:
            success(bool): Returns result.
        """
        if not isinstance(src, Uuid):
            src = src.uuid

        return self.connection.request_receive(self.remote, remove_container_atom(), src)[0]

    def get_container(self, src):
        """Get container from tree.

        Args:
            src(Item/Uuid): Item to find.

        Returns:
            Container(PlaylistTree): Returns result.
        """
        if not isinstance(src, Uuid):
            src = src.uuid

        result = self.connection.request_receive(self.remote, get_container_atom(), src)[0]
        return PlaylistTree(self.connection, self.remote, result)

    def set_on_screen_source(self, src):
        """Set the onscreen source.

        Args:
            src(Container): Playlist, Subset, ContactSheet, Timeline
        """
        self.connection.send(
            self.remote,
            active_media_container_atom(),
            src.uuid_actor().actor
            )

    @property
    def playlist_tree(self):
        """Get container tree.

        Returns:
            Tree(PlaylistTree): Returns result.
        """
        result = self.connection.request_receive(self.remote, get_container_atom())[0]
        return PlaylistTree(self.connection, self.remote, result)

    def create_group(self, name="New Group", before=Uuid()):
        """Create new group.

        Kwargs:
            name(str): Name of new group
            before(Uuid): Insert before this item.

        Returns:
            group(PlaylistTree): Returns container.
        """
        if not isinstance(before, Uuid):
            before = before.uuid

        result = self.connection.request_receive(self.remote, create_group_atom(), name, before)[0]
        return PlaylistTree(self.connection, self.remote, result)

    def rename_container(self, name, container):
        """Rename container in tree.

        Args:
            name(str): New name.
            container(Container/Uuid): Item to rename.

        Returns:
            success(bool): Returns result.
        """
        if not isinstance(container, Uuid):
            container = container.uuid

        return self.connection.request_receive(self.remote, rename_container_atom(), name, container)[0]

    def reflag_container(self, flag, container):
        """Reflag container in tree.

        Args:
            flag(str): New flag.
            container(Container/Uuid): Item to reflag.

        Returns:
            success(bool): Returns result.
        """
        if not isinstance(container, Uuid):
            container = container.uuid

        return self.connection.request_receive(self.remote, reflag_container_atom(), flag, container)[0]

    def copy_containers_to(self, playlist, containers, before=Uuid(), into=False):
        """Create copy containers to playlist.

        Kwargs:
            playlist(Uuid/Playlist): Destination playlist.
            containers(list): Containers to copy
            before(Uuid): Insert before this item.
            into(bool): Insert into this container (not used).

        Returns:
            uuids(list[Uuid]): Returns list of new container uuids.
        """
        uuids = []
        for i in containers:
            if not isinstance(i, Uuid):
                i = i.uuid
            uuids.append(i)

        if not isinstance(before, Uuid):
            before = before.uuid
        if not isinstance(playlist, Uuid):
            playlist = playlist.uuid

        return self.connection.request_receive(self.remote, copy_container_to_atom(), playlist, VectorUuid(uuids), before, into)[0]

    def get_media(self):
        """Return all media over all playlists

        Returns:
            media(list[Media]): List of media objects.
        """
        result = self.connection.request_receive(self.remote, get_media_atom())[0]
        return [Media(self.connection, i.actor, i.uuid) for i in result]

    def save(self):
        """Save session

        Returns:
            int(hash]): Hash of saved session
        """
        return self.connection.request_receive(self.remote, save_atom())[0]

    def save_as(self, path):
        """Save session to path

        Args:
            path(str/uri): Path to save to.

        Returns:
            int(hash]): Hash of saved session
        """
        if not isinstance(path,URI):
            path = URI(path)

        return self.connection.request_receive(self.remote, save_atom(), path)[0]

    def quickview_media(self, media_paths, compare_mode="Grid"):
        """Load media item(s) into a QuickView playlist and show in a QuickView
        window
        Args:
            media_paths(str or list[str] or list[Uri]): media to load
            compare_mode(str): Compare mode where multiple media items are to
            be loaded. Options are "Grid"/"Off"/"A/B"/"Over" etc.
        """

        quickview_playlist = None
        for playlist in self.playlists:
            if playlist.name == "QuickView":
                quickview_playlist = playlist
        if not quickview_playlist:
            quickview_playlist = self.create_playlist("QuickView")[1]

        new_media = []
        if isinstance(media_paths, list):
            for mp in media_paths:
                new_media.append(quickview_playlist.add_media(mp))
        else:
            new_media.append(quickview_playlist.add_media(media_paths))

        from xstudio.core import UuidActorVec, UuidActor, open_quickview_window_atom

        media_actors = UuidActorVec()
        for m in new_media:
            media_actors.push_back(UuidActor(m.uuid, m.remote))

        self.connection.request_receive(
            self.connection.api.app.remote,
            open_quickview_window_atom(),
            media_actors,
            compare_mode)

    def render_container(
        self,
        container,
        output_file_path,
        output_audio_path="",
        width=1920,
        height=1080,
        in_frame=-1,
        out_frame=-1,
        frame_rate=None,
        video_codec_opts="-c:v libx264 -pix_fmt yuv420p -preset medium -tune film",
        audio_codec_opts="-c:a aac -ac 2 -ar 44100",
        auto_check_output=False,
        ocio_display="sRGB",
        ocio_view="Raw",
        timecode="",
        status_callback=None
        ):
        """Render a playlist, timeline, subset or contact sheet to a quicktime 
        or image sequence (jpg, tiff, png etc)
        Args:
            container(Playlist/Timeline/Subset/ContactSheet): The item to render
            output_file_path(str): Filesystem path to the output video file. For 
                frame based image sequences use # characters for frame number and 
                padding to zeros. E.g. /Users/ted/Video/my_render.####.jpg for 4 
                digit padded frame numbers
            output_audio_path(str): Filesystem path to the output audio file. This
                option ONLY applies if you are outputting frame based image sequences.
                Leave empty if you do not want audio in this case. A common audio
                encoding file extension is necessary such as .wav .aiff .ogg etc.
            width(int): Output image format width in pixels
            height(int): Output image format height in pixels
            in_frame(int): Only valid for timelines. Start frame for output render.
                Defaults to first frame
            out_frame(int): Only valid for timelines. End frame for output render.
                Defaults to timeline duration.
            frame_rate(FrameRate): Output video frame rate - defaults to the
                frame rate of the first media item in the playlist etc.
            video_codec_opts(str): FFMpeg video codec options. For example use
                "-c:v libx264 -pix_fmt yuv420p -preset medium -tune film" for an
                H264 encoding.
            audio_codec_opts(str): FFMpeg audio codec options. For example use
                "--c:a aac -ac 2 -ar 44100" for AAC 44.1kHz encoding.
            auto_check_output(bool): Use True to immediately launch a quickview
                window with the render result when it has completed.
            ocio_display(str): The OCIO display profile to apply when generating
                the output
            ocio_view(str): The OCIO view to apply when generating the output
            timecode(str/int): For containerised media a timecode string like 
                "00:00:41:17" will inject timecode data into the output file. For
                frame based media a frame number here will set the start frame
                numbering for the output image file sequence.
            status_callback(function(Uuid, dict)): An optional callback function. This 
                callback will be executed when the status of the render job has
                changed. The status is provided as a dictionary including a 
                'status' field which is set to 'Complete' when the job is
                finished successfully.

        Returns:
            job_id(Uuid): the uuid of the render job. Tjhis
        """
        vid_renderer = self.connection.api.plugin_manager.get_plugin_instance(
            Uuid("4147f82d-1006-4025-ac04-79c81cd5e7b7")
            )

        # set default output frame rate if required
        if not frame_rate:
            if isinstance(container, Timeline):
                frame_rate = container.rate
            else:
                frame_rate = container.media[0].media_source().rate

        # set default in/out frames to timeline duration if required
        if isinstance(container, Timeline) and in_frame == -1:
            in_frame = 0
        if isinstance(container, Timeline) and out_frame == -1:
            out_frame = container.trimmed_range.frame_duration().frames()

        job_id = self.connection.request_receive(
            vid_renderer,
            render_to_video_atom(),
            container.name,
            container.uuid if isinstance(container, Playlist) else container.parent_playlist.uuid if isinstance(container, Timeline) else container.playlist.uuid,
            container.uuid,
            output_file_path,
            output_audio_path,
            width,
            height,
            in_frame,
            out_frame,
            frame_rate if frame_rate != None else container.media[0].media_source().rate,
            video_codec_opts,
            "8",
            audio_codec_opts,
            "Via Python",
            ocio_display,
            ocio_view,
            auto_check_output,
            timecode)[0]

        # A callback has be passed in ... we need to subscribe to messages from
        # the video renderer plugin which will ping us when the render is complete
        # of if it has failed
        if status_callback != None and isinstance(job_id, Uuid):
            self.vid_render_job_callbacks[job_id] = status_callback

            if not self.subscribed_to_vid_renderer:
                # this call gets the event group for the video renderer
                self.subscribed_to_vid_renderer = True

                self.connection.link.add_message_callback(
                    vid_renderer, self.__vid_renderer_events
                    )

        return job_id

    def __vid_renderer_events(self, event_message_content):

        if len(event_message_content) == 4 and isinstance(event_message_content[0], event_atom):
            if event_message_content[1] in self.vid_render_job_callbacks:
                self.vid_render_job_callbacks[event_message_content[1]](
                    event_message_content[1],
                    event_message_content[2]
                    )
