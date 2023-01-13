#!/bin/env python
# SPDX-License-Identifier: Apache-2.0

from xstudio.connection import Connection

def clear_thumbnail_cache(api):
    """Clear thumbnail cache.
    """

    return api.thumbnail.clear(True, True)

if __name__ == "__main__":
    XSTUDIO = Connection(auto_connect=True)
    clear_thumbnail_cache(XSTUDIO.api)