// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>

#include "xstudio/ui/qml/model_data_ui.hpp"


CAF_PUSH_WARNINGS
#include <QThreadPool>
#include <QFutureWatcher>
#include <QtConcurrent>
CAF_POP_WARNINGS

namespace xstudio::ui::qml {
using namespace caf;

class HELPER_QML_EXPORT ModulesModelData : public UIModelData {

    Q_OBJECT

  public:
    Q_PROPERTY(QString singleAttributeName READ singleAttributeName WRITE setSingleAttributeName
                   NOTIFY singleAttributeNameChanged)

    explicit ModulesModelData(QObject *parent = nullptr);

    QString singleAttributeName() { return single_attr_name_; }

    Q_INVOKABLE void setSingleAttributeName(const QString single_attr_name);

    Q_INVOKABLE QVariant
    attributeRoleData(const QString attr_name, const QString role_name = QString("value"));

    void setModelData(const nlohmann::json &data) override;

  signals:

    void singleAttributeNameChanged();

  private:
    QString single_attr_name_;
};

} // namespace xstudio::ui::qml