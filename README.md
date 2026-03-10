# Welcome to xSTUDIO 1.1.0

xSTUDIO is a media playback and review application designed for professionals working in the film and TV post production industries, particularly the Visual Effects and Feature Animation sectors. xSTUDIO is focused on providing an intuitive, easy to use interface with a high performance playback engine at its core and C++ and Python APIs for pipeline integration and customisation for total flexibility.

This codebase builds xSTUDIO `v1.1.0`. The top-level documentation has been refreshed to match the current source tree and is split between the user guide in `docs/user_docs` and the developer reference in `docs/reference`.

There are still some known issues that are currently being worked on:

* Moderate audio distortion on playback (Windows only)
* Saved sessions might not restore media correctly (Windows only)

## Building xSTUDIO

This release of xSTUDIO can be built on various Linux distributions, Microsoft Windows, and macOS. We provide comprehensive build steps here.

### Building xSTUDIO for Linux

* [Linux Generic](docs/reference/build_guides/linux_generic.md)
* [CentOS 7](docs/reference/build_guides/centos_7.md)
* [Rocky Linux 9.1](docs/reference/build_guides/rocky_linux_9_1.md)
* [Ubuntu 22.04](docs/reference/build_guides/ubuntu_22_04.md)

### Building xSTUDIO for Windows 10 & 11

* [Windows](docs/reference/build_guides/windows.md)

### Building xSTUDIO for macOS

* [macOS](docs/reference/build_guides/macos.md)

### Documentation

xSTUDIO's documentation is built with Sphinx and Doxygen. The source lives under `docs/`, with end-user workflows in `docs/user_docs` and build/API reference material in `docs/reference`. The standard build keeps documentation disabled by default; enable it with `-DBUILD_DOCS=ON` if you want to generate the HTML site locally.

## Presets and Baselines

The repository now ships platform-specific `dev`, `ci`, and `perf` CMake presets without hardcoded local filesystem paths. Set `VCPKG_ROOT` and `Qt6_DIR` in your environment as needed, then list the available presets with:

```bash
cmake --list-presets
```

For repeatable JSON timing output, use the perf baseline helper:

```bash
python3 scripts/perf/baseline.py --output build/perf-baseline.json --command tests:"ctest --output-on-failure"
```
