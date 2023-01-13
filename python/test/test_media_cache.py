# SPDX-License-Identifier: Apache-2.0
import xstudio

def test_image_cache(spawn):
    assert isinstance(spawn.api.image_cache, xstudio.api.intrinsic.MediaCache)

def test_audio_cache(spawn):
    assert isinstance(spawn.api.audio_cache, xstudio.api.intrinsic.MediaCache)

def test_audio_count(spawn):
    assert spawn.api.audio_cache.count == 0

def test_image_count(spawn):
    assert spawn.api.image_cache.count == 0

def test_audio_size(spawn):
    assert spawn.api.audio_cache.size == 0

def test_image_size(spawn):
    assert spawn.api.image_cache.size == 0
