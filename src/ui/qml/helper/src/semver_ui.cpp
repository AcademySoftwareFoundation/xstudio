// SPDX-License-Identifier: Apache-2.0
#include "xstudio/ui/qml/helper_ui.hpp"

using namespace xstudio::ui::qml;

SemVer::SemVer(const QString &version, QObject *parent) : QObject(parent) {
    setVersion(version);
}

void SemVer::setVersion(const QString &version) {
    if (version_.from_string_noexcept(StdFromQString(version)))
        emit versionChanged();
}

int SemVer::compare(const QString &other) const {
    return version_.compare(*semver::from_string_noexcept(StdFromQString(other)));
}

int SemVer::compare(const SemVer &other) const { return version_.compare(other.version_); }
