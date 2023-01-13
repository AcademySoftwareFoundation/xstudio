# SPDX-License-Identifier: Apache-2.0
import xstudio

def test_global_store(spawn):
    assert isinstance(spawn.api.global_store, xstudio.api.intrinsic.GlobalStore)


