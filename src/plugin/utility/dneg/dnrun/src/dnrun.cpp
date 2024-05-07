// SPDX-License-Identifier: Apache-2.0
#ifdef __linux__
#include <unistd.h>
#endif
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/poll.h>
#include <cerrno>
#include <sys/ioctl.h>
#include <queue>

#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/plugin_manager/plugin_utility.hpp"

using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::plugin_manager;

static const int connection_max{10};

class DNRun : public Utility {
  public:
    DNRun(const utility::JsonStore &jsn = utility::JsonStore()) : Utility("DNRun", jsn) {
        memset(&sun_, 0, sizeof(sun_));

        try {
            // int on;
            auto show  = get_env("SHOW");
            auto login = get_login_name();

            if (not show or (*show).empty())
                throw std::runtime_error("SHOW envvar not set.");

            if (login.empty())
                throw std::runtime_error("Failed to get login name.");

            port_name_v1_ =
                std::string(fmt::format("dnrun-v1-{}-{}-{}", "xstudio", *show, login));

            if (port_name_v1_.size() + 1 > sizeof(sun_.sun_path))
                throw std::runtime_error("Domain name exceeds string limit.");

            try_connect();
        } catch (const std::exception &err) {
            spdlog::warn("DNRun : {}", err.what());
        }
    }

    ~DNRun() override {
        if (sock_ >= 0)
            close(sock_);

        // close open connections
        for (auto i = 1; i < nfds_; i++) {
            if (fds_[i].fd >= 0)
                close(fds_[i].fd);
        }
    }

    void try_connect(const bool silent = false) {
        try {
            sock_ = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
            if (sock_ < 0)
                throw std::runtime_error(
                    std::string("Failed to create socket. ") + strerror(errno));

            // if(ioctl(sock_, FIONBIO, &on) <0)
            //     throw std::runtime_error(std::string("Failed to ioctl socket. ") +
            //     strerror(errno));

            sun_.sun_family = AF_UNIX;
            strncpy(sun_.sun_path + 1, port_name_v1_.c_str(), sizeof(sun_.sun_path) - 1);

            if (bind(
                    sock_,
                    (struct sockaddr *)&sun_,
                    sizeof(sun_.sun_family) + port_name_v1_.size() + 1))
                throw std::runtime_error(
                    std::string("Failed to bind socket. ") + strerror(errno));

            if (listen(sock_, connection_max) < 0)
                throw std::runtime_error(
                    std::string("Failed to listen socket. ") + strerror(errno));

            memset(fds_.data(), 0, fds_.size() * sizeof(decltype(fds_)::value_type));

            // Set up the initial listening socket
            fds_[0].fd     = sock_;
            fds_[0].events = POLLIN;
            nfds_          = 1;

            spdlog::info("DNRun port created: {}", port_name_v1_.c_str());
        } catch (const std::exception &err) {
            if (sock_ >= 0)
                close(sock_);
            sock_ = -1;
            if (not silent)
                spdlog::warn("DNRun : {}", err.what());
        }
    }


    void preference_update(const utility::JsonStore &jsn) override {
        try {
        } catch (const std::exception &err) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        }
    }

    [[nodiscard]] std::vector<std::string> get_requests() {
        process_stream();

        // pass requests
        std::vector<std::string> results;
        while (not requests_.empty()) {
            results.push_back(requests_.front());
            requests_.pop();
        }

        return results;
    }

    [[nodiscard]] bool connected() const { return sock_ >= 0; }

  private:
    // buffer should contain netstrings.. process and add to requests
    void process_buffer(const std::vector<char> &data) {
        bool leading_zero   = false;
        bool require_header = true;
        bool require_tail   = false;
        std::string header;
        std::string netstring;
        int string_size = 0;

        try {
            // may contain one or more netstrings..
            for (auto i : data) {
                if (require_header) {
                    // excessive size
                    if (header.size() > 6)
                        throw std::runtime_error("Netstring size exceeds 6 characters");
                    // end of header
                    if (i == ':') {
                        require_header = false;
                        leading_zero   = false;
                        string_size    = std::atoi(header.c_str());
                        if (not string_size)
                            require_tail = true;
                    } else if (i >= '0' && i <= '9') {
                        // leading zero
                        if (leading_zero)
                            throw std::runtime_error("Leading zero in header");

                        if (header.empty() and i == '0')
                            leading_zero = true;

                        header += i;

                    } else {
                        // ignore \n as we get those when using socat etc.
                        if (i != '\n')
                            throw std::runtime_error(std::string(
                                fmt::format("Invalid character in header '{}'", i)));
                    }
                } else {
                    if (require_tail) {
                        if (i == ',') {
                            require_header = true;
                            require_tail   = false;
                            header.clear();
                            requests_.push(netstring);
                            netstring.clear();
                        } else {
                            throw std::runtime_error("Invalid tail character");
                        }
                    } else {
                        netstring += i;
                        string_size--;
                        if (not string_size)
                            require_tail = true;
                    }
                }
            }
            if (not netstring.empty())
                throw std::runtime_error("Missing tail");
        } catch (const std::exception &err) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        }
    }

    void process_stream() {
        if (sock_ == -1)
            try_connect(true);

        if (sock_ >= 0) {
            // wait for some event
            auto rc = poll(fds_.data(), nfds_, 100);
            if (rc < 0)
                spdlog::warn("{} poll failed", __PRETTY_FUNCTION__);
            else if (not rc) {
                // spdlog::warn("poll timeout");
                // timeout
            } else {
                auto compress_array = false;
                auto current_size   = nfds_;

                for (auto i = 0; i < current_size; i++) {
                    if (fds_[i].revents == 0)
                        continue;

                    if (fds_[i].revents != POLLIN and fds_[i].revents != (POLLIN | POLLHUP)) {
                        spdlog::warn(
                            "{} unexpected revents {}", __PRETTY_FUNCTION__, fds_[i].revents);
                        break;
                    }

                    if (fds_[i].fd == sock_) {
                        if (nfds_ < connection_max) {
                            auto new_sd = -1;
                            do {
                                new_sd = accept(sock_, nullptr, nullptr);
                                if (new_sd < 0) {
                                    if (errno != EWOULDBLOCK)
                                        spdlog::warn(
                                            "{} accept() failed {}",
                                            __PRETTY_FUNCTION__,
                                            strerror(errno));
                                    break;
                                }
                                fds_[nfds_].fd     = new_sd;
                                fds_[nfds_].events = POLLIN;
                                messages_[new_sd]  = std::vector<char>();
                                nfds_++;
                            } while (new_sd != -1);
                        }
                    } else {
                        auto close_conn = false;
                        do {
                            rc = recv(fds_[i].fd, read_buffer_.data(), read_buffer_.size(), 0);
                            if (rc < 0) {
                                if (errno != EWOULDBLOCK) {
                                    spdlog::warn(
                                        "{} recv() failed {}",
                                        __PRETTY_FUNCTION__,
                                        strerror(errno));
                                    close_conn = true;
                                }
                                break;
                            }

                            if (rc == 0) {
                                close_conn = true;
                                break;
                            }

                            auto old_size = messages_[fds_[i].fd].size();
                            messages_[fds_[i].fd].resize(old_size + rc);
                            memcpy(
                                messages_[fds_[i].fd].data() + old_size,
                                read_buffer_.data(),
                                rc);
                            /*****************************************************/
                            /* Echo the data back to the client                  */
                            /*****************************************************/
                            // rc = send(fds_[i].fd, read_buffer_, rc, 0);
                            // if (rc < 0)
                            // {
                            //    spdlog::warn("dnrun:  send() failed {}", strerror(errno));
                            //   close_conn = true;
                            //   break;
                            // }

                        } while (true);

                        if (close_conn) {
                            auto fd = fds_[i].fd;
                            close(fd);
                            process_buffer(messages_[fd]);
                            messages_.erase(fd);
                            fds_[i].fd     = -1;
                            compress_array = true;
                        }
                    }
                }

                if (compress_array) {
                    compress_array = false;
                    for (auto i = 0; i < nfds_; i++) {
                        if (fds_[i].fd == -1) {
                            for (auto j = i; j < nfds_ - 1; j++) {
                                fds_[j].fd = fds_[j + 1].fd;
                            }
                            i--;
                            nfds_--;
                        }
                    }
                }
            }
        }
    }

  private:
    std::string port_name_v1_;
    struct sockaddr_un sun_;
    int sock_{-1};
    std::queue<std::string> requests_;
    std::array<char, 1024> read_buffer_;
    std::array<struct pollfd, connection_max> fds_;
    int nfds_{1};
    std::map<int, std::vector<char>> messages_;
};

template <typename T> class DNRunPluginActor : public caf::event_based_actor {

  public:
    DNRunPluginActor(
        caf::actor_config &cfg, const utility::JsonStore &jsn = utility::JsonStore())
        : caf::event_based_actor(cfg), utility_(jsn) {

        spdlog::debug("Created DNRunPluginActor");

        // // watch for updates to global preferences
        // try {
        //     auto prefs = global_store::GlobalStoreHelper(system());
        //     utility::JsonStore j;
        //     join_broadcast(this, prefs.get_group(j));
        //     utility_.preference_update(j);
        // } catch (...) {
        // }

        send(this, utility::event_atom_v);

        behavior_.assign(
            // [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {},
            [=](utility::name_atom) -> std::string { return utility_.name(); },

            [=](utility::event_atom) {
                // check for event
                auto requests = utility_.get_requests();
                if (not requests.empty()) {
                    // connect to session..
                    scoped_actor sys{system()};
                    auto global = system().registry().template get<caf::actor>(global_registry);
                    auto pm =
                        system().registry().template get<caf::actor>(plugin_manager_registry);
                    auto session =
                        request_receive<caf::actor>(*sys, global, session::session_atom_v);
                    auto media_rate =
                        request_receive<FrameRate>(*sys, session, session::media_rate_atom_v);

                    for (const auto &i : requests) {

                        try {

                            auto jsn = nlohmann::json::parse(i);
                            // should be dict with paths: array
                            if (jsn["args"]["paths"].empty())
                                continue;

                            spdlog::info("DNRun received request {}", jsn.dump(2));

                            caf::actor playlist;

                            // first, try and add to existing 'current' playlist
                            try {
                                // this throws an exception if there is no 'current' playlist
                                playlist = request_receive<caf::actor>(
                                    *sys, session, session::current_playlist_atom_v);
                            } catch (...) {
                            }

                            // second, try and find the 'Ivy Media' named playlist
                            if (!playlist) {

                                playlist = request_receive<caf::actor>(
                                    *sys,
                                    session,
                                    session::get_playlist_atom_v,
                                    "DNRun Playlist");
                            }

                            // third, make a new 'Ivy Media' playlist
                            if (!playlist) {

                                playlist = request_receive<UuidUuidActor>(
                                               *sys,
                                               session,
                                               session::add_playlist_atom_v,
                                               "DNRun Playlist")
                                               .second.actor();
                            }

                            bool first = true;

                            bool quickview = false;
                            if (jsn.at("args").contains("quickview")) {
                                if (jsn.at("args")["quickview"].is_boolean()) {
                                    quickview = jsn.at("args").at("quickview");
                                } else if (jsn.at("args")["quickview"].is_string()) {
                                    quickview = jsn.at("args").at("quickview") == "true";
                                }
                            }
                            
                            bool ab_compare = jsn.at("args").contains("compare") &&
                                              jsn.at("args").at("compare") == "ab";

                            for (std::string path : jsn.at("args").at("paths")) {
                                // auto path = j.get<std::string>();
                                if (starts_with(path, "xstudio://")) {
                                    spdlog::warn(
                                        "{} Unsupported path {}", __PRETTY_FUNCTION__, path);
                                    continue;
                                }

                                if (utility::check_plugin_uri_request(path)) {

                                    // send to plugin manager..
                                    auto uri = caf::make_uri(path);
                                    if (uri)
                                        send_uri_request_to_plugin(
                                            *uri,
                                            media_rate,
                                            session,
                                            playlist,
                                            pm,
                                            quickview,
                                            ab_compare);
                                    else {
                                        spdlog::warn(
                                            "{} Invalid URI {}", __PRETTY_FUNCTION__, path);
                                    }

                                } else {
                                    try {
                                        FrameList fl;
                                        caf::uri uri = parse_cli_posix_path(path, fl, true);

                                        validate_media(uri, fl);

                                        UuidActor new_media;
                                        if (fl.empty())
                                            new_media = request_receive<UuidActor>(
                                                *sys,
                                                playlist,
                                                playlist::add_media_atom_v,
                                                path,
                                                uri,
                                                Uuid());
                                        else
                                            new_media = request_receive<UuidActor>(
                                                *sys,
                                                playlist,
                                                playlist::add_media_atom_v,
                                                path,
                                                uri,
                                                fl,
                                                Uuid());

                                        if (!new_media.uuid().is_null()) {
                                            auto selection_actor = request_receive<caf::actor>(
                                                *sys,
                                                playlist,
                                                playlist::selection_actor_atom_v);
                                            if (selection_actor) {
                                                if (first) {
                                                    // clear the selection
                                                    request_receive<bool>(
                                                        *sys,
                                                        selection_actor,
                                                        playlist::select_media_atom_v);
                                                    first = false;
                                                }
                                                anon_send(
                                                    selection_actor,
                                                    playlist::select_media_atom_v,
                                                    new_media.uuid());
                                            }

                                            // trigger the session to (perhaps -
                                            // depending on quick view preference)
                                            // launch a quick viewer for the new
                                            // media
                                            auto studio =
                                                system().registry().template get<caf::actor>(
                                                    studio_registry);

                                            anon_send(
                                                studio,
                                                ui::open_quickview_window_atom_v,
                                                utility::UuidActorVector({new_media}),
                                                "Off",
                                                quickview);
                                        }

                                    } catch (const std::exception &e) {
                                        spdlog::error(
                                            "{} Failed to load media '{}'",
                                            __PRETTY_FUNCTION__,
                                            e.what());
                                    }
                                }
                            }
                        } catch (const std::exception &err) {
                            spdlog::warn("{} {} {}", __PRETTY_FUNCTION__, err.what(), i);
                        }
                    }
                }

                if (utility_.connected())
                    delayed_send(this, std::chrono::milliseconds(100), utility::event_atom_v);
                else
                    delayed_send(this, std::chrono::milliseconds(5000), utility::event_atom_v);
            }

            // [=](json_store::update_atom,
            //     const utility::JsonStore & /*change*/,
            //     const std::string & /*path*/,
            //     const utility::JsonStore &full) {
            //     delegate(actor_cast<caf::actor>(this), json_store::update_atom_v, full);
            // },

            // [=](json_store::update_atom, const utility::JsonStore &js) {
            //     utility_.preference_update(js);
            // }
        );
    }

    caf::behavior make_behavior() override { return behavior_; }

  private:
    void send_uri_request_to_plugin(
        const caf::uri &uri,
        const FrameRate &rate,
        caf::actor session,
        caf::actor playlist,
        caf::actor plugin_manager,
        const bool quickview,
        const bool ab_compare);

    caf::behavior behavior_;
    T utility_;
};

template <class T>
void DNRunPluginActor<T>::send_uri_request_to_plugin(
    const caf::uri &uri,
    const FrameRate &rate,
    caf::actor session,
    caf::actor playlist,
    caf::actor plugin_manager,
    const bool quickview,
    const bool ab_compare) {

    request(
        plugin_manager, infinite, data_source::use_data_atom_v, uri, session, playlist, rate)
        .then(
            [=](UuidActorVector &new_media) {
                if (!new_media.size())
                    return;

                // check if we're loading media
                request(new_media[0].actor(), infinite, type_atom_v)
                    .then(
                        [=](const std::string &type) {
                            if (type == "Media") {
                                // trigger the session to (perhaps -
                                // depending on quick view preference)
                                // launch a quick viewer for the new
                                // media
                                auto studio = system().registry().template get<caf::actor>(
                                    studio_registry);
                                anon_send(
                                    studio,
                                    ui::open_quickview_window_atom_v,
                                    new_media,
                                    ab_compare ? "Off" : "A/B",
                                    quickview);
                            }
                        },
                        [=](error &err) mutable {
                            spdlog::error("{} {}", __PRETTY_FUNCTION__, to_string(err));
                        });
            },
            [=](error &err) mutable {
                spdlog::error("{} {}", __PRETTY_FUNCTION__, to_string(err));
            });
}

extern "C" {
plugin_manager::PluginFactoryCollection *plugin_factory_collection_ptr() {
    return new plugin_manager::PluginFactoryCollection(
        std::vector<std::shared_ptr<plugin_manager::PluginFactory>>(
            {std::make_shared<UtilityPlugin<DNRunPluginActor<DNRun>>>(
                Uuid("01f2ac71-c879-40d1-a334-0898e2141872"),
                "DNRun",
                "DNeg",
                "Enable DnRun SwiftWind support",
                semver::version("1.0.0"))}));
}
}
