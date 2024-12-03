// SPDX-License-Identifier: Apache-2.0
#pragma once


#include "xstudio/media_reader/image_buffer_set.hpp"
#include "xstudio/plugin_manager/plugin_base.hpp"
#include "xstudio/plugin_manager/plugin_manager.hpp"
#include "xstudio/media/media.hpp"

#define NO_FRAME INT_MIN


namespace xstudio {
namespace ui {
namespace viewport {

    /**
     *  @brief ViewportLayoutPlugin class.
     *
     *  @details
     *   Subclass from this to create custom layouts for mulitple images in the
     *   xSTUDIO viewport. The plugin is told about the number of images that
     *   are available for drawing into the viewport. The plugin is then 
     *   resposible for providing a transform matrix for each image to position
     *   each image in the viewport coordinate space. 
     *   Plugins can also provide their own custom GPU shader code for both the 
     *   vertex and fragment shaders at draw time.
     *   A complete draw routine can also be overidden for full control of how
     *   image pixels are rendered to the viewport.
     */

    class ViewportLayoutPlugin : public plugin::StandardPlugin {
      public:

        ViewportLayoutPlugin(
            caf::actor_config &cfg,
            const utility::JsonStore &init_settings);

      protected:

        /**
         *  @brief Calculate a transform matrix for each on-screen video item
         *  for displaying into the viewport coordinate space. Also provide an
         *  aspect ratio hint for the overall layout geometry.
         *
         *  @details To use the regular viewport renderer, which does a
         *  straightforward draw of each image, set the member data in the 
         *  layout_data. See image_buffer_set.hpp for more details.
         *        
         *  This method is called once only for a given image_set characteristics.
         *  The characteristics depends on the number of images and the size in
         *  pixels of each image in the set. If you want to re-compute the layout
         *  for a given image set characteritics call 'clear_cache()' to force
         *  a re-evaluation of this method.
         */
        virtual void do_layout(
            const std::string &layout_mode,
            const media_reader::ImageBufDisplaySetPtr &image_set,
            media_reader::ImageSetLayoutData &layout_data
            );

        /**
         *  @brief Override this method to provide your own, custom renderer. If
         *  you don't override the default renderer will be provided which may
         *  or may not be able to enable your desired viewport graphics layout.
         *
         *  @details See the default renderer implementations for a reference
         *  for creating a custom viewport renderer
         */
        virtual ViewportRenderer * make_renderer(const std::string &window_id) = 0;

        /**
         *  @brief Call this method to add a layout mode that your plugin
         *  provides. The string sets the name, this will appear in the list 
         *  of compare modes. If the name is already taken a warning is 
         *  generated.
         *  The AssemblyMode tells the playhead how many images are needed to 
         *  draw the viewport layout.
         *
         */
        void add_layout_mode(
          const std::string &name,
          const playhead::AssemblyMode mode,
          const playhead::AutoAlignMode default_auto_align = playhead::AutoAlignMode::AAM_ALIGN_OFF
          );

        /**
         *  @brief Expose an attribute in the Settings panel for your layout
         *  plugin. The button for the settings panel will show under the
         *  'Compare' viewport toolbar button.
         *
         */
        void add_layout_settings_attribute(module::Attribute *attr, const std::string &layout_name);

        /**
         *  @brief Call this method with necessary QML snippet to instance
         *  custom overlay graphics for the viewport.
         *  See plugins/viewport_layout/wipe_viewport_layout for example useage
         *
         */
        void add_viewport_layout_qml_overlay(
          const std::string &layout_name,
          const std::string &qml_code
          );

        void on_exit() override;

        private:

        void init();

          using LayoutDataRP = caf::typed_response_promise<media_reader::ImageSetLayoutDataPtr>;

          void attribute_changed(
              const utility::Uuid &attribute_uuid, const int /*role*/
              ) override;

          void __do_layout(
            const std::string &layout_mode,
            const media_reader::ImageBufDisplaySetPtr &image_set,
            LayoutDataRP rp);

          media_reader::ImageSetLayoutDataPtr python_layout_data_to_ours(
            const utility::JsonStore &python_data) const;

          caf::message_handler message_handler_extensions() override { return handler_; }

          caf::message_handler handler_;

          module::BooleanAttribute *settings_toggle_;

          std::map<utility::Uuid, module::StringAttribute *> layout_names_;

          std::map<std::string, std::map<size_t, media_reader::ImageSetLayoutDataPtr>> layouts_cache_;

          std::map<size_t, std::vector<LayoutDataRP>> pending_responses_;

          const bool is_python_plugin_;
          

          // Python ViewportLayout plugins work by setting this 'remote_'
          // member. This C++ base class talks to the remote_ (which is the 
          // Python class instance) to effectively provide the virtual methods.
          caf::actor event_group_;
          caf::actor layouts_manager_;
          caf::actor gobal_playhead_events_;

    };

    /**
     *  @brief ViewportLayoutManager class.
     *
     *  @details
     *   A singleton instance of this class is created by the GlobalActor. The
     *   ViewportLayoutManager instances ViewportLayoutPlugins, provides model
     *   data about them to the UI layer so that the Compare button/menu can
     *   be populated, and activates the layout plugin when the user or api
     *   switches the active layout.
     */

    class ViewportLayoutManager : public caf::event_based_actor {
      public:

        ViewportLayoutManager(
            caf::actor_config &cfg);

        ~ViewportLayoutManager() override;

        void on_exit() override;

        caf::behavior make_behavior() override { return behavior_; }

      private:

        void spawn_plugins();

        caf::behavior behavior_;

        std::map<std::string, caf::actor> viewport_layout_plugins_;
        std::map<std::string, std::pair<caf::actor, std::pair<playhead::AutoAlignMode, playhead::AssemblyMode> >> viewport_layouts_;        
        caf::actor event_group_;

    };

} // namespace viewport
} // namespace ui
} // namespace xstudio
