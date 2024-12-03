// SPDX-License-Identifier: Apache-2.0

#include "xstudio/http_client/http_client_actor.hpp"
#include "xstudio/atoms.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/utility/uuid.hpp"

using namespace xstudio;
using namespace xstudio::http_client;
using namespace xstudio::utility;
using namespace caf;

typedef http_client_error hce;

std::string HTTPWorker::get_error_string(const httplib::Error err) {
    switch (err) {
    case httplib::Error::Success:
        return "Success";
        break;
    case httplib::Error::Unknown:
        return "Unknown";
        break;
    case httplib::Error::Connection:
        return "Connection";
        break;
    case httplib::Error::BindIPAddress:
        return "BindIPAddress";
        break;
    case httplib::Error::Read:
        return "Read";
        break;
    case httplib::Error::Write:
        return "Write";
        break;
    case httplib::Error::ExceedRedirectCount:
        return "ExceedRedirectCount";
        break;
    case httplib::Error::Canceled:
        return "Canceled";
        break;
    case httplib::Error::SSLConnection:
        return "SSLConnection";
        break;
    case httplib::Error::SSLLoadingCerts:
        return "SSLLoadingCerts";
        break;
    case httplib::Error::SSLServerVerification:
        return "SSLServerVerification";
        break;
    case httplib::Error::UnsupportedMultipartBoundaryChars:
        return "UnsupportedMultipartBoundaryChars";
        break;
    case httplib::Error::Compression:
        return "Compression";
        break;
    default:
        break;
    }
    return "Unknown";
}

HTTPWorker::HTTPWorker(
    caf::actor_config &cfg,
    time_t connection_timeout,
    time_t read_timeout,
    time_t write_timeout)
    : caf::event_based_actor(cfg) {
    behavior_.assign(
        [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {},
        [=](http_delete_atom,
            const std::string &scheme_host_port,
            const std::string &path,
            const httplib::Headers &headers,
            const std::string &body,
            const std::string &content_type) -> result<httplib::Response> {
            // spdlog::warn("http_delete_atom {}", path);
            try {
                httplib::Client cli(scheme_host_port.c_str());
                cli.set_follow_location(true);
                cli.set_connection_timeout(connection_timeout, 0);
                cli.set_read_timeout(read_timeout, 0);
                cli.set_write_timeout(write_timeout, 0);
                auto res = [&]() -> httplib::Result {
                    if (content_type.empty())
                        return cli.Delete(path.c_str(), headers);
                    return cli.Delete(path.c_str(), headers, body, content_type.c_str());
                }();

                if (res.error() != httplib::Error::Success)
                    return make_error(hce::rest_error, get_error_string(res.error()));

                if (res)
                    return *res;
                return make_error(hce::connection_error, "Empty response");
            } catch (const std::exception &err) {
                return make_error(hce::connection_error, err.what());
            }
        },

        [=](http_delete_simple_atom,
            const std::string &scheme_host_port,
            const std::string &path,
            const httplib::Headers &headers,
            const std::string &body,
            const std::string &content_type) -> result<std::string> {
            auto rp = make_response_promise<std::string>();
            request(
                actor_cast<caf::actor>(this),
                infinite,
                http_delete_atom_v,
                scheme_host_port,
                path,
                headers,
                body,
                content_type)
                .then(
                    [=](const httplib::Response &response) mutable {
                        if (response.status != 200)
                            rp.deliver(make_error(
                                hce::rest_error,
                                std::string(httplib::detail::status_message(response.status)) +
                                    "\n" + response.body));
                        else
                            rp.deliver(response.body);
                    },
                    [=](error &err) mutable { rp.deliver(std::move(err)); });

            return rp;
        },

        [=](http_get_atom,
            const std::string &scheme_host_port,
            const std::string &path,
            const httplib::Headers &headers,
            const httplib::Params &params) -> result<httplib::Response> {
            // spdlog::warn("http_get_atom {}", path);

            try {
                httplib::Client cli(scheme_host_port.c_str());
                cli.set_follow_location(true);
                cli.set_connection_timeout(connection_timeout, 0);
                cli.set_read_timeout(read_timeout, 0);
                cli.set_write_timeout(write_timeout, 0);

                // cli.set_logger([](const auto& req, const auto& res) {
                //     spdlog::warn("{}", req.);
                // });

                auto result = cli.Get(path.c_str(), params, headers);

                if (result.error() != httplib::Error::Success) {
                    auto error = get_error_string(result.error());
                    if (result) {
                        auto response = *result;
                        error += " : " +
                                 std::string(httplib::detail::status_message(response.status));
                        error += "\n" + response.body;
                    }
                    return make_error(hce::rest_error, error);
                }

                if (result)
                    return *result;

                return make_error(hce::connection_error, "Empty response");
            } catch (const std::exception &err) {
                return make_error(hce::connection_error, err.what());
            }
        },

        [=](http_get_simple_atom,
            const std::string &scheme_host_port,
            const std::string &path,
            const httplib::Headers &headers,
            const httplib::Params &params) -> result<std::string> {
            auto rp = make_response_promise<std::string>();
            request(
                actor_cast<caf::actor>(this),
                infinite,
                http_get_atom_v,
                scheme_host_port,
                path,
                headers,
                params)
                .then(
                    [=](const httplib::Response &response) mutable {
                        if (response.status != 200)
                            rp.deliver(make_error(
                                hce::rest_error,
                                std::string(httplib::detail::status_message(response.status)) +
                                    "\n" + response.body));
                        else
                            rp.deliver(response.body);
                    },
                    [=](error &err) mutable { rp.deliver(std::move(err)); });

            return rp;
        },

        [=](http_post_atom,
            const std::string &scheme_host_port,
            const std::string &path,
            const httplib::Headers &headers,
            const httplib::Params &params,
            const std::string &body,
            const std::string &content_type) -> result<httplib::Response> {
            // spdlog::warn("http_post_atom {}", path);

            try {
                httplib::Client cli(scheme_host_port.c_str());
                cli.set_follow_location(true);
                cli.set_connection_timeout(connection_timeout, 0);
                cli.set_read_timeout(read_timeout, 0);
                cli.set_write_timeout(write_timeout, 0);
                auto res = [&]() -> httplib::Result {
                    if (content_type.empty())
                        return cli.Post(path.c_str(), headers, params);
                    return cli.Post(path.c_str(), headers, body, content_type.c_str());
                }();

                if (res.error() != httplib::Error::Success)
                    return make_error(hce::rest_error, get_error_string(res.error()));

                if (res)
                    return *res;
                return make_error(hce::connection_error, "Empty response");
            } catch (const std::exception &err) {
                return make_error(hce::connection_error, err.what());
            }
        },

        [=](http_post_simple_atom,
            const std::string &scheme_host_port,
            const std::string &path,
            const httplib::Headers &headers,
            const httplib::Params &params,
            const std::string &body,
            const std::string &content_type) -> result<std::string> {
            auto rp = make_response_promise<std::string>();
            request(
                actor_cast<caf::actor>(this),
                infinite,
                http_post_atom_v,
                scheme_host_port,
                path,
                headers,
                params,
                body,
                content_type)
                .then(
                    [=](const httplib::Response &response) mutable {
                        if (response.status != 200)
                            rp.deliver(make_error(
                                hce::rest_error,
                                std::string(httplib::detail::status_message(response.status)) +
                                    "\n" + response.body));
                        else
                            rp.deliver(response.body);
                    },
                    [=](error &err) mutable { rp.deliver(std::move(err)); });

            return rp;
        },

        [=](http_put_atom,
            const std::string &scheme_host_port,
            const std::string &path,
            const httplib::Headers &headers,
            const httplib::Params &params,
            const std::string &body,
            const std::string &content_type) -> result<httplib::Response> {
            // spdlog::warn("http_put_atom {}", path);

            try {
                httplib::Client cli(scheme_host_port.c_str());
                cli.set_follow_location(true);
                cli.set_connection_timeout(connection_timeout, 0);
                cli.set_read_timeout(read_timeout, 0);
                cli.set_write_timeout(write_timeout, 0);

                auto res = [&]() -> httplib::Result {
                    if (content_type.empty())
                        return cli.Put(path.c_str(), headers, params);

                    if (params.empty())
                        return cli.Put(path.c_str(), headers, body, content_type.c_str());

                    auto param_path = httplib::append_query_params(path, params);
                    return cli.Put(param_path.c_str(), headers, body, content_type.c_str());
                }();


                if (res.error() != httplib::Error::Success)
                    return make_error(hce::rest_error, get_error_string(res.error()));

                if (res)
                    return *res;
                return make_error(hce::connection_error, "Empty response");
            } catch (const std::exception &err) {
                return make_error(hce::connection_error, err.what());
            }
        },

        [=](http_put_simple_atom,
            const std::string &scheme_host_port,
            const std::string &path,
            const httplib::Headers &headers,
            const httplib::Params &params,
            const std::string &body,
            const std::string &content_type) -> result<std::string> {
            auto rp = make_response_promise<std::string>();
            request(
                actor_cast<caf::actor>(this),
                infinite,
                http_put_atom_v,
                scheme_host_port,
                path,
                headers,
                params,
                body,
                content_type)
                .then(
                    [=](const httplib::Response &response) mutable {
                        if (response.status != 200)
                            rp.deliver(make_error(
                                hce::rest_error,
                                std::string(httplib::detail::status_message(response.status)) +
                                    "\n" + response.body));
                        else
                            rp.deliver(response.body);
                    },
                    [=](error &err) mutable { rp.deliver(std::move(err)); });

            return rp;
        });
}

HTTPClientActor::HTTPClientActor(
    caf::actor_config &cfg,
    time_t connection_timeout,
    time_t read_timeout,
    time_t write_timeout)
    : caf::event_based_actor(cfg),
      connection_timeout_(connection_timeout),
      read_timeout_(read_timeout),
      write_timeout_(write_timeout) {
    init();
}

void HTTPClientActor::init() {
    size_t worker_count = 10;
    spdlog::debug("Created HTTPClientActor");
    print_on_exit(this, "HTTPClientActor");

    // try {
    // 	auto prefs = GlobalStoreHelper(system());
    // 	JsonStore j;
    // 	join_broadcast(this, prefs.get_group(j));
    // 	worker_count = preference_value<size_t>(j, "/core/media_hook/max_worker_count");
    // } catch(...) {
    // }
    auto pool = caf::actor_pool::make(
        system().dummy_execution_unit(),
        worker_count,
        [&] {
            return system().spawn<HTTPWorker>(
                connection_timeout_, read_timeout_, write_timeout_);
        },
        caf::actor_pool::round_robin());
    link_to(pool);

    behavior_.assign(
        [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {},
        [=](http_delete_atom atom,
            const std::string &scheme_host_port,
            const std::string &path) {
            return delegate(pool, atom, scheme_host_port, path, httplib::Headers(), "", "");
        },

        [=](http_delete_atom atom,
            const std::string &scheme_host_port,
            const std::string &path,
            const httplib::Headers &headers) {
            return delegate(pool, atom, scheme_host_port, path, headers, "", "");
        },

        [=](http_delete_atom atom,
            const std::string &scheme_host_port,
            const std::string &path,
            const httplib::Headers &headers,
            const std::string &body,
            const std::string &content_type) {
            return delegate(pool, atom, scheme_host_port, path, headers, body, content_type);
        },

        [=](http_delete_simple_atom atom,
            const std::string &scheme_host_port,
            const std::string &path) {
            return delegate(pool, atom, scheme_host_port, path, httplib::Headers(), "", "");
        },

        [=](http_delete_simple_atom atom,
            const std::string &scheme_host_port,
            const std::string &path,
            const httplib::Headers &headers) {
            return delegate(pool, atom, scheme_host_port, path, headers, "", "");
        },

        [=](http_delete_simple_atom atom,
            const std::string &scheme_host_port,
            const std::string &path,
            const httplib::Headers &headers,
            const std::string &body,
            const std::string &content_type) {
            return delegate(pool, atom, scheme_host_port, path, headers, body, content_type);
        },

        [=](http_get_atom atom, const std::string &scheme_host_port, const std::string &path) {
            return delegate(
                pool, atom, scheme_host_port, path, httplib::Headers(), httplib::Params());
        },

        [=](http_get_atom atom,
            const std::string &scheme_host_port,
            const std::string &path,
            const httplib::Headers &headers) {
            return delegate(pool, atom, scheme_host_port, path, headers, httplib::Params());
        },

        [=](http_get_atom atom,
            const std::string &scheme_host_port,
            const std::string &path,
            const httplib::Headers &headers,
            const httplib::Params &params) {
            return delegate(pool, atom, scheme_host_port, path, headers, params);
        },

        [=](http_get_simple_atom atom,
            const std::string &scheme_host_port,
            const std::string &path) {
            return delegate(
                pool, atom, scheme_host_port, path, httplib::Headers(), httplib::Params());
        },

        [=](http_get_simple_atom atom,
            const std::string &scheme_host_port,
            const std::string &path,
            const httplib::Headers &headers) {
            return delegate(pool, atom, scheme_host_port, path, headers, httplib::Params());
        },

        [=](http_get_simple_atom atom,
            const std::string &scheme_host_port,
            const std::string &path,
            const httplib::Headers &headers,
            const httplib::Params &params) {
            return delegate(pool, atom, scheme_host_port, path, headers, params);
        },

        [=](http_post_atom atom, const std::string &scheme_host_port, const std::string &path) {
            return delegate(
                pool,
                atom,
                scheme_host_port,
                path,
                httplib::Headers(),
                httplib::Params(),
                "",
                "");
        },

        [=](http_post_atom atom,
            const std::string &scheme_host_port,
            const std::string &path,
            const httplib::Headers &headers) {
            return delegate(
                pool, atom, scheme_host_port, path, headers, httplib::Params(), "", "");
        },

        [=](http_post_atom atom,
            const std::string &scheme_host_port,
            const std::string &path,
            const httplib::Headers &headers,
            const httplib::Params &params) {
            return delegate(pool, atom, scheme_host_port, path, headers, params, "", "");
        },

        [=](http_post_atom atom,
            const std::string &scheme_host_port,
            const std::string &path,
            const httplib::Headers &headers,
            const std::string &body,
            const std::string &content_type) {
            return delegate(
                pool,
                atom,
                scheme_host_port,
                path,
                headers,
                httplib::Params(),
                body,
                content_type);
        },

        [=](http_post_simple_atom atom,
            const std::string &scheme_host_port,
            const std::string &path) {
            return delegate(
                pool,
                atom,
                scheme_host_port,
                path,
                httplib::Headers(),
                httplib::Params(),
                "",
                "");
        },

        [=](http_post_simple_atom atom,
            const std::string &scheme_host_port,
            const std::string &path,
            const httplib::Headers &headers) {
            return delegate(
                pool, atom, scheme_host_port, path, headers, httplib::Params(), "", "");
        },

        [=](http_post_simple_atom atom,
            const std::string &scheme_host_port,
            const std::string &path,
            const httplib::Headers &headers,
            const httplib::Params &params) {
            return delegate(pool, atom, scheme_host_port, path, headers, params, "", "");
        },

        [=](http_post_simple_atom atom,
            const std::string &scheme_host_port,
            const std::string &path,
            const httplib::Headers &headers,
            const std::string &body,
            const std::string &content_type) {
            return delegate(
                pool,
                atom,
                scheme_host_port,
                path,
                headers,
                httplib::Params(),
                body,
                content_type);
        },

        [=](http_put_atom atom, const std::string &scheme_host_port, const std::string &path) {
            return delegate(
                pool,
                atom,
                scheme_host_port,
                path,
                httplib::Headers(),
                httplib::Params(),
                "",
                "");
        },

        [=](http_put_atom atom,
            const std::string &scheme_host_port,
            const std::string &path,
            const httplib::Headers &headers) {
            return delegate(
                pool, atom, scheme_host_port, path, headers, httplib::Params(), "", "");
        },

        [=](http_put_atom atom,
            const std::string &scheme_host_port,
            const std::string &path,
            const httplib::Headers &headers,
            const httplib::Params &params) {
            return delegate(pool, atom, scheme_host_port, path, headers, params, "", "");
        },

        [=](http_put_atom atom,
            const std::string &scheme_host_port,
            const std::string &path,
            const httplib::Headers &headers,
            const std::string &body,
            const std::string &content_type) {
            return delegate(
                pool,
                atom,
                scheme_host_port,
                path,
                headers,
                httplib::Params(),
                body,
                content_type);
        },

        [=](http_put_atom atom,
            const std::string &scheme_host_port,
            const std::string &path,
            const httplib::Headers &headers,
            const std::string &body,
            const httplib::Params &params,
            const std::string &content_type) {
            return delegate(
                pool, atom, scheme_host_port, path, headers, params, body, content_type);
        },

        [=](http_put_simple_atom atom,
            const std::string &scheme_host_port,
            const std::string &path) {
            return delegate(
                pool,
                atom,
                scheme_host_port,
                path,
                httplib::Headers(),
                httplib::Params(),
                "",
                "");
        },

        [=](http_put_simple_atom atom,
            const std::string &scheme_host_port,
            const std::string &path,
            const httplib::Headers &headers) {
            return delegate(
                pool, atom, scheme_host_port, path, headers, httplib::Params(), "", "");
        },

        [=](http_put_simple_atom atom,
            const std::string &scheme_host_port,
            const std::string &path,
            const httplib::Headers &headers,
            const httplib::Params &params) {
            return delegate(pool, atom, scheme_host_port, path, headers, params, "", "");
        },

        [=](http_put_simple_atom atom,
            const std::string &scheme_host_port,
            const std::string &path,
            const httplib::Headers &headers,
            const std::string &body,
            const std::string &content_type) {
            return delegate(
                pool,
                atom,
                scheme_host_port,
                path,
                headers,
                httplib::Params(),
                body,
                content_type);
        },

        [=](http_put_simple_atom atom,
            const std::string &scheme_host_port,
            const std::string &path,
            const httplib::Headers &headers,
            const std::string &body,
            const httplib::Params &params,
            const std::string &content_type) {
            return delegate(
                pool, atom, scheme_host_port, path, headers, params, body, content_type);
        });
}
