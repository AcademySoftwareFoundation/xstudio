// SPDX-License-Identifier: Apache-2.0

#include "xstudio/shotgun_client/shotgun_client.hpp"
#include "xstudio/utility/helpers.hpp"

using namespace xstudio::shotgun_client;
using namespace xstudio::utility;

httplib::Headers ShotgunClient::get_headers() const {
    return httplib::Headers(
        {{"Accept", content_type_hash()}, {"Accept-Encoding", "gzip, deflate"}});
}

httplib::Headers ShotgunClient::get_auth_headers(const std::string &content_type) const {
    auto content_type_ = content_type;
    if (content_type_.empty())
        content_type_ = content_type_hash();

    return httplib::Headers({
        {"Accept", content_type_},
        {"Accept-Encoding", "gzip, deflate"},
        {"Authorization", token_type_ + " " + access_token_}
        // {"Cookie", "_session_id=1bccf067d17eda48a77a34ca6f55f669"}
    });
}


httplib::Params ShotgunClient::get_auth_request_params(const std::string &auth_token) const {
    switch (authentication_method_) {
    case AM_SCRIPT:
        return httplib::Params(
            {{"client_id", id_},
             {"client_secret", secret_},
             {"grant_type", authentication_method_to_string(authentication_method_)},
             {"session_uuid", session_uuid_}});
        break;
    case AM_LOGIN:
        return httplib::Params(
            {{"username", id_},
             {"password", secret_},
             {"grant_type", authentication_method_to_string(authentication_method_)},
             {"session_uuid", session_uuid_},
             {"auth_token", auth_token}});
        break;
    case AM_SESSION:
        return httplib::Params(
            {{"session_token", id_},
             {"grant_type", authentication_method_to_string(authentication_method_)},
             {"session_uuid", session_uuid_}});
        break;
    case AM_UNDEFINED:
    default:
        break;
    }
    return httplib::Params();
}

httplib::Params ShotgunClient::get_auth_refresh_params() const {
    return httplib::Params(
        {{"refresh_token", refresh_token_}, {"grant_type", "refresh_token"}});
}

std::optional<ConditionalOperator>
xstudio::shotgun_client::conditional_operator_from_string(const std::string &value) {
    for (const auto &i : ConditionalOperatorMap) {
        if (i.second == value)
            return i.first;
    }
    return {};
}

std::optional<BoolOperator>
xstudio::shotgun_client::bool_operator_from_string(const std::string &value) {
    for (const auto &i : BoolOperatorMap) {
        if (i.second == value)
            return i.first;
    }
    return {};
}

std::optional<Period> xstudio::shotgun_client::period_from_string(const std::string &value) {
    for (const auto &i : PeriodMap) {
        if (i.second == value)
            return i.first;
    }
    return {};
}

void ShotgunClient::set_token(
    const std::string &token_type,
    const std::string &access_token,
    const std::string &refresh_token,
    const int expires_in) {
    token_type_   = token_type;
    access_token_ = access_token;
    set_refresh_token(refresh_token, expires_in);
}

void ShotgunClient::set_refresh_token(const std::string &refresh_token, const int expires_in) {
    refresh_token_         = refresh_token;
    expires_               = utility::clock::now() + std::chrono::seconds(expires_in);
    failed_authentication_ = false;
}

void ShotgunClient::set_credentials_method(const AuthenticateShotgun &auth) {
    failed_authentication_ = false;
    if (auth.authentication_method())
        authentication_method_ = *(auth.authentication_method());
    if (auth.session_uuid())
        session_uuid_ = *(auth.session_uuid());

    switch (authentication_method_) {
    case AM_UNDEFINED:
    default:
        break;
    case AM_SCRIPT:
        if (auth.client_id())
            id_ = *(auth.client_id());
        if (auth.client_secret())
            secret_ = *(auth.client_secret());
        break;
    case AM_LOGIN:
        if (auth.username())
            id_ = *(auth.username());
        if (auth.password())
            secret_ = *(auth.password());
        break;
    case AM_SESSION:
        if (auth.session_token())
            id_ = *(auth.session_token());
        if (auth.password())
            secret_ = "";
        break;
    }

    if (auth.refresh_token())
        set_refresh_token(*(auth.refresh_token()), 600);
    else {
        refresh_token_ = "";
        expires_       = utility::clock::now();
    }

    // spdlog::warn("{} {} {} {}", authentication_method_, id_, secret_, refresh_token_);

    access_token_ = "";
}

// better way of doing this ?
FilterBy::FilterBy(const utility::JsonStore &jsn) : Op(jsn["class"]) {
    auto op = bool_operator_from_string(jsn["op"]);
    if (op)
        op_ = *op;

    for (const auto &i : jsn["array"].get<nlohmann::json::array_t>()) {
        if (i["class"] == "FilterBy")
            push_back(FilterBy(utility::JsonStore(i)));
        else if (i["class"] == "Checkbox")
            push_back(Checkbox(utility::JsonStore(i)));
        else if (i["class"] == "Number")
            push_back(Number(utility::JsonStore(i)));
        else if (i["class"] == "Float")
            push_back(Float(utility::JsonStore(i)));
        else if (i["class"] == "Timecode")
            push_back(Timecode(utility::JsonStore(i)));
        else if (i["class"] == "Duration")
            push_back(Duration(utility::JsonStore(i)));
        else if (i["class"] == "Percent")
            push_back(Percent(utility::JsonStore(i)));
        else if (i["class"] == "Currency")
            push_back(Currency(utility::JsonStore(i)));
        else if (i["class"] == "Text")
            push_back(Text(utility::JsonStore(i)));
        else if (i["class"] == "Image")
            push_back(Image(utility::JsonStore(i)));
        else if (i["class"] == "List")
            push_back(List(utility::JsonStore(i)));
        else if (i["class"] == "StatusList")
            push_back(StatusList(utility::JsonStore(i)));
        else if (i["class"] == "DateTime")
            push_back(DateTime(utility::JsonStore(i)));
        else if (i["class"] == "Date")
            push_back(Date(utility::JsonStore(i)));
    }
}

EntityType::EntityType(const utility::JsonStore &jsn) : Op(jsn["class"]) {
    for (const auto &i : jsn["map"].get<nlohmann::json::object_t>())
        (*this)[i.first] = FilterBy(utility::JsonStore(i.second));
}
