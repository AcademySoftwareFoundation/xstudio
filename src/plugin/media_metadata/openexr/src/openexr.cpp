// SPDX-License-Identifier: Apache-2.0
#include <filesystem>


#include <ImfAttribute.h>
#include <ImfBoxAttribute.h>
#include <ImfChannelListAttribute.h>
#include <ImfChromaticitiesAttribute.h>
#include <ImfCompressionAttribute.h>
#include <ImfDoubleAttribute.h>
#include <ImfEnvmapAttribute.h>
#include <ImfFloatAttribute.h>
#include <ImfHeader.h>
#include <ImfInputFile.h>
#include <ImfIntAttribute.h>
#include <ImfKeyCodeAttribute.h>
#include <ImfLineOrderAttribute.h>
#include <ImfMatrixAttribute.h>
#include <ImfMultiPartInputFile.h>
#include <ImfName.h>
#include <ImfPreviewImageAttribute.h>
#include <ImfRationalAttribute.h>
#include <ImfStringAttribute.h>
#include <ImfStringVectorAttribute.h>
#include <ImfThreading.h>
#include <ImfTileDescriptionAttribute.h>
#include <ImfTimeCodeAttribute.h>
#include <ImfVecAttribute.h>
#include <ImfVersion.h>

#include <iomanip>
#include <iostream>
#include <sstream>

#include "nlohmann/json.hpp"
#include "xstudio/media_metadata/media_metadata.hpp"
#include "xstudio/utility/helpers.hpp"

namespace fs = std::filesystem;


using namespace xstudio::media_metadata;
using namespace xstudio::utility;
using namespace xstudio;

bool dump_json_headers(const Imf::Header &h, nlohmann::json &root);

class OpenEXRMediaMetadata : public MediaMetadata {
  public:
    OpenEXRMediaMetadata() : MediaMetadata("OpenEXR") {}
    ~OpenEXRMediaMetadata() override = default;
    MMCertainty
    supported(const caf::uri &uri, const std::array<uint8_t, 16> &signature) override;

  protected:
    nlohmann::json read_metadata(const caf::uri &uri) override;
    std::optional<StandardFields> fill_standard_fields(const nlohmann::json &metadata) override;
};

nlohmann::json OpenEXRMediaMetadata::read_metadata(const caf::uri &uri) {
    std::string path(uri_to_posix_path(uri));
    Imf::MultiPartInputFile input(path.c_str());
    int parts          = input.parts();
    bool file_complete = true;
    bool success       = false;
    for (auto i = 0; i < parts && file_complete; i++)
        if (!input.partComplete(i))
            file_complete = false;

    nlohmann::json json;
    json["xstudio"]["file_format_version"]["type"]  = "int";
    json["xstudio"]["file_format_version"]["value"] = Imf::getVersion(input.version());
    json["xstudio"]["file_format_flags"]["type"]    = "int";
    json["xstudio"]["file_format_flags"]["value"]   = Imf::getFlags(input.version());
    json["xstudio"]["file_complete"]["type"]        = "bool";
    json["xstudio"]["file_complete"]["value"]       = file_complete;

    try {
        for (auto p = 0; p < parts; p++) {
            const Imf::Header &h                    = input.header(p);
            json["headers"][p]["complete"]["type"]  = "bool";
            json["headers"][p]["complete"]["value"] = input.partComplete(p);
            success |= not dump_json_headers(h, json["headers"][p]);
        }
    } catch (const std::exception &e) {
        std::cout << e.what() << std::endl;
    }

    return json;
}

MMCertainty
OpenEXRMediaMetadata::supported(const caf::uri &uri, const std::array<uint8_t, 16> &) {
    fs::path path(uri_to_posix_path(uri));

    if (path.extension() == ".exr")
        return MMC_FULLY;

    if (path.extension() == ".sxr")
        return MMC_FULLY;

    return MMC_NO;
}

std::optional<MediaMetadata::StandardFields>
OpenEXRMediaMetadata::fill_standard_fields(const nlohmann::json &metadata) {

    StandardFields fields;
    fields.format_ = "OpenEXR";

    if (metadata.contains("headers")) {
        auto h = metadata["headers"].begin();
        if ((*h).contains("channels") && (*h)["channels"].contains("value")) {
            auto h_values = (*h)["channels"]["value"];
            for (auto chan_details : h_values) {
                if (chan_details.contains("pixelType")) {
                    fields.bit_depth_ = chan_details["pixelType"].get<std::string>();
                    break;
                }
            }
        }

        if ((*h).contains("displayWindow") && (*h)["displayWindow"].contains("value") &&
            (*h)["displayWindow"]["value"].is_array() &&
            (*h)["displayWindow"]["value"].size() == 2 &&
            (*h)["displayWindow"]["value"][0].size() == 2 &&
            (*h)["displayWindow"]["value"][1].size() == 2) {

            auto dwin_min      = (*h)["displayWindow"]["value"][0];
            auto dwin_max      = (*h)["displayWindow"]["value"][1];
            const int x        = dwin_max[0].get<int>() - dwin_min[0].get<int>() + 1;
            const int y        = dwin_max[1].get<int>() - dwin_min[1].get<int>() + 1;
            fields.resolution_ = fmt::format("{} x {}", x, y);
        }

        if ((*h).contains("pixelAspectRatio") && (*h)["pixelAspectRatio"].contains("value")) {

            auto aspect          = (*h)["pixelAspectRatio"]["value"];
            fields.pixel_aspect_ = aspect.get<float>();
        }

        if ((*h).contains("compression") && (*h)["compression"].contains("value")) {
            std::stringstream format_str;
            auto compression = (*h)["compression"]["value"].get<std::string>();
            if ((compression == "DWAA" || compression == "DWAB") &&
                (*h).contains("dwaCompressionLevel") &&
                (*h)["dwaCompressionLevel"].contains("value")) {
                auto c_level = int((*h)["dwaCompressionLevel"]["value"].get<float>());
                format_str << "EXR (" << compression << ", level=" << c_level << ")";
            } else {
                format_str << "EXR (" << compression << ")";
            }
            fields.format_ = format_str.str();
        }
    }
    return fields;
}

template <typename T> void to_json_value(nlohmann::json &root, const T *value) {
    root["type"]  = value->typeName();
    root["value"] = value->value();
}

template <typename T> void to_json_box_value(nlohmann::json &root, const T *value) {
    root["type"]        = value->typeName();
    root["value"][0][0] = value->value().min[0];
    root["value"][0][1] = value->value().min[1];
    root["value"][1][0] = value->value().max[0];
    root["value"][1][1] = value->value().max[1];
}

template <typename T> void to_json_vector_value(nlohmann::json &root, const T *value) {
    root["type"] = value->typeName();
    for (unsigned int i = 0; i < value->value().dimensions(); ++i) {
        root["value"][i] = value->value()[i];
    }
}

template <typename T> void to_json_compression_value(nlohmann::json &root, const T *value) {
    root["type"] = value->typeName();
    switch (value->value()) {
    case Imf::NO_COMPRESSION:
        root["value"] = "uncompressed";
        break;

    case Imf::RLE_COMPRESSION:
        root["value"] = "RLE";
        break;

    case Imf::ZIPS_COMPRESSION:
        root["value"] = "ZIP";
        break;

    case Imf::ZIP_COMPRESSION:
        root["value"] = "ZIPS";
        break;

    case Imf::PIZ_COMPRESSION:
        root["value"] = "PIZ";
        break;

    case Imf::PXR24_COMPRESSION:
        root["value"] = "PXR24";
        break;

    case Imf::B44_COMPRESSION:
        root["value"] = "B44";
        break;

    case Imf::B44A_COMPRESSION:
        root["value"] = "B44A";
        break;

    case Imf::DWAA_COMPRESSION:
        root["value"] = "DWAA";
        break;

    case Imf::DWAB_COMPRESSION:
        root["value"] = "DWAB";
        break;

    default:
        root["value"] = int(value->value());
        break;
    }
}

template <typename T> void to_json_lineorder_value(nlohmann::json &root, const T *value) {
    root["type"] = value->typeName();
    switch (value->value()) {
    case Imf::INCREASING_Y:
        root["value"] = "increasing y";
        break;

    case Imf::DECREASING_Y:
        root["value"] = "decreasing y";
        break;

    case Imf::RANDOM_Y:
        root["value"] = "random y";
        break;

    default:
        root["value"] = int(value->value());
        break;
    }
}


template <typename T> void to_json_chromaticities_value(nlohmann::json &root, const T *value) {
    root["type"]              = value->typeName();
    root["value"]["red"][0]   = value->value().red[0];
    root["value"]["red"][1]   = value->value().red[1];
    root["value"]["green"][0] = value->value().green[0];
    root["value"]["green"][1] = value->value().green[1];
    root["value"]["blue"][0]  = value->value().blue[0];
    root["value"]["blue"][1]  = value->value().blue[1];
    root["value"]["white"][0] = value->value().white[0];
    root["value"]["white"][1] = value->value().white[1];
}

template <typename T> void to_json_stdvector_value(nlohmann::json &root, const T *value) {
    root["type"] = value->typeName();
    for (unsigned int i = 0; i < value->value().size(); ++i) {
        root["value"][i] = value->value()[i];
    }
}

template <typename T>
void to_json_matrix_value(nlohmann::json &root, const T *value, int size) {
    root["type"] = value->typeName();
    for (int i = 0; i < size; ++i) {
        for (int j = 0; j < size; ++j) {
            root["value"][i][j] = value->value()[i][j];
        }
    }
}

template <typename T> void to_json_rational_value(nlohmann::json &root, const T *value) {
    root["type"]                 = value->typeName();
    root["value"]["numerator"]   = value->value().n;
    root["value"]["denominator"] = value->value().d;
    root["value"]["value"]       = double(value->value());
}

template <typename T> void to_json_envmap_value(nlohmann::json &root, const T *value) {
    root["type"] = value->typeName();

    switch (value->value()) {
    case Imf::ENVMAP_LATLONG:
        root["value"] = "latitude-longitude map";
        break;

    case Imf::ENVMAP_CUBE:
        root["value"] = "cube-face map";
        break;

    default:
        root["value"] = int(value->value());
        break;
    }
}


template <typename T> void to_json_channel_list_value(nlohmann::json &root, const T *value) {
    root["type"] = value->typeName();

    int j = 0;
    for (Imf::ChannelList::ConstIterator i = value->value().begin(); i != value->value().end();
         ++i, ++j) {
        root["value"][j]["name"] = i.name();

        switch (i.channel().type) {
        case Imf::UINT:
            root["value"][j]["pixelType"] = "32 bits uint";
            break;

        case Imf::HALF:
            root["value"][j]["pixelType"] = "16 bits float";
            break;

        case Imf::FLOAT:
            root["value"][j]["pixelType"] = "32 bits float";
            break;

        default:
            root["value"][j]["pixelType"] = int(i.channel().type);
            break;
        }

        root["value"][j]["xSampling"] = i.channel().xSampling;
        root["value"][j]["ySampling"] = i.channel().ySampling;
        root["value"][j]["plinear"]   = i.channel().pLinear;
    }
}

template <typename T> void to_json_key_code_value(nlohmann::json &root, const T *value) {
    root["type"]                            = value->typeName();
    root["value"]["film manufacturer code"] = value->value().filmMfcCode();
    root["value"]["film type code"]         = value->value().filmType();
    root["value"]["prefix"]                 = value->value().prefix();
    root["value"]["count"]                  = value->value().count();
    root["value"]["perf offset"]            = value->value().perfOffset();
    root["value"]["perfs per frame"]        = value->value().perfsPerFrame();
    root["value"]["perfs per count"]        = value->value().perfsPerCount();
}

template <typename T> void to_json_time_code_value(nlohmann::json &root, const T *value) {
    root["type"] = value->typeName();

    std::stringstream ss;
    ss << std::setfill('0') << std::setw(2) << value->value().hours() << ":" << std::setw(2)
       << value->value().minutes() << ":" << std::setw(2) << value->value().seconds() << ":"
       << std::setw(2) << value->value().frame();

    root["value"]["time"]        = ss.str();
    root["value"]["drop frame"]  = value->value().dropFrame();
    root["value"]["color frame"] = value->value().colorFrame();
    root["value"]["field/phase"] = value->value().fieldPhase();
    root["value"]["bgf"][0]      = value->value().bgf0();
    root["value"]["bgf"][1]      = value->value().bgf1();
    root["value"]["bgf"][2]      = value->value().bgf2();
    root["value"]["user data"]   = value->value().userData();
}

template <typename T> void to_json_preview_value(nlohmann::json &root, const T *value) {
    root["type"]            = value->typeName();
    root["value"]["width"]  = value->value().width();
    root["value"]["height"] = value->value().height();
}


template <typename T>
void to_json_tile_description_value(nlohmann::json &root, const T *value) {
    root["type"] = value->typeName();

    root["value"]["width"]  = value->value().xSize;
    root["value"]["height"] = value->value().ySize;

    switch (value->value().mode) {
    case Imf::ONE_LEVEL:
        root["value"]["level mode"] = "single level";
        break;

    case Imf::MIPMAP_LEVELS:
        root["value"]["level mode"] = "mip-map";
        break;

    case Imf::RIPMAP_LEVELS:
        root["value"]["level mode"] = "rip-map";
        break;

    default:
        root["value"]["level mode"] = int(value->value().mode);
        break;
    }

    if (value->value().mode != Imf::ONE_LEVEL) {
        switch (value->value().roundingMode) {
        case Imf::ROUND_DOWN:
            root["value"]["level rounding mode"] = "down";
            break;

        case Imf::ROUND_UP:
            root["value"]["level rounding mode"] = "up";
            break;

        default:
            root["value"]["level rounding mode"] = int(value->value().roundingMode);
            break;
        }
    }
}

bool dump_json_headers(const Imf::Header &h, nlohmann::json &root) {
    for (Imf::Header::ConstIterator i = h.begin(); i != h.end(); ++i) {
        try {
            if (auto ta = dynamic_cast<const Imf::StringAttribute *>(&i.attribute()))
                to_json_value(root[i.name()], ta);

            else if (auto ta = dynamic_cast<const Imf::FloatAttribute *>(&i.attribute()))
                to_json_value(root[i.name()], ta);

            else if (auto ta = dynamic_cast<const Imf::DoubleAttribute *>(&i.attribute()))
                to_json_value(root[i.name()], ta);

            else if (auto ta = dynamic_cast<const Imf::IntAttribute *>(&i.attribute()))
                to_json_value(root[i.name()], ta);

            else if (auto ta = dynamic_cast<const Imf::Box2iAttribute *>(&i.attribute()))
                to_json_box_value(root[i.name()], ta);

            else if (auto ta = dynamic_cast<const Imf::Box2fAttribute *>(&i.attribute()))
                to_json_box_value(root[i.name()], ta);

            else if (auto ta = dynamic_cast<const Imf::V2fAttribute *>(&i.attribute()))
                to_json_vector_value(root[i.name()], ta);

            else if (auto ta = dynamic_cast<const Imf::V2iAttribute *>(&i.attribute()))
                to_json_vector_value(root[i.name()], ta);

            else if (auto ta = dynamic_cast<const Imf::V3fAttribute *>(&i.attribute()))
                to_json_vector_value(root[i.name()], ta);

            else if (auto ta = dynamic_cast<const Imf::V3iAttribute *>(&i.attribute()))
                to_json_vector_value(root[i.name()], ta);

            else if (auto ta = dynamic_cast<const Imf::CompressionAttribute *>(&i.attribute()))
                to_json_compression_value(root[i.name()], ta);

            else if (
                auto ta = dynamic_cast<const Imf::ChromaticitiesAttribute *>(&i.attribute()))
                to_json_chromaticities_value(root[i.name()], ta);

            else if (auto ta = dynamic_cast<const Imf::StringVectorAttribute *>(&i.attribute()))
                to_json_stdvector_value(root[i.name()], ta);

            else if (auto ta = dynamic_cast<const Imf::M33fAttribute *>(&i.attribute()))
                to_json_matrix_value(root[i.name()], ta, 3);

            else if (auto ta = dynamic_cast<const Imf::M44fAttribute *>(&i.attribute()))
                to_json_matrix_value(root[i.name()], ta, 4);

            else if (auto ta = dynamic_cast<const Imf::LineOrderAttribute *>(&i.attribute()))
                to_json_lineorder_value(root[i.name()], ta);

            else if (auto ta = dynamic_cast<const Imf::ChannelListAttribute *>(&i.attribute()))
                to_json_channel_list_value(root[i.name()], ta);

            else if (auto ta = dynamic_cast<const Imf::RationalAttribute *>(&i.attribute()))
                to_json_rational_value(root[i.name()], ta);

            else if (auto ta = dynamic_cast<const Imf::EnvmapAttribute *>(&i.attribute()))
                to_json_envmap_value(root[i.name()], ta);

            else if (auto ta = dynamic_cast<const Imf::KeyCodeAttribute *>(&i.attribute()))
                to_json_key_code_value(root[i.name()], ta);

            else if (auto ta = dynamic_cast<const Imf::TimeCodeAttribute *>(&i.attribute()))
                to_json_time_code_value(root[i.name()], ta);

            else if (auto ta = dynamic_cast<const Imf::PreviewImageAttribute *>(&i.attribute()))
                to_json_preview_value(root[i.name()], ta);

            else if (
                auto ta = dynamic_cast<const Imf::TileDescriptionAttribute *>(&i.attribute()))
                to_json_tile_description_value(root[i.name()], ta);

            else {
                root[i.name()]["type"]  = i.attribute().typeName();
                root[i.name()]["value"] = nullptr;
            }
        } catch ([[maybe_unused]] const Iex::TypeExc &e) {
            root[i.name()]["type"]  = i.attribute().typeName();
            root[i.name()]["value"] = nullptr;
        }
    }

    return true;
}

extern "C" {
plugin_manager::PluginFactoryCollection *plugin_factory_collection_ptr() {
    return new plugin_manager::PluginFactoryCollection(
        std::vector<std::shared_ptr<plugin_manager::PluginFactory>>(
            {std::make_shared<MediaMetadataPlugin<MediaMetadataActor<OpenEXRMediaMetadata>>>(
                Uuid("9a2fd9ec-6678-44d8-9dec-f6e52c784cc0"),
                "OpenEXR",
                "xStudio",
                "OpenEXR Media Metadata",
                semver::version("1.0.0"))}));
}
}
