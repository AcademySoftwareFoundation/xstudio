// SPDX-License-Identifier: Apache-2.0
#include <cmath>
#include <iostream>
#include <memory.h>
#include <thread>

#include "xstudio/ui/opengl/opengl_multi_buffered_texture.hpp"
#include "xstudio/utility/chrono.hpp"

using namespace xstudio::ui::opengl;

namespace {

class DebugTimer {
  public:
    DebugTimer(std::string m) : message_(std::move(m)) { t1_ = xstudio::utility::clock::now(); }

    ~DebugTimer() {
        std::cerr << message_ << " completed in "
                  << std::chrono::duration_cast<std::chrono::microseconds>(
                         xstudio::utility::clock::now() - t1_)
                         .count()
                  << " microseconds\n";
    }

  private:
    xstudio::utility::clock::time_point t1_;
    const std::string message_;
};

} // namespace
 
namespace xstudio::ui::opengl {

class TextureTransferWorker {
    public:

    TextureTransferWorker(GLDoubleBufferedTexture * owner) {
        for (int i = 0; i < 8; ++i) {
            threads_.emplace_back(std::thread(&TextureTransferWorker::run, this));
        }
        owner_ = owner;
    }

    ~TextureTransferWorker() {

        for (auto &t: threads_) {
            // when any thread picks up an empty job it exits its loop
            add_job(GLDoubleBufferedTexture::GLBlindTexturePtr());
        }

        for (auto &t: threads_) {
            t.join();
        }
    }

    std::vector<std::thread> threads_;

    GLDoubleBufferedTexture::GLBlindTexturePtr get_job() {
        std::unique_lock lk(m);
        if (queue.empty()) {
            cv.wait(lk, [=]{ return !queue.empty(); });
        }
        auto rt = queue.front();
        queue.pop_front();
        return rt;
    }

    void clear_jobs() {
        std::lock_guard lk(m);
        while (queue.size()) {
            auto rt = queue.front();
            rt->cancel_upload();
            queue.pop_front();
        }
    }

    void add_job(GLDoubleBufferedTexture::GLBlindTexturePtr ptr) {

        {
            std::lock_guard lk(m);
            queue.push_back(ptr);

        }
        cv.notify_one();
    }

    void run()
    {
        while(1)  {
            
            // this blocks until there is something in queue for us
            GLDoubleBufferedTexture::GLBlindTexturePtr tex = get_job();
            if (!tex) break; // exit 
            tex->do_pixel_upload();

        }
    }

    std::mutex m;
    std::condition_variable cv;
    std::deque<GLDoubleBufferedTexture::GLBlindTexturePtr> queue;
    GLDoubleBufferedTexture * owner_;
};
}

GLDoubleBufferedTexture::GLDoubleBufferedTexture() {

    worker_ = new TextureTransferWorker(this);

}

void GLDoubleBufferedTexture::bind(const media_reader::ImageBufPtr &image, int &tex_index, Imath::V2i &dims) {

    auto p = textures_.find_pending(image);
    if (p != textures_.end()) {
        // the image is already uploaded to one of our unused textures
        (*p)->bind(tex_index, dims);
        return;
    }
}

void GLDoubleBufferedTexture::queue_image_set_for_upload(
    const media_reader::ImageBufDisplaySetPtr &image_set) {

    worker_->clear_jobs();

    // image_set includes images that are due to go on-screen NOW in the 
    // current draw, plus images that are not going on screen now but will
    // be soon.
    // We can do a-sync uploads of images to texture memory for performance
    // optimisation so that images due on screen soon are being uploaded while
    // we draw the images that are going on-screen now etc.

    // First, we loop over the 'onscreen_images' and put them in our ordered
    // queue of images that need to be in texture memory
    image_queue_.clear();

    const std::vector<int> & draw_order = image_set->layout_data()->image_draw_order_hint_;    

    auto available_textures = textures_;

    for (const auto &i: draw_order) {
        auto im = image_set->onscreen_image(i);
        queue_for_upload(available_textures, im);
    }

    // now queue images the we'll need in the *next* redraw
    for (const auto &i: draw_order) {
        if (image_set->future_images(i).size()) {
            auto im = image_set->future_images(i)[0];
            queue_for_upload(available_textures, im);
        }
    }

    // now queue images the we'll need in the *next* *next* redraw
    for (const auto &i: draw_order) {
        if (image_set->future_images(i).size() > 1) {
            auto im = image_set->future_images(i)[1];
            queue_for_upload(available_textures, im);
        }
    }
           
}

void GLDoubleBufferedTexture::queue_for_upload(
    GLDoubleBufferedTexture::TexSet &available_textures,
    const media_reader::ImageBufPtr &image
    ) 
{ 
    if (!image) return;
    auto q = available_textures.find_pending(image);
    if (q != available_textures.end()) {
        available_textures.erase(q);
    } else if (available_textures.size()) {
         q = available_textures.begin();
        (*q)->prepare_for_upload(image);
        worker_->add_job(*q);
        available_textures.erase(q);
    } else {
        image_queue_.push_back(image);
    }
}

void GLDoubleBufferedTexture::release(const media_reader::ImageBufPtr &image) { 
    
    // move the texture containing 'image' into the used_textures_ map so
    // it can be used for another image

    if (image_queue_.size()) {
        auto im = image_queue_.front();
        auto q = textures_.find(image);
        if (q != textures_.end()) {
            (*q)->prepare_for_upload(im);
            worker_->add_job(*q);
            image_queue_.pop_front();
        } else {
            //std::cerr << "Didn't release\n";
        }
    }

}
