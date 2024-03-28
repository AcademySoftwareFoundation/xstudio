// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <caf/allowed_unsafe_message_type.hpp>
#include <caf/type_id.hpp>
#include <memory>
#include <filesystem>

#include <cpp-httplib/httplib.h>
#include <stduuid/uuid.h>
#include <semver.hpp>

#include "flicks.hpp"
#include "xstudio/enums.hpp"
#include "xstudio/caf_error.hpp"
#include "xstudio/utility/chrono.hpp"
#include "xstudio/utility/uuid.hpp"
#include <Imath/ImathBox.h>
#include <Imath/ImathMatrix.h>
#include <Imath/ImathVec.h>

namespace xstudio {

const std::string audio_cache_registry{"AUDIOCACHE"};
const std::string audio_output_registry{"AUDIO_OUTPUT"};
const std::string colour_cache_registry{"COLOURCACHE"};
const std::string colour_pipeline_registry{"COLOURPIPELINE"};
const std::string conform_registry{"CONFORM"};
const std::string embedded_python_registry{"EMBEDDEDPYTHON"};
const std::string global_event_group{"XSTUDIO_EVENTS"};
const std::string global_registry{"GLOBAL"};
const std::string global_playhead_events_actor{"GLOBALPLAYHEADEVENTS"};
const std::string global_store_registry{"GLOBALSTORE"};
const std::string image_cache_registry{"IMAGECACHE"};
const std::string keyboard_events{"KEYBOARDEVENTS"};
const std::string media_hook_registry{"MEDIAHOOK"};
const std::string media_metadata_registry{"MEDIAMETADATA"};
const std::string media_reader_registry{"MEDIAREADER"};
const std::string module_events_registry{"MODULE_EVENTS"};
const std::string offscreen_viewport_registry{"OFFSCREEN_VIEWPORT"};
const std::string plugin_manager_registry{"PLUGINMNGR"};
const std::string scanner_registry{"SCANNER"};
const std::string studio_registry{"STUDIO"};
const std::string studio_ui_registry{"STUDIOUI"};
const std::string sync_gateway_manager_registry{"SYNCGATEMAN"};
const std::string sync_gateway_registry{"SYNCGATE"};
const std::string thumbnail_manager_registry{"THUMBNAIL"};
const std::string global_ui_model_data_registry{"GLOBALUIMODELDATA"};

namespace bookmark {
    class AnnotationBase;
    struct BookmarkDetail;
    struct BookmarkAndAnnotation;
    typedef std::shared_ptr<const BookmarkAndAnnotation> BookmarkAndAnnotationPtr;
} // namespace bookmark

namespace event {
    class Event;
}

namespace tag {
    class Tag;
}

namespace timeline {
    class Item;
}

namespace utility {
    class BlindDataObject;
    class ContainerTree;
    class EditList;
    class FrameList;
    class FrameRange;
    class FrameRate;
    class FrameRateDuration;
    class JsonStore;
    class MediaReference;
    class PlaylistTree;
    class Timecode;
    class UuidActor;
    struct absolute_receive_timeout;
    struct ContainerDetail;
    struct CopyResult;
} // namespace utility

namespace colour_pipeline {
    class ColourPipeline;
    struct ColourPipelineData;
    struct ColourOperationData;
    typedef std::shared_ptr<ColourPipelineData> ColourPipelineDataPtr;
    typedef std::shared_ptr<ColourOperationData> ColourOperationDataPtr;
} // namespace colour_pipeline

namespace media {
    class MediaDetail;
    class MediaKey;
    class AVFrameID;
    class StreamDetail;
    typedef std::map<timebase::flicks, std::shared_ptr<const AVFrameID>> FrameTimeMap;
    typedef std::vector<std::pair<utility::time_point, std::shared_ptr<const AVFrameID>>>
        AVFrameIDsAndTimePoints;
    typedef std::vector<std::shared_ptr<const AVFrameID>> AVFrameIDs;
    typedef std::vector<MediaKey> MediaKeyVector;
} // namespace media

namespace conform {
    struct ConformRequest;
    struct ConformReply;
} // namespace conform

namespace media_reader {
    class AudioBuffer;
    class AudioBufPtr;
    class ImageBufPtr;
    class MediaReaderManager;
    class PixelInfo;
} // namespace media_reader

namespace thumbnail {
    class ThumbnailBuffer;
    class ThumbnailKey;
    struct DiskCacheStat;
} // namespace thumbnail

namespace shotgun_client {
    class AuthenticateShotgun;
} // namespace shotgun_client

namespace ui {
    class Hotkey;
    class PointerEvent;
    struct Signature;
    namespace viewport {
        struct GPUShader;
        typedef std::shared_ptr<const GPUShader> GPUShaderPtr;
    } // namespace viewport

} // namespace ui

namespace plugin_manager {
    class PluginDetail;
} // namespace plugin_manager

namespace plugin {
    class ViewportOverlayRenderer;
    class GPUPreDrawHook;
} // namespace plugin

namespace module {
    class Attribute;
} // namespace module
} // namespace xstudio

using namespace caf;

#define ACTOR_INIT_GLOBAL_META(...)                                                            \
    caf::init_global_meta_objects<caf::id_block::xstudio_simple_types>();                      \
    caf::init_global_meta_objects<caf::id_block::xstudio_complex_types>();                     \
    caf::init_global_meta_objects<caf::id_block::xstudio_framework_atoms>();                   \
    caf::init_global_meta_objects<caf::id_block::xstudio_plugin_atoms>();                      \
    caf::init_global_meta_objects<caf::id_block::xstudio_session_atoms>();                     \
    caf::init_global_meta_objects<caf::id_block::xstudio_playback_atoms>();                    \
    caf::init_global_meta_objects<caf::id_block::xstudio_ui_atoms>();

CAF_ALLOW_UNSAFE_MESSAGE_TYPE(httplib::Headers)
CAF_ALLOW_UNSAFE_MESSAGE_TYPE(httplib::Params)
CAF_ALLOW_UNSAFE_MESSAGE_TYPE(httplib::Response)
CAF_ALLOW_UNSAFE_MESSAGE_TYPE(std::shared_ptr<const xstudio::module::Attribute>)
CAF_ALLOW_UNSAFE_MESSAGE_TYPE(std::shared_ptr<const xstudio::utility::BlindDataObject>)
CAF_ALLOW_UNSAFE_MESSAGE_TYPE(std::shared_ptr<xstudio::bookmark::AnnotationBase>)
CAF_ALLOW_UNSAFE_MESSAGE_TYPE(std::shared_ptr<xstudio::colour_pipeline::ColourPipeline>)
CAF_ALLOW_UNSAFE_MESSAGE_TYPE(std::shared_ptr<xstudio::media::AVFrameID>)
CAF_ALLOW_UNSAFE_MESSAGE_TYPE(std::shared_ptr<xstudio::media_reader::AudioBuffer>)
CAF_ALLOW_UNSAFE_MESSAGE_TYPE(std::shared_ptr<xstudio::media_reader::MediaReaderManager>)
CAF_ALLOW_UNSAFE_MESSAGE_TYPE(std::shared_ptr<xstudio::plugin::ViewportOverlayRenderer>)
CAF_ALLOW_UNSAFE_MESSAGE_TYPE(std::shared_ptr<xstudio::plugin::GPUPreDrawHook>)
CAF_ALLOW_UNSAFE_MESSAGE_TYPE(xstudio::bookmark::BookmarkAndAnnotationPtr)
CAF_ALLOW_UNSAFE_MESSAGE_TYPE(xstudio::colour_pipeline::ColourOperationDataPtr)
CAF_ALLOW_UNSAFE_MESSAGE_TYPE(xstudio::colour_pipeline::ColourPipelineDataPtr)
CAF_ALLOW_UNSAFE_MESSAGE_TYPE(xstudio::media::FrameTimeMap)
CAF_ALLOW_UNSAFE_MESSAGE_TYPE(xstudio::media::AVFrameIDs)
CAF_ALLOW_UNSAFE_MESSAGE_TYPE(xstudio::media::AVFrameIDsAndTimePoints)
CAF_ALLOW_UNSAFE_MESSAGE_TYPE(xstudio::media_reader::AudioBuffer)
CAF_ALLOW_UNSAFE_MESSAGE_TYPE(xstudio::media_reader::AudioBufPtr)
CAF_ALLOW_UNSAFE_MESSAGE_TYPE(xstudio::media_reader::ImageBufPtr)
CAF_ALLOW_UNSAFE_MESSAGE_TYPE(xstudio::media_reader::PixelInfo)
CAF_ALLOW_UNSAFE_MESSAGE_TYPE(xstudio::ui::Hotkey)
CAF_ALLOW_UNSAFE_MESSAGE_TYPE(xstudio::ui::viewport::GPUShaderPtr)

// clang-format off
// offset first_custom_type_id by first custom qt event
#define FIRST_CUSTOM_ID (first_custom_type_id + 1000 + 100)

CAF_BEGIN_TYPE_ID_BLOCK(xstudio_simple_types, FIRST_CUSTOM_ID)

    CAF_ADD_TYPE_ID(xstudio_simple_types, (httplib::Headers))
    CAF_ADD_TYPE_ID(xstudio_simple_types, (httplib::Params))
    CAF_ADD_TYPE_ID(xstudio_simple_types, (httplib::Response))
    CAF_ADD_TYPE_ID(xstudio_simple_types, (Imath::M44f))
    CAF_ADD_TYPE_ID(xstudio_simple_types, (Imath::V2f))
    CAF_ADD_TYPE_ID(xstudio_simple_types, (Imath::V2i))
    CAF_ADD_TYPE_ID(xstudio_simple_types, (timebase::flicks))
    CAF_ADD_TYPE_ID(xstudio_simple_types, (xstudio::bookmark::BookmarkDetail))
    CAF_ADD_TYPE_ID(xstudio_simple_types, (xstudio::bookmark::BookmarkAndAnnotationPtr))
    CAF_ADD_TYPE_ID(xstudio_simple_types, (xstudio::colour_pipeline::ColourOperationDataPtr))
    CAF_ADD_TYPE_ID(xstudio_simple_types, (xstudio::colour_pipeline::ColourPipelineDataPtr))
    CAF_ADD_TYPE_ID(xstudio_simple_types, (xstudio::event::Event))
    CAF_ADD_TYPE_ID(xstudio_simple_types, (xstudio::global::StatusType))
    CAF_ADD_TYPE_ID(xstudio_simple_types, (xstudio::http_client::http_client_error))
    CAF_ADD_TYPE_ID(xstudio_simple_types, (xstudio::media::AVFrameID))
    CAF_ADD_TYPE_ID(xstudio_simple_types, (xstudio::media::AVFrameIDs))
    CAF_ADD_TYPE_ID(xstudio_simple_types, (xstudio::media::AVFrameIDsAndTimePoints))
    CAF_ADD_TYPE_ID(xstudio_simple_types, (xstudio::media::FrameTimeMap))
    CAF_ADD_TYPE_ID(xstudio_simple_types, (xstudio::media::media_error))
    CAF_ADD_TYPE_ID(xstudio_simple_types, (xstudio::media::MediaDetail))
    CAF_ADD_TYPE_ID(xstudio_simple_types, (xstudio::media::MediaKey))
    CAF_ADD_TYPE_ID(xstudio_simple_types, (xstudio::media::MediaStatus))
    CAF_ADD_TYPE_ID(xstudio_simple_types, (xstudio::media::MediaType))
    CAF_ADD_TYPE_ID(xstudio_simple_types, (xstudio::media::StreamDetail))
    CAF_ADD_TYPE_ID(xstudio_simple_types, (xstudio::media_metadata::MMCertainty))
    CAF_ADD_TYPE_ID(xstudio_simple_types, (xstudio::media_reader::AudioBufPtr))
    CAF_ADD_TYPE_ID(xstudio_simple_types, (xstudio::media_reader::ImageBufPtr))
    CAF_ADD_TYPE_ID(xstudio_simple_types, (xstudio::media_reader::MRCertainty))
    CAF_ADD_TYPE_ID(xstudio_simple_types, (xstudio::media_reader::PixelInfo))
    CAF_ADD_TYPE_ID(xstudio_simple_types, (xstudio::playhead::CompareMode))
    CAF_ADD_TYPE_ID(xstudio_simple_types, (xstudio::playhead::LoopMode))
    CAF_ADD_TYPE_ID(xstudio_simple_types, (xstudio::playhead::OverflowMode))
    CAF_ADD_TYPE_ID(xstudio_simple_types, (xstudio::plugin_manager::PluginDetail))
    CAF_ADD_TYPE_ID(xstudio_simple_types, (xstudio::session::ExportFormat))
    CAF_ADD_TYPE_ID(xstudio_simple_types, (xstudio::shotgun_client::AuthenticateShotgun))
    CAF_ADD_TYPE_ID(xstudio_simple_types, (xstudio::shotgun_client::AUTHENTICATION_METHOD))
    CAF_ADD_TYPE_ID(xstudio_simple_types, (xstudio::shotgun_client::shotgun_client_error))
    CAF_ADD_TYPE_ID(xstudio_simple_types, (xstudio::tag::Tag))
    CAF_ADD_TYPE_ID(xstudio_simple_types, (xstudio::thumbnail::DiskCacheStat))
    CAF_ADD_TYPE_ID(xstudio_simple_types, (xstudio::thumbnail::THUMBNAIL_FORMAT))
    CAF_ADD_TYPE_ID(xstudio_simple_types, (xstudio::thumbnail::ThumbnailBuffer))
    CAF_ADD_TYPE_ID(xstudio_simple_types, (xstudio::thumbnail::ThumbnailKey))
    CAF_ADD_TYPE_ID(xstudio_simple_types, (xstudio::timeline::Item))
    CAF_ADD_TYPE_ID(xstudio_simple_types, (xstudio::ui::Hotkey))
    CAF_ADD_TYPE_ID(xstudio_simple_types, (xstudio::ui::PointerEvent))
    CAF_ADD_TYPE_ID(xstudio_simple_types, (xstudio::ui::Signature))
    CAF_ADD_TYPE_ID(xstudio_simple_types, (xstudio::ui::viewport::FitMode))
    CAF_ADD_TYPE_ID(xstudio_simple_types, (xstudio::ui::viewport::GPUShaderPtr))
    CAF_ADD_TYPE_ID(xstudio_simple_types, (xstudio::utility::absolute_receive_timeout))
    CAF_ADD_TYPE_ID(xstudio_simple_types, (xstudio::utility::ContainerDetail))
    CAF_ADD_TYPE_ID(xstudio_simple_types, (xstudio::utility::CopyResult))
    CAF_ADD_TYPE_ID(xstudio_simple_types, (xstudio::utility::EditList))
    CAF_ADD_TYPE_ID(xstudio_simple_types, (xstudio::utility::FrameList))
    CAF_ADD_TYPE_ID(xstudio_simple_types, (xstudio::utility::FrameRange))
    CAF_ADD_TYPE_ID(xstudio_simple_types, (xstudio::utility::FrameRate))
    CAF_ADD_TYPE_ID(xstudio_simple_types, (xstudio::utility::FrameRateDuration))
    CAF_ADD_TYPE_ID(xstudio_simple_types, (xstudio::utility::JsonStore))
    CAF_ADD_TYPE_ID(xstudio_simple_types, (xstudio::utility::MediaReference))
    CAF_ADD_TYPE_ID(xstudio_simple_types, (xstudio::utility::PlaylistTree))
    CAF_ADD_TYPE_ID(xstudio_simple_types, (xstudio::utility::time_point))
    CAF_ADD_TYPE_ID(xstudio_simple_types, (xstudio::utility::Timecode))
    CAF_ADD_TYPE_ID(xstudio_simple_types, (xstudio::utility::TimeSourceMode))
    CAF_ADD_TYPE_ID(xstudio_simple_types, (xstudio::utility::Uuid))
    CAF_ADD_TYPE_ID(xstudio_simple_types, (xstudio::utility::UuidActorMap))
    CAF_ADD_TYPE_ID(xstudio_simple_types, (xstudio::xstudio_error))
    CAF_ADD_TYPE_ID(xstudio_simple_types, (xstudio::ui::viewport::ImageFormat))

CAF_END_TYPE_ID_BLOCK(xstudio_simple_types)

// xstudio_simple_types_last_type_id
CAF_BEGIN_TYPE_ID_BLOCK(xstudio_complex_types, FIRST_CUSTOM_ID + 200)

    CAF_ADD_TYPE_ID(xstudio_complex_types, (std::array<uint8_t, 16>))
    CAF_ADD_TYPE_ID(xstudio_complex_types, (std::list<xstudio::utility::Uuid>))
    CAF_ADD_TYPE_ID(xstudio_complex_types, (std::map<float, std::set<caf::actor>>))
    CAF_ADD_TYPE_ID(xstudio_complex_types, (std::map<xstudio::utility::Uuid, xstudio::utility::Uuid>))
    CAF_ADD_TYPE_ID(xstudio_complex_types, (std::optional<xstudio::utility::FrameRange>))
    CAF_ADD_TYPE_ID(xstudio_complex_types, (std::optional<xstudio::utility::time_point>))
    CAF_ADD_TYPE_ID(xstudio_complex_types, (std::pair<caf::actor, xstudio::utility::JsonStore>))
    CAF_ADD_TYPE_ID(xstudio_complex_types, (std::pair<caf::uri, std::filesystem::file_time_type>))
    CAF_ADD_TYPE_ID(xstudio_complex_types, (std::pair<int, xstudio::utility::time_point>))
    CAF_ADD_TYPE_ID(xstudio_complex_types, (std::pair<int, xstudio::utility::UuidActor>))
    CAF_ADD_TYPE_ID(xstudio_complex_types, (std::pair<std::string, std::string>))
    CAF_ADD_TYPE_ID(xstudio_complex_types, (std::pair<std::string, uintmax_t>))
    CAF_ADD_TYPE_ID(xstudio_complex_types, (std::pair<std::string, xstudio::media_metadata::MMCertainty>))
    CAF_ADD_TYPE_ID(xstudio_complex_types, (std::pair<std::string,std::vector<std::byte>>))
    CAF_ADD_TYPE_ID(xstudio_complex_types, (std::pair<xstudio::utility::JsonStore, int>))
    CAF_ADD_TYPE_ID(xstudio_complex_types, (std::pair<xstudio::utility::JsonStore, xstudio::timeline::Item>))
    CAF_ADD_TYPE_ID(xstudio_complex_types, (std::pair<xstudio::utility::JsonStore, std::vector<xstudio::timeline::Item>>))
    CAF_ADD_TYPE_ID(xstudio_complex_types, (std::pair<xstudio::utility::Timecode, int>))
    CAF_ADD_TYPE_ID(xstudio_complex_types, (std::pair<xstudio::utility::Uuid, xstudio::media_reader::MRCertainty>))
    CAF_ADD_TYPE_ID(xstudio_complex_types, (std::pair<xstudio::utility::Uuid, xstudio::utility::MediaReference>))
    CAF_ADD_TYPE_ID(xstudio_complex_types, (std::pair<xstudio::utility::Uuid, xstudio::utility::UuidActor>))
    CAF_ADD_TYPE_ID(xstudio_complex_types, (std::pair<xstudio::utility::Uuid,std::string>))
    CAF_ADD_TYPE_ID(xstudio_complex_types, (std::pair<xstudio::utility::UuidActor, xstudio::utility::UuidActor>))
    CAF_ADD_TYPE_ID(xstudio_complex_types, (std::pair<xstudio::utility::UuidActor,xstudio::utility::JsonStore>))
    CAF_ADD_TYPE_ID(xstudio_complex_types, (std::set<int>))
    CAF_ADD_TYPE_ID(xstudio_complex_types, (std::shared_ptr<const xstudio::module::Attribute>))
    CAF_ADD_TYPE_ID(xstudio_complex_types, (std::shared_ptr<const xstudio::utility::BlindDataObject>))
    CAF_ADD_TYPE_ID(xstudio_complex_types, (std::shared_ptr<xstudio::bookmark::AnnotationBase>))
    CAF_ADD_TYPE_ID(xstudio_complex_types, (std::shared_ptr<xstudio::colour_pipeline::ColourPipeline>))
    CAF_ADD_TYPE_ID(xstudio_complex_types, (std::shared_ptr<xstudio::media::AVFrameID>))
    CAF_ADD_TYPE_ID(xstudio_complex_types, (std::shared_ptr<xstudio::media_reader::AudioBuffer>))
    CAF_ADD_TYPE_ID(xstudio_complex_types, (std::shared_ptr<xstudio::plugin::ViewportOverlayRenderer>))
    CAF_ADD_TYPE_ID(xstudio_complex_types, (std::shared_ptr<xstudio::thumbnail::ThumbnailBuffer>))
    CAF_ADD_TYPE_ID(xstudio_complex_types, (std::tuple<int, double, xstudio::utility::Timecode>))
    CAF_ADD_TYPE_ID(xstudio_complex_types, (std::tuple<std::optional<std::string>, std::optional<std::string>>))
    CAF_ADD_TYPE_ID(xstudio_complex_types, (std::tuple<std::string, std::string>))
    CAF_ADD_TYPE_ID(xstudio_complex_types, (std::tuple<xstudio::utility::JsonStore, xstudio::utility::JsonStore, xstudio::utility::ContainerDetail>))
    CAF_ADD_TYPE_ID(xstudio_complex_types, (std::tuple<xstudio::utility::Uuid, std::string, std::string, double, xstudio::media::StreamDetail, std::vector<xstudio::utility::UuidActor>, xstudio::utility::Uuid>))
    CAF_ADD_TYPE_ID(xstudio_complex_types, (std::tuple<xstudio::utility::UuidActor, xstudio::utility::UuidActor, caf::actor>))
    CAF_ADD_TYPE_ID(xstudio_complex_types, (std::vector<bool>))
    CAF_ADD_TYPE_ID(xstudio_complex_types, (std::vector<caf::uri>))
    CAF_ADD_TYPE_ID(xstudio_complex_types, (std::vector<int16_t>))
    CAF_ADD_TYPE_ID(xstudio_complex_types, (std::vector<int>))
    CAF_ADD_TYPE_ID(xstudio_complex_types, (std::vector<size_t>))
    CAF_ADD_TYPE_ID(xstudio_complex_types, (std::vector<std::byte>))
    CAF_ADD_TYPE_ID(xstudio_complex_types, (std::vector<std::pair<int, int>>))
    CAF_ADD_TYPE_ID(xstudio_complex_types, (std::vector<std::pair<int, xstudio::utility::time_point>>))
    CAF_ADD_TYPE_ID(xstudio_complex_types, (std::vector<std::pair<xstudio::media::MediaKey, xstudio::utility::time_point>>))
    CAF_ADD_TYPE_ID(xstudio_complex_types, (std::vector<std::pair<xstudio::utility::EditList, caf::actor>>))
    CAF_ADD_TYPE_ID(xstudio_complex_types, (std::vector<std::pair<xstudio::utility::Uuid, std::string>>))
    CAF_ADD_TYPE_ID(xstudio_complex_types, (std::vector<std::pair<xstudio::utility::Uuid,xstudio::utility::UuidActorVector>>))
    CAF_ADD_TYPE_ID(xstudio_complex_types, (std::vector<std::pair<xstudio::utility::UuidActor, xstudio::utility::UuidActor>>))
    CAF_ADD_TYPE_ID(xstudio_complex_types, (std::vector<std::pair<xstudio::utility::UuidActor,xstudio::utility::JsonStore>>))
    CAF_ADD_TYPE_ID(xstudio_complex_types, (std::vector<std::shared_ptr<const xstudio::module::Attribute>>))
    CAF_ADD_TYPE_ID(xstudio_complex_types, (std::vector<std::string>))
    CAF_ADD_TYPE_ID(xstudio_complex_types, (std::vector<std::tuple<std::string, caf::uri, xstudio::utility::FrameList>>))
    CAF_ADD_TYPE_ID(xstudio_complex_types, (std::vector<std::tuple<xstudio::utility::Uuid, std::string, int, int>>))
    CAF_ADD_TYPE_ID(xstudio_complex_types, (std::vector<xstudio::bookmark::BookmarkDetail>))
    CAF_ADD_TYPE_ID(xstudio_complex_types, (std::vector<xstudio::colour_pipeline::ColourOperationDataPtr>))
    CAF_ADD_TYPE_ID(xstudio_complex_types, (std::vector<xstudio::colour_pipeline::ColourPipelineDataPtr>))
    CAF_ADD_TYPE_ID(xstudio_complex_types, (std::vector<xstudio::media::AVFrameID>))
    CAF_ADD_TYPE_ID(xstudio_complex_types, (std::vector<xstudio::media::MediaKey>))
    CAF_ADD_TYPE_ID(xstudio_complex_types, (std::vector<xstudio::media_reader::AudioBufPtr>))
    CAF_ADD_TYPE_ID(xstudio_complex_types, (std::vector<xstudio::media_reader::ImageBufPtr>))
    CAF_ADD_TYPE_ID(xstudio_complex_types, (std::vector<xstudio::plugin_manager::PluginDetail>))
    CAF_ADD_TYPE_ID(xstudio_complex_types, (std::vector<xstudio::tag::Tag>))
    CAF_ADD_TYPE_ID(xstudio_complex_types, (std::vector<xstudio::ui::Hotkey>))
    CAF_ADD_TYPE_ID(xstudio_complex_types, (std::vector<xstudio::ui::viewport::GPUShaderPtr>))
    CAF_ADD_TYPE_ID(xstudio_complex_types, (std::vector<xstudio::utility::ContainerDetail>))
    CAF_ADD_TYPE_ID(xstudio_complex_types, (std::vector<xstudio::utility::EditList>))
    CAF_ADD_TYPE_ID(xstudio_complex_types, (std::vector<xstudio::utility::FrameRate>))
    CAF_ADD_TYPE_ID(xstudio_complex_types, (std::vector<xstudio::utility::JsonStore>))
    CAF_ADD_TYPE_ID(xstudio_complex_types, (std::vector<xstudio::utility::MediaReference>))
    CAF_ADD_TYPE_ID(xstudio_complex_types, (std::vector<xstudio::utility::time_point>))
    CAF_ADD_TYPE_ID(xstudio_complex_types, (std::vector<xstudio::utility::Uuid>))
    CAF_ADD_TYPE_ID(xstudio_complex_types, (xstudio::utility::UuidActor))
    CAF_ADD_TYPE_ID(xstudio_complex_types, (xstudio::utility::UuidActorVector))

    CAF_ADD_TYPE_ID(xstudio_complex_types, (xstudio::conform::ConformRequest))
    CAF_ADD_TYPE_ID(xstudio_complex_types, (xstudio::conform::ConformReply))
    CAF_ADD_TYPE_ID(xstudio_complex_types, (std::set<xstudio::utility::Uuid>))
    CAF_ADD_TYPE_ID(xstudio_complex_types, (std::shared_ptr<xstudio::plugin::GPUPreDrawHook>))

CAF_END_TYPE_ID_BLOCK(xstudio_complex_types)


CAF_BEGIN_TYPE_ID_BLOCK(xstudio_framework_atoms, FIRST_CUSTOM_ID + (200 * 2))

    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::broadcast, broadcast_down_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::broadcast, join_broadcast_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::broadcast, leave_broadcast_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::global, api_exit_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::global, busy_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::global, create_studio_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::global, exit_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::global, get_actor_from_registry_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::global, get_api_mode_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::global, get_application_mode_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::global, get_global_audio_cache_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::global, get_global_image_cache_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::global, get_global_playhead_events_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::global, get_global_store_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::global, get_global_thumbnail_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::global, get_plugin_manager_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::global, get_python_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::global, get_scanner_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::global, get_studio_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::global, remote_session_name_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::global, status_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::global_store, autosave_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::global_store, do_autosave_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::global_store, save_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::history, history_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::history, log_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::history, redo_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::history, undo_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::json_store, erase_json_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::json_store, get_json_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::json_store, jsonstore_change_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::json_store, merge_json_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::json_store, patch_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::json_store, set_json_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::json_store, subscribe_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::json_store, unsubscribe_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::json_store, update_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::module, add_attribute_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::module, attribute_deleted_event_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::module, attribute_role_data_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::module, attribute_uuids_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::module, attribute_value_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::module, change_attribute_event_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::module, change_attribute_request_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::module, change_attribute_value_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::module, connect_to_ui_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::module, current_viewport_playhead_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::module, deserialise_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::module, disconnect_from_ui_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::module, full_attributes_description_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::module, get_ui_focus_events_group_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::module, grab_all_keyboard_input_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::module, grab_all_mouse_input_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::module, grab_ui_focus_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::module, join_module_attr_events_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::module, leave_module_attr_events_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::module, link_module_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::module, module_ui_events_group_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::module, redraw_viewport_group_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::module, release_ui_focus_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::module, remove_attrs_from_ui_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::module, request_full_attributes_description_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::module, request_menu_attributes_description_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::module, update_attribute_in_preferences_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::sync, authorise_connection_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::sync, get_sync_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::sync, request_connection_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::thumbnail, cache_path_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::thumbnail, cache_stats_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::utility, change_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::utility, clear_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::utility, detail_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::utility, duplicate_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::utility, event_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::utility, get_event_group_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::utility, get_group_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::utility, last_changed_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::utility, move_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::utility, name_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::utility, parent_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::utility, rate_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::utility, remove_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::utility, serialise_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::utility, type_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::utility, uuid_atom)
    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::utility, version_atom)

    CAF_ADD_ATOM(xstudio_framework_atoms, xstudio::module, remove_attribute_atom)

CAF_END_TYPE_ID_BLOCK(xstudio_framework_atoms)


CAF_BEGIN_TYPE_ID_BLOCK(xstudio_plugin_atoms,  FIRST_CUSTOM_ID + (200 * 3))

    CAF_ADD_ATOM(xstudio_plugin_atoms, xstudio::data_source, get_data_atom)
    CAF_ADD_ATOM(xstudio_plugin_atoms, xstudio::data_source, post_data_atom)
    CAF_ADD_ATOM(xstudio_plugin_atoms, xstudio::data_source, put_data_atom)
    CAF_ADD_ATOM(xstudio_plugin_atoms, xstudio::data_source, use_data_atom)
    CAF_ADD_ATOM(xstudio_plugin_atoms, xstudio::embedded_python, python_create_session_atom)
    CAF_ADD_ATOM(xstudio_plugin_atoms, xstudio::embedded_python, python_eval_atom)
    CAF_ADD_ATOM(xstudio_plugin_atoms, xstudio::embedded_python, python_eval_file_atom)
    CAF_ADD_ATOM(xstudio_plugin_atoms, xstudio::embedded_python, python_eval_locals_atom)
    CAF_ADD_ATOM(xstudio_plugin_atoms, xstudio::embedded_python, python_exec_atom)
    CAF_ADD_ATOM(xstudio_plugin_atoms, xstudio::embedded_python, python_output_atom)
    CAF_ADD_ATOM(xstudio_plugin_atoms, xstudio::embedded_python, python_remove_session_atom)
    CAF_ADD_ATOM(xstudio_plugin_atoms, xstudio::embedded_python, python_session_input_atom)
    CAF_ADD_ATOM(xstudio_plugin_atoms, xstudio::embedded_python, python_session_interrupt_atom)
    CAF_ADD_ATOM(xstudio_plugin_atoms, xstudio::http_client, http_delete_atom)
    CAF_ADD_ATOM(xstudio_plugin_atoms, xstudio::http_client, http_delete_simple_atom)
    CAF_ADD_ATOM(xstudio_plugin_atoms, xstudio::http_client, http_get_atom)
    CAF_ADD_ATOM(xstudio_plugin_atoms, xstudio::http_client, http_get_simple_atom)
    CAF_ADD_ATOM(xstudio_plugin_atoms, xstudio::http_client, http_post_atom)
    CAF_ADD_ATOM(xstudio_plugin_atoms, xstudio::http_client, http_post_simple_atom)
    CAF_ADD_ATOM(xstudio_plugin_atoms, xstudio::http_client, http_put_atom)
    CAF_ADD_ATOM(xstudio_plugin_atoms, xstudio::http_client, http_put_simple_atom)
    CAF_ADD_ATOM(xstudio_plugin_atoms, xstudio::media_hook, check_media_hook_plugin_versions_atom)
    CAF_ADD_ATOM(xstudio_plugin_atoms, xstudio::media_hook, gather_media_sources_atom)
    CAF_ADD_ATOM(xstudio_plugin_atoms, xstudio::media_hook, get_media_hook_atom)
    CAF_ADD_ATOM(xstudio_plugin_atoms, xstudio::plugin_manager, add_path_atom)
    CAF_ADD_ATOM(xstudio_plugin_atoms, xstudio::plugin_manager, enable_atom)
    CAF_ADD_ATOM(xstudio_plugin_atoms, xstudio::plugin_manager, get_resident_atom)
    CAF_ADD_ATOM(xstudio_plugin_atoms, xstudio::plugin_manager, spawn_plugin_atom)
    CAF_ADD_ATOM(xstudio_plugin_atoms, xstudio::plugin_manager, spawn_plugin_base_atom)
    CAF_ADD_ATOM(xstudio_plugin_atoms, xstudio::plugin_manager, spawn_plugin_ui_atom)
    CAF_ADD_ATOM(xstudio_plugin_atoms, xstudio::shotgun_client, shotgun_acquire_authentication_atom)
    CAF_ADD_ATOM(xstudio_plugin_atoms, xstudio::shotgun_client, shotgun_acquire_token_atom)
    CAF_ADD_ATOM(xstudio_plugin_atoms, xstudio::shotgun_client, shotgun_authenticate_atom)
    CAF_ADD_ATOM(xstudio_plugin_atoms, xstudio::shotgun_client, shotgun_authentication_source_atom)
    CAF_ADD_ATOM(xstudio_plugin_atoms, xstudio::shotgun_client, shotgun_create_entity_atom)
    CAF_ADD_ATOM(xstudio_plugin_atoms, xstudio::shotgun_client, shotgun_credential_atom)
    CAF_ADD_ATOM(xstudio_plugin_atoms, xstudio::shotgun_client, shotgun_delete_entity_atom)
    CAF_ADD_ATOM(xstudio_plugin_atoms, xstudio::shotgun_client, shotgun_entity_atom)
    CAF_ADD_ATOM(xstudio_plugin_atoms, xstudio::shotgun_client, shotgun_entity_filter_atom)
    CAF_ADD_ATOM(xstudio_plugin_atoms, xstudio::shotgun_client, shotgun_entity_search_atom)
    CAF_ADD_ATOM(xstudio_plugin_atoms, xstudio::shotgun_client, shotgun_groups_atom)
    CAF_ADD_ATOM(xstudio_plugin_atoms, xstudio::shotgun_client, shotgun_host_atom)
    CAF_ADD_ATOM(xstudio_plugin_atoms, xstudio::shotgun_client, shotgun_image_atom)
    CAF_ADD_ATOM(xstudio_plugin_atoms, xstudio::shotgun_client, shotgun_info_atom)
    CAF_ADD_ATOM(xstudio_plugin_atoms, xstudio::shotgun_client, shotgun_link_atom)
    CAF_ADD_ATOM(xstudio_plugin_atoms, xstudio::shotgun_client, shotgun_preferences_atom)
    CAF_ADD_ATOM(xstudio_plugin_atoms, xstudio::shotgun_client, shotgun_projects_atom)
    CAF_ADD_ATOM(xstudio_plugin_atoms, xstudio::shotgun_client, shotgun_refresh_token_atom)
    CAF_ADD_ATOM(xstudio_plugin_atoms, xstudio::shotgun_client, shotgun_schema_atom)
    CAF_ADD_ATOM(xstudio_plugin_atoms, xstudio::shotgun_client, shotgun_schema_entity_atom)
    CAF_ADD_ATOM(xstudio_plugin_atoms, xstudio::shotgun_client, shotgun_schema_entity_fields_atom)
    CAF_ADD_ATOM(xstudio_plugin_atoms, xstudio::shotgun_client, shotgun_text_search_atom)
    CAF_ADD_ATOM(xstudio_plugin_atoms, xstudio::shotgun_client, shotgun_update_entity_atom)
    CAF_ADD_ATOM(xstudio_plugin_atoms, xstudio::shotgun_client, shotgun_upload_atom)
    CAF_ADD_ATOM(xstudio_plugin_atoms, xstudio::shotgun_client, shotgun_attachment_atom)

    // **************** add new entries here ******************
    CAF_ADD_ATOM(xstudio_plugin_atoms, xstudio::conform, conform_atom)
    CAF_ADD_ATOM(xstudio_plugin_atoms, xstudio::conform, conform_tasks_atom)

CAF_END_TYPE_ID_BLOCK(xstudio_plugin_atoms)


CAF_BEGIN_TYPE_ID_BLOCK(xstudio_session_atoms, FIRST_CUSTOM_ID + (200 * 4))

    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::bookmark, add_annotation_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::bookmark, add_bookmark_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::bookmark, associate_bookmark_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::bookmark, bookmark_change_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::bookmark, bookmark_detail_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::bookmark, build_annotation_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::bookmark, default_category_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::bookmark, get_annotation_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::bookmark, get_bookmark_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::bookmark, get_bookmarks_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::bookmark, remove_bookmark_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::bookmark, render_annotations_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::media, acquire_media_detail_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::media, add_media_source_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::media, add_media_stream_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::media, checksum_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::media, current_media_source_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::media, current_media_stream_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::media, decompose_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::media, get_edit_list_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::media, get_media_details_atom) //DEPRECATED
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::media, get_media_pointer_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::media, get_media_pointers_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::media, get_media_source_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::media, get_media_source_names_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::media, get_media_stream_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::media, get_media_type_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::media, get_stream_detail_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::media, invalidate_cache_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::media, media_reference_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::media, media_status_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::media, relink_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::media, rescan_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::media, source_offset_frames_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::media_metadata, get_metadata_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::playlist, add_media_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::playlist, convert_to_contact_sheet_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::playlist, convert_to_subset_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::playlist, convert_to_timeline_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::playlist, copy_container_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::playlist, copy_container_to_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::playlist, copy_media_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::playlist, create_contact_sheet_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::playlist, create_container_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::playlist, create_divider_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::playlist, create_group_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::playlist, create_playhead_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::playlist, create_subset_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::playlist, create_timeline_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::playlist, duplicate_container_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::playlist, get_change_event_group_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::playlist, get_container_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::playlist, get_media_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::playlist, get_media_uuid_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::playlist, get_next_media_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::playlist, get_playhead_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::playlist, insert_container_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::playlist, loading_media_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::playlist, media_content_changed_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::playlist, move_container_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::playlist, move_container_to_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::playlist, move_media_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::playlist, reflag_container_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::playlist, remove_container_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::playlist, remove_media_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::playlist, remove_orphans_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::playlist, rename_container_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::playlist, select_all_media_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::playlist, select_media_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::playlist, selection_actor_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::playlist, set_playlist_in_viewer_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::playlist, sort_alphabetically_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::session, add_playlist_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::session, current_playlist_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::session, export_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::session, get_playlist_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::session, get_playlists_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::session, import_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::session, load_uris_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::session, media_rate_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::session, merge_playlist_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::session, merge_session_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::session, path_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::session, remove_serialise_target_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::session, session_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::session, session_request_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::tag, add_tag_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::tag, get_tag_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::tag, get_tags_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::tag, remove_tag_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::timeline, active_range_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::timeline, available_range_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::timeline, duration_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::timeline, erase_item_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::timeline, erase_item_at_frame_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::timeline, insert_item_at_frame_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::timeline, insert_item_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::timeline, item_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::timeline, item_name_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::timeline, link_media_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::timeline, move_item_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::timeline, move_item_at_frame_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::timeline, remove_item_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::timeline, remove_item_at_frame_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::timeline, split_item_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::timeline, split_item_at_frame_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::timeline, trimmed_range_atom)

    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::timeline, item_flag_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::media, metadata_selection_atom)
    CAF_ADD_ATOM(xstudio_session_atoms, xstudio::timeline, focus_atom)

CAF_END_TYPE_ID_BLOCK(xstudio_session_atoms)


CAF_BEGIN_TYPE_ID_BLOCK(xstudio_playback_atoms, FIRST_CUSTOM_ID + (200 * 5))

    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::audio, get_samples_for_soundcard_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::audio, push_samples_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::colour_pipeline, colour_operation_uniforms_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::colour_pipeline, colour_pipeline_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::colour_pipeline, connect_to_viewport_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::colour_pipeline, display_colour_transform_hash_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::colour_pipeline, get_colour_pipe_data_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::colour_pipeline, get_colour_pipe_params_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::colour_pipeline, get_thumbnail_colour_pipeline_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::colour_pipeline, pixel_info_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::colour_pipeline, set_colour_pipe_params_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::media_cache, cached_frames_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::media_cache, count_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::media_cache, erase_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::media_cache, keys_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::media_cache, preserve_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::media_cache, retrieve_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::media_cache, size_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::media_cache, store_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::media_cache, unpreserve_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::media_reader, cancel_thumbnail_request_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::media_reader, clear_precache_queue_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::media_reader, do_precache_work_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::media_reader, get_audio_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::media_reader, get_future_frames_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::media_reader, get_image_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::media_reader, get_media_detail_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::media_reader, get_reader_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::media_reader, get_thumbnail_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::media_reader, playback_precache_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::media_reader, precache_audio_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::media_reader, process_thumbnail_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::media_reader, push_image_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::media_reader, read_precache_audio_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::media_reader, read_precache_image_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::media_reader, retire_readers_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::media_reader, static_precache_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::media_reader, supported_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::playhead, actual_playback_rate_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::playhead, align_clips_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::playhead, buffer_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::playhead, check_logical_frame_changing_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::playhead, child_playheads_deleted_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::playhead, clear_precache_requests_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::playhead, colour_pipeline_lookahead_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::playhead, compare_mode_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::playhead, delete_selection_from_playlist_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::playhead, dropped_frame_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::playhead, duration_flicks_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::playhead, duration_frames_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::playhead, evict_selection_from_cache_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::playhead, first_frame_media_pointer_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::playhead, flicks_to_logical_frame_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::playhead, fps_event_group_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::playhead, full_precache_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::playhead, future_frames_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::playhead, get_selected_sources_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::playhead, get_selection_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::playhead, jump_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::playhead, key_child_playhead_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::playhead, key_playhead_index_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::playhead, last_frame_media_pointer_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::playhead, logical_frame_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::playhead, logical_frame_to_flicks_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::playhead, loop_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::playhead, media_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::playhead, media_events_group_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::playhead, media_frame_to_flicks_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::playhead, media_logical_frame_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::playhead, media_source_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::playhead, monitored_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::playhead, overflow_mode_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::playhead, play_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::playhead, play_forward_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::playhead, play_rate_mode_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::playhead, playhead_rate_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::playhead, position_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::playhead, precache_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::playhead, redraw_viewport_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::playhead, scrub_frame_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::playhead, select_next_media_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::playhead, selection_changed_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::playhead, show_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::playhead, simple_loop_end_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::playhead, simple_loop_start_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::playhead, skip_through_sources_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::playhead, sound_audio_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::playhead, source_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::playhead, step_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::playhead, use_loop_range_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::playhead, velocity_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::playhead, velocity_multiplier_atom)
    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::playhead, viewport_events_group_atom)

    CAF_ADD_ATOM(xstudio_playback_atoms, xstudio::playhead, media_frame_atom)

CAF_END_TYPE_ID_BLOCK(xstudio_playback_atoms)

CAF_BEGIN_TYPE_ID_BLOCK(xstudio_ui_atoms,  FIRST_CUSTOM_ID + (200 * 6))

    CAF_ADD_ATOM(xstudio_ui_atoms, xstudio::ui, show_buffer_atom)
    CAF_ADD_ATOM(xstudio_ui_atoms, xstudio::ui::fps_monitor, connect_to_playhead_atom)
    CAF_ADD_ATOM(xstudio_ui_atoms, xstudio::ui::fps_monitor, fps_meter_update_atom)
    CAF_ADD_ATOM(xstudio_ui_atoms, xstudio::ui::fps_monitor, framebuffer_swapped_atom)
    CAF_ADD_ATOM(xstudio_ui_atoms, xstudio::ui::fps_monitor, update_actual_fps_atom)
    CAF_ADD_ATOM(xstudio_ui_atoms, xstudio::ui::keypress_monitor, all_keys_up_atom)
    CAF_ADD_ATOM(xstudio_ui_atoms, xstudio::ui::keypress_monitor, hotkey_atom)
    CAF_ADD_ATOM(xstudio_ui_atoms, xstudio::ui::keypress_monitor, hotkey_event_atom)
    CAF_ADD_ATOM(xstudio_ui_atoms, xstudio::ui::keypress_monitor, key_down_atom)
    CAF_ADD_ATOM(xstudio_ui_atoms, xstudio::ui::keypress_monitor, key_pressed_atom)
    CAF_ADD_ATOM(xstudio_ui_atoms, xstudio::ui::keypress_monitor, key_released_atom)
    CAF_ADD_ATOM(xstudio_ui_atoms, xstudio::ui::keypress_monitor, key_up_atom)
    CAF_ADD_ATOM(xstudio_ui_atoms, xstudio::ui::keypress_monitor, mouse_event_atom)
    CAF_ADD_ATOM(xstudio_ui_atoms, xstudio::ui::keypress_monitor, pressed_keys_changed_atom)
    CAF_ADD_ATOM(xstudio_ui_atoms, xstudio::ui::keypress_monitor, register_hotkey_atom)
    CAF_ADD_ATOM(xstudio_ui_atoms, xstudio::ui::keypress_monitor, skipped_mouse_event_atom)
    CAF_ADD_ATOM(xstudio_ui_atoms, xstudio::ui::keypress_monitor, text_entry_atom)
    CAF_ADD_ATOM(xstudio_ui_atoms, xstudio::ui::model_data, insert_or_update_menu_node_atom)
    CAF_ADD_ATOM(xstudio_ui_atoms, xstudio::ui::model_data, insert_rows_atom)
    CAF_ADD_ATOM(xstudio_ui_atoms, xstudio::ui::model_data, menu_node_activated_atom)
    CAF_ADD_ATOM(xstudio_ui_atoms, xstudio::ui::model_data, model_data_atom)
    CAF_ADD_ATOM(xstudio_ui_atoms, xstudio::ui::model_data, register_model_data_atom)
    CAF_ADD_ATOM(xstudio_ui_atoms, xstudio::ui::model_data, remove_node_atom)
    CAF_ADD_ATOM(xstudio_ui_atoms, xstudio::ui::model_data, remove_rows_atom)
    CAF_ADD_ATOM(xstudio_ui_atoms, xstudio::ui::model_data, set_node_data_atom)
    CAF_ADD_ATOM(xstudio_ui_atoms, xstudio::ui::qml, backend_atom)
    CAF_ADD_ATOM(xstudio_ui_atoms, xstudio::ui::viewport, enable_hud_atom)
    CAF_ADD_ATOM(xstudio_ui_atoms, xstudio::ui::viewport, fit_mode_atom)
    CAF_ADD_ATOM(xstudio_ui_atoms, xstudio::ui::viewport, other_viewport_atom)
    CAF_ADD_ATOM(xstudio_ui_atoms, xstudio::ui::viewport, overlay_render_function_atom)
    CAF_ADD_ATOM(xstudio_ui_atoms, xstudio::ui::viewport, prepare_overlay_render_data_atom)
    CAF_ADD_ATOM(xstudio_ui_atoms, xstudio::ui::viewport, render_viewport_to_image_atom)
    CAF_ADD_ATOM(xstudio_ui_atoms, xstudio::ui::viewport, screen_info_atom)
    CAF_ADD_ATOM(xstudio_ui_atoms, xstudio::ui::viewport, viewport_get_next_frames_for_display_atom)
    CAF_ADD_ATOM(xstudio_ui_atoms, xstudio::ui::viewport, viewport_pan_atom)
    CAF_ADD_ATOM(xstudio_ui_atoms, xstudio::ui::viewport, viewport_pixel_zoom_atom)
    CAF_ADD_ATOM(xstudio_ui_atoms, xstudio::ui::viewport, viewport_playhead_atom)
    CAF_ADD_ATOM(xstudio_ui_atoms, xstudio::ui::viewport, viewport_scale_atom)
    CAF_ADD_ATOM(xstudio_ui_atoms, xstudio::ui::viewport, viewport_set_scene_coordinates_atom)
    CAF_ADD_ATOM(xstudio_ui_atoms, xstudio::ui::viewport, quickview_media_atom)
    CAF_ADD_ATOM(xstudio_ui_atoms, xstudio::ui::viewport, connect_to_viewport_toolbar_atom)

    CAF_ADD_ATOM(xstudio_ui_atoms, xstudio::ui, open_quickview_window_atom)
    CAF_ADD_ATOM(xstudio_ui_atoms, xstudio::ui, show_message_box_atom)
    CAF_ADD_ATOM(xstudio_ui_atoms, xstudio::ui::viewport, viewport_atom)
    CAF_ADD_ATOM(xstudio_ui_atoms, xstudio::ui::viewport, pre_render_gpu_hook_atom)

    CAF_ADD_ATOM(xstudio_ui_atoms, xstudio::ui, offscreen_viewport_atom)    
    CAF_ADD_ATOM(xstudio_ui_atoms, xstudio::ui, video_output_actor_atom)
    CAF_ADD_ATOM(xstudio_ui_atoms, xstudio::ui::viewport, aux_shader_uniforms_atom)

CAF_END_TYPE_ID_BLOCK(xstudio_ui_atoms)

// clang-format on

CAF_ERROR_CODE_ENUM(xstudio::xstudio_error)
CAF_ERROR_CODE_ENUM(xstudio::http_client::http_client_error)
CAF_ERROR_CODE_ENUM(xstudio::media::media_error)
CAF_ERROR_CODE_ENUM(xstudio::shotgun_client::shotgun_client_error)

namespace xstudio::utility {
inline auto make_get_version_handler() {
    return [=](version_atom) -> std::string { return PROJECT_VERSION; };
}
} // namespace xstudio::utility

namespace semver {
template <class Inspector> bool inspect(Inspector &f, version &x) {
    return f.object(x).fields(
        f.field("major", x.major),
        f.field("minor", x.minor),
        f.field("patch", x.patch),
        f.field("pretype", x.prerelease_type),
        f.field("prenumber", x.prerelease_number));
}
} // namespace semver

namespace std::chrono {
template <class Inspector> bool inspect(Inspector &f, xstudio::utility::time_point &x) {
    using Clock      = xstudio::utility::clock;
    using Time_point = Clock::time_point;
    using Duration   = Clock::duration;

    auto get_ts = [&x]() -> decltype(auto) { return x.time_since_epoch().count(); };
    auto set_ts = [&x](Duration::rep value) {
        Duration d(value);
        Time_point tp1(d);
        x = tp1;
        return true;
    };

    return f.object(x).fields(f.field("ts", get_ts, set_ts));
}

template <class Inspector> bool inspect(Inspector &f, std::filesystem::file_time_type &x) {
    using TP = std::filesystem::file_time_type;
    using D  = std::filesystem::file_time_type::duration;

    auto get_ts = [&x]() -> decltype(auto) { return x.time_since_epoch().count(); };
    auto set_ts = [&x](D::rep value) {
        D d(value);
        TP tp1(d);
        x = tp1;
        return true;
    };

    return f.object(x).fields(f.field("ts", get_ts, set_ts));
}
} // namespace std::chrono

namespace IMATH_INTERNAL_NAMESPACE {
template <class Inspector> bool inspect(Inspector &f, M44f &x) {
    return f.object(x).fields(f.field("x", x.x));
}

template <class Inspector> bool inspect(Inspector &f, V2i &x) {
    return f.object(x).fields(f.field("x", x.x), f.field("y", x.y));
}

template <class Inspector> bool inspect(Inspector &f, V2f &x) {
    return f.object(x).fields(f.field("x", x.x), f.field("y", x.y));
}
} // namespace IMATH_INTERNAL_NAMESPACE

namespace xstudio {
enum class xstudio_error_type : uint8_t {
    runtime_error = 1,
    caf_error,
    xstudio_error,
    http_client_error,
    media_error,
    shotgun_client_error,
};


inline std::string to_string(xstudio_error_type x) {
    switch (x) {
    case xstudio_error_type::runtime_error:
        return "runtime_error";
    case xstudio_error_type::caf_error:
        return "caf_error";
    case xstudio_error_type::xstudio_error:
        return "xstudio_error";
    case xstudio_error_type::http_client_error:
        return "http_client_error";
    case xstudio_error_type::media_error:
        return "media_error";
    case xstudio_error_type::shotgun_client_error:
        return "shotgun_client_error";
    default:
        return "-unknown-error-";
    }
}

class XStudioError : public std::runtime_error {
  public:
    XStudioError()
        : runtime_error("xStudio error"), error_type_(xstudio_error_type::xstudio_error) {}
    XStudioError(const std::string &msg)
        : runtime_error(msg.c_str()), error_type_(xstudio_error_type::xstudio_error) {}
    XStudioError(const std::runtime_error &err) : runtime_error(err) {}
    XStudioError(const caf::error &err) : runtime_error(to_string(err)), caf_error_(err) {
        if (err.category() == caf::type_id_v<http_client::http_client_error>) {
            error_type_ = xstudio_error_type::http_client_error;
        } else if (err.category() == caf::type_id_v<shotgun_client::shotgun_client_error>) {
            error_type_ = xstudio_error_type::shotgun_client_error;
        } else if (err.category() == caf::type_id_v<media::media_error>) {
            error_type_ = xstudio_error_type::media_error;
        } else if (err.category() == caf::type_id_v<xstudio_error>) {
            error_type_ = xstudio_error_type::xstudio_error;
        } else {
            error_type_ = xstudio_error_type::caf_error;
        }
    }

    xstudio_error_type type() const { return error_type_; }
    caf::error caf_error() const { return caf_error_; }

  private:
    xstudio_error_type error_type_{xstudio_error_type::runtime_error};
    caf::error caf_error_{};
};
} // namespace xstudio
