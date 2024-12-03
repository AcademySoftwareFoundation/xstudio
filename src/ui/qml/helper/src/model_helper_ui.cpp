#include "xstudio/ui/qml/helper_ui.hpp"
#include "xstudio/utility/helpers.hpp"

using namespace xstudio::ui::qml;

#include <QJSValue>
#include <QItemSelectionRange>
#include <QVector4D>

void ModelRowCount::setCount(const int count) {
    if (count != count_) {
        count_ = count;
        emit countChanged();
    }
}

void ModelRowCount::inserted(const QModelIndex &parent, int first, int last) {
    if (index_.isValid() and parent == index_) {
        setCount(count_ + ((last - first) + 1));
    }
}

void ModelRowCount::moved(
    const QModelIndex &sourceParent,
    int sourceStart,
    int sourceEnd,
    const QModelIndex &destinationParent,
    int destinationRow) {
    if (index_.isValid()) {
        if (sourceParent == destinationParent) {
        } else if (sourceParent == index_) {
            setCount(count_ - ((sourceEnd - sourceStart) + 1));
        } else if (destinationParent == index_) {
            setCount(count_ + ((sourceEnd - sourceStart) + 1));
        }
    }
}

void ModelRowCount::removed(const QModelIndex &parent, int first, int last) {
    if (index_.isValid() and parent == index_) {
        setCount(count_ - ((last - first) + 1));
    }
}


void ModelRowCount::setIndex(const QModelIndex &index) {
    if (index.isValid()) {
        if (index_.isValid()) {
            disconnect(
                index_.model(),
                &QAbstractItemModel::rowsRemoved,
                this,
                &ModelRowCount::removed);
            disconnect(
                index_.model(),
                &QAbstractItemModel::rowsInserted,
                this,
                &ModelRowCount::inserted);
            disconnect(
                index_.model(), &QAbstractItemModel::rowsMoved, this, &ModelRowCount::moved);
        }

        connect(index.model(), &QAbstractItemModel::rowsRemoved, this, &ModelRowCount::removed);
        connect(
            index.model(), &QAbstractItemModel::rowsInserted, this, &ModelRowCount::inserted);
        connect(index.model(), &QAbstractItemModel::rowsMoved, this, &ModelRowCount::moved);

        index_ = QPersistentModelIndex(index);
        emit indexChanged();

        setCount(index_.model()->rowCount(index_));
    } else {
        index_ = QPersistentModelIndex(index);
        emit indexChanged();
        setCount(0);
    }
}


void ModelProperty::setIndex(const QModelIndex &index) {
    if (index.isValid()) {
        if (index_.isValid()) {
            disconnect(
                index_.model(),
                &QAbstractItemModel::dataChanged,
                this,
                &ModelProperty::dataChanged);
            disconnect(
                index_.model(),
                &QAbstractItemModel::rowsRemoved,
                this,
                &ModelProperty::removed);
        }

        connect(
            index.model(), &QAbstractItemModel::dataChanged, this, &ModelProperty::dataChanged);

        connect(index.model(), &QAbstractItemModel::rowsRemoved, this, &ModelProperty::removed);

        index_ = QPersistentModelIndex(index);
        emit indexChanged();
        setRole(role_);
        if (updateValue())
            emit valueChanged();
    } else {
        index_ = QPersistentModelIndex(index);
        emit indexChanged();
        value_ = QVariant();
        emit valueChanged();
    }
}

void ModelProperty::removed(const QModelIndex &parent, int first, int last) {
    if (not index_.isValid()) {
        setIndex(parent.model()->index(-1, -1, parent));
    }
}

void ModelProperty::dataChanged(
    const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles) {
    if (index_.isValid() and QItemSelectionRange(topLeft, bottomRight).contains(index_) and
        (roles.empty() or roles.contains(role_id_)) and updateValue())
        emit valueChanged();
}

bool ModelProperty::updateValue() {
    bool result = false;
    auto v      = QVariant();

    if (index_.isValid())
        v = index_.data(role_id_);

    if (v != value_) {
        value_ = v;
        result = true;
    }

    return result;
}

void ModelProperty::setRole(const QString &role) {
    if (role != role_) {
        role_ = role;
        emit roleChanged();
    }

    auto id = getRoleId(role_);
    if (role_id_ != id) {
        role_id_ = id;
        emit roleIdChanged();

        if (updateValue())
            emit valueChanged();
    }
}

int ModelProperty::getRoleId(const QString &role) const {
    auto result = -1;

    // valid index, resolve role from model.
    if (index_.isValid() and index_.model()) {
        QHashIterator<int, QByteArray> it(index_.model()->roleNames());
        if (it.findNext(role.toUtf8()))
            result = it.key();
    }

    return result;
}

void ModelProperty::setValue(const QVariant &value) {
    if (value != value_ and index_.isValid() and index_.model() and
        index_.data(role_id_) != value) {
        // this maybe be bad!
        const_cast<QAbstractItemModel *>(index_.model())->setData(index_, value, role_id_);
    }
}

void PreferencePropertyMap::setMyValue(const QVariant &value) {
    if (values_->value("valueRole") != value) {
        values_->setProperty("valueRole", value);
        emit myValueChanged();
    }
}

void PreferencePropertyMap::valueChanged(const QString &key, const QVariant &value) {
    ModelPropertyMap::valueChanged(key, value);
    emitChange(key);
}

void PreferencePropertyMap::emitChange(const QString &key) {
    if (key == "valueRole")
        emit myValueChanged();
    else if (key == "datatypeRole")
        emit dataTypeChanged();
    else if (key == "contextRole")
        emit contextChanged();
    else if (key == "nameRole")
        emit nameChanged();
    else if (key == "defaultValueRole")
        emit defaultValueChanged();
    else if (key == "jsonTextRole")
        emit jsonStringChanged();
}

ModelPropertyMap::ModelPropertyMap(QObject *parent) : QObject(parent) {
    values_ = new QQmlPropertyMap(this);
    connect(values_, &QQmlPropertyMap::valueChanged, this, &ModelPropertyMap::valueChangedSlot);
}

void ModelPropertyMap::setIndex(const QModelIndex &index) {

    if (index != index_) {
        auto model_change = true;

        if (index.isValid()) {
            if (index_.isValid()) {
                if (index.model() == index_.model()) {
                    model_change = false;
                } else {
                    disconnect(
                        index_.model(),
                        &QAbstractItemModel::dataChanged,
                        this,
                        &ModelPropertyMap::dataChanged);
                }
            }

            if (model_change) {
                connect(
                    index.model(),
                    &QAbstractItemModel::dataChanged,
                    this,
                    &ModelPropertyMap::dataChanged);

                // clear old model
                // values_->clear();
                for (const auto &i : index.model()->roleNames())
                    values_->insert(QString(i), QVariant());
            }

            index_ = QPersistentModelIndex(index);

            updateValues();

            emit indexChanged();

            if (model_change) {
                emit valuesChanged();
                emit contentChanged();
            }
        } else {
            index_ = QPersistentModelIndex(index);
            updateValues();
            emit indexChanged();
            emit valuesChanged();
            emit contentChanged();
        }
    } else if (not index.isValid()) {
        // force update as will auto become invalid
        index_ = QPersistentModelIndex(index);
        updateValues();
        emit indexChanged();
        emit valuesChanged();
        emit contentChanged();
    }
}

void ModelPropertyMap::dump() {

    if (index_.isValid()) {
        auto hash = index_.model()->roleNames();

        QHash<int, QByteArray>::const_iterator i = hash.constBegin();
        while (i != hash.constEnd()) {
            const auto role   = i.key();
            auto model_value  = index_.data(role);
            auto propery_name = QString(i.value());
            qDebug() << propery_name << " " << model_value << "\n";
            ++i;
        }
    }
}

bool ModelPropertyMap::updateValues(const QVector<int> &roles) {
    auto changed = false;
    if (index_.isValid()) {
        auto hash = index_.model()->roleNames();

        QHash<int, QByteArray>::const_iterator i = hash.constBegin();
        while (i != hash.constEnd()) {
            const auto role = i.key();
            if (roles.empty() or roles.contains(role)) {
                auto model_value  = index_.data(role);
                auto propery_name = QString(i.value());

                if (model_value != (*values_)[propery_name]) {
                    values_->setProperty(StdFromQString(propery_name).c_str(), model_value);
                    changed = true;
                }
            }
            ++i;
        }
    } else {
        // clear values.
        for (const auto &i : values_->keys()) {
            values_->setProperty(StdFromQString(i).c_str(), QVariant());
        }
        changed = true;
    }

    return changed;
}

void ModelPropertyMap::dataChanged(
    const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles) {
    if (index_.isValid() and QItemSelectionRange(topLeft, bottomRight).contains(index_)) {
        if (updateValues(roles))
            emit contentChanged();
    }
}

void ModelPropertyMap::valueChangedSlot(const QString &key, const QVariant &value) {
    valueChanged(key, value);
}

void ModelPropertyMap::valueChanged(const QString &key, const QVariant &value) {
    // spdlog::warn("ModelPropertyMap::valueChanged {} {} update ? {} ", StdFromQString(key),
    // mapFromValue(value).dump(2), values_->value(key) != value); if (values_->value(key) !=
    // value) { propate to backend. find id..
    auto id = getRoleId(key);
    if (id != -1 and index_.isValid() and index_.model() and index_.data(id) != value) {
        // spdlog::warn(
        //     "ModelPropertyMap update model {} {}", StdFromQString(key),
        //     mapFromValue(value).dump(2));
        const_cast<QAbstractItemModel *>(index_.model())->setData(index_, value, id);
    }
    // }
}

int ModelPropertyMap::getRoleId(const QString &role) const {
    auto result = -1;

    // valid index, resolve role from model.
    if (index_.isValid() and index_.model()) {
        QHashIterator<int, QByteArray> it(index_.model()->roleNames());
        if (it.findNext(role.toUtf8()))
            result = it.key();
    }

    return result;
}


// change from backend.
void ModelNestedPropertyMap::valueChanged(const QString &key, const QVariant &value) {
    auto id = getRoleId(key);

    if (index_.isValid()) {
        if (id != -1) {
            if (index_.data(id) != value)
                const_cast<QAbstractItemModel *>(index_.model())->setData(index_, value, id);
        } else {
            id          = getRoleId(data_role_);
            auto jvalue = mapFromValue(index_.data(id));
            auto skey   = StdFromQString(key);
            auto svalue = mapFromValue(value);
            if (jvalue.is_object() and (not jvalue.count(skey) or svalue != jvalue.at(skey))) {
                jvalue[skey] = svalue;
                const_cast<QAbstractItemModel *>(index_.model())
                    ->setData(index_, mapFromValue(jvalue), id);
            }
        }
    }
}

// change from frontend.
bool ModelNestedPropertyMap::updateValues(const QVector<int> &roles) {
    // populate QMLPropertyMap from value.
    if (index_.isValid()) {
        auto jvalue = mapFromValue(index_.data(getRoleId(data_role_)));

        std::set<std::string> mapped;

        if (jvalue.is_object()) {
            for (const auto &[k, v] : jvalue.items()) {
                auto qk = QStringFromStd(k);
                auto qv = mapFromValue(v);

                mapped.insert(k);

                if (values_->contains(qk) and qv != (*values_)[qk])
                    values_->setProperty(k.c_str(), qv);
                else
                    values_->insert(qk, qv);
            }
        }

        // add any missing defaults
        auto dvalue = mapFromValue(index_.data(getRoleId(default_role_)));
        if (dvalue.is_object()) {
            for (const auto &[k, v] : dvalue.items()) {
                auto qk = QStringFromStd(k);
                auto qv = mapFromValue(v);

                if (not mapped.count(k)) {
                    if (values_->contains(qk) and qv != (*values_)[qk])
                        values_->setProperty(k.c_str(), qv);
                    else
                        values_->insert(qk, qv);
                }
            }
        }
    }
    return true;
}


QVariant xstudio::ui::qml::mapFromValue(const nlohmann::json &value) {
    auto result = QVariant();

    if (value.is_boolean())
        result = QVariant::fromValue(value.get<bool>());
    else if (value.is_number_integer())
        result = QVariant::fromValue(value.get<long long>());
    else if (value.is_number_unsigned())
        result = QVariant::fromValue(value.get<unsigned int>());
    else if (value.is_number_float())
        result = QVariant::fromValue(value.get<float>());
    else if (value.is_string())
        result = QVariant::fromValue(QStringFromStd(value.get<std::string>()));
    else if (value.is_array()) {
        if (value.size() == 5 && value[0].is_string() &&
            value[0].get<std::string>() == "colour") {

            // it should be a color!
            const auto t = value.get<utility::ColourTriplet>();
            QColor c(
                static_cast<int>(round(t.r * 255.0f)),
                static_cast<int>(round(t.g * 255.0f)),
                static_cast<int>(round(t.b * 255.0f)));
            result = QVariant(c);
        } else if (
            value.size() == 6 && value[0].is_string() &&
            value[0].get<std::string>() == "vec4") {

            // it should be a color!
            const auto t = value.get<Imath::V4f>();
            QVector4D rt;
            rt[0] = t[0];
            rt[1] = t[1];
            rt[2] = t[2];
            rt[3] = t[3];
            return QVariant(rt);
        } else {
            result = QVariantListFromJson(utility::JsonStore(value));
        }
    } else if (value.is_object()) {
        QVariantMap r;
        for (auto p = value.begin(); p != value.end(); ++p) {
            r[QStringFromStd(p.key())] = mapFromValue(p.value());
        }
        result = r;
    }

    return result;
}

nlohmann::json xstudio::ui::qml::mapFromValue(const QVariant &value) {
    nlohmann::json result;

    if (value.userType() == qMetaTypeId<QJSValue>()) {
        QVariant v = qvariant_cast<QJSValue>(value).toVariant();

        switch (static_cast<int>(v.type())) {
        case QMetaType::QVariantMap:
            result = nlohmann::json::parse(
                QJsonDocument(v.toJsonObject()).toJson(QJsonDocument::Compact).constData());
            break;

        case QMetaType::QVariantList:
            result = nlohmann::json::parse(
                QJsonDocument(v.toJsonArray()).toJson(QJsonDocument::Compact).constData());
            break;

        case QMetaType::QColor: {
            auto c = v.value<QColor>();
            result = nlohmann::json(utility::ColourTriplet(
                float(c.red()) / 255.0f, float(c.green()) / 255.0f, float(c.blue()) / 255.0f));
        } break;

        case QMetaType::QVector4D: {
            auto c = v.value<QVector4D>();
            result = nlohmann::json(Imath::V4f(c[0], c[1], c[2], c[3]));
        } break;

        default:
            spdlog::warn("1 Unsupported datatype {} {}", v.type(), v.typeName());
            break;
        }

    } else {

        switch (value.userType()) {

        case QMetaType::Nullptr:
            result = nullptr;
            break;
        case QMetaType::Bool:
            result = value.toBool();
            break;
        case QMetaType::Double:
            result = value.toDouble();
            break;
        case QMetaType::Float:
            result = value.toDouble();
            break;
        case QMetaType::Int:
            result = value.toInt();
            break;
        case QMetaType::ULong:
            result = value.toULongLong();
            break;
        case QMetaType::QUrl:
            result = to_string(UriFromQUrl(value.toUrl()));
            break;
        case QMetaType::QUuid:
            result = UuidFromQUuid(value.toUuid());
            break;
        case QMetaType::LongLong:
            result = value.toLongLong();
            break;
        case QMetaType::QString:
            result = StdFromQString(value.toString());
            break;

        case QMetaType::QVariantMap:
            result = nlohmann::json::parse(
                QJsonDocument(value.toJsonObject()).toJson(QJsonDocument::Compact).constData());
            break;

        case QMetaType::QStringList:
            result = nlohmann::json::parse(
                QJsonDocument(value.toJsonArray()).toJson(QJsonDocument::Compact).constData());
            break;

        case QMetaType::QVariantList:
            result = nlohmann::json::parse(
                QJsonDocument(value.toJsonArray()).toJson(QJsonDocument::Compact).constData());
            break;

        case QMetaType::QColor: {
            auto c = value.value<QColor>();
            result = nlohmann::json(utility::ColourTriplet(
                float(c.red()) / 255.0f, float(c.green()) / 255.0f, float(c.blue()) / 255.0f));
        } break;

        case QMetaType::QVector4D: {
            auto c = value.value<QVector4D>();
            result = nlohmann::json(Imath::V4f(c[0], c[1], c[2], c[3]));
        } break;

        case QMetaType::QJsonDocument: {
            QVariant v = value;
            if (value.userType() == qMetaTypeId<QJSValue>())
                v = qvariant_cast<QJSValue>(value).toVariant();

            switch (static_cast<int>(v.type())) {
            case QMetaType::QVariantMap:
                result = nlohmann::json::parse(
                    QJsonDocument(v.toJsonObject()).toJson(QJsonDocument::Compact).constData());
                break;

            case QMetaType::QVariantList:
                result = nlohmann::json::parse(
                    QJsonDocument(v.toJsonArray()).toJson(QJsonDocument::Compact).constData());
                break;

            default:
                spdlog::warn("2 Unsupported datatype {}", v.userType());
                break;
            }
        } break;

        default:
            if (value.typeName())
                spdlog::warn(
                    "3 Unsupported datatype {} {}", value.typeName(), value.userType());
            else
                spdlog::warn("3 Unsupported datatype '' {}", value.userType());
            break;
        }
    }

    return result;
}

void ModelPropertyTree::setIndex(const QModelIndex &index) {
    if (index.isValid()) {
        if (index_.isValid())
            disconnect(
                index_.model(),
                &QAbstractItemModel::dataChanged,
                this,
                &ModelPropertyTree::myDataChanged);

        connect(
            index.model(),
            &QAbstractItemModel::dataChanged,
            this,
            &ModelPropertyTree::myDataChanged);

        index_ = QPersistentModelIndex(index);
        emit indexChanged();
        setRole(role_);
        if (updateValue())
            setModelData(mapFromValue(value_));
    }
}

void ModelPropertyTree::myDataChanged(
    const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles) {
    if (index_.isValid() and QItemSelectionRange(topLeft, bottomRight).contains(index_) and
        (roles.empty() or roles.contains(role_id_)) and updateValue()) {
        // rebuild model.
        setModelData(mapFromValue(value_));
    }
}

bool ModelPropertyTree::updateValue() {
    bool result = false;
    auto v      = QVariant();

    if (index_.isValid())
        v = index_.data(role_id_);

    if (v != value_) {
        value_ = v;
        result = true;
    }

    return result;
}

void ModelPropertyTree::setRole(const QString &role) {
    if (role != role_) {
        role_ = role;
        emit roleChanged();
    }

    auto id = getRoleId(role_);
    if (role_id_ != id) {
        role_id_ = id;
        emit roleIdChanged();

        if (updateValue())
            setModelData(mapFromValue(value_));
    }
}

int ModelPropertyTree::getRoleId(const QString &role) const {
    auto result = -1;

    // valid index, resolve role from model.
    if (index_.isValid() and index_.model()) {
        QHashIterator<int, QByteArray> it(index_.model()->roleNames());
        if (it.findNext(role.toUtf8()))
            result = it.key();
    }

    return result;
}
