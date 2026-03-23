// SPDX-License-Identifier: Apache-2.0
#include <exception>
#include <filesystem>

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <QPdfDocument>

#include "pdf.hpp"
#include "xstudio/media_reader/media_reader.hpp"
#include "xstudio/media/media_error.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/ui/opengl/shader_program_base.hpp"
#include "xstudio/ui/qml/helper_ui.hpp"

namespace fs = std::filesystem;


using namespace xstudio::media_reader;
using namespace xstudio::utility;
using namespace xstudio::ui::qml;
using namespace xstudio;

namespace {

static Uuid myshader_uuid{"52141ad7-0eeb-4b80-8881-b62cfecbf9f1"};
static Uuid s_plugin_uuid{"4a1db5da-610a-4f41-917d-fd7016948ead"};

static std::string myshader{R"(
#version 330 core
uniform int width;
uniform int bytes_per_channel;

// forward declaration
uvec4 get_image_data_4bytes(int byte_address);

vec4 fetch_rgba_pixel(ivec2 image_coord)
{
    int address = (image_coord.x + image_coord.y*width)*3;
    uvec4 c = get_image_data_4bytes(address);
    return vec4(float(c.x)/255.0f,float(c.y)/255.0f,float(c.z)/255.0f,1.0f);
}
)"};

static ui::viewport::GPUShaderPtr
    pdf_shader(new ui::opengl::OpenGLShader(myshader_uuid, myshader));

} // namespace

// QPdfDocument::Error::None   0   No error occurred.
// QPdfDocument::Error::Unknown    1   Unknown type of error.
// QPdfDocument::Error::DataNotYetAvailable    2   The document is still loading, it's too early
// to attempt the operation. QPdfDocument::Error::FileNotFound   3   The file given to load()
// was not found. QPdfDocument::Error::InvalidFileFormat  4   The file given to load() is not a
// valid PDF file. QPdfDocument::Error::IncorrectPassword  5   The password given to
// setPassword() is not correct for this file. QPdfDocument::Error::UnsupportedSecurityScheme

void PDFMediaReader::update_preferences(const utility::JsonStore &prefs) {
    try {
        supported_ = global_store::preference_value<utility::JsonStore>(
            prefs, "/plugin/media_reader/pdf/supported");

        dpi_ =
            global_store::preference_value<uint32_t>(prefs, "/plugin/media_reader/pdf/dpi");

    } catch (const std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }
}

uint64_t PDFMediaReader::points_to_pixels(const double points) const {
    uint64_t value = (points / 72.0f) * dpi_;

    return (value / 32) * 32;
}

media::MediaDetail PDFMediaReader::detail(const caf::uri &uri) const {

    std::vector<media::StreamDetail> streams;
    std::shared_ptr<QPdfDocument> pdf;
    auto frame_count = 0;

    if (cache_.count(uri))
        pdf = cache_[uri];
    else {
        pdf         = std::make_shared<QPdfDocument>();
        auto loaded = pdf->load(QStringFromStd(uri_to_posix_path(uri)));
        if (loaded != QPdfDocument::Error::None) {
            throw media_unreadable_error("Unable to open " + to_string(uri));
        }

        if (not cache_.count(uri))
            cache_[uri] = pdf;
    }

    frame_count = pdf->pageCount();
    auto point_size = pdf->pagePointSize(0);
    auto width = points_to_pixels(point_size.width());
    auto height = points_to_pixels(point_size.height());

    streams.emplace_back(media::StreamDetail(
        utility::FrameRateDuration(
            frame_count, FrameRate(timebase::k_flicks_one_twenty_fourth_of_second)),
        "Pages",
        media::MT_IMAGE,
        "{0}@{1}/{2},{3}",
        Imath::V2i(width, height),
        1.0f,
        0));

    return xstudio::media::MediaDetail(name(), streams);
}


ImageBufPtr PDFMediaReader::image(const media::AVFrameID &mptr) {
    ImageBufPtr buf;

    std::shared_ptr<QPdfDocument> pdf;

    if (cache_.count(mptr.uri()))
        pdf = cache_[mptr.uri()];
    else {
        pdf = std::make_shared<QPdfDocument>();
        if (auto loaded = pdf->load(QStringFromStd(uri_to_posix_path(mptr.uri())));
            loaded != QPdfDocument::Error::None) {
            throw media_unreadable_error("Unable to open " + to_string(mptr.uri()));
        }
        if (not cache_.count(mptr.uri()))
            cache_[mptr.uri()] = pdf;
    }

    auto point_size = pdf->pagePointSize(mptr.frame());
    auto width = points_to_pixels(point_size.width());
    auto height = points_to_pixels(point_size.height());
    auto image = pdf->render(mptr.frame(), QSize(width, height), QPdfDocumentRenderOptions());

    image.convertTo(QImage::Format_RGB888);

    JsonStore jsn;
    jsn["bytes_per_channel"] = 3;
    jsn["width"]             = width;
    jsn["height"]            = height;

    buf.reset(new ImageBuffer(myshader_uuid, jsn));
    buf->allocate(width * height * 3);
    buf->set_shader(pdf_shader);
    buf->set_image_dimensions(Imath::V2i(width, height));

    byte *buffer = buf->buffer();

    std::memcpy((char *)buffer, image.constBits(), width * height * 3);

    return buf;
}

std::shared_ptr<thumbnail::ThumbnailBuffer>
PDFMediaReader::thumbnail(const media::AVFrameID &mptr, const size_t thumb_size) {
    try {
        auto pdf = std::make_shared<QPdfDocument>();

        if (auto loaded = pdf->load(QStringFromStd(uri_to_posix_path(mptr.uri())));
            loaded != QPdfDocument::Error::None) {
            throw media_unreadable_error("Unable to open " + to_string(mptr.uri()));
        }

        auto point_size = pdf->pagePointSize(mptr.frame());
        auto ratio = point_size.width() / point_size.height();
        auto height = (static_cast<uint32_t>(thumb_size / ratio) / 32) * 32;

        auto image =
            pdf->render(mptr.frame(), QSize(thumb_size, height), QPdfDocumentRenderOptions());
        auto thumb = std::make_shared<thumbnail::ThumbnailBuffer>(
            thumb_size, height, thumbnail::TF_RGB24);

        image.convertTo(QImage::Format_RGB888);
        std::memcpy((char *)(thumb->data().data()), image.constBits(), thumb_size * height * 3);

        return thumb;
    } catch ([[maybe_unused]] std::exception &e) {
        throw;
    }
}

std::vector<std::string> PDFMediaReader::supported_extensions() const {
    auto result = std::vector<std::string>();

    for (const auto &i : supported_.items()) {
        if (from_string(i.value()) != MRC_NO)
            result.push_back(i.key());
    }

    return result;
}

MRCertainty PDFMediaReader::supported(const caf::uri &uri, const std::array<uint8_t, 16> &sig) {
    auto result = MRC_NO;

    fs::path p(uri_to_posix_path(uri));

#ifdef _WIN32
    std::string ext = ltrim_char(to_upper_path(p.extension()), '.');
#else
    std::string ext = ltrim_char(to_upper(p.extension().string()), '.');
#endif

    try {
        auto value = supported_.value(ext, "");
        if (value.empty()) {
            result = from_string(supported_.value("", "MRC_NO"));
        } else
            result = from_string(value);
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    return result;
}

utility::Uuid PDFMediaReader::plugin_uuid() const { return s_plugin_uuid; }

extern "C" {
plugin_manager::PluginFactoryCollection *plugin_factory_collection_ptr() {
    return new plugin_manager::PluginFactoryCollection(
        std::vector<std::shared_ptr<plugin_manager::PluginFactory>>({std::make_shared<
            MediaReaderPlugin<MediaReaderActor<PDFMediaReader>>>(
            s_plugin_uuid, "PDF", "xStudio", "PDF Media Reader", semver::version("1.0.0"))}));
}
}
