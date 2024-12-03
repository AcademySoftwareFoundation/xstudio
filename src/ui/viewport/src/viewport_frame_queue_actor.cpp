// SPDX-License-Identifier: Apache-2.0
#include "xstudio/atoms.hpp"
#include "xstudio/media_reader/image_buffer_set.hpp"
#include "xstudio/ui/viewport/viewport_frame_queue_actor.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/utility/uuid.hpp"

using namespace xstudio::ui::viewport;
using namespace xstudio::utility;
using namespace xstudio;

ViewportFrameQueueActor::ViewportFrameQueueActor(
    caf::actor_config &cfg, caf::actor viewport, std::map<utility::Uuid, caf::actor> overlay_actors,
    const std::string &viewport_name, caf::actor colour_pipeline)
    : caf::event_based_actor(cfg), viewport_(viewport), viewport_overlay_plugins_(std::move(overlay_actors)),
    viewport_name_(viewport_name),
    colour_pipeline_(colour_pipeline) {

    print_on_exit(this, "ViewportFrameQueueActor");

    set_default_handler(caf::drop);

    set_down_handler([=](down_msg &msg) {
        // find in playhead list..
        if (msg.source == playhead_) {
            demonitor(playhead_);
            playhead_ = caf::actor();
            frames_to_draw_per_playhead_.clear();
        }
    });

    behavior_.assign(
        [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {},

        [=](viewport_playhead_atom,
            caf::actor playhead,
            const bool prefetch_inital_image) -> result<bool> {
            return set_playhead(playhead, prefetch_inital_image);            
        },

        [=](playhead::child_playheads_deleted_atom,
            const std::vector<utility::Uuid> &child_playhead_uuids) {
            for (const auto &uuid : child_playhead_uuids) {
                deleted_playheads_.insert(uuid);
            }
        },

        [=](playhead::key_child_playhead_atom,
            const utility::UuidVector &child_playhead_uuids) {
            sub_playhead_ids_ = child_playhead_uuids;
        },

        [=](playhead::key_child_playhead_atom,
            utility::UuidActor sub_playhead,
            const utility::time_point tp) {
            // playhead is telling us its key sub-playhead has changed. Request
            // an image buffer from the sub-playhead.

            // The logic here is to try and ensure the viewport updates with an
            // imag when the user is quickly scrolling through the media list.
            // Every time the media selection in the UI changes, the playhead
            // creates a new subplayhead for each bit of media selected and
            // destroys the old sub-playheads for media that was previously
            // selected but is no longer selected.

            // It might be that the sub-playheads are created at a faster rate
            // than the media reader can load/decode a frame for the given
            // media for display.

            // So, we do our best here. We just need to make sure we are showing
            // the most recent frame that has come from the subplayhead(s).

            // check if we've got this switch message out-of-order, i.e. we have
            // already processed a message that was sent *AFTER* the one we are
            // processing now.
            if (tp < last_playhead_switch_tp_)
                return;
            last_playhead_switch_tp_ = tp;

            request(sub_playhead.actor(), infinite, playhead::buffer_atom_v)
                .then(
                    [=](media_reader::ImageBufPtr &intial_frame) {
                        // ensure we only store this intial_frame for the viewport
                        // and set the playhead ID if it's come via a message that came
                        // *after* the last time we set the current_key_sub_playhead_id_
                        if (tp >= last_playhead_set_tp_) {

                            queue_image_buffer_for_drawing(intial_frame, sub_playhead.uuid());
                            if (current_key_sub_playhead_id_ != sub_playhead.uuid()) {
                                previous_key_sub_playhead_id_ = current_key_sub_playhead_id_;
                            }
                            current_key_sub_playhead_id_ = sub_playhead.uuid();
                            clear_images_from_old_playheads();
                            last_playhead_set_tp_ = tp;
                        }
                    },
                    [=](caf::error &err) {
                        // playhead failed to return an image for the given sub playhead id.
                        // Ignore the error, as it's quite likely due to the sub playhead
                        // exiting as the parent playhead rebuilt itself as user rapidly
                        // switched the media they were viewing
                    });
        },

        [=](playhead::colour_pipeline_lookahead_atom,
            const media::AVFrameIDsAndTimePoints &frame_ids_for_colour_mgmnt_lookeahead) {
            // For each media source that is within the lookahead read region, during playback,
            // we are given a single AVFrameID. The AVFrameID carries all of the colour
            // management metadata for the given source needed by the colour management plugin
            // ('colour_pipeline_' to set-up LUTs and other variables for displaying frames from
            // the source on-screen in the desired colourspace etc. Here, we get the
            // colour_pipeline_ to compute all the data it needs for these frames, even though
            // they aren't going to immediately be on screen. This should give enough time to do
            // slow work and prevent stuttering that would otherwise happen while we wait for
            // colour pipe to do its thing at draw time. We assume that the colour_pipeline_ has
            // an effective local cacheing system so when we do actually need that data
            // immediately at draw time it will be available immediately.
            anon_send(
                colour_pipeline_,
                colour_pipeline::get_colour_pipe_data_atom_v,
                frame_ids_for_colour_mgmnt_lookeahead);
        },

        [=](playhead::play_atom, const bool playing) { playing_ = playing; },

        [=](playhead::play_forward_atom, const bool forward) { playing_forwards_ = forward; },

        [=](ui::fps_monitor::framebuffer_swapped_atom,
            const utility::time_point &message_send_tp,
            const timebase::flicks video_refresh_rate_hint) {

            // this incoming message originates from the video layer and 'message_send_tp'
            // should be, as accurately as possible, the actual time that the framebuffer was
            // swapped to the screen.
            if (playing_) {
                video_refresh_data_.refresh_history_.push_back(message_send_tp);
                if (video_refresh_data_.refresh_history_.size() > 128) {
                    video_refresh_data_.refresh_history_.pop_front();
                }
            }
            video_refresh_data_.refresh_rate_hint_  = video_refresh_rate_hint;
            video_refresh_data_.last_video_refresh_ = message_send_tp;
            
        },

        [=](playhead::show_atom,
            const utility::Uuid &playhead_uuid,
            media_reader::ImageBufPtr buf,
            const utility::FrameRate & /*rate*/,
            const bool is_playing,
            const bool is_onscreen_frame) {

            playing_ = is_playing;
            queue_image_buffer_for_drawing(buf, playhead_uuid);
            drop_old_frames(utility::clock::now() - std::chrono::milliseconds(100));
            anon_send(viewport_, playhead::redraw_viewport_atom_v);

        },

        // these are frame bufs that we expect to draw in the very near future
        [=](playhead::show_atom,
            const utility::Uuid &playhead_uuid,
            std::vector<media_reader::ImageBufPtr> future_bufs) {                
            // now insert the new future frames ready for drawing
            for (auto &buf : future_bufs) {
                if (buf) {
                    queue_image_buffer_for_drawing(buf, playhead_uuid);
                }
            }
            drop_old_frames(utility::clock::now() - std::chrono::milliseconds(100));
        },

        [=](viewport_get_next_frames_for_display_atom,
            const utility::time_point &when_going_on_screen)
            -> result<media_reader::ImageBufDisplaySetPtr> {
            auto rp = make_response_promise<media_reader::ImageBufDisplaySetPtr>();
            get_frames_for_display(rp, when_going_on_screen);
            return rp;
        },

        [=](viewport_get_next_frames_for_display_atom)
            -> result<media_reader::ImageBufDisplaySetPtr> {
            auto rp = make_response_promise<media_reader::ImageBufDisplaySetPtr>();
            get_frames_for_display(rp);
            return rp;
        },

        [=](viewport_get_next_frames_for_display_atom,
            const bool force_sync) -> result<media_reader::ImageBufDisplaySetPtr> {
            auto rp = make_response_promise<media_reader::ImageBufDisplaySetPtr>();
            if (!force_sync || !playhead_) {
                // here the result is based on the frames that the playhead is
                // broadcasting asynchronously to us
                rp.delegate(
                    caf::actor_cast<caf::actor>(this),
                    viewport_get_next_frames_for_display_atom_v);
            } else {
                get_frames_for_display_sync(rp);
            }
            return rp;
        },

        [=](viewport_get_next_frames_for_display_atom,
            const media_reader::ImageBufPtr &lone_image) -> result<media_reader::ImageBufDisplaySetPtr> {

            // If something wants the viewport to render a single image that has 
            // been fetched independently (for example this is how offscreen 
            // generation of thumbnail images is donw) we need to run this 
            // through our routine that adds colour management and overlay data
            // to the ImageBuffer and buid an ImageBufDisplaySetPtr for rendering.
            auto rp = make_response_promise<media_reader::ImageBufDisplaySetPtr>();
            static const utility::Uuid dummy_playhead_id = utility::Uuid::generate();
            media_reader::ImageBufDisplaySet * result = new media_reader::ImageBufDisplaySet({dummy_playhead_id});
            result->add_on_screen_image(dummy_playhead_id, lone_image);
            append_overlays_data(rp, result);
            return rp;
        },

        [=](viewport_layout_atom, caf::actor layout_manager, const std::string & layout_name) {
            viewport_layout_manager_ = layout_manager;
            viewport_layout_mode_name_ = layout_name;
        },

        [=](utility::event_atom,
            playhead::media_source_atom,
            caf::actor media_actor,
            const utility::Uuid &media_uuid) {
            anon_send(
                colour_pipeline_,
                utility::event_atom_v,
                playhead::media_source_atom_v,
                media_actor,
                media_uuid);
        },
        [=](utility::event_atom, playhead::velocity_atom, const float velocity) {
            playhead_velocity_ = velocity;
        },
        [=](const error &err) mutable { aout(this) << err << std::endl; });
}

ViewportFrameQueueActor::~ViewportFrameQueueActor() {
}

void ViewportFrameQueueActor::on_exit() {
    viewport_layout_manager_ = caf::actor();
    playhead_ = caf::actor();
    caf::event_based_actor::on_exit();
}

xstudio::media_reader::ImageBufPtr ViewportFrameQueueActor::get_least_old_image_in_set(
    const OrderedImagesToDraw &frames_queued_for_display) {

    if (frames_queued_for_display.empty())
        return media_reader::ImageBufPtr();
    media_reader::ImageBufPtr least_old_buf;
    auto r        = frames_queued_for_display.begin();
    least_old_buf = r->second;
    r++;
    while (r != frames_queued_for_display.end()) {
        if (r->second.when_to_display_ > least_old_buf.when_to_display_) {
            least_old_buf = r->second;
        }
        r++;
    }
    return least_old_buf;
}

void ViewportFrameQueueActor::drop_old_frames(const utility::time_point out_of_date_threshold) {

    // remove old frames from the queue against a threshold

    for (auto &p : frames_to_draw_per_playhead_) {
        auto &frames_queued_for_display = p.second;
        media_reader::ImageBufPtr least_old_buf =
            get_least_old_image_in_set(frames_queued_for_display);
        auto r = frames_queued_for_display.begin();
        while (r != frames_queued_for_display.end()) {
            if (r->second.when_to_display_ < out_of_date_threshold) {
                r = frames_queued_for_display.erase(r);
            } else {
                r++;
            }
        }
        // if all the images in the queue are older than out_of_date_threshold then
        // we add back in the 'least old' buffer so we have something to show.
        if (frames_queued_for_display.empty() && least_old_buf) {
            frames_queued_for_display[least_old_buf.timeline_timestamp()] = least_old_buf;
        }
    }
}

void ViewportFrameQueueActor::queue_image_buffer_for_drawing(
    media_reader::ImageBufPtr &buf, const utility::Uuid &playhead_id) {

    auto &frames_queued_for_display = frames_to_draw_per_playhead_[playhead_id];
    frames_queued_for_display[buf.timeline_timestamp()] = buf;

}

void ViewportFrameQueueActor::get_frames_for_display_sync(
    caf::typed_response_promise<media_reader::ImageBufDisplaySetPtr> rp)
{

    // in 'force_sync' mode we request and wait for the playhead
    // to read all images for the current set of on-screen sources
    media_reader::ImageBufDisplaySet * result = new media_reader::ImageBufDisplaySet(sub_playhead_ids_);
    auto count = std::make_shared<int>(sub_playhead_ids_.size());
    int k_idx = 0;
    for (const auto playhead_id: sub_playhead_ids_) {

        if (current_key_sub_playhead_id_ == playhead_id) {
            result->set_hero_sub_playhead_index(k_idx);
        }
        k_idx++;
        // here we fetch the on-screen image buffer for the given sub-playhead
        // from the playhead
        const auto id = playhead_id;
        request(playhead_, infinite, playhead::buffer_atom_v, playhead_id)
            .then(
            [=](const media_reader::ImageBufPtr &buf) mutable {
                if (buf) {
                    result->add_on_screen_image(id, buf);
                }
                (*count)--;
                if (!(*count)) {
                    // we have all images, now proceed to add extra data
                    append_overlays_data(rp, result);
                }
                
            },
            [=](caf::error &err) mutable { 
                rp.deliver(err); 
                (*count)--;
                if (!(*count)) {
                    append_overlays_data(rp, result);
                }
            });
    }
                
}
                

void ViewportFrameQueueActor::get_frames_for_display(
    caf::typed_response_promise<media_reader::ImageBufDisplaySetPtr> rp,
    const utility::time_point &when_going_on_screen
    ) {

    // evaluate the position of the playhead at the timepoint when the viewport
    // redraw happens (or, more precisely, when the buffer that it is drawn
    // to is swapped to the display)
    const auto playhead_position = when_going_on_screen == utility::time_point()
                                       ? predicted_playhead_position_at_next_video_refresh()
                                       : predicted_playhead_position(when_going_on_screen);

    media_reader::ImageBufDisplaySet * result = new media_reader::ImageBufDisplaySet(sub_playhead_ids_);

    int k_idx = 0;
    for (const auto playhead_id: sub_playhead_ids_) {

        if (current_key_sub_playhead_id_ == playhead_id) {
            result->set_hero_sub_playhead_index(k_idx);
        }
        if (previous_key_sub_playhead_id_ == playhead_id) {
            result->set_previous_hero_sub_playhead_index(k_idx);
        }
        k_idx++;

        if (frames_to_draw_per_playhead_.find(playhead_id) == frames_to_draw_per_playhead_.end()) {
            // no images queued for display for the indicated playhead
            continue;
        }

        // find the entry in our queue of frames to draw whose display timestamp
        // is closest to 'now'
        auto &frames_queued_for_display = frames_to_draw_per_playhead_[playhead_id];
        if (frames_queued_for_display.empty()) {
            continue;
        }

        auto r = frames_queued_for_display.upper_bound(playhead_position);

        if (r == frames_queued_for_display.end()) {
            // No entry in the queue has a higher display timestamp
            // than the current playhead position, so pick the last
            r--;
        } else if (
            r != frames_queued_for_display.begin() &&
            r->second.timeline_timestamp() != playhead_position) {

            // upper bound gives us the first frame whose timeline
            // timestamp is *after* playhead_position, so we decrement
            // it once to get the frame that should be on screen.
            r--;
        }

        result->add_on_screen_image(playhead_id, r->second);

        // now we add 'future frames' - i.e. frames that are not onscreen now 
        // but will be going on-screen next. We supply these to the viewport so
        // that it can do asynchronous transfers of data to VRAM, i.e. copying
        // the image data to the GPU for the next image(s) while the current image
        // is being drawn to the screen.
        auto r_next = r;
        if (playing_forwards_) {
            r_next++;
            while (r_next != frames_queued_for_display.end() && result->num_future_images(playhead_id) < 2) {

                result->append_future_image(playhead_id, r_next->second);
                r_next++;
                if (r_next == frames_queued_for_display.end()) {
                    r_next = frames_queued_for_display.begin();
                }
            }
        } else {
            while (r_next != frames_queued_for_display.begin() && result->num_future_images(playhead_id) < 2) {
                r_next--;
                result->append_future_image(playhead_id, r_next->second);
                if (r_next == frames_queued_for_display.begin()) {
                    r_next = frames_queued_for_display.end();
                    r_next--;
                }
            }
        }

    }

    // now that we have picked frames for display, the first frame in 'next_images'
    // is the one that should be on-screen now ... and frames that should have been
    // displayed before this one can be dropped from the queue
    if (!result->empty()) {
        // drop_old_frames(result->when_to_display());
    }

    append_overlays_data(rp, result);

}

void ViewportFrameQueueActor::append_overlays_data(
    caf::typed_response_promise<media_reader::ImageBufDisplaySetPtr> rp,
    media_reader::ImageBufDisplaySet * result) {

    // In the next steps we are doing a-sync requests to get data added to
    // 'result'... we make a counter of the number of requests we'll be making
    // and decrement it until it hits zero then we know we are finished. The
    // counter has to be a shared pointer as the lambda request response handlers
    // make their own copy of response_count. If it weren't a shared pointer the
    // handler would be decrementing their copy, not a global (shared) counter.
    auto response_count = std::make_shared<int>(viewport_overlay_plugins_.size()*result->num_onscreen_images());
    if (!*response_count) {
        result->finalise();
        rp.deliver(media_reader::ImageBufDisplaySetPtr(result));
        return;
    }

    // In this loop, we add colour management data to each of the images that
    // is about to go on-screen. Within the loop, we then give any overlay
    // plugins an opportunity to add data to the images that they will need
    // at draw time to draw graphics onto the screen
    for (int img_idx = 0; img_idx < result->num_onscreen_images(); ++img_idx) {
        
        if (!result->onscreen_image(img_idx)) {
            // no image ? Not expected, but we'll handle this just in case. Skip
            // adding overlay data or colour data as there is no image.
            *response_count = *response_count - (viewport_overlay_plugins_.size());
            if (!*response_count) {
                result->finalise();
                rp.deliver(media_reader::ImageBufDisplaySetPtr(result));
                break;
            } else {
                continue;
            }
        }

        request(
            colour_pipeline_,
            infinite,
            colour_pipeline::get_colour_pipe_data_atom_v,
            result->onscreen_image(img_idx)).then(
                [=](media_reader::ImageBufPtr image_with_colour_data) mutable {
                    result->set_on_screen_image(img_idx, image_with_colour_data);
                    append_overlays_data(rp, img_idx, result, response_count);

                },
                [=](caf::error & err) mutable {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                    append_overlays_data(rp, img_idx, result, response_count);
                });
    }

}
            
void ViewportFrameQueueActor::append_overlays_data(
    caf::typed_response_promise<media_reader::ImageBufDisplaySetPtr> rp,
    const int img_idx,
    media_reader::ImageBufDisplaySet * result,
    std::shared_ptr<int> response_count
    ) 
{

    auto check_and_respond = [=]() mutable {
        (*response_count)--;
        if (!*response_count) {
            media_reader::ImageBufDisplaySetPtr v(result);
            if (viewport_layout_manager_) {
                // if we have a layout manager, get it to provide the transforms
                // for the layout. Some layout plugins are provided by python,
                // and could be slower so we have 0.25s timeout
                result->finalise();
                request(viewport_layout_manager_, std::chrono::milliseconds(250), viewport_layout_atom_v, viewport_layout_mode_name_, v).then(
                    [=](const media_reader::ImageSetLayoutDataPtr &layout_data) mutable {
                        
                        result->set_layout_data(layout_data);

                        // here we set the layout transform matrix on the 
                        // image buffers, if available
                        for (int i = 0; i< result->num_onscreen_images(); ++i) {
                            if (i <= (int)layout_data->image_transforms_.size()) {
                                media_reader::ImageBufPtr &im = result->onscreen_image_m(i);
                                im.set_layout_transform(layout_data->image_transforms_[i]);
                            }
                        }

                        rp.deliver(v);
                    }, 
                    [=](caf::error & err) mutable {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                        result->finalise();
                        rp.deliver(v);
                    });
            } else {
                result->finalise();
                rp.deliver(v);
            }
        }
    };

    for (auto p: viewport_overlay_plugins_) {

        utility::Uuid overlay_plugin_uuid = p.first;
        caf::actor overlay_plugin         = p.second;

        request(
            overlay_plugin,
            infinite,
            prepare_overlay_render_data_atom_v,
            result->onscreen_image(img_idx),
            viewport_name_).then(
            [=](const utility::BlindDataObjectPtr &bdata) mutable {

                result->onscreen_image_m(img_idx).add_plugin_blind_data(
                    overlay_plugin_uuid, bdata);
                check_and_respond();
                
            }, 
            [=](caf::error & err) mutable {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                check_and_respond();
            });
    }
                    
}

void ViewportFrameQueueActor::clear_images_from_old_playheads() {

    for (const auto &uuid : deleted_playheads_) {

        // even if current_key_sub_playhead_id_ has been deleted, we don't
        // want to clear its images as we need to hang onto them for display
        // until we get a new current_key_sub_playhead_id_ (with images)
        if (uuid == current_key_sub_playhead_id_)
            continue;

        auto it = frames_to_draw_per_playhead_.find(uuid);
        if (it != frames_to_draw_per_playhead_.end()) {
            frames_to_draw_per_playhead_.erase(it);
        }
    }
    deleted_playheads_.clear();
}

timebase::flicks
ViewportFrameQueueActor::predicted_playhead_position(const utility::time_point &when) {
    if (!playhead_)
        return timebase::flicks(0);

    const timebase::flicks video_refresh_period = compute_video_refresh();

    caf::scoped_actor sys(system());
    try {

        // we need the playhead to estimate what its position will be when we next swap an image
        // onto the screen. We then use this to pick which of the frames that we've been sent to
        // show we actually show.
        auto estimate_playhead_position_at_next_redraw = request_receive_wait<timebase::flicks>(
            *sys,
            playhead_,
            std::chrono::milliseconds(100),
            playhead::position_atom_v,
            when,
            video_refresh_period);

        if (!playing_)
            return estimate_playhead_position_at_next_redraw;

        // To enact a stable pulldown we need to round this value down to a multiple of the
        // video_refresh_period. There is a catch, though. The playhead is 'free running' and is
        // not synced in any way to the video refresh. There will be some error in
        // 'estimate_playhead_position_at_next_redraw' due to jitter or innacuracies in the
        // video refresh signal we get from the UI layer. Doing a straight rounding of the
        // number could therefore mean dropping frames or otherwise innacurate pulldown as the
        // rounded playhead position might erratically bounce around the frame transition
        // boundary in the timeline. To overcome this we add a 'phase adjustment' that tries to
        // ensure we are about mid way between video refresh beats before we do the rounding.
        // The key is that we only change this phase adjustment occasionally as the phase
        // between the playhead and the video refresh beats drifts.

        const long playhead_velocity_ct = long(double(video_refresh_period.count())*playhead_velocity_);
        timebase::flicks phase_adjusted_tp =
            estimate_playhead_position_at_next_redraw + playhead_vid_sync_phase_adjust_;
        timebase::flicks rounded_phase_adjusted_tp = timebase::flicks(
            playhead_velocity_ct *
            (phase_adjusted_tp.count() / playhead_velocity_ct));
        const double phase =
            timebase::to_seconds(phase_adjusted_tp - rounded_phase_adjusted_tp) /
            timebase::to_seconds(video_refresh_period);

        if (phase < 0.1 || phase > 0.9) {
            playhead_vid_sync_phase_adjust_ = timebase::flicks(
                playhead_velocity_ct / 2 -
                estimate_playhead_position_at_next_redraw.count() +
                playhead_velocity_ct *
                    (estimate_playhead_position_at_next_redraw.count() /
                     playhead_velocity_ct));
            phase_adjusted_tp =
                estimate_playhead_position_at_next_redraw + playhead_vid_sync_phase_adjust_;
            rounded_phase_adjusted_tp = timebase::flicks(
                playhead_velocity_ct *
                (phase_adjusted_tp.count() / playhead_velocity_ct));
        }
        return rounded_phase_adjusted_tp;


    } catch (std::exception &e) {
        spdlog::debug("{} {}", __PRETTY_FUNCTION__, e.what());
    }
    return timebase::flicks(0);
}


timebase::flicks ViewportFrameQueueActor::predicted_playhead_position_at_next_video_refresh() {

    if (!playhead_)
        return timebase::flicks(0);

    const timebase::flicks video_refresh_period     = compute_video_refresh();
    const utility::time_point next_video_refresh_tp = next_video_refresh(video_refresh_period);
    return predicted_playhead_position(next_video_refresh_tp);
}

xstudio::utility::time_point ViewportFrameQueueActor::next_video_refresh(
    const timebase::flicks &video_refresh_period) const {

    // it's possible we are not receiving refresh signals from the UI layer on
    // completion of the glXSwapBuffers(), so we have to do some sanity checks
    // and then make up an appropriate refresh time if we need to.
    utility::time_point last_vid_refresh;
    if (video_refresh_data_.refresh_history_.empty()) {

        last_vid_refresh =
            utility::clock::now() -
            std::chrono::duration_cast<std::chrono::microseconds>(video_refresh_period);

    } else if (video_refresh_data_.refresh_history_.size() > 64) {

        // refresh_history_ is a list of recent timepoints (system steady clock) when we were
        // told (utlimately by Qt or graphics driver) that the video frame buffer was swapped.
        // We're using this data to predict when the video buffer will be swapped to the
        // screen NEXT time and therefore pick the correct frame to go up on the screen.
        //
        // We might know the video refresh exactly, or we might have been lied to, but either
        // way we need to know the phase of the refresh beat to predict when the next refresh
        // is due. So we need to fit a line to the video refresh events (as measured by the
        // system clock) and filter out events that are innaccurate and also take account
        // of moments when a video refresh was missed completely.


        // average cadence of video refresh...
        const double expected_video_refresh_period = average_video_refresh_period();

        // Here we are essentially fitting a straight line to the video refresh event
        // timepoints - we use the line to predict when the next video refresh is
        // going to happen.
        auto now                   = utility::clock::now();
        double next_refresh        = 0.0;
        double refresh_event_index = 1.0;
        double estimate_count      = 0.0;
        auto p                     = video_refresh_data_.refresh_history_.rbegin();
        auto p2                    = p;
        p2++;
        while (p2 != video_refresh_data_.refresh_history_.rend()) {

            // period between subsequent video refreshes
            auto delta = std::chrono::duration_cast<timebase::flicks>(*p - *p2);

            // how many whole video refresh beats is this? It's possible that sometimes
            // a redraw doesn't happen within the video refresh period. We need to take
            // account of that when using the timepoints of video refreshes to predict
            // the next refresh
            double n_refreshes_between_events =
                round(timebase::to_seconds(delta) / expected_video_refresh_period);

            auto estimate_refresh =
                timebase::to_seconds(std::chrono::duration_cast<timebase::flicks>(*p - now)) +
                refresh_event_index * expected_video_refresh_period;
            next_refresh += estimate_refresh;
            estimate_count++;
            p++;
            p2++;
            refresh_event_index += n_refreshes_between_events;
        }

        next_refresh *= 1.0 / estimate_count;
        auto offset = std::chrono::duration_cast<std::chrono::microseconds>(
            timebase::to_flicks(next_refresh));
        auto result = now + offset;
        return result;

    } else {

        if (std::chrono::duration_cast<timebase::flicks>(
                utility::clock::now() - video_refresh_data_.last_video_refresh_) <
            timebase::k_flicks_one_fifteenth_second) {
            last_vid_refresh = video_refresh_data_.last_video_refresh_;
        } else {
            last_vid_refresh =
                utility::clock::now() -
                std::chrono::duration_cast<std::chrono::microseconds>(video_refresh_period);
        }
    }
    return last_vid_refresh +
           std::chrono::duration_cast<std::chrono::microseconds>(video_refresh_period);
}

timebase::flicks ViewportFrameQueueActor::compute_video_refresh() const {

    if (video_refresh_data_.refresh_rate_hint_ != timebase::k_flicks_zero_seconds) {

        // we've got system information about the refresh, let's trust it
        return video_refresh_data_.refresh_rate_hint_;

    } else if (video_refresh_data_.refresh_history_.size() > 64) {

        // This measurement of the refresh rate is only accurate if the UI layer
        // (probably Qt) is giving us time-accurate signals when the GLXSwapBuffers
        // call completes. Also the assumption is that the UI redraw is limited to
        // the display refresh rate (which happens if the draw time isn't longer than
        // the refresh period and also that the swap buffers is synced to VBlank).
        // Here we try to match out measurement with commong video refresh rates:

        // Assume 24fps is the minimum refresh we'll ever encounter
        const int hertz_refresh =
            std::max(24, int(round(1.0 / average_video_refresh_period())));

        static const std::vector<int> common_refresh_rates(
            {24, 25, 30, 48, 60, 75, 90, 120, 144, 240, 360});
        auto match = std::lower_bound(
            common_refresh_rates.begin(), common_refresh_rates.end(), hertz_refresh);
        if (match != common_refresh_rates.end() && abs(*match - hertz_refresh) < 2) {
            return timebase::to_flicks(1.0 / double(*match));
        }
    }

    // default fallback to 60Hz
    return timebase::k_flicks_one_sixtieth_second;
}

double ViewportFrameQueueActor::average_video_refresh_period() const {

    if (video_refresh_data_.refresh_history_.size() < 64) {
        return 1.0 / 60.0;
    }

    // Here, take the delta time between subsequent video refresh messages
    // and take the average. Ignore the lowest 8 and highest 8 deltas ..
    std::vector<timebase::flicks> deltas;
    deltas.reserve(video_refresh_data_.refresh_history_.size());
    auto p  = video_refresh_data_.refresh_history_.begin();
    auto pp = p;
    pp++;
    while (pp != video_refresh_data_.refresh_history_.end()) {
        deltas.push_back(std::chrono::duration_cast<timebase::flicks>(*pp - *p));
        pp++;
        p++;
    }
    std::sort(deltas.begin(), deltas.end());

    auto r = deltas.begin() + 8;
    int ct = int(deltas.size()) - 16;
    timebase::flicks t(0);
    while (ct--) {
        t += *(r++);
    }

    return timebase::to_seconds(t) / (double(deltas.size() - 16));
}

caf::typed_response_promise<bool> ViewportFrameQueueActor::set_playhead(caf::actor playhead, const bool prefetch_inital_image)
{

    auto self = caf::actor_cast<caf::actor>(this);

    if (playhead_broadcast_group_) {
        // stop getting broadcasts from the previous playhead
        anon_send(playhead_broadcast_group_, broadcast::leave_broadcast_atom_v, self);
        playhead_broadcast_group_ = caf::actor();
    }

    auto rp = make_response_promise<bool>();
    // join the playhead's broadcast group - image buffers are streamed to
    // us via the broacast group
    request(playhead, infinite, broadcast::join_broadcast_atom_v, self).then(
        [=](const bool) mutable {

            // Get the 'key' child playhead UUID
            request(playhead, infinite, playhead::key_child_playhead_atom_v).then(
                [=](utility::UuidVector curr_playhead_uuids) mutable {

                    if (playhead_)
                        demonitor(playhead_);
                    playhead_ = playhead;
                    monitor(playhead_);

                    // this message will make the playhead re-broadcaset the media_source_atom
                    // event to it's 'broacast' group (of which we are a member). This info
                    // from the playhead is received in a message handler below and we
                    // send on the info about the media source to our colour pipeline
                    // which needs to do some set-up.
                    send(playhead_, playhead::media_source_atom_v, true, true);
                    send(playhead_, playhead::jump_atom_v);
                    request(playhead_, infinite, playhead::velocity_atom_v).then(
                        [=](float v) {
                            playhead_velocity_ = v;
                        },
                        [=](caf::error &err) {});

                    if (curr_playhead_uuids.empty()) return;
                    current_key_sub_playhead_id_ = curr_playhead_uuids.back();
                    curr_playhead_uuids.pop_back();
                    sub_playhead_ids_ = curr_playhead_uuids;
                    rp.deliver(true);
                    
                },
                [=](const error &err) mutable { 
                    rp.deliver(err); 
                });
        },
        [=](const error &err) mutable {
            rp.deliver(err); 
        });
    return rp;
}