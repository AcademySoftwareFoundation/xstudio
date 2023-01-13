// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "xstudio/ui/qt/viewport_widget.hpp"
#include "xstudio/thumbnail/thumbnail.hpp"

#include <QString>
#include <QUrl>
#include <QObject>
//#include <QOpenGLFramebufferObject>
#include <QImage>
#include <QOpenGLContext>
#include <QOffscreenSurface>

namespace opengl {
class OpenGLViewportRenderer;
}

namespace xstudio {
namespace ui {
    namespace qt {

        class OffscreenViewport : public caf::mixin::actor_object<QObject> {

            // Q_OBJECT
            using super = caf::mixin::actor_object<QObject>;

          public:
            OffscreenViewport(QObject *parent = nullptr);
            ~OffscreenViewport() override;

            // Direct rendering to an output file
            void renderSnapshot(
                caf::actor playhead,
                const int width,
                const int height,
                const int compression,
                const bool bakeColor,
                const caf::uri path);

            void moveToOwnThread();

          private:
            thumbnail::ThumbnailBufferPtr
            renderOffscreen(const int w, const int h, const media_reader::ImageBufPtr &image);

            thumbnail::ThumbnailBufferPtr renderToThumbnail(
                caf::actor playhead,
                const thumbnail::THUMBNAIL_FORMAT format,
                const int width,
                const bool render_annotations,
                const bool fit_to_annotations_outside_image);

            void initGL();

            void exportToEXR(thumbnail::ThumbnailBufferPtr r, const caf::uri path);

            void exportToCompressedFormat(
                thumbnail::ThumbnailBufferPtr r,
                const caf::uri path,
                int compression,
                const std::string &ext);

            media_reader::ImageBufPtr get_image_from_playhead(caf::actor playhead);

            std::shared_ptr<ui::viewport::Viewport> viewport_renderer_;
            QOpenGLContext *gl_context_ = {nullptr};
            QOffscreenSurface *surface_ = {nullptr};
            QThread *thread_            = {nullptr};
            caf::actor middleman_;

            // TODO: will remove once everything done
            const char *formatSuffixes[4] = {"EXR", "JPG", "PNG", "TIFF"};
        };
    } // namespace qt
} // namespace ui
} // namespace xstudio