#pragma once

/*Simple helper class to wrap pixel cast/swizzle & bitshift ops.

Some of these will be very heavy on CPU side. A better approach would be
that the viewport glsl shader does some conversions at draw time - for
example packing 10 bit RGB into RGBA8 (unsigned byte) output when we need
RGB 10_10_10_2 for the SDI Output card.
*/
#include <cstddef>
#include <memory>
#include <condition_variable>
#include <deque>
#include <vector>
#include <thread>

namespace xstudio {
  namespace bm_decklink_plugin_1_0 {

    struct JobFunc {
        virtual void doit() = 0;
        virtual ~JobFunc() = default;
    };

    class SimpleThreadPool {

    public:

        SimpleThreadPool(const int num_threads = std::max(8, static_cast<int>(std::thread::hardware_concurrency() / 2))) {
            for (int i = 0; i < num_threads; ++i) {
                threads_.emplace_back(std::thread(&SimpleThreadPool::run, this));
            }
        }

        typedef std::shared_ptr<JobFunc> JobFuncPtr;

        ~SimpleThreadPool() {

            for (auto &t : threads_) {
                // when any thread picks up an em
                {
                    std::lock_guard lk(m);
                    queue.emplace_back(JobFuncPtr()); // null job func to signal exit
                }
                cv.notify_one();
            }

            for (auto &t : threads_) {
                t.join();
            }
        }

        template <class JobType, typename... Args>
        void add_job(Args&&... args) {
            {
                std::lock_guard lk(m);
                JobType * job = new JobType(std::forward<Args>(args)...);
                queue.emplace_back(std::shared_ptr<JobFunc>(job));
            }
            cv.notify_one();
        }

        void wait_for_completion() {
            std::unique_lock lk(m);
            if (!queue.empty() || in_progress) {
                cv2.wait(lk, [=] { return queue.empty() && !in_progress; });
            }
        }

        size_t num_threads() const { return threads_.size(); }

    private:

        JobFuncPtr get_job() {
            std::unique_lock lk(m);
            if (queue.empty()) {
                cv.wait(lk, [=] { return !queue.empty(); });
            }
            auto rt = queue.front();
            queue.pop_front();
            in_progress++;
            return rt;
        }

        void run() {
            while (1) {
                // this blocks until there is something in queue for us
                JobFuncPtr j = get_job();
                if (!j) {
                    break; // exit
                }
                j->doit();
                m.lock();
                in_progress--;
                m.unlock();
                cv2.notify_one();
            }
        }

        std::mutex m;
        std::condition_variable cv, cv2;
        int in_progress = 0;
        std::deque<JobFuncPtr> queue;
        std::vector<std::thread> threads_;
    };

struct RGBA16_to_10bitRGB : public JobFunc {
    RGBA16_to_10bitRGB(uint16_t * src, uint32_t * dst, size_t n) : _src(src), _dst(dst), n(n) {}
    uint16_t * _src;
    uint32_t * _dst;
    size_t n;
    void doit() override;
};

struct RGBA16_to_10bitRGBX : public JobFunc {
    RGBA16_to_10bitRGBX(uint16_t * src, uint32_t * dst, size_t n) : _src(src), _dst(dst), n(n) {}
    uint16_t * _src;
    uint32_t * _dst;
    size_t n;
    void doit() override;
};

struct RGBA16_to_10bitRGBXLE : public JobFunc {
    RGBA16_to_10bitRGBXLE(uint16_t * src, uint32_t * dst, size_t n) : _src(src), _dst(dst), n(n) {}
    uint16_t * _src;
    uint32_t * _dst;
    size_t n;
    void doit() override;
};

struct RGBA16_to_12bitRGBLE : public JobFunc {
    RGBA16_to_12bitRGBLE(uint16_t * src, uint16_t * dst, size_t n) : _src(src), _dst(dst), n(n) {}
    uint16_t * _src;
    uint16_t * _dst;
    size_t n;
    void doit() override;
};

struct RGBA16_to_12bitRGB : public JobFunc {
    RGBA16_to_12bitRGB(uint16_t * src, uint16_t * dst, size_t n) : _src(src), _dst(dst), n(n) {}
    uint16_t * _src;
    uint16_t * _dst;
    size_t n;
    void doit() override;
};

class PixelSwizzler : public SimpleThreadPool {
    
    public:

        template < typename JobFunc>
        void copy_frame_buffer_10bit(void * _dst, void * _src, size_t num_pix) {

            size_t step = ((num_pix / num_threads()) / 4096) * 4096;
            uint32_t *dst = (uint32_t *)_dst;
            uint16_t *src = (uint16_t *)_src;

            for (int i = 0; i < num_threads(); ++i) {
                add_job<JobFunc>(src, dst, std::min(num_pix, step));
                dst += step;
                src += step*4;
                num_pix -= step;
            }

            wait_for_completion();

        }

        template < typename JobFunc>
        void copy_frame_buffer_12bit(void * _dst, void * _src, size_t num_pix) {

            size_t step = ((num_pix / num_threads()) / 4128) * 4128;

            uint16_t *dst = (uint16_t *)_dst;
            uint16_t *src = (uint16_t *)_src;

            for (int i = 0; i < num_threads(); ++i) {
                add_job<JobFunc>(src, dst, std::min(num_pix, step));
                dst += (step*9)/4;
                src += step*4;
                num_pix -= step;
            }

            wait_for_completion();

        }

};
} // namespace xstudio::bm_decklink_plugin_1_0
} // namespace xstudio