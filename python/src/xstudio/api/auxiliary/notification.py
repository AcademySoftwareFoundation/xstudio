# SPDX-License-Identifier: Apache-2.0
from xstudio.core import notification_atom, Notification
import json

class NotificationPy():
    """Wrapper for Notifaction"""
    def __init__(self, actorconnection, notification):
        """Create NotificationHandler object.

        Args:
            actorconnection(ActorConnection): Connection object

        """

        self._actor_connection = actorconnection
        self._notification = notification

    @property
    def type(self):
        return self._notification.type()

    @type.setter
    def type(self, value):
        self._notification.set_type(ntype)

    @property
    def text(self):
        return self._notification.text()

    @text.setter
    def text(self, value):
        self._notification.set_text(value)

    @property
    def uuid(self):
        return self._notification.uuid()

    @uuid.setter
    def uuid(self, value):
        self._notification.set_uuid(value)

    @property
    def progress(self):
        return self._notification.progress()

    @progress.setter
    def progress(self, value):
        self._notification.set_progress(value)

    @property
    def progress_maximum(self):
        return self._notification.progress_maximum()

    @progress_maximum.setter
    def progress_maximum(self, value):
        self._notification.set_progress_maximum(value)

    @property
    def progress_minimum(self):
        return self._notification.progress_minimum()

    @progress_minimum.setter
    def progress_minimum(self, value):
        self._notification.set_progress_minimum(value)

    @property
    def progress_percentage(self):
        return self._notification.progress_percentage()

    @property
    def progress_text_percentage(self):
        return self._notification.progress_text_percentage()

    @property
    def progress_text_range(self):
        return self._notification.progress_text_range()

    def remove(self):
        """Remove Notification"""
        return self._actor_connection.connection.request_receive(self._actor_connection.remote, notification_atom(), self.uuid)[0]

    def push_update(self):
        """Push changes"""
        return self._actor_connection.connection.request_receive(self._actor_connection.remote, notification_atom(), self._notification)[0]

    def push_progress(self, progress):
        """Push changes"""
        return self._actor_connection.connection.request_receive(self._actor_connection.remote, notification_atom(), self.uuid, progress)[0]


class NotificationHandler():
    """Access and create notification events"""
    def __init__(self, actorconnection):
        """Create NotificationHandler object.

        Args:
            actorconnection(ActorConnection): Connection object

        """

        self._actor_connection = actorconnection

    @property
    def notification_digest(self):
        return json.loads(self._actor_connection.connection.request_receive(self._actor_connection.remote, notification_atom())[0].dump())

    @property
    def notifications(self):
        result = []
        # turn into array of NotificationPy

        tmp = self._actor_connection.connection.request_receive(self._actor_connection.remote, notification_atom(), notification_atom())[0]
        for i in tmp:
            result.append(NotificationPy(self._actor_connection, i))

        return result

    def createInfoNotification(self, text, expiresSeconds=10):
        """Create Info Notification.

        Args:
            text(str): Text of notification
            expiresSeconds(int): Expiration in seconds.

        Returns:
            notification(NotificationPy): Notification  created
        """

        result = Notification.InfoNotification(text, expiresSeconds)
        self._actor_connection.connection.request_receive(self._actor_connection.remote, notification_atom(), result)
        return NotificationPy(self._actor_connection, result)

    def createWarnNotification(self, text, expiresSeconds=10):
        """Create Info Notification.

        Args:
            text(str): Text of notification
            expiresSeconds(int): Expiration in seconds.

        Returns:
            notification(NotificationPy): Notification  created
        """

        result = Notification.WarnNotification(text, expiresSeconds)
        self._actor_connection.connection.request_receive(self._actor_connection.remote, notification_atom(), result)
        return NotificationPy(self._actor_connection, result)

    def createProcessingNotification(self, text):
        """Create Info Notification.

        Args:
            text(str): Text of notification
            expiresSeconds(int): Expiration in seconds.

        Returns:
            notification(NotificationPy): Notification  created
        """

        result = Notification.ProcessingNotification(text)
        self._actor_connection.connection.request_receive(self._actor_connection.remote, notification_atom(), result)
        return NotificationPy(self._actor_connection, result)

    def createProgressPercentageNotification(self, text, progress=0.0, expiresSeconds=600):
        """Create Info Notification.

        Args:
            text(str): Text of notification
            expiresSeconds(int): Expiration in seconds.

        Returns:
            notification(NotificationPy): Notification  created
        """

        result = Notification.ProgressPercentageNotification(text, progress, expiresSeconds)
        self._actor_connection.connection.request_receive(self._actor_connection.remote, notification_atom(), result)
        return NotificationPy(self._actor_connection, result)

    def createProgressRangeNotification(self, text, progress=0.0, progress_min=0.0, progress_max=0.0, expiresSeconds=600):
        """Create Info Notification.

        Args:
            text(str): Text of notification
            expiresSeconds(int): Expiration in seconds.

        Returns:
            notification(NotificationPy): Notification  created
        """

        result = Notification.ProgressRangeNotification(text, progress, progress_min, progress_max, expiresSeconds)
        self._actor_connection.connection.request_receive(self._actor_connection.remote, notification_atom(), result)
        return NotificationPy(self._actor_connection, result)














