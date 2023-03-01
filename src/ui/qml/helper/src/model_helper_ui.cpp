#include "xstudio/ui/qml/helper_ui.hpp"
#include "xstudio/utility/helpers.hpp"

using namespace xstudio::ui::qml;

#include <QJSValue>
#include <QItemSelectionRange>

void ModelProperty::setIndex(const QModelIndex &index) {
    if (index.isValid()) {
        if (index_.isValid())
            disconnect(
                index_.model(),
                &QAbstractItemModel::dataChanged,
                this,
                &ModelProperty::dataChanged);

        connect(
            index.model(), &QAbstractItemModel::dataChanged, this, &ModelProperty::dataChanged);

        index_ = QPersistentModelIndex(index);
        emit indexChanged();
        setRole(role_);
        if (updateValue())
            emit valueChanged();
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
    if (value != value_ and index_.isValid() and index_.model()) {

        // this maybe be bad!
        auto result =
            const_cast<QAbstractItemModel *>(index_.model())->setData(index_, value, role_id_);
        if (result) {
            value_ = value;
            emit valueChanged();
        }
    }
}

ModelPropertyMap::ModelPropertyMap(QObject *parent) : QObject(parent) {
    values_ = new QQmlPropertyMap(this);
    connect(values_, &QQmlPropertyMap::valueChanged, this, &ModelPropertyMap::valueChangedSlot);
}

void ModelPropertyMap::setIndex(const QModelIndex &index) {
    if (index.isValid()) {
        if (index_.isValid())
            disconnect(
                index_.model(),
                &QAbstractItemModel::dataChanged,
                this,
                &ModelPropertyMap::dataChanged);

        connect(
            index.model(),
            &QAbstractItemModel::dataChanged,
            this,
            &ModelPropertyMap::dataChanged);

        index_ = QPersistentModelIndex(index);
        emit indexChanged();

        for (const auto &i : index.model()->roleNames())
            values_->insert(QString(i), QVariant());

        updateValues();
        emit valuesChanged();
    }
}

void ModelPropertyMap::updateValues(const QVector<int> &roles) {
    if (index_.isValid()) {
        auto hash = index_.model()->roleNames();

        QHash<int, QByteArray>::const_iterator i = hash.constBegin();
        while (i != hash.constEnd()) {
            if (roles.empty() or roles.contains(i.key())) {
                auto value = index_.data(i.key());
                if (value != (*values_)[QString(i.value())])
                    (*values_)[QString(i.value())] = index_.data(i.key());
            }
            ++i;
        }
    }
}

void ModelPropertyMap::dataChanged(
    const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles) {
    if (index_.isValid() and QItemSelectionRange(topLeft, bottomRight).contains(index_))
        updateValues(roles);
}

void ModelPropertyMap::valueChangedSlot(const QString &key, const QVariant &value) {
    valueChanged(key, value);
}

void ModelPropertyMap::valueChanged(const QString &key, const QVariant &value) {
    // propate tobackend.
    // find id..
    auto id = getRoleId(key);
    if (id != -1 and index_.isValid() and index_.model()) {
        const_cast<QAbstractItemModel *>(index_.model())->setData(index_, value, id);
    }
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


void ModelNestedPropertyMap::valueChanged(const QString &key, const QVariant &value) {
    auto id = getRoleId(key);
    if (index_.isValid() and index_.model()) {
        if (id != -1)
            const_cast<QAbstractItemModel *>(index_.model())->setData(index_, value, id);
        else {
            id          = getRoleId(data_role_);
            auto jvalue = mapFromValue(index_.data(id));
            auto skey   = StdFromQString(key);
            auto svalue = mapFromValue(value);
            if (jvalue.is_object() and jvalue.count(skey) and svalue != jvalue.at(skey)) {
                // spdlog::warn("changed {} {}", svalue.dump(2), jvalue.at(skey).dump(2));
                jvalue[skey] = svalue;
                const_cast<QAbstractItemModel *>(index_.model())
                    ->setData(index_, mapFromValue(jvalue), id);
            }
        }
    }
}

void ModelNestedPropertyMap::updateValues(const QVector<int> &roles) {
    ModelPropertyMap::updateValues(roles);

    // populate QMLPropertyMap from value.
    if (index_.isValid()) {
        auto jvalue = mapFromValue(index_.data(getRoleId(data_role_)));
        if (jvalue.is_object()) {
            for (const auto &[k, v] : jvalue.items()) {
                // spdlog::warn("{} {}", k, v.dump(2));
                auto qk = QStringFromStd(k);
                auto qv = mapFromValue(v);

                if (values_->contains(qk) and qv != (*values_)[qk])
                    (*values_)[qk] = qv;
                else
                    values_->insert(qk, qv);
            }
        }
    }
}


QVariant xstudio::ui::qml::mapFromValue(const nlohmann::json &value) {
    auto result = QVariant();

    if (value.is_boolean())
        result = QVariant::fromValue(value.get<bool>());
    else if (value.is_number_integer())
        result = QVariant::fromValue(value.get<int>());
    else if (value.is_number_unsigned())
        result = QVariant::fromValue(value.get<int>());
    else if (value.is_number_float())
        result = QVariant::fromValue(value.get<float>());
    else if (value.is_string())
        result = QVariant::fromValue(QStringFromStd(value.get<std::string>()));
    else if (value.is_array())
        result = QVariantListFromJson(utility::JsonStore(value));
    else if (value.is_object())
        result = QVariantMapFromJson(value);

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

        default:
            spdlog::warn("Unsupported datatype {} {}", v.type(), v.typeName());
            break;
        }

    } else {

        switch (value.userType()) {
        case QMetaType::Bool:
            result = value.toBool();
            break;
        case QMetaType::Double:
            result = value.toDouble();
            break;
        case QMetaType::Int:
            result = value.toInt();
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

        case QMetaType::QVariantList:
            result = nlohmann::json::parse(
                QJsonDocument(value.toJsonArray()).toJson(QJsonDocument::Compact).constData());
            break;

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
                spdlog::warn("Unsupported datatype {}", v.userType());
                break;
            }
        } break;

        default:
            spdlog::warn("Unsupported datatype {} {}", value.typeName(), value.userType());
            break;
        }
    }

    return result;
}
