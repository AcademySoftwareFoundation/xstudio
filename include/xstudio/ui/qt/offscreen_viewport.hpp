// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "xstudio/ui/qt/viewport_widget.hpp"
#include "xstudio/thumbnail/thumbnail.hpp"

#include <QString>
#include <QUrl>
#include <QObject>
// #include <QOpenGLFramebufferObject>
#include <QImage>
#include <QOpenGLContext>
#include <QOffscreenSurface>

namespace opengl {
class OpenGLViewportRenderer;
}

class QQuickWindow;
class QQuickItem;
class QQmlComponent;
class QQuickRenderControl;
class QQmlEngine;

namespace xstudio {
namespace ui {

    namespace qml {
        class Helpers;
    }

    namespace qt {

        class OffscreenViewport : public caf::mixin::actor_object<QObject> {

            Q_OBJECT
            using super = caf::mixin::actor_object<QObject>;

          public:
            OffscreenViewport(const std::string name, bool include_qml_overlays = true);
            ~OffscreenViewport() override;

            // Direct rendering to an output file
            void
            renderSnapshot(const int width, const int height, const caf::uri path = caf::uri());

            void setPlayhead(const QString &playheadAddress);

            std::string name() { return viewport_renderer_->name(); }

            void stop();

          public slots:

            void autoDelete();
            void sceneChanged();

          private:
            caf::actor_system &system() { return self()->home_system(); }

            void receive_change_notification(viewport::Viewport::ChangeCallbackId id);

            thumbnail::ThumbnailBufferPtr renderOffscreen(
                const int w,
                const int h,
                const media_reader::ImageBufPtr &image = media_reader::ImageBufPtr());

            thumbnail::ThumbnailBufferPtr renderToThumbnail(
                const thumbnail::THUMBNAIL_FORMAT format,
                const int width,
                const bool auto_scale,
                const bool show_annotations);

            thumbnail::ThumbnailBufferPtr renderToThumbnail(
                const thumbnail::THUMBNAIL_FORMAT format, const int width, const int height);

            void renderToImageBuffer(
                const int w,
                const int h,
                media_reader::ImageBufPtr &image,
                const viewport::ImageFormat format,
                const bool force_sync,
                const utility::time_point &tp = utility::time_point());

            void initGL();

            void exportToEXR(const media_reader::ImageBufPtr &image, const caf::uri path);

            thumbnail::ThumbnailBufferPtr renderMediaFrameToThumbnail(
                caf::actor media_actor,
                const int media_frame,
                const thumbnail::THUMBNAIL_FORMAT format,
                const int width,
                const bool auto_scale,
                const bool show_annotations);

            thumbnail::ThumbnailBufferPtr renderMediaFrameToThumbnail(
                caf::actor media_actor,
                const timebase::flicks media_timepoint,
                const thumbnail::THUMBNAIL_FORMAT format,
                const int width,
                const bool auto_scale,
                const bool show_annotations);

            void exportToCompressedFormat(
                const media_reader::ImageBufPtr &buf,
                const caf::uri path,
                const std::string &ext);

            void setupTextureAndFrameBuffer(
                const int width, const int height, const viewport::ImageFormat format);

            void make_conversion_lut();

            bool loadQMLOverlays();

            thumbnail::ThumbnailBufferPtr
            rgb96thumbFromHalfFloatImage(const media_reader::ImageBufPtr &image);

            ui::viewport::Viewport *viewport_renderer_ = nullptr;
            QOpenGLContext *gl_context_                = {nullptr};
            QOffscreenSurface *surface_                = {nullptr};
            QThread *thread_                           = {nullptr};

            // TODO: will remove once everything done
            const char *formatSuffixes[4] = {"EXR", "JPG", "PNG", "TIFF"};

            int tex_width_              = 0;
            int tex_height_             = 0;
            int pix_buf_size_           = 0;
            GLuint texId_               = 0;
            GLuint fboId_               = 0;
            GLuint depth_texId_         = 0;
            GLuint pixel_buffer_object_ = 0;

            int vid_out_width_                    = 0;
            int vid_out_height_                   = 0;
            viewport::ImageFormat vid_out_format_ = viewport::ImageFormat::RGBA_16;
            caf::actor video_output_actor_;
            std::vector<media_reader::ImageBufPtr> output_buffers_;
            media_reader::ImageBufPtr last_rendered_frame_;
            std::vector<uint32_t> half_to_int_32_lut_;

            caf::actor local_playhead_;
            QString session_actor_addr_;

            QQuickWindow *quick_win_             = nullptr;
            QQuickItem *root_qml_overlays_item_  = nullptr;
            QQmlComponent *qml_component_        = nullptr;
            QQuickRenderControl *render_control_ = nullptr;
            QQmlEngine *qml_engine_              = nullptr;
            ui::qml::Helpers *helper_            = nullptr;
            bool overlays_loaded_                = false;
            bool include_qml_overlays_           = true;
        };
    } // namespace qt
} // namespace ui
} // namespace xstudio