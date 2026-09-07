# SPDX-License-Identifier: Apache-2.0
import pytest
from xstudio.core import version_atom


def test_response_already_received_is_not_a_timeout(spawn):
    """A response dequeued by another consumer must be returned, not reported
    as a timeout.

    _dequeue_messages files every response it dequeues into self.responses,
    whatever request it was watching for - only its break is specific to
    watch_for. So any other consumer of the queue can pull our response and
    store it correctly while we are still waiting for it.

    dequeue_messages() here is that other consumer. Calling it on this thread
    rather than another removes the race without changing the mechanism: it
    pumps with watch_for unset, so it files the response and carries on instead
    of breaking on it.
    """
    req_id = spawn.request(spawn.remote(), version_atom())
    spawn.dequeue_messages(300)

    assert spawn.responses[req_id] is not None, "precondition: the answer was recorded"
    assert spawn.response(req_id, 300) is not None


def test_genuine_timeout_still_raises(spawn):
    """No response was ever recorded for this id, so it must still raise."""
    unused_req_id = 0x7FFFFFF0
    assert unused_req_id not in spawn.responses

    with pytest.raises(TimeoutError):
        spawn.response(unused_req_id, 300)
