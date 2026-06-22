// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <string>
#include <memory>

#include "xstudio/utility/json_store.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}


namespace xstudio {
namespace ffprobe {

    typedef struct MediaStream {
        AVStream *st{nullptr};
        AVCodecContext *dec_ctx{nullptr};
    } MediaStream;

    typedef struct MediaFile {
        AVFormatContext *fmt_ctx{nullptr};

        std::vector<MediaStream> streams;
        int nb_streams{0};

        ~MediaFile();

    } MediaFile;

    class FFProbe {
      public:
        FFProbe();
        virtual ~FFProbe();

        utility::JsonStore probe_file(const caf::uri &path);
        std::string probe_file(const std::string &path);

      private:
        std::shared_ptr<MediaFile> open_file(const std::string &path);
    };

} // namespace ffprobe
} // namespace xstudio
