// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <memory>
#include "xstudio/media_reader/image_buffer_set.hpp"
#include "xstudio/plugin_manager/plugin_base.hpp"

namespace xstudio {
namespace ui {
    namespace viewport {

        enum RenderHints {
            AlwaysNearestPixel,
            AlwaysBilinear,
            BilinearWhenZoomedOut,
            AlwaysCubic,
            CubicWhenZoomedOut
        };

        /**
         *  @brief ViewportRenderer class.
         *
         *  @details
         *   Pure abstract base class used by Viewport class for rendering the xstudio
         *   viewport into a graphics surface (e.g. OpenGL).
         */

        class ViewportRenderer {

          public:
            ViewportRenderer()          = default;
            virtual ~ViewportRenderer() = default;

            /**
             *  @brief Render the viewport.
             *
             *  @details Render the image data into some initialised graphics resource (e.g.
             *  an active OpenGL context)
             */
            virtual void render(
                const media_reader::ImageBufDisplaySetPtr &images,
                const Imath::M44f &window_to_viewport_matrix,
                const Imath::M44f &viewport_to_image_matrix,
                const std::map<utility::Uuid, plugin::ViewportOverlayRendererPtr> &overlay_renderers) = 0;

            /**
             *  @brief Provide default preference dictionary for the viewport renderer
             *
             *  @details ViewportRenderer implementations can declare a json dict for
             * preferences specific to the given implementation - for example, texture mode or
             * async upload settings that may be necessary for tuning performance on different
             * systems. The xSTUDIO UI layer will expose these preferences in the prefs panel.
             */
            virtual utility::JsonStore default_prefs() = 0;

            /**
             *  @brief Set rendering preferences
             *
             *  @details A JSON dict passed in from UI layer - store the preference values as
             * requires
             */
            virtual void set_prefs(const utility::JsonStore &prefs) = 0;

            void
            set_pre_renderer_hooks(const std::map<utility::Uuid, plugin::GPUPreDrawHookPtr> &hooks) {
                pre_render_gpu_hooks_ = hooks;
            }

            void set_render_hints(RenderHints hint) { render_hints_ = hint; }

            inline static const std::vector<
                std::tuple<RenderHints, std::string, std::string, bool>>
                pixel_filter_mode_names = {
                    {AlwaysNearestPixel, "Nearest Pixel", "Nearest", true},
                    {BilinearWhenZoomedOut, "Auto Nearest/Bilinear", "Auto", true},
                    {AlwaysBilinear, "Bilinear", "Bilinear", true} /*,
                     {AlwaysCubic, "Cubic When Zoomed Out", "Auto Cubic", false},
                     {CubicWhenZoomedOut, "Cubic", "Cubic", false}*/
            };

            void init() {
                if (!done_init_) {
                    pre_init();
                    done_init_ = true;
                }
            }

          protected:

            /**
             *  @brief Create a json dict with essential shader parameters for drawing 
             *  the image to the viewport.
             *
             *  @details The json dictionary returned from this function is as follows:
             *
             *  mat4 to_coord_system (matrix to project from viewport to viewport coordinate system
             *  mat4 to_canvas (matrix to transform from window area to viewport area)
             *  bool use_bilinear_filtering (tells us whether to use bilinear pixel filtering)
             *  mat4 image_transform_matrix (transform image into viewport coordinate system)
             *
             */
            virtual utility::JsonStore core_shader_params(
                const media_reader::ImageBufPtr &image_to_be_drawn,
                const Imath::M44f &window_to_viewport_matrix,
                const Imath::M44f &viewport_to_image_space,
                const float viewport_du_dx,
                const utility::JsonStore & layout_data,
                const int image_index) const;

        Imath::M44f mat;
          
            /**
             *  @brief Initialise the viewport.
             *
             *  @details Carry out first time render one-shot initialisation of any member
             * data or other state/data initialisation of graphics resources
             */
            virtual void pre_init() = 0;

            std::map<utility::Uuid, plugin::GPUPreDrawHookPtr> pre_render_gpu_hooks_;

            RenderHints render_hints_ = {BilinearWhenZoomedOut};
            bool done_init_           = false;
        };

        typedef std::shared_ptr<ViewportRenderer> ViewportRendererPtr;

    } // namespace viewport
} // namespace ui
} // namespace xstudio
