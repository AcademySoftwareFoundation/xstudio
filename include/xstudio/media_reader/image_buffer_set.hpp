// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "xstudio/media_reader/image_buffer.hpp"

namespace xstudio {
namespace media_reader {

    // A simple struct that contains matrices for transforming a set of images,
    // a vector of indexes providing the draw order of images in a set, the
    // overall display aspect of the layout and a json store for any custom
    // layout data. Layout data is attached to the ImageBufDisplaySet just 
    // before the viewport is rendererd.
    struct ImageSetLayoutData {        
        std::vector<Imath::M44f> image_transforms_;
        std::vector<int> image_draw_order_hint_;
        float layout_aspect_;
        utility::JsonStore custom_layout_data_;
    };
    typedef std::shared_ptr<const ImageSetLayoutData> ImageSetLayoutDataPtr;

    // A set of ImageBufPtrs for display. When xSTUDIO displays multiple images
    // on-screen, this is achieved by the playhead having multiple 'subPlayheads'.
    // Each sub-playhead is attached to a source, and requests image reads from
    // the media readers and then broadcasts the images to the parent playhead.
    // The parent playhead then re-broadcasts the images to the viewport.
    // The ImageBufDisplaySet is used to gather the images coming from the 
    // multiple sub-playheads into one object which is finally used at draw-
    // time for rendering the images to screen. The sub-playheads are somewhat
    // independent of each other, so the synchronisation at display time of
    // which images should be on-screen is handled by the ViewportFrameQueueActor
    class ImageBufDisplaySet {
        public:

            ImageBufDisplaySet() = default;
            ImageBufDisplaySet(const ImageBufDisplaySet & o) = default;
            ImageBufDisplaySet(const utility::UuidVector & sub_playhead_ids) : sub_playhead_ids_(sub_playhead_ids) {}

            // For a given sub-playhead, set the image that should be on-screen for the next viewport
            // redraw
            void add_on_screen_image(const utility::Uuid &sub_playhead_id, const ImageBufPtr & image) {
                image_set(sub_playhead_id)->on_screen_image = image;
            }

            // Overwrite the on-screen image at sub_playhead_idx
            void set_on_screen_image(const int &sub_playhead_idx, ImageBufPtr & image) {
                std::swap(onscreen_image_m(sub_playhead_idx), image);
            }

            // For a given sub-playhead, add and image that is expected to go on screen soon. These
            // should be added in the order that they are expected to hit the screen
            void append_future_image(const utility::Uuid &sub_playhead_id, const ImageBufPtr & image) {
                image_set(sub_playhead_id)->future_images.push_back(image);
            }

            // Loop through all images in the set to get the *earliest* display
            // timestamp in the set. This gives us the overall 'when_to_display'
            // estimate for the set. It's used by ViewportFrameQueueActor to
            // purge stale images from it's cache.
            [[nodiscard]] utility::time_point when_to_display() const {
                utility::time_point t = utility::time_point::max();
                for (const auto &p: sub_playhead_sets_) {
                    for (const auto & d: p.second->future_images) {
                        t = std::min(d.when_to_display_, t);
                    }
                }
                return t;
            }

            [[nodiscard]] const std::vector<ImageBufPtr> & future_images(const int sub_playhead_index) const { 
                return image_set(sub_playhead_index)->future_images; 
            }
            [[nodiscard]] int num_future_images(const utility::Uuid &playhead_id) const { 
                return int(image_set(playhead_id)->future_images.size()); 
            }
            [[nodiscard]] int num_future_images(const int sub_playhead_index) const { 
                return int(image_set(sub_playhead_index)->future_images.size());
            }
            [[nodiscard]] size_t images_keys_hash() const { return images_hash_; }
            [[nodiscard]] int num_onscreen_images() const { return int(sub_playhead_ids_.size()); }
            [[nodiscard]] bool empty() const { return sub_playhead_ids_.empty(); }
            [[nodiscard]] float layout_aspect() const { return layout_data_ ? layout_data_->layout_aspect_ : 16.0f/9.0f; }
            [[nodiscard]] int hero_sub_playhead_index() const { return hero_sub_playhead_index_; }
            [[nodiscard]] int previous_hero_sub_playhead_index() const { return previous_hero_sub_playhead_index_; }
            [[nodiscard]] const ImageSetLayoutDataPtr & layout_data() const { return layout_data_; }
            [[nodiscard]] const ImageBufPtr & onscreen_image(const int sub_playhead_index) const {
                return image_set(sub_playhead_index)->on_screen_image;
            }
            [[nodiscard]] const ImageBufPtr & hero_image() const {
                return image_set(hero_sub_playhead_index_)->on_screen_image;
            }

            [[nodiscard]] ImageBufPtr & onscreen_image_m(const int sub_playhead_index) {
                return image_set(sub_playhead_index)->on_screen_image;
            }
            [[nodiscard]] const utility::JsonStore & as_json() const { return as_json_; }
            [[nodiscard]] size_t images_layout_hash() const { return hash_; }

            void set_layout_data(const ImageSetLayoutDataPtr &layout_data) { layout_data_ = layout_data; }
            void set_hero_sub_playhead_index(const int idx) { hero_sub_playhead_index_ = idx; }
            void set_previous_hero_sub_playhead_index(const int idx) { previous_hero_sub_playhead_index_ = idx; }

            // this is called after an ImageBufDisplaySet has been built to set
            // up some internal read-only data
            void finalise();

        private:

            struct ImageSet {
                ImageBufPtr on_screen_image;
                std::vector<ImageBufPtr> future_images;
            };

            const std::shared_ptr<ImageSet> & image_set(const int sub_playhead_index) const {
                if (sub_playhead_index >= (int)sub_playhead_ids_.size()) return null_set_;
                return image_set(sub_playhead_ids_[sub_playhead_index]);
            }

            std::shared_ptr<ImageSet> & image_set(const int &sub_playhead_index) {
                if (sub_playhead_index >= (int)sub_playhead_ids_.size()) return null_set_;
                return image_set(sub_playhead_ids_[sub_playhead_index]);
            }

            const std::shared_ptr<ImageSet> & image_set(const utility::Uuid &playhead_id) const {
                auto p = sub_playhead_sets_.find(playhead_id);
                if (p == sub_playhead_sets_.end()) return null_set_;
                return p->second;
            }

            std::shared_ptr<ImageSet> & image_set(const utility::Uuid &playhead_id) {
                if (!sub_playhead_sets_[playhead_id]) {
                    sub_playhead_sets_[playhead_id].reset(new ImageSet);                    
                }
                return sub_playhead_sets_[playhead_id];
            }

            // using shared_ptr to reduce overhead of map rearranging itself
            // and copying elements
            std::map<utility::Uuid, std::shared_ptr<ImageSet>> sub_playhead_sets_;

            // ordered vec of the IDs of the sub-playheads that are suppling
            // the image sets. The first in the vec is the 'key' subplayhead
            // corresponding to the first item in the playhead selection.
            utility::UuidVector sub_playhead_ids_;

            int hero_sub_playhead_index_ = {0};
            int previous_hero_sub_playhead_index_ = {-1};

            std::shared_ptr<ImageSet> null_set_ = {std::shared_ptr<ImageSet>(new ImageSet)};

            ImageSetLayoutDataPtr layout_data_;

            utility::JsonStore as_json_;
            size_t hash_ = {0};
            size_t images_hash_ = {0};

    };

    typedef std::shared_ptr<const ImageBufDisplaySet> ImageBufDisplaySetPtr;

} // namespace media_reader
} // namespace xstudio
