# SPDX-License-Identifier: Apache-2.0
import xstudio

def test_type(spawn):
    assert spawn.app_type == spawn.APP_TYPE_XSTUDIO

def test_api_session(spawn):
    assert isinstance(spawn.api.session, xstudio.api.session.Session)

def test_app_version(spawn):
    assert isinstance(spawn.app_version, str)
