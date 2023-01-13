// SPDX-License-Identifier: Apache-2.0
#include "xstudio/atoms.hpp"
#include "xstudio/ui/viewport/viewport_frame_queue_actor.hpp"
#include "xstudio/utility/edit_list.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/utility/uuid.hpp"

using namespace xstudio::ui::viewport;
using namespace xstudio::utility;

ViewportFrameQueueActor::ViewportFrameQueueActor(
    caf::actor_config &cfg, std::map<utility::Uuid, caf::actor> overlay_actors)
    : caf::event_based_actor(cfg), overlay_actors_(std::move(overlay_actors)) {

    set_default_handler(caf::drop);

    behavior_.assign(
        [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {},

        [=](viewport_playhead_atom,
            caf::actor playhead,
            const bool prefetch_inital_image) -> result<bool> {
            auto rp = make_response_promise<bool>();
            // join the playhead's broadcast grop
            request(playhead, infinite, utility::get_group_atom_v)
                .then(
                    [=](caf::actor broadcast_group) mutable {
                        caf::scoped_actor sys(system());

                        if (playhead_broadcast_group_) {
                            try {
                                request_receive<bool>(
                                    *sys,
                                    playhead_broadcast_group_,
                                    broadcast::leave_broadcast_atom_v,
                                    this);
                            } catch (...) {
                            }
                        }
                        playhead_broadcast_group_ = broadcast_group;
                        request_receive<bool>(
                            *sys,
                            playhead_broadcast_group_,
                            broadcast::join_broadcast_atom_v,
                            this);
                    },
                    [=](const error &err) mutable {

                    });

            // Get the 'key' child playhead UUID
            request(playhead, infinite, playhead::key_child_playhead_atom_v)
                .then(
                    [=](const utility::Uuid &curr_playhead_uuid) mutable {
                        current_playhead_ = curr_playhead_uuid;
                        // fetch a frame so we have somethign to show immediately
                        if (prefetch_inital_image) {
                            request(playhead, infinite, playhead::buffer_atom_v)
                                .then(
                                    [=](const media_reader::ImageBufPtr &buf) mutable {
                                        onscreen_image_[curr_playhead_uuid] = buf;
                                        queue_image_buffer_for_drawing(
                                            onscreen_image_[curr_playhead_uuid],
                                            curr_playhead_uuid);
                                        update_blind_data(
                                            std::vector<media_reader::ImageBufPtr>({buf}),
                                            false);
                                        rp.deliver(true);
                                    },
                                    [=](const error &err) mutable { rp.deliver(err); });
                        } else {
                            rp.deliver(true);
                        }
                    },
                    [=](const error &err) mutable { rp.deliver(err); });
            return rp;
        },

        [=](playhead::child_playheads_deleted_atom,
            const std::vector<utility::Uuid> &child_playhead_uuids) {
            child_playheads_deleted(child_playhead_uuids);
        },

        [=](playhead::key_child_playhead_atom,
            const utility::Uuid &playhead_uuid,
            const utility::time_point &tp) { current_playhead_ = playhead_uuid; },

        [=](playhead::play_atom, const bool playing) {
            if (!playing)
                drop_future_frames();
        },

        [=](playhead::show_atom,
            const utility::Uuid &playhead_uuid,
            media_reader::ImageBufPtr buf,
            const utility::FrameRate & /*rate*/,
            const bool is_playing,
            const bool is_onscreen_frame) {
            playing_ = is_playing;
            if (is_onscreen_frame) {
                onscreen_image_[playhead_uuid] = buf;
                queue_image_buffer_for_drawing(onscreen_image_[playhead_uuid], playhead_uuid);
            } else {
                queue_image_buffer_for_drawing(buf, playhead_uuid);
            }

            update_blind_data(std::vector<media_reader::ImageBufPtr>({buf}), false);
        },

        // these are frame bufs that we expect to draw in the very near future
        [=](playhead::show_atom,
            const utility::Uuid &playhead_uuid,
            std::vector<media_reader::ImageBufPtr> future_bufs) {
            // put the current frame back in the queue of frame buffers to be
            // drawn, if there is one
            if (onscreen_image_.find(playhead_uuid) != onscreen_image_.end()) {
                queue_image_buffer_for_drawing(onscreen_image_[playhead_uuid], playhead_uuid);
            }
            // now insert the new future frames ready for drawing
            for (auto &buf : future_bufs) {
                if (buf && buf.colour_pipe_data_) {
                    queue_image_buffer_for_drawing(buf, playhead_uuid);
                }
            }
            update_blind_data(future_bufs);
        },

        [=](viewport_get_next_frames_for_display_atom)
            -> result<std::vector<media_reader::ImageBufPtr>> {
            auto rp = make_response_promise<std::vector<media_reader::ImageBufPtr>>();

            std::vector<media_reader::ImageBufPtr> r;
            get_frames_for_display(current_playhead_, r);
            if (r.size() != 1 || playing_) {
                rp.deliver(r);
            } else if (r[0]) {

                update_image_blind_data_and_deliver(r[0], rp);

            } else {
                rp.deliver(r);
            }

            return rp;
        },

        [=](const error &err) mutable { aout(this) << err << std::endl; });
}

void ViewportFrameQueueActor::drop_future_frames() {
    // if playback has stopped, to prevent 'future frames' that are queued to display with only
    // a system clock timestamp from being put on screen we empty the queue here
    const auto now = utility::clock::now();
    for (auto &p : frames_to_draw_per_playhead_) {
        auto &image_set = p.second;
        auto r          = std::lower_bound(image_set.begin(), image_set.end(), now);
        if (r != image_set.begin())
            r--;
        image_set.erase(r, image_set.end());
    }
}

void ViewportFrameQueueActor::queue_image_buffer_for_drawing(
    media_reader::ImageBufPtr &buf, const utility::Uuid &playhead_id) {

    auto &image_set = frames_to_draw_per_playhead_[playhead_id];

    auto p = image_set.begin();
    while (p != image_set.end()) {
        if (*p == buf) {
            (*p).when_to_display_ = buf.when_to_display_;
            buf                   = (*p); // this copies blind data from our cache back to buf
            break;
        }
        p++;
    }

    if (p != frames_to_draw_per_playhead_[playhead_id].end()) {

        std::sort(image_set.begin(), image_set.end());

    } else {

        auto r = std::lower_bound(image_set.begin(), image_set.end(), buf.when_to_display_);
        image_set.insert(r, buf);
    }

    {
        // purge images in the queue that were supposed to be displayed before now.
        const auto now = utility::clock::now();

        auto r = std::lower_bound(image_set.begin(), image_set.end(), now);
        if (r != image_set.begin())
            r--;
        // remove any images in the queue that are out of date (their display timestamp is
        // further back in time than the image we are returning)
        auto n = std::distance(image_set.begin(), r);
        while (n) {
            image_set.erase(image_set.begin());
            n--;
        }
    }
}


void ViewportFrameQueueActor::get_frames_for_display(
    const utility::Uuid &playhead_id, std::vector<media_reader::ImageBufPtr> &next_images) {

    if (onscreen_image_.find(playhead_id) != onscreen_image_.end()) {
        // the first image in the list to be displayed is the image that should
        // be on-screen right now
        next_images.push_back(onscreen_image_[playhead_id]);
    }

    // if not in playback, we do not need to upload 'future' frames
    if (!playing_)
        return;

    if (frames_to_draw_per_playhead_.find(playhead_id) == frames_to_draw_per_playhead_.end()) {
        // no images queued for display for the indicated playhead
        return;
    }

    // find the entry in our queue of frames to draw whose display timestamp
    // is closest to 'now'
    auto &image_set = frames_to_draw_per_playhead_[playhead_id];
    if (image_set.empty()) {
        return;
    }

    const auto now = utility::clock::now();
    auto r         = std::lower_bound(image_set.begin(), image_set.end(), now);

    if (r == image_set.end()) {
        // No entry in the queue has a higher timestamp than 'now', so
        // use last entry in the queue.
        r--;
    } else if (r != image_set.begin()) {

        // lower bound gives us the first element that is NOT LESS than now.
        // If it's not the first element, then we want to use the previous frame
        // as it is the one that should be on screen NOW
        r--;
    }

    // check we haven't already added this via the 'onsreen_image_'
    if (next_images.size() && next_images.front() != *r) {
        next_images.push_back(*r);
    }

    auto r_next = r;
    r_next++;
    while (r_next != image_set.end() && next_images.size() < 4) {
        next_images.push_back(*r_next);
        r_next++;
    }


    // remove any images in the queue that are out of date (their display timestamp is
    // further back in time than the image we are returning)
    auto n = std::distance(image_set.begin(), r);
    while (n) {
        image_set.erase(image_set.begin());
        n--;
    }
}

void ViewportFrameQueueActor::child_playheads_deleted(
    const std::vector<utility::Uuid> &child_playhead_uuids) {

    for (const auto &uuid : child_playhead_uuids) {

        auto it = frames_to_draw_per_playhead_.find(uuid);
        if (it != frames_to_draw_per_playhead_.end()) {
            frames_to_draw_per_playhead_.erase(it);
        }
        auto it2 = onscreen_image_.find(uuid);
        if (it2 != onscreen_image_.end())
            onscreen_image_.erase(it2);
    }
}

void ViewportFrameQueueActor::add_blind_data_to_image_in_queue(
    const media_reader::ImageBufPtr &image,
    const utility::BlindDataObjectPtr &bdata,
    const utility::Uuid &overlay_actor_uuid) {
    for (auto &ons_pph : onscreen_image_) {
        if (ons_pph.second == image) {
            ons_pph.second.add_plugin_blind_data(overlay_actor_uuid, bdata);
        }
    }

    for (auto &per_playhead : frames_to_draw_per_playhead_) {
        auto &image_set = per_playhead.second;
        for (auto &im : image_set) {
            if (im == image) {
                im.add_plugin_blind_data(overlay_actor_uuid, bdata);
            }
        }
    }
}

void ViewportFrameQueueActor::update_blind_data(
    const std::vector<media_reader::ImageBufPtr> bufs, const bool wait) {

    // any overlay plugins that need to do computation before drawing
    // to the screen are given the opportunity to do that now, ahead
    // of time
    if (wait) {
        auto sys = caf::scoped_actor(home_system());
        for (auto p : overlay_actors_) {

            utility::Uuid overlay_actor_uuid = p.first;
            caf::actor overlay_actor         = p.second;
            for (const media_reader::ImageBufPtr im : bufs) {
                try {
                    auto bdata = request_receive<utility::BlindDataObjectPtr>(
                        *sys, overlay_actor, prepare_overlay_render_data_atom_v, im);
                    add_blind_data_to_image_in_queue(im, bdata, overlay_actor_uuid);
                } catch (std::exception &e) {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
                }
            }
        }

    } else {
        for (auto p : overlay_actors_) {

            utility::Uuid overlay_actor_uuid = p.first;
            caf::actor overlay_actor         = p.second;
            for (const media_reader::ImageBufPtr im : bufs) {
                request(overlay_actor, infinite, prepare_overlay_render_data_atom_v, im)
                    .then(
                        [=](const utility::BlindDataObjectPtr &bdata) {
                            add_blind_data_to_image_in_queue(im, bdata, overlay_actor_uuid);
                        },
                        [=](caf::error &err) {
                            spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                        });
            }
        }
    }
}

void ViewportFrameQueueActor::update_image_blind_data_and_deliver(
    const media_reader::ImageBufPtr &buf,
    caf::typed_response_promise<std::vector<media_reader::ImageBufPtr>> rp) {

    // make a copy of our pointer to the image buffer
    auto image_cpy = std::make_shared<media_reader::ImageBufPtr>(buf);

    // we need a ref counted counter to keep track of how many responses we expect
    auto ct = std::make_shared<int>((int)overlay_actors_.size());

    for (auto p : overlay_actors_) {

        utility::Uuid overlay_actor_uuid = p.first;
        caf::actor overlay_actor         = p.second;
        request(overlay_actor, infinite, prepare_overlay_render_data_atom_v, *image_cpy)
            .then(
                [=](const utility::BlindDataObjectPtr &bdata) mutable {
                    image_cpy->add_plugin_blind_data(overlay_actor_uuid, bdata);
                    (*ct)--;
                    if (!*ct) {
                        rp.deliver(std::vector<media_reader::ImageBufPtr>({*image_cpy}));
                    }
                },
                [=](caf::error &err) mutable {
                    *ct = 100;
                    rp.deliver(err);
                });
    }
}
