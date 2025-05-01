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

namespace xstudio::exr_reader {

bool dump_json_headers(const Imf::Header &h, nlohmann::json &root);

template <typename T> void to_json_value(nlohmann::json &root, const T *value) {
    root = value->value();
}

template <typename T> void to_json_box_value(nlohmann::json &root, const T *value) {
    root[0][0] = value->value().min[0];
    root[0][1] = value->value().min[1];
    root[1][0] = value->value().max[0];
    root[1][1] = value->value().max[1];
}

template <typename T> void to_json_vector_value(nlohmann::json &root, const T *value) {
    for (unsigned int i = 0; i < value->value().dimensions(); ++i) {
        root[i] = value->value()[i];
    }
}

template <typename T> void to_json_compression_value(nlohmann::json &root, const T *value) {
    switch (value->value()) {
    case Imf::NO_COMPRESSION:
        root = "uncompressed";
        break;

    case Imf::RLE_COMPRESSION:
        root = "RLE";
        break;

    case Imf::ZIPS_COMPRESSION:
        root = "ZIP";
        break;

    case Imf::ZIP_COMPRESSION:
        root = "ZIPS";
        break;

    case Imf::PIZ_COMPRESSION:
        root = "PIZ";
        break;

    case Imf::PXR24_COMPRESSION:
        root = "PXR24";
        break;

    case Imf::B44_COMPRESSION:
        root = "B44";
        break;

    case Imf::B44A_COMPRESSION:
        root = "B44A";
        break;

    case Imf::DWAA_COMPRESSION:
        root = "DWAA";
        break;

    case Imf::DWAB_COMPRESSION:
        root = "DWAB";
        break;

    default:
        root = int(value->value());
        break;
    }
}

template <typename T> void to_json_lineorder_value(nlohmann::json &root, const T *value) {
    switch (value->value()) {
    case Imf::INCREASING_Y:
        root = "increasing y";
        break;

    case Imf::DECREASING_Y:
        root = "decreasing y";
        break;

    case Imf::RANDOM_Y:
        root = "random y";
        break;

    default:
        root = int(value->value());
        break;
    }
}


template <typename T> void to_json_chromaticities_value(nlohmann::json &root, const T *value) {
    root["red"][0]   = value->value().red[0];
    root["red"][1]   = value->value().red[1];
    root["green"][0] = value->value().green[0];
    root["green"][1] = value->value().green[1];
    root["blue"][0]  = value->value().blue[0];
    root["blue"][1]  = value->value().blue[1];
    root["white"][0] = value->value().white[0];
    root["white"][1] = value->value().white[1];
}

template <typename T> void to_json_stdvector_value(nlohmann::json &root, const T *value) {
    for (unsigned int i = 0; i < value->value().size(); ++i) {
        root[i] = value->value()[i];
    }
}

template <typename T>
void to_json_matrix_value(nlohmann::json &root, const T *value, int size) {
    for (int i = 0; i < size; ++i) {
        for (int j = 0; j < size; ++j) {
            root[i][j] = value->value()[i][j];
        }
    }
}

template <typename T> void to_json_rational_value(nlohmann::json &root, const T *value) {
    root["numerator"]   = value->value().n;
    root["denominator"] = value->value().d;
    root                = double(value->value());
}

template <typename T> void to_json_envmap_value(nlohmann::json &root, const T *value) {

    switch (value->value()) {
    case Imf::ENVMAP_LATLONG:
        root = "latitude-longitude map";
        break;

    case Imf::ENVMAP_CUBE:
        root = "cube-face map";
        break;

    default:
        root = int(value->value());
        break;
    }
}


template <typename T> void to_json_channel_list_value(nlohmann::json &root, const T *value) {

    // this is surely too verbose for our needs ...
    /*int j = 0;
    for (Imf::ChannelList::ConstIterator i = value->value().begin(); i != value->value().end();
         ++i, ++j) {
        root[j]["name"] = i.name();

        switch (i.channel().type) {
        case Imf::UINT:
            root[j]["pixelType"] = "32 bits uint";
            break;

        case Imf::HALF:
            root[j]["pixelType"] = "16 bits float";
            break;

        case Imf::FLOAT:
            root[j]["pixelType"] = "32 bits float";
            break;

        default:
            root[j]["pixelType"] = int(i.channel().type);
            break;
        }

        root[j]["xSampling"] = i.channel().xSampling;
        root[j]["ySampling"] = i.channel().ySampling;
        root[j]["plinear"]   = i.channel().pLinear;
    }*/

    // Instead of above, names and format, just list channel names and
    // pixel type in one string entry
    int fmt = -1;
    for (Imf::ChannelList::ConstIterator i = value->value().begin(); i != value->value().end();
         ++i) {

        if (fmt == -1) {
            fmt = i.channel().type;
        } else if (fmt != i.channel().type) {
            fmt = -2;
            break;
        }
    }

    std::string chan_names;
    if (fmt != -2) {

        for (Imf::ChannelList::ConstIterator i = value->value().begin();
             i != value->value().end();
             ++i) {
            if (i != value->value().begin())
                chan_names += ", ";
            chan_names += i.name();
        }

        switch (fmt) {
        case Imf::UINT:
            chan_names += " (32 bits uint)";
            break;

        case Imf::HALF:
            chan_names += " (16 bits float)";
            break;

        case Imf::FLOAT:
            chan_names += " (32 bits float)";
            break;

        default:
            chan_names += fmt::format(" (pix fmt id {})", fmt);
            break;
        }

    } else {

        for (Imf::ChannelList::ConstIterator i = value->value().begin();
             i != value->value().end();
             ++i) {

            if (i != value->value().begin())
                chan_names += ", ";
            chan_names += i.name();

            switch (i.channel().type) {
            case Imf::UINT:
                chan_names += " (32 bits uint)";
                break;

            case Imf::HALF:
                chan_names += " (16 bits float)";
                break;

            case Imf::FLOAT:
                chan_names += " (32 bits float)";
                break;

            default:
                chan_names += fmt::format(" (pix fmt id {})", fmt);
                break;
            }
        }
    }

    root = chan_names;
}

template <typename T> void to_json_key_code_value(nlohmann::json &root, const T *value) {
    root["film manufacturer code"] = value->value().filmMfcCode();
    root["film type code"]         = value->value().filmType();
    root["prefix"]                 = value->value().prefix();
    root["count"]                  = value->value().count();
    root["perf offset"]            = value->value().perfOffset();
    root["perfs per frame"]        = value->value().perfsPerFrame();
    root["perfs per count"]        = value->value().perfsPerCount();
}

template <typename T> void to_json_time_code_value(nlohmann::json &root, const T *value) {
    std::stringstream ss;
    ss << std::setfill('0') << std::setw(2) << value->value().hours() << ":" << std::setw(2)
       << value->value().minutes() << ":" << std::setw(2) << value->value().seconds() << ":"
       << std::setw(2) << value->value().frame();

    root["time"]        = ss.str();
    root["drop frame"]  = value->value().dropFrame();
    root["color frame"] = value->value().colorFrame();
    root["field/phase"] = value->value().fieldPhase();
    root["bgf"][0]      = value->value().bgf0();
    root["bgf"][1]      = value->value().bgf1();
    root["bgf"][2]      = value->value().bgf2();
    root["user data"]   = value->value().userData();
}

template <typename T> void to_json_preview_value(nlohmann::json &root, const T *value) {
    root["width"]  = value->value().width();
    root["height"] = value->value().height();
}


template <typename T>
void to_json_tile_description_value(nlohmann::json &root, const T *value) {

    root["width"]  = value->value().xSize;
    root["height"] = value->value().ySize;

    switch (value->value().mode) {
    case Imf::ONE_LEVEL:
        root["level mode"] = "single level";
        break;

    case Imf::MIPMAP_LEVELS:
        root["level mode"] = "mip-map";
        break;

    case Imf::RIPMAP_LEVELS:
        root["level mode"] = "rip-map";
        break;

    default:
        root["level mode"] = int(value->value().mode);
        break;
    }

    if (value->value().mode != Imf::ONE_LEVEL) {
        switch (value->value().roundingMode) {
        case Imf::ROUND_DOWN:
            root["level rounding mode"] = "down";
            break;

        case Imf::ROUND_UP:
            root["level rounding mode"] = "up";
            break;

        default:
            root["level rounding mode"] = int(value->value().roundingMode);
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
                root[i.name()] = nullptr;
            }
        } catch ([[maybe_unused]] const Iex::TypeExc &e) {
            root[i.name()] = nullptr;
        }
    }

    return true;
}
} // namespace xstudio::exr_reader