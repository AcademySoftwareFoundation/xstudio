# SPDX-License-Identifier: Apache-2.0
try:
    from .scene_cut_detector_plugin import create_plugin_instance
except ImportError:
    # The 'xstudio' package is only available inside xSTUDIO's embedded
    # Python environment, not when running the unit tests in test/
    # standalone. Importing this package must not fail in that case.
    create_plugin_instance = None

