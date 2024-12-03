// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <nlohmann/json.hpp>

#include "xstudio/utility/json_store.hpp"

// Action templates

// GET

const auto GetVersionIvyUuid =
    R"({"operation": "VersionIvyUuid", "job":null, "ivy_uuid": null})"_json;

const auto GetShotFromId = R"({"operation": "GetShotFromId", "shot_id": null})"_json;

const auto GetLinkMedia = R"({"operation": "LinkMedia", "playlist_uuid": null})"_json;

const auto GetVersionArtist = R"({"operation": "VersionArtist", "version_id": null})"_json;

const auto GetFields =
    R"({"operation": "GetFields", "id": null, "entity": null, "fields": []})"_json;

const auto GetExecutePreset = R"({
    "operation": "ExecutePreset",
    "project_id": 0,
    "query_ms": 0,
    "preset_path": null,
    "preset_paths": [],
    "preset_id": null,
    "preset_ids": [],
    "metadata": {},
    "context": {},
    "env": {},
    "custom_terms": []
})"_json;

const auto GetValidMediaCount = R"({"operation": "MediaCount", "playlist_uuid": null})"_json;

const auto GetDownloadMedia = R"({"operation": "DownloadMedia", "media_uuid": null})"_json;

const auto GetPrepareNotes = R"({
    "operation":"PrepareNotes",
    "playlist_uuid": null,
    "media_uuids": [],
    "notify_owner": false,
    "notify_group_ids": [],
    "combine": false,
    "add_time": false,
    "add_playlist_name": false,
    "add_type": false,
    "anno_requires_note": true,
    "skip_already_published": false,
    "default_type": null
})"_json;

const auto GetQueryResult = R"({
    "operation": "Query",
    "context": null,
    "project_id": 0,
    "env": null,
    "page": 1,
    "execution_ms": 0,
    "max_result": 4999,
    "entity": null,
    "fields": [],
    "order": [],
    "query": null,
    "result": null
})"_json;

const auto GetPrecache = R"({"operation": "Precache", "project_id": -1})"_json;

const auto GetData = R"({"operation": "GetData", "type": "", "project_id": -1})"_json;

// POST

const auto PostRenameTag = R"({"operation": "RenameTag", "tag_id": null, "value": null})"_json;
const auto PostCreateTag = R"({"operation": "CreateTag", "value": null})"_json;

const auto PostTagEntity =
    R"({"operation": "TagEntity", "entity": null, "entity_id": null, "tag_id": null})"_json;

const auto PostUnTagEntity =
    R"({"operation": "UnTagEntity", "entity": null, "entity_id": null, "tag_id": null})"_json;

const auto PostCreatePlaylist =
    R"({"operation": "CreatePlaylist", "playlist_uuid": null, "project_id": null, "code": null, "location": null, "playlist_type": "Dailies"})"_json;

const auto PostCreateNotes =
    R"({"operation": "CreateNotes", "playlist_uuid": null, "payload": []})"_json;

// PUT

const auto PutUpdatePlaylistVersions =
    R"({"operation": "UpdatePlaylistVersions", "playlist_uuid": null})"_json;

// USE

const auto UseLoadPlaylist = R"({"operation": "LoadPlaylist", "playlist_id": 0})"_json;

const auto UseRefreshPlaylist =
    R"({"operation": "RefreshPlaylist", "playlist_uuid": null, "match_order": false})"_json;

// const auto RefreshPlaylistNotesJSON =
//     R"({"entity":"Playlist", "relationship": "Note", "playlist_uuid": null})"_json;


const auto PublishNoteTemplateJSON = R"(
{
    "bookmark_uuid": [],
    "shot": "",
    "payload": {
            "project":{ "type": "Project", "id":0 },
            "note_links": [
                { "type": "Playlist", "id":0 },
                { "type": "Sequence", "id":0 },
                { "type": "Shot", "id":0 },
                { "type": "Version", "id":0 }
            ],

            "addressings_to": [
                { "type": "HumanUser", "id": 0}
            ],

            "addressings_cc": [
            ],

            "sg_note_type": null,
            "sg_status_list":"opn",
            "subject": null,
            "content": null
    }
}
)"_json;

const auto locationsJSON = R"([
        {"name": "chn", "id": 4},
        {"name": "lon", "id": 1},
        {"name": "mtl", "id": 52},
        {"name": "mum", "id": 3},
        {"name": "syd", "id": 99},
        {"name": "van", "id": 2}])"_json;

const auto VersionFields = std::vector<std::string>(
    {"id",
     "code",
     "created_at",
     "created_by",
     "cut_order",
     "entity",
     "frame_count",
     "frame_range",
     "notes",
     "project",
     "sg_client_filename",
     "sg_client_send_stage",
     "sg_comp_in",
     "sg_comp_out",
     "sg_comp_range",
     "sg_cut_in",
     "sg_cut_order",
     "sg_cut_out",
     "sg_cut_range",
     "sg_date_submitted_to_client",
     "sg_dneg_version",
     "sg_frames_have_slate",
     "sg_ivy_dnuuid",
     "sg_movie_has_slate",
     "sg_on_disk_chn",
     "sg_on_disk_lon",
     "sg_on_disk_mtl",
     "sg_on_disk_mum",
     "sg_on_disk_syd",
     "sg_on_disk_van",
     "sg_path_to_frames",
     "sg_path_to_movie",
     "sg_pipeline_step",
     "sg_production_status",
     "sg_project_name",
     "sg_status_list",
     "sg_submit_dailies",
     "sg_submit_dailies_chn",
     "sg_submit_dailies_mtl",
     "sg_submit_dailies_mum",
     "sg_submit_dailies_van",
     "sg_twig_name",
     "sg_twig_type",
     "sg_twig_type_code",
     "tags",
     "user",
     "image"});

const auto ProjectFields = std::vector<std::string>(
    {"id",
     "created_at",
     "name",
     "sg_description",
     "sg_division",
     "sg_type",
     "sg_project_status",
     "sg_status"});

const auto NoteFields = std::vector<std::string>(
    {"id",
     "created_by",
     "created_at",
     "client_note",
     "sg_location",
     "sg_note_type",
     "sg_artist",
     "sg_pipeline_step",
     "subject",
     "content",
     "project",
     "note_links",
     "addressings_to",
     "addressings_cc",
     "attachments"});

const auto PlaylistFields = std::vector<std::string>(
    {"code",
     "sg_location",
     "updated_at",
     "created_at",
     "sg_date_and_time",
     "sg_type",
     "created_by",
     "sg_department_unit"});

const auto SequenceFields = std::vector<std::string>(
    {"id", "code", "shots", "type", "sg_parent", "sg_sequence_type", "sg_status_list"});
const auto SequenceShotFields = std::vector<std::string>({"id", "sg_status_list", "sg_unit"});

const auto ShotFields = std::vector<std::string>(
    {"id",
     "code",
     "sg_comp_range",
     "sg_cut_range",
     "project",
     "sg_unit",
     "sg_status_list",
     "sg_current_stage"});

const std::string shotbrowser_datasource_registry{"SHOTBROWSER"};

const auto ShotgunMetadataPath = std::string("/metadata/shotgun");

const auto TwigTypeCodes = xstudio::utility::JsonStore(R"([
    {"id": "anm", "name": "anim/dnanim"},
    {"id": "anmg", "name": "anim/group"},
    {"id": "pose", "name": "anim/pose"},
    {"id": "poseg", "name": "anim/posegroup"},
    {"id": "animcon", "name": "anim_concept"},
    {"id": "anno", "name": "annotation"},
    {"id": "aovc", "name": "aovconfig"},
    {"id": "apr", "name": "aov_presets"},
    {"id": "ably", "name": "assembly"},
    {"id": "asset", "name": "asset"},
    {"id": "assetl", "name": "assetl"},
    {"id": "acls", "name": "asset_class"},
    {"id": "alc", "name": "asset_library_config"},
    {"id": "abo", "name": "assisted_breakout"},
    {"id": "avpy", "name": "astrovalidate/check"},
    {"id": "avc", "name": "astrovalidate/checklist"},
    {"id": "ald", "name": "atmospheric_lookup_data"},
    {"id": "aud", "name": "audio"},
    {"id": "bsc", "name": "batch_script"},
    {"id": "buildcon", "name": "build_concept"},
    {"id": "imbl", "name": "bundle/image_map"},
    {"id": "texbl", "name": "bundle/texture"},
    {"id": "bch", "name": "cache/bgeo"},
    {"id": "fch", "name": "cache/fluid"},
    {"id": "gch", "name": "cache/geometry"},
    {"id": "houcache", "name": "cache/houdini"},
    {"id": "pch", "name": "cache/particle"},
    {"id": "vol", "name": "cache/volume"},
    {"id": "hcd", "name": "camera/chandata"},
    {"id": "cnv", "name": "camera/convergence"},
    {"id": "lnd", "name": "camera/lensdata"},
    {"id": "lnp", "name": "camera/lensprofile"},
    {"id": "cam", "name": "camera/mono"},
    {"id": "rtm", "name": "camera/retime"},
    {"id": "crig", "name": "camera/rig"},
    {"id": "camsheet", "name": "camera_sheet_ref"},
    {"id": "csht", "name": "charactersheet"},
    {"id": "cpk", "name": "charpik_pagedata"},
    {"id": "clrsl", "name": "clarisse/look"},
    {"id": "cdxc", "name": "codex_config"},
    {"id": "cpal", "name": "colourPalette"},
    {"id": "colsup", "name": "colour_setup"},
    {"id": "cpnt", "name": "component"},
    {"id": "artcon", "name": "concept_art"},
    {"id": "reicfg", "name": "config/rei"},
    {"id": "csc", "name": "contact_sheet_config"},
    {"id": "csp", "name": "contact_sheet_preset"},
    {"id": "cst", "name": "contact_sheet_template"},
    {"id": "convt", "name": "converter_template"},
    {"id": "crowda", "name": "crowd_actor"},
    {"id": "crowdc", "name": "crowd_cache"},
    {"id": "cdl", "name": "data/cdl"},
    {"id": "cut", "name": "data/clip/cut"},
    {"id": "edl", "name": "data/edl"},
    {"id": "lup", "name": "data/lineup"},
    {"id": "ref", "name": "data/ref"},
    {"id": "dspj", "name": "dossier_project"},
    {"id": "dvis", "name": "doublevision/scene"},
    {"id": "ecd", "name": "encoder_data"},
    {"id": "iss", "name": "framework/ivy/style"},
    {"id": "spt", "name": "framework/shotbuild/template"},
    {"id": "fbcv", "name": "furball/curve"},
    {"id": "fbgr", "name": "furball/groom"},
    {"id": "fbnt", "name": "furball/network"},
    {"id": "gsi", "name": "generics_instance"},
    {"id": "gss", "name": "generics_set"},
    {"id": "gst", "name": "generics_template"},
    {"id": "gft", "name": "giftwrap"},
    {"id": "grade", "name": "grade"},
    {"id": "llut", "name": "grade/looklut"},
    {"id": "artgfx", "name": "graphic_art"},
    {"id": "grm", "name": "groom"},
    {"id": "hbcfg", "name": "hotbuildconfig"},
    {"id": "hbcfgs", "name": "hotbuildconfig_set"},
    {"id": "hcpio", "name": "houdini_archive"},
    {"id": "ht", "name": "houdini_template"},
    {"id": "htp", "name": "houdini_template_params"},
    {"id": "idt", "name": "identity"},
    {"id": "art", "name": "image/artwork"},
    {"id": "ipg", "name": "image/imageplane"},
    {"id": "stb", "name": "image/storyboard"},
    {"id": "ibl", "name": "image_based_lighting"},
    {"id": "jgs", "name": "jigsaw"},
    {"id": "klr", "name": "katana/lightrig"},
    {"id": "klg", "name": "katana/livegroup"},
    {"id": "klf", "name": "katana/look"},
    {"id": "kr", "name": "katana/recipe"},
    {"id": "kla", "name": "katana_look_alias"},
    {"id": "kmac", "name": "katana_macro"},
    {"id": "lng", "name": "lensgrid"},
    {"id": "ladj", "name": "lighting_adjust"},
    {"id": "look", "name": "look"},
    {"id": "mtdd", "name": "material_data_driven"},
    {"id": "mtddcfg", "name": "material_data_driven_config"},
    {"id": "mtpc", "name": "material_plus_config"},
    {"id": "mtpg", "name": "material_plus_generator"},
    {"id": "mtpt", "name": "material_plus_template"},
    {"id": "mtpr", "name": "material_preset"},
    {"id": "moba", "name": "mob/actor"},
    {"id": "mobr", "name": "mob/rig"},
    {"id": "mobs", "name": "mob/sim"},
    {"id": "mcd", "name": "mocap/data"},
    {"id": "mcr", "name": "mocap/ref"},
    {"id": "mdl", "name": "model"},
    {"id": "mup", "name": "muppet"},
    {"id": "mupa", "name": "muppet/data"},
    {"id": "ndlr", "name": "noodle"},
    {"id": "nkc", "name": "nuke_config"},
    {"id": "ocean", "name": "ocean"},
    {"id": "omd", "name": "onset/metadata"},
    {"id": "otla", "name": "other/otlasset"},
    {"id": "omm", "name": "outsource/matchmove"},
    {"id": "apkg", "name": "package/asset"},
    {"id": "prm", "name": "params"},
    {"id": "psref", "name": "photoscan"},
    {"id": "pxt", "name": "pinocchio_extension"},
    {"id": "plt", "name": "plate"},
    {"id": "plook", "name": "preview_look"},
    {"id": "pbxt", "name": "procedural_build_extension"},
    {"id": "qcs", "name": "qcsheet"},
    {"id": "imageref", "name": "ref"},
    {"id": "osref", "name": "ref/onset"},
    {"id": "refbl", "name": "reference_bundle"},
    {"id": "render", "name": "render"},
    {"id": "2d", "name": "render/2D"},
    {"id": "cgr", "name": "render/cg"},
    {"id": "deepr", "name": "render/deep"},
    {"id": "elmr", "name": "render/element"},
    {"id": "foxr", "name": "render/forex"},
    {"id": "out", "name": "render/out"},
    {"id": "mov", "name": "render/playblast"},
    {"id": "movs", "name": "render/playblast/scene"},
    {"id": "wpb", "name": "render/playblast/working"},
    {"id": "scrr", "name": "render/scratch"},
    {"id": "testr", "name": "render/test"},
    {"id": "wrf", "name": "render/wireframe"},
    {"id": "wormr", "name": "render/worm"},
    {"id": "rpr", "name": "render_presets"},
    {"id": "repo2d", "name": "reposition_data_2d"},
    {"id": "zmdl", "name": "rexasset/model"},
    {"id": "rig", "name": "rig"},
    {"id": "lgtr", "name": "rig/light"},
    {"id": "rigs", "name": "rig_script"},
    {"id": "rigssn", "name": "rig_session"},
    {"id": "scan", "name": "scan"},
    {"id": "sctr", "name": "scatterer"},
    {"id": "sctrp", "name": "scatterer_preset"},
    {"id": "casc", "name": "scene/cascade"},
    {"id": "clrs", "name": "scene/clarisse"},
    {"id": "clwscn", "name": "scene/clarisse/working"},
    {"id": "hip", "name": "scene/houdini"},
    {"id": "scn", "name": "scene/maya"},
    {"id": "fxs", "name": "scene/maya/effects"},
    {"id": "gchs", "name": "scene/maya/geometry"},
    {"id": "lgt", "name": "scene/maya/lighting"},
    {"id": "ldv", "name": "scene/maya/lookdev"},
    {"id": "mod", "name": "scene/maya/model"},
    {"id": "mods", "name": "scene/maya/model/extended"},
    {"id": "mwscn", "name": "scene/maya/working"},
    {"id": "pycl", "name": "script/clarisse/python"},
    {"id": "otl", "name": "script/houdini/otl"},
    {"id": "pyh", "name": "script/houdini/python"},
    {"id": "mel", "name": "script/maya/mel"},
    {"id": "pym", "name": "script/maya/python"},
    {"id": "nkt", "name": "script/nuke/template"},
    {"id": "pcrn", "name": "script/popcorn"},
    {"id": "pys", "name": "script/python"},
    {"id": "artset", "name": "set_drawing"},
    {"id": "shot", "name": "shot"},
    {"id": "shotl", "name": "shot_layer"},
    {"id": "stig", "name": "stig"},
    {"id": "hdr", "name": "stig/hdr"},
    {"id": "sft", "name": "submission/subform/template"},
    {"id": "sbsd", "name": "substance_designer"},
    {"id": "sbsp", "name": "substance_painter"},
    {"id": "sprst", "name": "superset"},
    {"id": "surfs", "name": "surfacing_scene"},
    {"id": "nuketex", "name": "texture/nuke"},
    {"id": "texs", "name": "texture/sequence"},
    {"id": "texref", "name": "texture_ref"},
    {"id": "tvp", "name": "texture_viewport"},
    {"id": "tstl", "name": "tool_searcher_tool"},
    {"id": "veg", "name": "vegetation"},
    {"id": "vidref", "name": "video_ref"},
    {"id": "witvidref", "name": "video_ref_witness"},
    {"id": "wgt", "name": "weightmap"},
    {"id": "wsf", "name": "working_source_file"}
])"_json);
