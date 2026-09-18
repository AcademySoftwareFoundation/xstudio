# SPDX-License-Identifier: Apache-2.0
import json

from xstudio.api.session.container import Container
from xstudio.core import bookmark_detail_atom, BookmarkDetail, add_annotation_atom, serialise_atom, JsonStore
from xstudio.api.session.media import Media

# This is defined in annotations_core_plugin.hpp 
ANNOTATIONS_PLUGIN_UUID = "46f386a0-cb9a-4820-8e99-fb53f6c019eb"
_ANNO_SERIALISER_VERSION = (3 << 8)  # v3.0


class Bookmark(Container):
    """Bookmark object."""

    def __init__(self, connection, remote, uuid=None):
        """Create Bookmark object.

        Args:
            connection(Connection): Connection object
            remote(actor): Remote actor object

        Kwargs:
            uuid(Uuid): Uuid of remote actor.
        """
        Container.__init__(self, connection, remote, uuid)

    @property
    def detail(self):
        """Bookmark detail.

        Returns:
            result(BookmarkDetail): The bookmark's detail 
        """
        return self.connection.request_receive(self.remote, bookmark_detail_atom())[0]

    @property
    def media(self):
        """Get the Media object to which the bookmark is 
        attached.

        Returns:
            result(Media): The bookmark's detail 
        """
        result = None

        if self.detail.owner and self.detail.owner.actor:            
            result = Media(self.connection, self.detail.owner.actor, self.detail.owner.uuid)

        return result

    @property
    def enabled(self):
        """Is enabled.

        Returns:
            result(bool): Enabled ?
        """
        return self.connection.request_receive(self.remote, bookmark_detail_atom())[0].enabled

    @enabled.setter
    def enabled(self, x):
        detail = BookmarkDetail()
        detail.enabled = x
        self.connection.request_receive(self.remote, bookmark_detail_atom(), detail)

    @property
    def has_focus(self):
        """Is enabled.

        Returns:
            result(bool): Enabled ?
        """

        return self.connection.request_receive(self.remote, bookmark_detail_atom())[0].has_focus

    @has_focus.setter
    def has_focus(self, x):
        detail = BookmarkDetail()
        detail.has_focus = x
        self.connection.request_receive(self.remote, bookmark_detail_atom(), detail)

    @property
    def start(self):
        """Is enabled.

        Returns:
            result(bool): Enabled ?
        """

        return self.connection.request_receive(self.remote, bookmark_detail_atom())[0].start

    @start.setter
    def start(self, x):
        detail = BookmarkDetail()
        detail.start = x
        self.connection.request_receive(self.remote, bookmark_detail_atom(), detail)

    @property
    def duration(self):
        """Is enabled.

        Returns:
            result(bool): Enabled ?
        """

        return self.connection.request_receive(self.remote, bookmark_detail_atom())[0].duration

    @duration.setter
    def duration(self, x):
        detail = BookmarkDetail()
        detail.duration = x
        self.connection.request_receive(self.remote, bookmark_detail_atom(), detail)


    @property
    def user_data(self):
        """Is enabled.

        Returns:
            result(bool): Enabled ?
        """

        return self.connection.request_receive(self.remote, bookmark_detail_atom())[0].user_data

    @user_data.setter
    def user_data(self, x):
        detail = BookmarkDetail()
        detail.user_data = x
        self.connection.request_receive(self.remote, bookmark_detail_atom(), detail)

    @property
    def author(self):
        """Is enabled.

        Returns:
            result(bool): Enabled ?
        """

        return self.connection.request_receive(self.remote, bookmark_detail_atom())[0].author

    @author.setter
    def author(self, x):
        detail = BookmarkDetail()
        detail.author = x
        self.connection.request_receive(self.remote, bookmark_detail_atom(), detail)

    @property
    def subject(self):
        """Is enabled.

        Returns:
            result(bool): Enabled ?
        """

        return self.connection.request_receive(self.remote, bookmark_detail_atom())[0].subject

    @subject.setter
    def subject(self, x):
        detail = BookmarkDetail()
        detail.subject = x
        self.connection.request_receive(self.remote, bookmark_detail_atom(), detail)

    @property
    def category(self):
        """Is enabled.

        Returns:
            result(bool): Enabled ?
        """

        return self.connection.request_receive(self.remote, bookmark_detail_atom())[0].category

    @category.setter
    def category(self, x):
        detail = BookmarkDetail()
        detail.category = x
        self.connection.request_receive(self.remote, bookmark_detail_atom(), detail)

    @property
    def colour(self):
        """Is enabled.

        Returns:
            result(bool): Enabled ?
        """

        return self.connection.request_receive(self.remote, bookmark_detail_atom())[0].colour

    @colour.setter
    def colour(self, x):
        detail = BookmarkDetail()
        detail.colour = x
        self.connection.request_receive(self.remote, bookmark_detail_atom(), detail)

    @property
    def note(self):
        """Is enabled.

        Returns:
            result(bool): Enabled ?
        """
        return self.connection.request_receive(self.remote, bookmark_detail_atom())[0].note

    @note.setter
    def note(self, x):
        detail = BookmarkDetail()
        detail.note = x
        self.connection.request_receive(self.remote, bookmark_detail_atom(), detail)

    @property
    def created(self):
        """Is enabled.

        Returns:
            result(bool): Enabled ?
        """

        return self.connection.request_receive(self.remote, bookmark_detail_atom())[0].created

    @created.setter
    def created(self, x):
        detail = BookmarkDetail()
        detail.created = x
        self.connection.request_receive(self.remote, bookmark_detail_atom(), detail)

    @property
    def has_annotation(self):
        """Whether this bookmark has annotation drawing data.

        Returns:
            result(bool): True if annotation data exists.
        """
        return self.connection.request_receive(self.remote, bookmark_detail_atom())[0].has_annotation

    @property
    def annotation_data(self):
        """Raw annotation data as a dict, or None if no annotation exists.

        Reads the annotation from the bookmark's serialised JSON.  This works
        even without ``add_annotation_atom`` being called, so it is suitable
        for read-only inspection.

        Returns:
            result(dict | None): Canvas data under the ``"Data"`` key, or None.
        """
        try:
            raw = self.connection.request_receive(self.remote, serialise_atom())[0]
            bm = json.loads(raw.dump())
            return bm.get("base", {}).get("annotation")
        except Exception:
            return None

    def set_annotation(self, strokes=None, captions=None, quads=None, polygons=None, ellipses=None):
        """Set annotation drawing data on this bookmark.

        All geometry uses normalised image-space coordinates (roughly -1..1 on
        the short axis, Y-up).

        Args:
            strokes (list[dict] | None): Pen stroke dicts.  Each dict must
                contain:
                  - ``"points"`` – flat list [x, y, size_pressure,
                    opacity_pressure, ...] (size/opacity pressure both 1.0 is
                    a good default)
                  - ``"r"``, ``"g"``, ``"b"`` – colour (0.0–1.0)
                  - ``"opacity"`` (0.0–1.0)
                  - ``"thickness"`` – line width in image-space units (~0.003–0.02)
                  - ``"softness"`` (0.0–1.0, default 0.0)
                  - ``"size_sensitivity"`` (default 1.0)
                  - ``"opacity_sensitivity"`` (default 1.0)
                  - ``"is_erase_stroke"`` (bool, default False)
            captions (list[dict] | None): Text annotation dicts.
            quads (list[dict] | None): Quad shape dicts.
            polygons (list[dict] | None): Polygon shape dicts.
            ellipses (list[dict] | None): Ellipse shape dicts.
        """

        canvas = {
            "pen_strokes": strokes or [],
            "captions": captions or [],
            "quads": quads or [],
            "polygons": polygons or [],
            "ellipses": ellipses or [],
        }
        anno_json = {
            "plugin_uuid": ANNOTATIONS_PLUGIN_UUID,
            "Annotation Serialiser Version": _ANNO_SERIALISER_VERSION,
            "Data": canvas,
        }
        js = JsonStore()
        js.parse_string(json.dumps(anno_json))
        self.connection.request_receive(self.remote, add_annotation_atom(), js)
