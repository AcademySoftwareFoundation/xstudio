// SPDX-License-Identifier: Apache-2.0
#include "ffprobe_lib.hpp"
#include "xstudio/utility/helpers.hpp"

#include <string>

#include <fmt/format.h>
#include <nlohmann/json.hpp>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavdevice/avdevice.h>
#include <libavformat/avformat.h>
#include <libavutil/bprint.h>
#include <libavutil/pixdesc.h>
#include <libavutil/timecode.h>
}

#ifdef __GNUC__ // Check if GCC compiler is being used
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

using namespace xstudio;
using namespace xstudio::ffprobe;

namespace {

const auto av_time_base_q = av_get_time_base_q(); // READ https://libav-devel.libav.narkive.com/ZQCWfTun/patch-0-2-fix-avutil-h-usage-from-c

int check_stream_specifier(AVFormatContext *avfs, AVStream *avs, const char *spec) {
    auto result = avformat_match_stream_specifier(avfs, avs, spec);

    if (result < 0)
        spdlog::warn("Invalid stream specifier: {}", spec);

    return result;
}


AVDictionary *filter_codec_opts(
    AVDictionary *opts,
    const AVCodecID codec_id,
    AVFormatContext *avfc,
    AVStream *avs,
    const AVCodec *codec) {
    const AVClass *avc      = avcodec_get_class();
    AVDictionary *result    = nullptr;
    AVDictionaryEntry *avdt = nullptr;
    int flags   = avfc->oformat ? AV_OPT_FLAG_ENCODING_PARAM : AV_OPT_FLAG_DECODING_PARAM;
    char prefix = 0;

    switch (avs->codecpar->codec_type) {
    case AVMEDIA_TYPE_VIDEO:
        prefix = 'v';
        flags |= AV_OPT_FLAG_VIDEO_PARAM;
        break;

    case AVMEDIA_TYPE_AUDIO:
        prefix = 'a';
        flags |= AV_OPT_FLAG_AUDIO_PARAM;
        break;

    case AVMEDIA_TYPE_SUBTITLE:
        prefix = 's';
        flags |= AV_OPT_FLAG_SUBTITLE_PARAM;
        break;
    }

    while (avdt = av_dict_get(opts, "", avdt, AV_DICT_IGNORE_SUFFIX)) {
        char *ss = strchr(avdt->key, ':');

        if (ss) {
            switch (check_stream_specifier(avfc, avs, ss + 1)) {
            case 1:
                *ss = 0;
                break;

            case 0:
                continue;

            default:
                break;
            }
        }

        const AVClass *priv_class = codec->priv_class;
        if (av_opt_find(&avc, avdt->key, nullptr, flags, AV_OPT_SEARCH_FAKE_OBJ) or not codec or
            (priv_class and
             av_opt_find(&priv_class, avdt->key, nullptr, flags, AV_OPT_SEARCH_FAKE_OBJ))) {
            av_dict_set(&result, avdt->key, avdt->value, 0);
        } else if (
            avdt->key[0] == prefix and
            av_opt_find(&avc, avdt->key + 1, nullptr, flags, AV_OPT_SEARCH_FAKE_OBJ)) {
            av_dict_set(&result, avdt->key + 1, avdt->value, 0);
        }

        if (ss)
            *ss = ':';
    }
    return result;
}

AVDictionary **init_find_stream_opts(AVFormatContext *avfc, AVDictionary *codec_opts) {
    AVDictionary **result = nullptr;

    if (avfc->nb_streams) {
#ifdef _WIN32
        result = (AVDictionary **)av_calloc(avfc->nb_streams, sizeof(*result));
#else
        result = (AVDictionary **)av_malloc_array(avfc->nb_streams, sizeof(*result));
#endif

        if (result) {
            for (unsigned int i = 0; i < avfc->nb_streams; i++)
                result[i] = filter_codec_opts(
                    codec_opts,
                    avfc->streams[i]->codecpar->codec_id,
                    avfc,
                    avfc->streams[i],
                    avfc->oformat ? avcodec_find_encoder(avfc->streams[i]->codecpar->codec_id)
                                  : avcodec_find_decoder(avfc->streams[i]->codecpar->codec_id));

        } else {
            spdlog::warn("Could not alloc memory for stream options");
        }
    }

    return result;
}


std::optional<double>
duration_as_double(int64_t ts, const AVRational *time_base, const bool is_duration = true) {
    if ((not is_duration and ts == AV_NOPTS_VALUE) or (is_duration and ts == 0))
        return {};
    return static_cast<double>(ts * av_q2d(*time_base));
}

std::optional<double> time_as_double(int64_t ts, const AVRational *time_base) {
    return duration_as_double(ts, time_base, false);
}

std::optional<int64_t> duration_as_int(const int64_t ts, const bool is_duration = true) {
    if ((not is_duration and ts == AV_NOPTS_VALUE) or (is_duration and ts == 0))
        return {};
    return ts;
}

std::optional<int64_t> time_as_int(const int64_t ts, const bool is_duration = true) {
    return duration_as_int(ts, false);
}

std::string rational_as_string(const AVRational &q, const char *sep = "/") {
    return std::to_string(q.num) + std::string(sep) + std::to_string(q.den);
}

std::optional<std::string> color_range_as_string(const AVColorRange color_range) {
    const char *val = av_color_range_name(color_range);

    if (not val or color_range == AVCOL_RANGE_UNSPECIFIED)
        return {};

    return val;
}

std::optional<std::string> color_space_as_string(const AVColorSpace color_space) {
    const char *val = av_color_space_name(color_space);

    if (not val or color_space == AVCOL_SPC_UNSPECIFIED)
        return {};

    return val;
}

std::optional<std::string> color_primaries_as_string(const AVColorPrimaries color_primaries) {
    const char *val = av_color_primaries_name(color_primaries);

    if (not val or color_primaries == AVCOL_PRI_UNSPECIFIED)
        return {};

    return val;
}

std::optional<std::string> color_trc_as_string(const AVColorTransferCharacteristic color_trc) {
    const char *val = av_color_transfer_name(color_trc);

    if (not val or color_trc == AVCOL_TRC_UNSPECIFIED)
        return {};

    return val;
}

std::optional<std::string> chroma_location_as_string(const AVChromaLocation chroma_location) {
    const char *val = av_chroma_location_name(chroma_location);

    if (not val or chroma_location == AVCHROMA_LOC_UNSPECIFIED)
        return {};

    return val;
}

nlohmann::json populate_tags(AVDictionary *tags) {
    auto result = R"({})"_json;

    AVDictionaryEntry *tag = nullptr;

    if (tags) {
        while ((tag = av_dict_get(tags, "", tag, AV_DICT_IGNORE_SUFFIX))) {
            result[tag->key] = tag->value;
        }
    }

    return result;
}

nlohmann::json populate_format(MediaFile &src) {

    AVFormatContext *avfc = src.fmt_ctx;
    auto result           = R"({})"_json;

    result["filename"]    = avfc->url;
    result["nb_streams"]  = avfc->nb_streams;
    result["nb_programs"] = avfc->nb_programs;
    result["format_name"] = avfc->iformat->name;
    result["probe_score"] = avfc->probe_score;

    result["format_long_name"] = nullptr;
    result["bit_rate"]         = nullptr;
    result["size"]             = nullptr;
    result["start_time"]       = nullptr;
    result["duration"]         = nullptr;

    if (avfc->iformat->long_name)
        result["format_long_name"] = avfc->iformat->long_name;

    int64_t size = avfc->pb ? avio_size(avfc->pb) : -1;
    if (size >= 0)
        result["size"] = size;

    if (avfc->bit_rate > 0)
        result["bit_rate"] = avfc->bit_rate;

    auto dtmp = time_as_double(avfc->start_time, &av_time_base_q);
    if (dtmp)
        result["start_time"] = *dtmp;

    dtmp = duration_as_double(avfc->duration, &av_time_base_q);
    if (dtmp)
        result["duration"] = *dtmp;

    result["tags"] = populate_tags(avfc->metadata);

    return result;
}

nlohmann::json populate_stream(AVFormatContext *avfc, int index, MediaStream *ist) {
    auto result                 = R"({})"_json;
    AVStream *stream            = ist->st;
    AVCodecContext *dec_ctx     = ist->dec_ctx;
    AVCodecParameters *par      = stream->codecpar;
    const AVCodecDescriptor *cd = nullptr;
    const char *profile         = nullptr;

    result["index"]           = stream->index;
    result["codec_name"]      = nullptr;
    result["codec_long_name"] = nullptr;

    if (cd = avcodec_descriptor_get(par->codec_id)) {
        result["codec_name"] = cd->name;
        if (cd->long_name)
            result["codec_long_name"] = cd->long_name;
    }

    result["profile"] = nullptr;
    if (profile = avcodec_profile_name(par->codec_id, par->profile))
        result["profile"] = profile;
    else if (par->profile != FF_PROFILE_UNKNOWN) {
        result["profile"] = std::to_string(par->profile);
    }

    result["codec_type"] = nullptr;

    if (auto s = av_get_media_type_string(par->codec_type))
        result["codec_type"] = s;

#if FF_API_LAVF_AVCTX
    if (dec_ctx)
        result["codec_time_base"] = rational_as_string(dec_ctx->time_base);
#endif


    char cc4buf[AV_FOURCC_MAX_STRING_SIZE];
    result["codec_tag_string"] = av_fourcc_make_string(cc4buf, par->codec_tag);
    result["codec_tag"]        = fmt::format("{:#04x}", par->codec_tag);

    switch (par->codec_type) {
    case AVMEDIA_TYPE_VIDEO: {
        result["width"]  = par->width;
        result["height"] = par->height;
#if FF_API_LAVF_AVCTX
        if (dec_ctx) {
            result["coded_width"]  = dec_ctx->coded_width;
            result["coded_height"] = dec_ctx->coded_height;
        }
#endif
        result["has_b_frames"]         = par->video_delay;
        result["sample_aspect_ratio"]  = nullptr;
        result["display_aspect_ratio"] = nullptr;

        auto sar = av_guess_sample_aspect_ratio(avfc, stream, nullptr);
        if (sar.num) {
            result["sample_aspect_ratio"] = rational_as_string(sar, ":");

            AVRational dar;
            av_reduce(
                &dar.num, &dar.den, par->width * sar.num, par->height * sar.den, 1024 * 1024);
            result["display_aspect_ratio"] = rational_as_string(dar, ":");
        }

        result["pix_fmt"] = nullptr;
        auto s            = av_get_pix_fmt_name((AVPixelFormat)par->format);
        if (s)
            result["pix_fmt"] = s;

        result["level"] = par->level;

        result["color_range"]     = nullptr;
        result["color_space"]     = nullptr;
        result["color_primaries"] = nullptr;
        result["color_transfer"]  = nullptr;
        result["chroma_location"] = nullptr;

        auto stmp = color_range_as_string(par->color_range);
        if (stmp)
            result["color_range"] = *stmp;

        stmp = color_space_as_string(par->color_space);
        if (stmp)
            result["color_space"] = *stmp;

        stmp = color_primaries_as_string(par->color_primaries);
        if (stmp)
            result["color_primaries"] = *stmp;

        stmp = color_trc_as_string(par->color_trc);
        if (stmp)
            result["color_transfer"] = *stmp;

        stmp = chroma_location_as_string(par->chroma_location);
        if (stmp)
            result["chroma_location"] = *stmp;

        result["field_order"] = nullptr;
        if (par->field_order == AV_FIELD_PROGRESSIVE)
            result["field_order"] = "progressive";
        else if (par->field_order == AV_FIELD_TT)
            result["field_order"] = "tt";
        else if (par->field_order == AV_FIELD_BB)
            result["field_order"] = "bb";
        else if (par->field_order == AV_FIELD_TB)
            result["field_order"] = "tb";
        else if (par->field_order == AV_FIELD_BT)
            result["field_order"] = "bt";

#if FF_API_PRIVATE_OPT
        result["timecode"] = nullptr;
        if (dec_ctx and dec_ctx->timecode_frame_start >= 0) {
            char tcbuf[AV_TIMECODE_STR_SIZE];
            av_timecode_make_mpeg_tc_string(tcbuf, dec_ctx->timecode_frame_start);
            result["timecode"] = tcbuf;
        }
#endif
        if (dec_ctx)
            result["refs"] = dec_ctx->refs;
    } break;

    case AVMEDIA_TYPE_AUDIO: {
        result["sample_fmt"]     = nullptr;
        result["channel_layout"] = nullptr;

        auto s = av_get_sample_fmt_name((AVSampleFormat)par->format);
        if (s)
            result["sample_fmt"] = s;

        result["sample_rate"] = par->sample_rate;
        result["channels"]    = par->channels;

        if (par->channel_layout) {
            AVBPrint buf;
            av_bprint_init(&buf, 1, AV_BPRINT_SIZE_UNLIMITED);
            av_bprint_channel_layout(&buf, par->channels, par->channel_layout);
            result["channel_layout"] = buf.str;
            av_bprint_finalize(&buf, nullptr);
        }
        result["bits_per_sample"] = av_get_bits_per_sample(par->codec_id);
    } break;

    case AVMEDIA_TYPE_SUBTITLE:
        result["width"]  = nullptr;
        result["height"] = nullptr;
        if (par->width)
            result["width"] = par->width;
        if (par->height)
            result["height"] = par->height;
        break;
    }

    result["id"] = nullptr;

    if (avfc->iformat->flags & AVFMT_SHOW_IDS)
        result["id"] = fmt::format("{:#0x}", stream->id);

    result["r_frame_rate"]   = rational_as_string(stream->r_frame_rate);
    result["avg_frame_rate"] = rational_as_string(stream->avg_frame_rate);
    result["time_base"]      = rational_as_string(stream->time_base);


    result["start_pts"]   = nullptr;
    result["start_time"]  = nullptr;
    result["duration_ts"] = nullptr;
    result["duration"]    = nullptr;

    auto itmp = time_as_int(stream->start_time);
    if (itmp)
        result["start_pts"] = *itmp;
    itmp = duration_as_int(stream->duration);
    if (itmp)
        result["duration_ts"] = *itmp;

    auto dtmp = time_as_double(stream->start_time, &stream->time_base);
    if (dtmp)
        result["start_time"] = *dtmp;

    dtmp = duration_as_double(stream->duration, &stream->time_base);
    if (dtmp)
        result["duration"] = *dtmp;

    result["bit_rate"] = nullptr;
    if (par->bit_rate > 0)
        result["bit_rate"] = par->bit_rate;

#if FF_API_LAVF_AVCTX
    result["max_bit_rate"] = nullptr;
    if (stream->codec->rc_max_rate > 0)
        result["max_bit_rate"] = stream->codec->rc_max_rate;
#endif

    result["bits_per_raw_sample"] = nullptr;
    if (dec_ctx and dec_ctx->bits_per_raw_sample > 0)
        result["bits_per_raw_sample"] = dec_ctx->bits_per_raw_sample;

    result["nb_frames"] = nullptr;
    if (stream->nb_frames)
        result["nb_frames"] = stream->nb_frames;

    result["disposition"]["attached_pic"]  = stream->disposition & AV_DISPOSITION_ATTACHED_PIC;
    result["disposition"]["clean_effects"] = stream->disposition & AV_DISPOSITION_CLEAN_EFFECTS;
    result["disposition"]["comment"]       = stream->disposition & AV_DISPOSITION_COMMENT;
    result["disposition"]["default"]       = stream->disposition & AV_DISPOSITION_DEFAULT;
    result["disposition"]["dub"]           = stream->disposition & AV_DISPOSITION_DUB;
    result["disposition"]["forced"]        = stream->disposition & AV_DISPOSITION_FORCED;
    result["disposition"]["hearing_impaired"] =
        stream->disposition & AV_DISPOSITION_HEARING_IMPAIRED;
    result["disposition"]["karaoke"]  = stream->disposition & AV_DISPOSITION_KARAOKE;
    result["disposition"]["lyrics"]   = stream->disposition & AV_DISPOSITION_LYRICS;
    result["disposition"]["original"] = stream->disposition & AV_DISPOSITION_ORIGINAL;
    result["disposition"]["timed_thumbnails"] =
        stream->disposition & AV_DISPOSITION_TIMED_THUMBNAILS;
    result["disposition"]["visual_impaired"] =
        stream->disposition & AV_DISPOSITION_VISUAL_IMPAIRED;

    result["tags"] = populate_tags(stream->metadata);

    return result;
}

nlohmann::json populate_streams(MediaFile &src) {
    auto result = R"([])"_json;

    for (auto i = 0; i < src.nb_streams; i++)
        result[i] = populate_stream(src.fmt_ctx, i, &(src.streams[i]));

    return result;
}
} // namespace


FFProbe::FFProbe() {
    av_log_set_level(AV_LOG_QUIET);
    avformat_network_init();
}

FFProbe::~FFProbe() { avformat_network_deinit(); }

utility::JsonStore FFProbe::probe_file(const caf::uri &uri_path) {
    auto result = R"({})"_json;
    auto ptr    = open_file(utility::uri_to_posix_path(uri_path));

    if (ptr) {
        try {
            result["streams"] = populate_streams(*ptr);
            result["format"]  = populate_format(*ptr);
        } catch (const std::exception &err) {
            spdlog::warn("{}", err.what());
        }
    }

    return utility::JsonStore(result);
}

std::string FFProbe::probe_file(const std::string &path) {
    return probe_file(utility::posix_path_to_uri(path)).dump(2);
}

std::shared_ptr<MediaFile> FFProbe::open_file(const std::string &path) {
    auto result                  = std::make_shared<MediaFile>();
    AVDictionary *format_opts    = nullptr;
    AVDictionary *codec_opts     = nullptr;
    AVDictionaryEntry *t         = nullptr;
    const AVInputFormat *iformat = nullptr;

    try {
        result->fmt_ctx = avformat_alloc_context();

        if (not result->fmt_ctx) {
            throw std::runtime_error("Failed to create context");
        }

        av_dict_set(&format_opts, "scan_all_pmts", "1", AV_DICT_DONT_OVERWRITE);
        auto scan_all_pmts_set = 1;

        iformat = av_find_input_format("format");
        if (not iformat) {
            // throw std::runtime_error("Unknown input format");
        }

        if (avformat_open_input(&(result->fmt_ctx), path.c_str(), iformat, &format_opts) < 0) {
            throw std::runtime_error("Failed to open file");
        }

        if (scan_all_pmts_set)
            av_dict_set(&format_opts, "scan_all_pmts", nullptr, AV_DICT_MATCH_CASE);

        // what is this even doing ?
        if ((t = av_dict_get(format_opts, "", nullptr, AV_DICT_IGNORE_SUFFIX))) {
            throw std::runtime_error("Option scan_all_pmts not found.");
        }

        {
            AVDictionary **opts  = init_find_stream_opts(result->fmt_ctx, codec_opts);
            auto orig_nb_streams = result->fmt_ctx->nb_streams;
            auto err             = avformat_find_stream_info(result->fmt_ctx, opts);

            for (unsigned int i = 0; i < orig_nb_streams; i++)
                av_dict_free(&opts[i]);
            av_freep(&opts);

            if (err < 0)
                throw std::runtime_error("Failed to find streams");
        }

        av_dump_format(result->fmt_ctx, 0, path.c_str(), 0);

        result->nb_streams = result->fmt_ctx->nb_streams;
        result->streams.resize(result->nb_streams);

        /* bind a decoder to each input stream */
        for (size_t i = 0; i < result->streams.size(); i++) {
            MediaStream *ist = &(result->streams[i]);
            AVStream *stream = result->fmt_ctx->streams[i];

            ist->st = stream;

            if (stream->codecpar->codec_id == AV_CODEC_ID_PROBE) {
                spdlog::debug("Failed to probe codec for input stream {}", stream->index);
                continue;
            }

            const AVCodec *codec = avcodec_find_decoder(stream->codecpar->codec_id);
            if (not codec) {
                spdlog::debug(
                    "Unsupported codec with id {} for input stream {}",
                    stream->codecpar->codec_id,
                    stream->index);
                continue;
            }

            {
                AVDictionary *opts = filter_codec_opts(
                    codec_opts, stream->codecpar->codec_id, result->fmt_ctx, stream, codec);

                ist->dec_ctx = avcodec_alloc_context3(codec);
                if (not ist->dec_ctx)
                    throw std::runtime_error("Failed to allocate context");

                if (avcodec_parameters_to_context(ist->dec_ctx, stream->codecpar) < 0)
                    throw std::runtime_error("Failed to avcodec_parameters_to_context");

                ist->dec_ctx->pkt_timebase = stream->time_base;
                ist->dec_ctx->framerate    = stream->avg_frame_rate;
#if FF_API_LAVF_AVCTX
                ist->dec_ctx->coded_width  = stream->codec->coded_width;
                ist->dec_ctx->coded_height = stream->codec->coded_height;
#endif

                if (avcodec_open2(ist->dec_ctx, codec, &opts) < 0)
                    throw std::runtime_error(
                        fmt::format("Could not open codec for input stream {}", stream->index));

                if ((t = av_dict_get(opts, "", nullptr, AV_DICT_IGNORE_SUFFIX))) {
                    throw std::runtime_error(fmt::format(
                        "Option {} for input stream {} not found", t->key, stream->index));
                }
            }
        }

    } catch (const std::exception &err) {
        spdlog::debug("{} {}", __PRETTY_FUNCTION__, err.what());
        result.reset();
    }

    if (format_opts)
        av_dict_free(&format_opts);

    if (codec_opts)
        av_dict_free(&codec_opts);

    return result;
}

MediaFile::~MediaFile() {
    for (size_t i = 0; i < streams.size(); i++)
        if (streams[i].st->codecpar->codec_id != AV_CODEC_ID_NONE)
            avcodec_free_context(&streams[i].dec_ctx);

    if (fmt_ctx)
        avformat_close_input(&fmt_ctx);
}
