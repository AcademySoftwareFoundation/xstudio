// SPDX-License-Identifier: Apache-2.0
#include "data_source_shotgun_ui.hpp"
#include "shotgun_model_ui.hpp"
#include "../data_source_shotgun.hpp"
#include "xstudio/utility/string_helpers.hpp"
#include "xstudio/ui/qml/json_tree_model_ui.hpp"
#include "xstudio/global_store/global_store.hpp"
#include "xstudio/atoms.hpp"
#include "xstudio/ui/qml/module_ui.hpp"
#include "xstudio/utility/chrono.hpp"

#include <QProcess>
#include <QQmlExtensionPlugin>
#include <qdebug.h>

using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::shotgun_client;
using namespace xstudio::ui::qml;
using namespace std::chrono_literals;
using namespace xstudio::global_store;

auto SHOTGUN_TIMEOUT = 120s;

const auto PresetModelLookup = std::map<std::string, std::string>(
    {{"edit", "editPresetsModel"},
     {"edit_filter", "editFilterModel"},
     {"media_action", "mediaActionPresetsModel"},
     {"media_action_filter", "mediaActionFilterModel"},
     {"note", "notePresetsModel"},
     {"note_filter", "noteFilterModel"},
     {"note_tree", "noteTreePresetsModel"},
     {"playlist", "playlistPresetsModel"},
     {"playlist_filter", "playlistFilterModel"},
     {"reference", "referencePresetsModel"},
     {"reference_filter", "referenceFilterModel"},
     {"shot", "shotPresetsModel"},
     {"shot_filter", "shotFilterModel"},
     {"shot_tree", "shotTreePresetsModel"}});

const auto PresetPreferenceLookup = std::map<std::string, std::string>(
    {{"edit", "presets/edit"},
     {"edit_filter", "global_filters/edit"},
     {"media_action", "presets/media_action"},
     {"media_action_filter", "global_filters/media_action"},
     {"note", "presets/note"},
     {"note_filter", "global_filters/note"},
     {"note_tree", "presets/note_tree"},
     {"playlist", "presets/playlist"},
     {"playlist_filter", "global_filters/playlist"},
     {"reference", "presets/reference"},
     {"reference_filter", "global_filters/reference"},
     {"shot", "presets/shot"},
     {"shot_filter", "global_filters/shot"},
     {"shot_tree", "presets/shot_tree"}});


const auto TwigTypeCodes = JsonStore(R"([
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

ShotgunDataSourceUI::ShotgunDataSourceUI(QObject *parent) : QMLActor(parent) {

    term_models_   = new QQmlPropertyMap(this);
    result_models_ = new QQmlPropertyMap(this);
    preset_models_ = new QQmlPropertyMap(this);

    term_models_->insert(
        "primaryLocationModel", QVariant::fromValue(new ShotgunListModel(this)));
    qvariant_cast<ShotgunListModel *>(term_models_->value("primaryLocationModel"))
        ->populate(R"([
        {"name": "chn", "id": 4},
        {"name": "lon", "id": 1},
        {"name": "mtl", "id": 52},
        {"name": "mum", "id": 3},
        {"name": "syd", "id": 99},
        {"name": "van", "id": 2}
    ])"_json);


    term_models_->insert("stepModel", QVariant::fromValue(new ShotgunListModel(this)));
    qvariant_cast<ShotgunListModel *>(term_models_->value("stepModel"))->populate(R"([
        {"name": "None"}
    ])"_json);

    term_models_->insert("twigTypeCodeModel", QVariant::fromValue(new ShotgunListModel(this)));
    qvariant_cast<ShotgunListModel *>(term_models_->value("twigTypeCodeModel"))
        ->populate(TwigTypeCodes);

    term_models_->insert("onDiskModel", QVariant::fromValue(new ShotgunListModel(this)));
    qvariant_cast<ShotgunListModel *>(term_models_->value("onDiskModel"))->populate(R"([
        {"name": "chn"},
        {"name": "lon"},
        {"name": "mtl"},
        {"name": "mum"},
        {"name": "syd"},
        {"name": "van"}
    ])"_json);

    term_models_->insert("lookbackModel", QVariant::fromValue(new ShotgunListModel(this)));
    qvariant_cast<ShotgunListModel *>(term_models_->value("lookbackModel"))->populate(R"([
        {"name": "Today"},
        {"name": "1 Day"},
        {"name": "3 Days"},
        {"name": "7 Days"},
        {"name": "20 Days"},
        {"name": "30 Days"},
        {"name": "30-60 Days"},
        {"name": "60-90 Days"},
        {"name": "100-150 Days"},
        {"name": "Future Only"}
    ])"_json);

    term_models_->insert("boolModel", QVariant::fromValue(new ShotgunListModel(this)));
    qvariant_cast<ShotgunListModel *>(term_models_->value("boolModel"))->populate(R"([
        {"name": "True"},
        {"name": "False"}
    ])"_json);

    term_models_->insert("resultLimitModel", QVariant::fromValue(new ShotgunListModel(this)));
    qvariant_cast<ShotgunListModel *>(term_models_->value("resultLimitModel"))->populate(R"([
        {"name": "1"},
        {"name": "2"},
        {"name": "4"},
        {"name": "8"},
        {"name": "10"},
        {"name": "20"},
        {"name": "40"},
        {"name": "100"},
        {"name": "200"},
        {"name": "500"},
        {"name": "1000"},
        {"name": "2500"},
        {"name": "4500"}
    ])"_json);

    term_models_->insert("orderByModel", QVariant::fromValue(new ShotgunListModel(this)));
    qvariant_cast<ShotgunListModel *>(term_models_->value("orderByModel"))->populate(R"([
        {"name": "Date And Time ASC"},
        {"name": "Date And Time DESC"},
        {"name": "Created ASC"},
        {"name": "Created DESC"},
        {"name": "Updated ASC"},
        {"name": "Updated DESC"},
        {"name": "Client Submit ASC"},
        {"name": "Client Submit DESC"},
        {"name": "Version ASC"},
        {"name": "Version DESC"}
    ])"_json);

    updateQueryValueCache(
        "Completion Location",
        qvariant_cast<ShotgunListModel *>(term_models_->value("primaryLocationModel"))
            ->modelData());
    updateQueryValueCache("TwigTypeCode", TwigTypeCodes);

    term_models_->insert("shotStatusModel", QVariant::fromValue(new ShotgunListModel(this)));
    term_models_->insert("projectModel", QVariant::fromValue(new ShotgunListModel(this)));
    term_models_->insert("userModel", QVariant::fromValue(new ShotgunListModel(this)));
    term_models_->insert("departmentModel", QVariant::fromValue(new ShotgunListModel(this)));
    term_models_->insert("locationModel", QVariant::fromValue(new ShotgunListModel(this)));
    term_models_->insert(
        "reviewLocationModel", QVariant::fromValue(new ShotgunListModel(this)));
    term_models_->insert("playlistTypeModel", QVariant::fromValue(new ShotgunListModel(this)));
    term_models_->insert(
        "productionStatusModel", QVariant::fromValue(new ShotgunListModel(this)));
    term_models_->insert("noteTypeModel", QVariant::fromValue(new ShotgunListModel(this)));
    term_models_->insert(
        "pipelineStatusModel", QVariant::fromValue(new ShotgunListModel(this)));

    {
        auto model  = new ShotModel(this);
        auto filter = new ShotgunFilterModel(this);

        model->setSequenceMap(&sequences_map_);
        model->setQueryValueCache(&query_value_cache_);
        filter->setSourceModel(model);

        result_models_->insert("shotResultsModel", QVariant::fromValue(filter));
        result_models_->insert("shotResultsBaseModel", QVariant::fromValue(model));
    }

    {
        auto model  = new ShotModel(this);
        auto filter = new ShotgunFilterModel(this);

        model->setSequenceMap(&sequences_map_);
        model->setQueryValueCache(&query_value_cache_);
        filter->setSourceModel(model);

        result_models_->insert("shotTreeResultsModel", QVariant::fromValue(filter));
        result_models_->insert("shotTreeResultsBaseModel", QVariant::fromValue(model));
    }

    {
        auto model  = new PlaylistModel(this);
        auto filter = new ShotgunFilterModel(this);

        model->setQueryValueCache(&query_value_cache_);
        filter->setSourceModel(model);

        result_models_->insert("playlistResultsModel", QVariant::fromValue(filter));
        result_models_->insert("playlistResultsBaseModel", QVariant::fromValue(model));
    }

    {
        auto model = new EditModel(this);
        model->populate(
            R"([{"name": "Edit 001"}, {"name": "Edit 002"}, {"name": "Edit 003"}, {"name": "Edit 004"}])"_json);

        auto filter = new ShotgunFilterModel(this);

        model->setQueryValueCache(&query_value_cache_);
        filter->setSourceModel(model);

        result_models_->insert("editResultsModel", QVariant::fromValue(filter));
        result_models_->insert("editResultsBaseModel", QVariant::fromValue(model));
    }

    {
        auto model  = new ReferenceModel(this);
        auto filter = new ShotgunFilterModel(this);

        model->setQueryValueCache(&query_value_cache_);
        model->setSequenceMap(&sequences_map_);
        filter->setSourceModel(model);

        result_models_->insert("referenceResultsModel", QVariant::fromValue(filter));
        result_models_->insert("referenceResultsBaseModel", QVariant::fromValue(model));
    }

    {
        auto model  = new NoteModel(this);
        auto filter = new ShotgunFilterModel(this);

        model->setQueryValueCache(&query_value_cache_);
        filter->setSourceModel(model);

        result_models_->insert("noteResultsModel", QVariant::fromValue(filter));
        result_models_->insert("noteResultsBaseModel", QVariant::fromValue(model));
    }

    {
        auto model  = new NoteModel(this);
        auto filter = new ShotgunFilterModel(this);

        model->setQueryValueCache(&query_value_cache_);
        filter->setSourceModel(model);

        result_models_->insert("noteTreeResultsModel", QVariant::fromValue(filter));
        result_models_->insert("noteTreeResultsBaseModel", QVariant::fromValue(model));
    }

    {
        auto model  = new MediaActionModel(this);
        auto filter = new ShotgunFilterModel(this);

        model->setQueryValueCache(&query_value_cache_);
        filter->setSourceModel(model);
        model->setSequenceMap(&sequences_map_);

        result_models_->insert("mediaActionResultsModel", QVariant::fromValue(filter));
        result_models_->insert("mediaActionResultsBaseModel", QVariant::fromValue(model));
    }

    for (const auto &[m, f] : std::vector<std::tuple<std::string, std::string>>{
             {"shotPresetsModel", "shotFilterModel"},
             {"shotTreePresetsModel", "shotFilterModel"},
             {"playlistPresetsModel", "playlistFilterModel"},
             {"editPresetsModel", "editFilterModel"},
             {"referencePresetsModel", "referenceFilterModel"},
             {"notePresetsModel", "noteFilterModel"},
             {"noteTreePresetsModel", "noteFilterModel"},
             {"mediaActionPresetsModel", "mediaActionFilterModel"}}) {

        if (not preset_models_->contains(QStringFromStd(m))) {
            auto model = new ShotgunTreeModel(this);
            model->setSequenceMap(&sequences_map_);
            preset_models_->insert(QStringFromStd(m), QVariant::fromValue(model));
        }

        if (not preset_models_->contains(QStringFromStd(f))) {
            auto filter = new ShotgunTreeModel(this);
            preset_models_->insert(QStringFromStd(f), QVariant::fromValue(filter));
        }
    }

    init(CafSystemObject::get_actor_system());

    for (const auto &[m, n] : std::vector<std::tuple<std::string, std::string>>{
             {"shotPresetsModel", "shot"},
             {"shotTreePresetsModel", "shot_tree"},
             {"playlistPresetsModel", "playlist"},
             {"editPresetsModel", "edit"},
             {"referencePresetsModel", "reference"},
             {"notePresetsModel", "note"},
             {"noteTreePresetsModel", "note_tree"},
             {"mediaActionPresetsModel", "media_action"},

             {"shotFilterModel", "shot_filter"},
             {"playlistFilterModel", "playlist_filter"},
             {"editFilterModel", "edit_filter"},
             {"referenceFilterModel", "reference_filter"},
             {"noteFilterModel", "note_filter"},
             {"mediaActionFilterModel", "media_action_filter"}}) {
        connect(
            qvariant_cast<ShotgunTreeModel *>(preset_models_->value(QStringFromStd(m))),
            &ShotgunTreeModel::modelChanged,
            this,
            [this, n = n]() { syncModelChanges(QStringFromStd(n)); });
    }
}

QObject *ShotgunDataSourceUI::groupModel(const int project_id) {
    if (not groups_map_.count(project_id)) {
        groups_map_[project_id]        = new ShotgunListModel(this);
        groups_filter_map_[project_id] = new ShotgunFilterModel(this);
        groups_filter_map_[project_id]->setSourceModel(groups_map_[project_id]);

        getGroupsFuture(project_id);
    }

    return groups_filter_map_[project_id];
}

void ShotgunDataSourceUI::createSequenceModels(const int project_id) {
    if (not sequences_map_.count(project_id)) {
        sequences_map_[project_id]      = new ShotgunListModel(this);
        sequences_tree_map_[project_id] = new ShotgunSequenceModel(this);
        getSequencesFuture(project_id);
    }
}

QObject *ShotgunDataSourceUI::sequenceModel(const int project_id) {
    createSequenceModels(project_id);
    return sequences_map_[project_id];
}

QObject *ShotgunDataSourceUI::sequenceTreeModel(const int project_id) {
    createSequenceModels(project_id);
    return sequences_tree_map_[project_id];
}

void ShotgunDataSourceUI::createShotModels(const int project_id) {
    if (not shots_map_.count(project_id)) {
        shots_map_[project_id]        = new ShotgunListModel(this);
        shots_filter_map_[project_id] = new ShotgunFilterModel(this);
        shots_filter_map_[project_id]->setSourceModel(shots_map_[project_id]);
        getShotsFuture(project_id);
    }
}

void ShotgunDataSourceUI::createCustomEntity24Models(const int project_id) {
    if (not custom_entity_24_map_.count(project_id)) {
        custom_entity_24_map_[project_id] = new ShotgunListModel(this);
        getCustomEntity24Future(project_id);
    }
}


QObject *ShotgunDataSourceUI::shotModel(const int project_id) {
    createShotModels(project_id);
    return shots_map_[project_id];
}

QObject *ShotgunDataSourceUI::customEntity24Model(const int project_id) {
    createCustomEntity24Models(project_id);
    return custom_entity_24_map_[project_id];
}

QObject *ShotgunDataSourceUI::shotSearchFilterModel(const int project_id) {
    createShotModels(project_id);
    return shots_filter_map_[project_id];
}

QObject *ShotgunDataSourceUI::playlistModel(const int project_id) {
    if (not playlists_map_.count(project_id)) {
        playlists_map_[project_id] = new ShotgunListModel(this);
        getPlaylistsFuture(project_id);
    }

    return playlists_map_[project_id];
}

//  unused ?
QString ShotgunDataSourceUI::getShotSequence(const int project_id, const QString &shot) {
    QString result;

    if (sequences_map_.count(project_id)) {
        // get data..
        const auto &data = sequences_map_[project_id]->modelData();

        auto needle = StdFromQString(shot);

        for (const auto &i : data) {
            try {
                for (const auto &s : i.at("relationships").at("shots").at("data")) {
                    if (s.at("type") == "Shot" and s.at("name") == needle) {
                        result = QStringFromStd(i.at("attributes").at("code"));
                        break;
                    }
                }
            } catch (...) {
            }
            if (not result.isEmpty())
                break;
        }
    }

    return result;
}

void ShotgunDataSourceUI::syncModelChanges(const QString &preset) {
    auto tmp = StdFromQString(preset);

    // the change has already happened
    if (not(preset_update_pending_.count(tmp) and preset_update_pending_[tmp])) {
        preset_update_pending_[tmp] = true;

        delayed_anon_send(as_actor(), std::chrono::seconds(5), shotgun_preferences_atom_v, tmp);
    }
}

void ShotgunDataSourceUI::updateQueryValueCache(
    const std::string &type, const utility::JsonStore &data, const int project_id) {
    std::map<std::string, JsonStore> cache;

    auto _type = type;
    if (project_id != -1)
        _type += "-" + std::to_string(project_id);

    // load map..
    if (not data.is_null()) {
        try {
            for (const auto &i : data) {
                if (i.count("name"))
                    cache[i.at("name").get<std::string>()] = i.at("id");
                else if (i.at("attributes").count("name"))
                    cache[i.at("attributes").at("name").get<std::string>()] = i.at("id");
                else if (i.at("attributes").count("code"))
                    cache[i.at("attributes").at("code").get<std::string>()] = i.at("id");
            }
        } catch (...) {
        }

        // add reverse map
        try {
            for (const auto &i : data) {
                if (i.count("name"))
                    cache[i.at("id").get<std::string>()] = i.at("name");
                else if (i.at("attributes").count("name"))
                    cache[i.at("id").get<std::string>()] = i.at("attributes").at("name");
                else if (i.at("attributes").count("code"))
                    cache[i.at("id").get<std::string>()] = i.at("attributes").at("code");
            }
        } catch (...) {
        }
    }

    query_value_cache_[_type] = cache;
}

utility::JsonStore ShotgunDataSourceUI::getQueryValue(
    const std::string &type, const utility::JsonStore &value, const int project_id) const {
    // look for map
    auto _type        = type;
    auto mapped_value = utility::JsonStore();

    if (_type == "Author" || _type == "Recipient")
        _type = "User";

    if (project_id != -1)
        _type += "-" + std::to_string(project_id);

    try {
        auto val = value.get<std::string>();
        if (query_value_cache_.count(_type)) {
            if (query_value_cache_.at(_type).count(val)) {
                mapped_value = query_value_cache_.at(_type).at(val);
            }
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {} {} {}", _type, __PRETTY_FUNCTION__, err.what(), value.dump(2));
    }

    if (mapped_value.is_null())
        throw XStudioError("Invalid term value " + value.dump());

    return mapped_value;
}


// merge global filters with Preset.
// Not sure if this should really happen here..
// DST = PRESET src == Global

QVariant ShotgunDataSourceUI::mergeQueries(
    const QVariant &dst, const QVariant &src, const bool ignore_duplicates) const {


    JsonStore dst_qry;
    JsonStore src_qry;

    try {
        if (std::string(dst.typeName()) == "QJSValue") {
            dst_qry = nlohmann::json::parse(
                QJsonDocument::fromVariant(dst.value<QJSValue>().toVariant())
                    .toJson(QJsonDocument::Compact)
                    .constData());
        } else {
            dst_qry = nlohmann::json::parse(
                QJsonDocument::fromVariant(dst).toJson(QJsonDocument::Compact).constData());
        }

        if (std::string(src.typeName()) == "QJSValue") {
            src_qry = nlohmann::json::parse(
                QJsonDocument::fromVariant(src.value<QJSValue>().toVariant())
                    .toJson(QJsonDocument::Compact)
                    .constData());
        } else {
            src_qry = nlohmann::json::parse(
                QJsonDocument::fromVariant(src).toJson(QJsonDocument::Compact).constData());
        }

        // we need to preprocess for Disable Global flags..
        auto disable_globals = std::set<std::string>();
        for (const auto &i : dst_qry["queries"]) {
            if (i.at("enabled").get<bool>() and i.at("term") == "Disable Global")
                disable_globals.insert(i.at("value").get<std::string>());
        }

        // if term already exists in dst, then don't append.
        if (ignore_duplicates) {
            auto dup = std::set<std::string>();
            for (const auto &i : dst_qry["queries"])
                if (i.at("enabled").get<bool>())
                    dup.insert(i.at("term").get<std::string>());

            for (const auto &i : src_qry.at("queries")) {
                auto term = i.at("term").get<std::string>();
                if (not dup.count(term) and not disable_globals.count(term))
                    dst_qry["queries"].push_back(i);
            }
        } else {
            for (const auto &i : src_qry.at("queries")) {
                auto term = i.at("term").get<std::string>();
                if (not disable_globals.count(term))
                    dst_qry["queries"].push_back(i);
            }
        }

    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    return QVariantMapFromJson(dst_qry);
}

void ShotgunDataSourceUI::setLiveLinkMetadata(QString &data) {
    if (data == "null")
        data = "{}";

    try {
        if (data != live_link_metadata_string_) {
            try {
                live_link_metadata_ = JsonStore(nlohmann::json::parse(StdFromQString(data)));
                live_link_metadata_string_ = data;
            } catch (...) {
            }

            try {
                auto project = live_link_metadata_.at("metadata")
                                   .at("shotgun")
                                   .at("version")
                                   .at("relationships")
                                   .at("project")
                                   .at("data")
                                   .at("name")
                                   .get<std::string>();
                auto project_id = live_link_metadata_.at("metadata")
                                      .at("shotgun")
                                      .at("version")
                                      .at("relationships")
                                      .at("project")
                                      .at("data")
                                      .at("id")
                                      .get<int>();

                // update model caches.
                groupModel(project_id);
                sequenceModel(project_id);
                shotModel(project_id);
                playlistModel(project_id);

                emit projectChanged(project_id, QStringFromStd(project));
            } catch (...) {
            }

            // trigger update of models with new livelink data..
            for (const auto &i :
                 {"shotPresetsModel",
                  "shotTreePresetsModel",
                  "playlistPresetsModel",
                  "editPresetsModel",
                  "referencePresetsModel",
                  "notePresetsModel",
                  "noteTreePresetsModel",
                  "mediaActionPresetsModel"})
                qvariant_cast<ShotgunTreeModel *>(preset_models_->value(i))
                    ->updateLiveLinks(live_link_metadata_);

            emit liveLinkMetadataChanged();
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }
}

QFuture<QString> ShotgunDataSourceUI::executeQuery(
    const QString &context,
    const int project_id,
    const QVariant &query,
    const bool update_result_model) {
    // build and dispatch query, we then pass result via message back to ourself.
    auto cxt = StdFromQString(context);
    JsonStore qry;


    try {
        qry = JsonStore(nlohmann::json::parse(
            QJsonDocument::fromVariant(query.value<QJSValue>().toVariant())
                .toJson(QJsonDocument::Compact)
                .constData()));

    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }


    return QtConcurrent::run([=]() {
        if (backend_ and not qry.is_null()) {
            JsonStore data;
            auto model_update = JsonStore(
                R"({
                    "type": null,
                    "epoc": null,
                    "audio_source": "",
                    "visual_source": "",
                    "flag_text": "",
                    "flag_colour": ""
                })"_json);

            model_update["epoc"] = utility::to_epoc_milliseconds(utility::clock::now());

            if (context == "Playlists")
                model_update["type"] = "playlist_result";
            else if (context == "Versions")
                model_update["type"] = "shot_result";
            else if (context == "Reference")
                model_update["type"] = "reference_result";
            else if (context == "Versions Tree")
                model_update["type"] = "shot_tree_result";
            else if (context == "Menu Setup")
                model_update["type"] = "media_action_result";
            else if (context == "Notes")
                model_update["type"] = "note_result";
            else if (context == "Notes Tree")
                model_update["type"] = "note_tree_result";

            try {
                scoped_actor sys{system()};

                const auto &[filter, orderby, max_count, source_selection, flag_selection] =
                    buildQuery(cxt, project_id, qry);

                model_update["visual_source"] = source_selection.first;
                model_update["audio_source"]  = source_selection.second;
                model_update["flag_text"]     = flag_selection.first;
                model_update["flag_colour"]   = flag_selection.second;
                model_update["truncated"]     = false;

                if (context == "Playlists") {
                    data = request_receive_wait<JsonStore>(
                        *sys,
                        backend_,
                        SHOTGUN_TIMEOUT,
                        shotgun_entity_search_atom_v,
                        "Playlists",
                        filter,
                        std::vector<std::string>(
                            {"code",
                             "versions",
                             "sg_location",
                             "updated_at",
                             "created_at",
                             "sg_date_and_time",
                             "sg_type",
                             "created_by",
                             "sg_department_unit",
                             "notes"}),
                        orderby,
                        1,
                        max_count);
                } else if (
                    context == "Versions" or context == "Versions Tree" or
                    context == "Reference" or context == "Menu Setup") {
                    data = request_receive_wait<JsonStore>(
                        *sys,
                        backend_,
                        SHOTGUN_TIMEOUT,
                        shotgun_entity_search_atom_v,
                        "Versions",
                        filter,
                        VersionFields,
                        orderby,
                        1,
                        max_count);

                } else if (context == "Notes" or context == "Notes Tree") {
                    data = request_receive_wait<JsonStore>(
                        *sys,
                        backend_,
                        SHOTGUN_TIMEOUT,
                        shotgun_entity_search_atom_v,
                        "Notes",
                        filter,
                        std::vector<std::string>(
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
                             "attachments"}),
                        orderby,
                        1,
                        max_count);
                }

                if (static_cast<int>(data.at("data").size()) == max_count)
                    model_update["truncated"] = true;
                data["preferred_visual_source"] = source_selection.first;
                data["preferred_audio_source"]  = source_selection.second;
                data["flag_text"]               = flag_selection.first;
                data["flag_colour"]             = flag_selection.second;

                if (update_result_model)
                    anon_send(as_actor(), shotgun_info_atom_v, model_update, data);

                return QStringFromStd(data.dump());

            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                // silence error..
                if (update_result_model)
                    anon_send(
                        as_actor(),
                        shotgun_info_atom_v,
                        model_update,
                        JsonStore(R"({"data":[]})"_json));

                if (starts_with(std::string(err.what()), "LiveLink ")) {
                    return QStringFromStd(R"({"data":[]})");
                }

                return QStringFromStd(err.what());
            }
        }
        return QString();
    });
}

Text ShotgunDataSourceUI::addTextValue(
    const std::string &filter, const std::string &value, const bool negated) const {
    if (starts_with(value, "^") and ends_with(value, "$")) {
        if (negated)
            return Text(filter).is_not(value.substr(0, value.size() - 1).substr(1));

        return Text(filter).is(value.substr(0, value.size() - 1).substr(1));
    } else if (ends_with(value, "$")) {
        return Text(filter).ends_with(value.substr(0, value.size() - 1));
    } else if (starts_with(value, "^")) {
        return Text(filter).starts_with(value.substr(1));
    }
    if (negated)
        return Text(filter).not_contains(value);

    return Text(filter).contains(value);
}

void ShotgunDataSourceUI::addTerm(
    const int project_id, const std::string &context, FilterBy *qry, const JsonStore &term) {
    // qry->push_back(Text("versions").is_not_null());
    auto trm     = term.at("term").get<std::string>();
    auto val     = term.at("value").get<std::string>();
    auto live    = term.value("livelink", false);
    auto negated = term.value("negated", false);


    // kill queries with invalid shot live link.
    if (val.empty() and live and trm == "Shot") {
        auto rel = R"({"type": "Shot", "id":0})"_json;
        qry->push_back(RelationType("entity").is(JsonStore(rel)));
    }

    if (val.empty()) {
        throw XStudioError("Empty query value " + trm);
    }

    if (context == "Playlists") {
        if (trm == "Lookback") {
            if (val == "Today")
                qry->push_back(DateTime("updated_at").in_calendar_day(0));
            else if (val == "1 Day")
                qry->push_back(DateTime("updated_at").in_last(1, Period::DAY));
            else if (val == "3 Days")
                qry->push_back(DateTime("updated_at").in_last(3, Period::DAY));
            else if (val == "7 Days")
                qry->push_back(DateTime("updated_at").in_last(7, Period::DAY));
            else if (val == "20 Days")
                qry->push_back(DateTime("updated_at").in_last(20, Period::DAY));
            else if (val == "30 Days")
                qry->push_back(DateTime("updated_at").in_last(30, Period::DAY));
            else if (val == "30-60 Days") {
                qry->push_back(DateTime("updated_at").not_in_last(30, Period::DAY));
                qry->push_back(DateTime("updated_at").in_last(60, Period::DAY));
            } else if (val == "60-90 Days") {
                qry->push_back(DateTime("updated_at").not_in_last(60, Period::DAY));
                qry->push_back(DateTime("updated_at").in_last(90, Period::DAY));
            } else if (val == "100-150 Days") {
                qry->push_back(DateTime("updated_at").not_in_last(100, Period::DAY));
                qry->push_back(DateTime("updated_at").in_last(150, Period::DAY));
            } else if (val == "Future Only") {
                qry->push_back(DateTime("sg_date_and_time").in_next(30, Period::DAY));
            } else {
                throw XStudioError("Invalid query term " + trm + " " + val);
            }
        } else if (trm == "Playlist Type") {
            if (negated)
                qry->push_back(Text("sg_type").is_not(val));
            else
                qry->push_back(Text("sg_type").is(val));
        } else if (trm == "Has Contents") {
            if (val == "False")
                qry->push_back(Text("versions").is_null());
            else if (val == "True")
                qry->push_back(Text("versions").is_not_null());
            else
                throw XStudioError("Invalid query term " + trm + " " + val);
        } else if (trm == "Site") {
            if (negated)
                qry->push_back(Text("sg_location").is_not(val));
            else
                qry->push_back(Text("sg_location").is(val));
        } else if (trm == "Review Location") {
            if (negated)
                qry->push_back(Text("sg_review_location_1").is_not(val));
            else
                qry->push_back(Text("sg_review_location_1").is(val));
        } else if (trm == "Department") {
            if (negated)
                qry->push_back(Number("sg_department_unit.Department.id")
                                   .is_not(getQueryValue(trm, JsonStore(val)).get<int>()));
            else
                qry->push_back(Number("sg_department_unit.Department.id")
                                   .is(getQueryValue(trm, JsonStore(val)).get<int>()));
        } else if (trm == "Author") {
            qry->push_back(Number("created_by.HumanUser.id")
                               .is(getQueryValue(trm, JsonStore(val)).get<int>()));
        } else if (trm == "Filter") {
            qry->push_back(addTextValue("code", val, negated));
        } else if (trm == "Tag") {
            qry->push_back(addTextValue("tags.Tag.name", val, negated));
        } else if (trm == "Has Notes") {
            if (val == "False")
                qry->push_back(Text("notes").is_null());
            else if (val == "True")
                qry->push_back(Text("notes").is_not_null());
            else
                throw XStudioError("Invalid query term " + trm + " " + val);
        } else if (trm == "Unit") {
            auto tmp  = R"({"type": "CustomEntity24", "id":0})"_json;
            tmp["id"] = getQueryValue(trm, JsonStore(val), project_id).get<int>();
            if (negated)
                qry->push_back(RelationType("sg_unit2").in({JsonStore(tmp)}));
            else
                qry->push_back(RelationType("sg_unit2").not_in({JsonStore(tmp)}));
        }

    } else if (context == "Notes" || context == "Notes Tree") {
        if (trm == "Lookback") {
            if (val == "Today")
                qry->push_back(DateTime("created_at").in_calendar_day(0));
            else if (val == "1 Day")
                qry->push_back(DateTime("created_at").in_last(1, Period::DAY));
            else if (val == "3 Days")
                qry->push_back(DateTime("created_at").in_last(3, Period::DAY));
            else if (val == "7 Days")
                qry->push_back(DateTime("created_at").in_last(7, Period::DAY));
            else if (val == "20 Days")
                qry->push_back(DateTime("created_at").in_last(20, Period::DAY));
            else if (val == "30 Days")
                qry->push_back(DateTime("created_at").in_last(30, Period::DAY));
            else if (val == "30-60 Days") {
                qry->push_back(DateTime("created_at").not_in_last(30, Period::DAY));
                qry->push_back(DateTime("created_at").in_last(60, Period::DAY));
            } else if (val == "60-90 Days") {
                qry->push_back(DateTime("created_at").not_in_last(60, Period::DAY));
                qry->push_back(DateTime("created_at").in_last(90, Period::DAY));
            } else if (val == "100-150 Days") {
                qry->push_back(DateTime("created_at").not_in_last(100, Period::DAY));
                qry->push_back(DateTime("created_at").in_last(150, Period::DAY));
            } else
                throw XStudioError("Invalid query term " + trm + " " + val);
        } else if (trm == "Filter") {
            qry->push_back(addTextValue("subject", val, negated));
        } else if (trm == "Note Type") {
            if (negated)
                qry->push_back(Text("sg_note_type").is_not(val));
            else
                qry->push_back(Text("sg_note_type").is(val));
        } else if (trm == "Author") {
            qry->push_back(Number("created_by.HumanUser.id")
                               .is(getQueryValue(trm, JsonStore(val)).get<int>()));
        } else if (trm == "Recipient") {
            auto tmp  = R"({"type": "HumanUser", "id":0})"_json;
            tmp["id"] = getQueryValue(trm, JsonStore(val)).get<int>();
            qry->push_back(RelationType("addressings_to").in({JsonStore(tmp)}));
        } else if (trm == "Shot") {
            auto tmp  = R"({"type": "Shot", "id":0})"_json;
            tmp["id"] = getQueryValue(trm, JsonStore(val), project_id).get<int>();
            qry->push_back(RelationType("note_links").in({JsonStore(tmp)}));
        } else if (trm == "Sequence") {
            try {
                if (sequences_map_.count(project_id)) {
                    auto row = sequences_map_[project_id]->search(
                        QVariant::fromValue(QStringFromStd(val)), QStringFromStd("display"), 0);
                    if (row != -1) {
                        auto rel = std::vector<JsonStore>();
                        // auto sht   = R"({"type": "Shot", "id":0})"_json;
                        // auto shots = sequences_map_[project_id]
                        //                  ->modelData()
                        //                  .at(row)
                        //                  .at("relationships")
                        //                  .at("shots")
                        //                  .at("data");

                        // for (const auto &i : shots) {
                        //     sht["id"] = i.at("id").get<int>();
                        //     rel.emplace_back(sht);
                        // }
                        auto seq = R"({"type": "Sequence", "id":0})"_json;
                        seq["id"] =
                            sequences_map_[project_id]->modelData().at(row).at("id").get<int>();
                        rel.emplace_back(seq);

                        qry->push_back(RelationType("note_links").in(rel));
                    } else
                        throw XStudioError("Invalid query term " + trm + " " + val);
                }
            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                throw XStudioError("Invalid query term " + trm + " " + val);
            }
        } else if (trm == "Playlist") {
            auto tmp  = R"({"type": "Playlist", "id":0})"_json;
            tmp["id"] = getQueryValue(trm, JsonStore(val), project_id).get<int>();
            qry->push_back(RelationType("note_links").in({JsonStore(tmp)}));
        } else if (trm == "Version Name") {
            qry->push_back(addTextValue("note_links.Version.code", val, negated));
        } else if (trm == "Tag") {
            qry->push_back(addTextValue("tags.Tag.name", val, negated));
        } else if (trm == "Twig Type") {
            if (negated)
                qry->push_back(
                    Text("note_links.Version.sg_twig_type_code")
                        .is_not(
                            getQueryValue("TwigTypeCode", JsonStore(val)).get<std::string>()));
            else
                qry->push_back(
                    Text("note_links.Version.sg_twig_type_code")
                        .is(getQueryValue("TwigTypeCode", JsonStore(val)).get<std::string>()));
        } else if (trm == "Twig Name") {
            qry->push_back(addTextValue("note_links.Version.sg_twig_name", val, negated));
        } else if (trm == "Client Note") {
            if (val == "False")
                qry->push_back(Checkbox("client_note").is(false));
            else if (val == "True")
                qry->push_back(Checkbox("client_note").is(true));
            else
                throw XStudioError("Invalid query term " + trm + " " + val);

        } else if (trm == "Pipeline Step") {
            if (negated) {
                if (val == "None")
                    qry->push_back(Text("sg_pipeline_step").is_not_null());
                else
                    qry->push_back(Text("sg_pipeline_step").is_not(val));
            } else {
                if (val == "None")
                    qry->push_back(Text("sg_pipeline_step").is_null());
                else
                    qry->push_back(Text("sg_pipeline_step").is(val));
            }
        }

    } else if (
        context == "Versions" or context == "Reference" or context == "Versions Tree" or
        context == "Menu Setup") {
        if (trm == "Lookback") {
            if (val == "Today")
                qry->push_back(DateTime("created_at").in_calendar_day(0));
            else if (val == "1 Day")
                qry->push_back(DateTime("created_at").in_last(1, Period::DAY));
            else if (val == "3 Days")
                qry->push_back(DateTime("created_at").in_last(3, Period::DAY));
            else if (val == "7 Days")
                qry->push_back(DateTime("created_at").in_last(7, Period::DAY));
            else if (val == "20 Days")
                qry->push_back(DateTime("created_at").in_last(20, Period::DAY));
            else if (val == "30 Days")
                qry->push_back(DateTime("created_at").in_last(30, Period::DAY));
            else if (val == "30-60 Days") {
                qry->push_back(DateTime("created_at").not_in_last(30, Period::DAY));
                qry->push_back(DateTime("created_at").in_last(60, Period::DAY));
            } else if (val == "60-90 Days") {
                qry->push_back(DateTime("created_at").not_in_last(60, Period::DAY));
                qry->push_back(DateTime("created_at").in_last(90, Period::DAY));
            } else if (val == "100-150 Days") {
                qry->push_back(DateTime("created_at").not_in_last(100, Period::DAY));
                qry->push_back(DateTime("created_at").in_last(150, Period::DAY));
            } else
                throw XStudioError("Invalid query term " + trm + " " + val);
        } else if (trm == "Playlist") {
            auto tmp  = R"({"type": "Playlist", "id":0})"_json;
            tmp["id"] = getQueryValue(trm, JsonStore(val), project_id).get<int>();
            qry->push_back(RelationType("playlists").in({JsonStore(tmp)}));
        } else if (trm == "Author") {
            qry->push_back(Number("created_by.HumanUser.id")
                               .is(getQueryValue(trm, JsonStore(val)).get<int>()));
        } else if (trm == "Older Version") {
            qry->push_back(Number("sg_dneg_version").less_than(std::stoi(val)));
        } else if (trm == "Newer Version") {
            qry->push_back(Number("sg_dneg_version").greater_than(std::stoi(val)));
        } else if (trm == "Site") {
            if (negated)
                qry->push_back(Text("sg_location").is_not(val));
            else
                qry->push_back(Text("sg_location").is(val));
        } else if (trm == "On Disk") {
            std::string prop = std::string("sg_on_disk_") + val;
            if (negated)
                qry->push_back(Text(prop).is("None"));
            else
                qry->push_back(FilterBy().Or(Text(prop).is("Full"), Text(prop).is("Partial")));
        } else if (trm == "Pipeline Step") {
            if (negated) {
                if (val == "None")
                    qry->push_back(Text("sg_pipeline_step").is_not_null());
                else
                    qry->push_back(Text("sg_pipeline_step").is_not(val));
            } else {
                if (val == "None")
                    qry->push_back(Text("sg_pipeline_step").is_null());
                else
                    qry->push_back(Text("sg_pipeline_step").is(val));
            }
        } else if (trm == "Pipeline Status") {
            if (negated)
                qry->push_back(
                    Text("sg_status_list")
                        .is_not(getQueryValue(trm, JsonStore(val)).get<std::string>()));
            else
                qry->push_back(Text("sg_status_list")
                                   .is(getQueryValue(trm, JsonStore(val)).get<std::string>()));
        } else if (trm == "Production Status") {
            if (negated)
                qry->push_back(
                    Text("sg_production_status")
                        .is_not(getQueryValue(trm, JsonStore(val)).get<std::string>()));
            else
                qry->push_back(Text("sg_production_status")
                                   .is(getQueryValue(trm, JsonStore(val)).get<std::string>()));
        } else if (trm == "Shot Status") {
            if (negated)
                qry->push_back(
                    Text("entity.Shot.sg_status_list")
                        .is_not(getQueryValue(trm, JsonStore(val)).get<std::string>()));
            else
                qry->push_back(Text("entity.Shot.sg_status_list")
                                   .is(getQueryValue(trm, JsonStore(val)).get<std::string>()));
        } else if (trm == "Exclude Shot Status") {
            qry->push_back(Text("entity.Shot.sg_status_list")
                               .is_not(getQueryValue(trm, JsonStore(val)).get<std::string>()));
        } else if (trm == "Latest Version") {
            if (val == "False")
                qry->push_back(Text("sg_latest").is_null());
            else if (val == "True")
                qry->push_back(Text("sg_latest").is("Yes"));
            else
                throw XStudioError("Invalid query term " + trm + " " + val);
        } else if (trm == "Is Hero") {
            if (val == "False")
                qry->push_back(Checkbox("sg_is_hero").is(false));
            else if (val == "True")
                qry->push_back(Checkbox("sg_is_hero").is(true));
            else
                throw XStudioError("Invalid query term " + trm + " " + val);
        } else if (trm == "Shot") {
            auto rel  = R"({"type": "Shot", "id":0})"_json;
            rel["id"] = getQueryValue(trm, JsonStore(val), project_id).get<int>();
            qry->push_back(RelationType("entity").is(JsonStore(rel)));
        } else if (trm == "Sequence") {
            try {
                if (sequences_map_.count(project_id)) {
                    auto row = sequences_map_[project_id]->search(
                        QVariant::fromValue(QStringFromStd(val)), QStringFromStd("display"), 0);
                    if (row != -1) {
                        auto rel = std::vector<JsonStore>();
                        // auto sht   = R"({"type": "Shot", "id":0})"_json;
                        // auto shots = sequences_map_[project_id]
                        //                  ->modelData()
                        //                  .at(row)
                        //                  .at("relationships")
                        //                  .at("shots")
                        //                  .at("data");

                        // for (const auto &i : shots) {
                        //     sht["id"] = i.at("id").get<int>();
                        //     rel.emplace_back(sht);
                        // }
                        auto seq = R"({"type": "Sequence", "id":0})"_json;
                        seq["id"] =
                            sequences_map_[project_id]->modelData().at(row).at("id").get<int>();
                        rel.emplace_back(seq);

                        qry->push_back(RelationType("entity").in(rel));
                    } else
                        throw XStudioError("Invalid query term " + trm + " " + val);
                }
            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                throw XStudioError("Invalid query term " + trm + " " + val);
            }
        } else if (trm == "Sent To Client") {
            if (val == "False")
                qry->push_back(DateTime("sg_date_submitted_to_client").is_null());
            else if (val == "True")
                qry->push_back(DateTime("sg_date_submitted_to_client").is_not_null());
            else
                throw XStudioError("Invalid query term " + trm + " " + val);


        } else if (trm == "Sent To Dailies") {
            if (val == "False")
                qry->push_back(FilterBy().And(
                    DateTime("sg_submit_dailies").is_null(),
                    DateTime("sg_submit_dailies_chn").is_null(),
                    DateTime("sg_submit_dailies_mtl").is_null(),
                    DateTime("sg_submit_dailies_van").is_null(),
                    DateTime("sg_submit_dailies_mum").is_null()));
            else if (val == "True")
                qry->push_back(FilterBy().Or(
                    DateTime("sg_submit_dailies").is_not_null(),
                    DateTime("sg_submit_dailies_chn").is_not_null(),
                    DateTime("sg_submit_dailies_mtl").is_not_null(),
                    DateTime("sg_submit_dailies_van").is_not_null(),
                    DateTime("sg_submit_dailies_mum").is_not_null()));
            else
                throw XStudioError("Invalid query term " + trm + " " + val);
        } else if (trm == "Has Notes") {
            if (val == "False")
                qry->push_back(Text("notes").is_null());
            else if (val == "True")
                qry->push_back(Text("notes").is_not_null());
            else
                throw XStudioError("Invalid query term " + trm + " " + val);
        } else if (trm == "Filter") {
            qry->push_back(addTextValue("code", val, negated));
        } else if (trm == "Tag") {
            qry->push_back(addTextValue("entity.Shot.tags.Tag.name", val, negated));
        } else if (trm == "Tag (Version)") {
            qry->push_back(addTextValue("tags.Tag.name", val, negated));
        } else if (trm == "Twig Name") {
            qry->push_back(addTextValue("sg_twig_name", val, negated));
        } else if (trm == "Twig Type") {
            if (negated)
                qry->push_back(
                    Text("sg_twig_type_code")
                        .is_not(
                            getQueryValue("TwigTypeCode", JsonStore(val)).get<std::string>()));
            else
                qry->push_back(
                    Text("sg_twig_type_code")
                        .is(getQueryValue("TwigTypeCode", JsonStore(val)).get<std::string>()));
        } else if (trm == "Completion Location") {
            auto rel  = R"({"type": "CustomNonProjectEntity16", "id":0})"_json;
            rel["id"] = getQueryValue(trm, JsonStore(val)).get<int>();
            if (negated)
                qry->push_back(RelationType("entity.Shot.sg_primary_shot_location")
                                   .is_not(JsonStore(rel)));
            else
                qry->push_back(
                    RelationType("entity.Shot.sg_primary_shot_location").is(JsonStore(rel)));

        } else {
            spdlog::warn("{} Unhandled {} {}", __PRETTY_FUNCTION__, trm, val);
        }
    }
}

std::tuple<
    utility::JsonStore,
    std::vector<std::string>,
    int,
    std::pair<std::string, std::string>,
    std::pair<std::string, std::string>>
ShotgunDataSourceUI::buildQuery(
    const std::string &context, const int project_id, const utility::JsonStore &query) {

    int max_count = maximum_result_count_;
    std::vector<std::string> order_by;
    std::pair<std::string, std::string> source_selection;
    std::pair<std::string, std::string> flag_selection;

    FilterBy qry;
    try {

        std::multimap<std::string, JsonStore> qry_terms;

        // collect terms in map
        for (const auto &i : query.at("queries")) {
            if (i.at("enabled").get<bool>()) {
                // filter out order by and max count..
                if (i.at("term") == "Disable Global") {
                    // filtered out
                } else if (i.at("term") == "Result Limit") {
                    max_count = std::stoi(i.at("value").get<std::string>());
                } else if (i.at("term") == "Preferred Visual") {
                    source_selection.first = i.at("value").get<std::string>();
                } else if (i.at("term") == "Preferred Audio") {
                    source_selection.second = i.at("value").get<std::string>();
                } else if (i.at("term") == "Flag Media") {
                    flag_selection.first = i.at("value").get<std::string>();
                    if (flag_selection.first == "Red")
                        flag_selection.second = "#FFFF0000";
                    else if (flag_selection.first == "Green")
                        flag_selection.second = "#FF00FF00";
                    else if (flag_selection.first == "Blue")
                        flag_selection.second = "#FF0000FF";
                    else if (flag_selection.first == "Yellow")
                        flag_selection.second = "#FFFFFF00";
                    else if (flag_selection.first == "Orange")
                        flag_selection.second = "#FFFFA500";
                    else if (flag_selection.first == "Purple")
                        flag_selection.second = "#FF800080";
                    else if (flag_selection.first == "Black")
                        flag_selection.second = "#FF000000";
                    else if (flag_selection.first == "White")
                        flag_selection.second = "#FFFFFFFF";
                } else if (i.at("term") == "Order By") {
                    auto val        = i.at("value").get<std::string>();
                    bool descending = false;

                    if (ends_with(val, " ASC")) {
                        val = val.substr(0, val.size() - 4);
                    } else if (ends_with(val, " DESC")) {
                        val        = val.substr(0, val.size() - 5);
                        descending = true;
                    }

                    std::string field = "";
                    // get sg term..
                    if (context == "Playlists") {
                        if (val == "Date And Time")
                            field = "sg_date_and_time";
                        else if (val == "Created")
                            field = "created_at";
                        else if (val == "Updated")
                            field = "updated_at";
                    } else if (
                        context == "Versions" or context == "Versions Tree" or
                        context == "Reference" or context == "Menu Setup") {
                        if (val == "Date And Time")
                            field = "created_at";
                        else if (val == "Created")
                            field = "created_at";
                        else if (val == "Updated")
                            field = "updated_at";
                        else if (val == "Client Submit")
                            field = "sg_date_submitted_to_client";
                        else if (val == "Version")
                            field = "sg_dneg_version";
                    } else if (context == "Notes" or context == "Notes Tree") {
                        if (val == "Created")
                            field = "created_at";
                        else if (val == "Updated")
                            field = "updated_at";
                    }

                    if (not field.empty())
                        order_by.push_back(descending ? "-" + field : field);
                } else {
                    // add normal term to map.
                    qry_terms.insert(std::make_pair(
                        std::string(i.value("negated", false) ? "Not " : "") +
                            i.at("term").get<std::string>(),
                        i));
                }
            }

            // set defaults if not specified
            if (source_selection.first.empty())
                source_selection.first = "SG Movie";
            if (source_selection.second.empty())
                source_selection.second = source_selection.first;
        }

        // add terms we always want.
        if (context == "Playlists") {
            qry.push_back(Number("project.Project.id").is(project_id));
        } else if (
            context == "Versions" or context == "Versions Tree" or context == "Menu Setup") {
            qry.push_back(Number("project.Project.id").is(project_id));
            qry.push_back(Text("sg_deleted").is_null());
            // qry.push_back(Entity("entity").type_is("Shot"));
            qry.push_back(FilterBy().Or(
                Text("sg_path_to_movie").is_not_null(),
                Text("sg_path_to_frames").is_not_null()));
        } else if (context == "Reference") {
            qry.push_back(Number("project.Project.id").is(project_id));
            qry.push_back(Text("sg_deleted").is_null());
            qry.push_back(FilterBy().Or(
                Text("sg_path_to_movie").is_not_null(),
                Text("sg_path_to_frames").is_not_null()));
            // qry.push_back(Entity("entity").type_is("Asset"));
        } else if (context == "Notes" or context == "Notes Tree") {
            qry.push_back(Number("project.Project.id").is(project_id));
        }

        // create OR group for multiples of same term.
        std::string key;
        FilterBy *dest = &qry;
        for (const auto &i : qry_terms) {
            if (key != i.first) {
                key = i.first;
                // multiple identical terms OR / AND them..
                if (qry_terms.count(key) > 1) {
                    if (starts_with(key, "Not ") or starts_with(key, "Exclude "))
                        qry.push_back(FilterBy(BoolOperator::AND));
                    else
                        qry.push_back(FilterBy(BoolOperator::OR));
                    dest = &std::get<FilterBy>(qry.back());
                } else {
                    dest = &qry;
                }
            }
            try {
                addTerm(project_id, context, dest, i.second);
            } catch (const std::exception &) {
                //  bad term.. we ignore them..

                // if(i.second.value("livelink", false))
                //     throw XStudioError(std::string("LiveLink ") + err.what());

                // throw;
            }
        }
    } catch (const std::exception &err) {
        throw;
    }

    if (order_by.empty()) {
        if (context == "Playlists")
            order_by.emplace_back("-created_at");
        else if (context == "Versions" or context == "Versions Tree")
            order_by.emplace_back("-created_at");
        else if (context == "Reference")
            order_by.emplace_back("-created_at");
        else if (context == "Menu Setup")
            order_by.emplace_back("-created_at");
        else if (context == "Notes" or context == "Notes Tree")
            order_by.emplace_back("-created_at");
    }

    // spdlog::warn("{}", JsonStore(qry).dump(2));
    // spdlog::warn("{}", join_as_string(order_by,","));
    return std::make_tuple(
        JsonStore(qry), order_by, max_count, source_selection, flag_selection);
}

QString ShotgunDataSourceUI::getShotgunUserName() {
    QString result; // = QString(get_user_name());

    auto ind =
        qvariant_cast<ShotgunListModel *>(term_models_->value("userModel"))
            ->search(QVariant::fromValue(QString(get_login_name().c_str())), "loginRole");
    if (ind != -1)
        result = qvariant_cast<ShotgunListModel *>(term_models_->value("userModel"))
                     ->get(ind)
                     .toString();

    return result;
}

// remove old system presets from user preferences.
utility::JsonStore ShotgunDataSourceUI::purgeOldSystem(
    const utility::JsonStore &vprefs, const utility::JsonStore &dprefs) const {

    utility::JsonStore result = dprefs;

    size_t count = 0;

    for (const auto &i : vprefs) {
        try {
            if (i.value("type", "") == "system")
                count++;
        } catch (...) {
        }
    }

    if (count != result.size()) {
        spdlog::warn("Purging old presets. {} {}", count, result.size());
        for (const auto &i : vprefs) {
            try {
                if (i.value("type", "") == "system")
                    continue;
            } catch (...) {
            }
            result.push_back(i);
        }
    } else {
        result = vprefs;
    }

    return result;
}

void ShotgunDataSourceUI::populatePresetModel(
    const utility::JsonStore &prefs,
    const std::string &path,
    ShotgunTreeModel *model,
    const bool purge_old,
    const bool clear_flags) {
    try {
        // replace embedded envvars
        std::string tmp;

        // spdlog::warn("path {}", path);

        if (purge_old)
            tmp = expand_envvars(purgeOldSystem(
                                     preference_value<JsonStore>(prefs, path),
                                     preference_default_value<JsonStore>(prefs, path))
                                     .dump());
        else
            tmp = expand_envvars(preference_value<JsonStore>(prefs, path).dump());

        model->populate(JsonStore(nlohmann::json::parse(tmp)));
        if (clear_flags) {
            model->clearLoaded();
            model->clearExpanded();
            model->clearLiveLinks();
        } else {
            // filter model set index 0 as active.
            model->setActivePreset(0);
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }
}

Q_INVOKABLE void ShotgunDataSourceUI::resetPreset(const QString &qpreset, const int index) {
    try {
        auto prefs              = GlobalStoreHelper(system());
        auto preset             = StdFromQString(qpreset);
        auto defval             = JsonStore();
        ShotgunTreeModel *model = nullptr;

        if (preset == "Versions")
            preset = "shot";
        else if (preset == "Playlists")
            preset = "playlist";
        else if (preset == "Notes")
            preset = "note";
        else if (preset == "Media Actions")
            preset = "media_action";
        else if (preset == "Edits")
            preset = "edit";
        else if (preset == "Reference")
            preset = "reference";
        else if (preset == "Versions Tree")
            preset = "shot_tree";
        else if (preset == "Notes Tree")
            preset = "note_tree";

        if (PresetModelLookup.count(preset)) {
            defval = prefs.default_value<JsonStore>(
                "/plugin/data_source/shotgun/" + PresetPreferenceLookup.at(preset));
            model = qvariant_cast<ShotgunTreeModel *>(
                preset_models_->value(QStringFromStd(PresetModelLookup.at(preset))));
        }

        if (model == nullptr)
            return;

        // expand envs
        defval = JsonStore(nlohmann::json::parse(expand_envvars(defval.dump())));

        // update and add
        // update globals ?
        // and presets..
        JsonStore data(model->modelData().at("queries"));

        // massage format..
        for (const auto &i : defval) {
            auto name = i.at("name").get<std::string>();
            // find name in current..
            auto found = false;
            for (int j = 0; j < static_cast<int>(data.size()); j++) {
                if (data[j].at("name") == name) {
                    found = true;

                    if (index != -1 && j != index)
                        break;

                    data[j] = i;
                    break;
                }
            }
            // no match then insert,,

            if (not found)
                data.push_back(i);
        }

        // reset model..
        model->populate(data);

    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }
}

void ShotgunDataSourceUI::loadPresets(const bool purge_old) {
    try {
        auto prefs = GlobalStoreHelper(system());
        JsonStore js;
        prefs.get_group(js);

        try {
            maximum_result_count_ =
                preference_value<int>(js, "/plugin/data_source/shotgun/maximum_result_count");
        } catch (const std::exception &err) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        }

        try {
            qvariant_cast<ShotgunListModel *>(term_models_->value("stepModel"))
                ->populate(
                    preference_value<JsonStore>(js, "/plugin/data_source/shotgun/pipestep"));
        } catch (const std::exception &err) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        }

        for (const auto &[m, f] : std::vector<std::tuple<std::string, std::string>>{
                 {"shot", "shotPresetsModel"},
                 {"shot_tree", "shotTreePresetsModel"},
                 {"playlist", "playlistPresetsModel"},
                 {"reference", "referencePresetsModel"},
                 {"edit", "editPresetsModel"},
                 {"note", "notePresetsModel"},
                 {"note_tree", "noteTreePresetsModel"},
                 {"media_action", "mediaActionPresetsModel"}}) {
            populatePresetModel(
                js,
                "/plugin/data_source/shotgun/presets/" + m,
                qvariant_cast<ShotgunTreeModel *>(preset_models_->value(QStringFromStd(f))),
                purge_old,
                true);
        }

        for (const auto &[m, f] : std::vector<std::tuple<std::string, std::string>>{
                 {"shot", "shotFilterModel"},
                 {"playlist", "playlistFilterModel"},
                 {"reference", "referenceFilterModel"},
                 {"edit", "editFilterModel"},
                 {"note", "noteFilterModel"},
                 {"media_action", "mediaActionFilterModel"}}) {
            populatePresetModel(
                js,
                "/plugin/data_source/shotgun/global_filters/" + m,
                qvariant_cast<ShotgunTreeModel *>(preset_models_->value(QStringFromStd(f))),
                false,
                false);
        }

        disable_flush_ = false;
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }
}

void ShotgunDataSourceUI::handleResult(
    const JsonStore &request,
    const JsonStore &data,
    const std::string &model,
    const std::string &name) {
    if (not epoc_map_.count(request.at("type")) or
        epoc_map_.at(request.at("type")) < request.at("epoc")) {
        auto slm =
            qvariant_cast<ShotgunListModel *>(result_models_->value(QStringFromStd(model)));

        slm->populate(data.at("data"));
        slm->setTruncated(request.at("truncated").get<bool>());

        source_selection_[name] = std::make_pair(
            request.at("visual_source").get<std::string>(),
            request.at("audio_source").get<std::string>());
        flag_selection_[name] = std::make_pair(
            request.at("flag_text").get<std::string>(),
            request.at("flag_colour").get<std::string>());
        epoc_map_[request.at("type")] = request.at("epoc");
    }
}

void ShotgunDataSourceUI::init(caf::actor_system &system) {
    QMLActor::init(system);

    // access global data store to retrieve stored filters.
    // delay this just in case we're to early..
    delayed_anon_send(as_actor(), std::chrono::seconds(2), shotgun_preferences_atom_v);

    set_message_handler([=](actor_companion *) -> message_handler {
        return {
            [=](shotgun_acquire_authentication_atom, const std::string &message) {
                emit requestSecret(QStringFromStd(message));
            },
            [=](shotgun_preferences_atom) { loadPresets(); },

            [=](shotgun_preferences_atom, const std::string &preset) { flushPreset(preset); },

            // catchall for dealing with results from shotgun
            [=](shotgun_info_atom, const JsonStore &request, const JsonStore &data) {
                try {
                    if (request.at("type") == "project")
                        qvariant_cast<ShotgunListModel *>(term_models_->value("projectModel"))
                            ->populate(data.at("data"));
                    else if (request.at("type") == "user") {
                        qvariant_cast<ShotgunListModel *>(term_models_->value("userModel"))
                            ->populate(data.at("data"));
                        updateQueryValueCache("User", data.at("data"));
                    } else if (request.at("type") == "department") {
                        qvariant_cast<ShotgunListModel *>(
                            term_models_->value("departmentModel"))
                            ->populate(data.at("data"));
                        updateQueryValueCache("Department", data.at("data"));
                    } else if (request.at("type") == "location")
                        qvariant_cast<ShotgunListModel *>(term_models_->value("locationModel"))
                            ->populate(data.at("data"));
                    else if (request.at("type") == "review_location")
                        qvariant_cast<ShotgunListModel *>(
                            term_models_->value("reviewLocationModel"))
                            ->populate(data.at("data"));
                    else if (request.at("type") == "shot_status") {
                        updateQueryValueCache("Exclude Shot Status", data.at("data"));
                        updateQueryValueCache("Shot Status", data.at("data"));
                        qvariant_cast<ShotgunListModel *>(
                            term_models_->value("shotStatusModel"))
                            ->populate(data.at("data"));
                    } else if (request.at("type") == "playlist_type")
                        qvariant_cast<ShotgunListModel *>(
                            term_models_->value("playlistTypeModel"))
                            ->populate(data.at("data"));
                    else if (request.at("type") == "custom_entity_24") {
                        custom_entity_24_map_[request.at("id")]->populate(data.at("data"));
                        updateQueryValueCache(
                            "Unit", data.at("data"), request.at("id").get<int>());
                    } else if (request.at("type") == "production_status") {
                        qvariant_cast<ShotgunListModel *>(
                            term_models_->value("productionStatusModel"))
                            ->populate(data.at("data"));
                        updateQueryValueCache("Production Status", data.at("data"));
                    } else if (request.at("type") == "note_type")
                        qvariant_cast<ShotgunListModel *>(term_models_->value("noteTypeModel"))
                            ->populate(data.at("data"));
                    else if (request.at("type") == "pipeline_status") {
                        qvariant_cast<ShotgunListModel *>(
                            term_models_->value("pipelineStatusModel"))
                            ->populate(data.at("data"));
                        updateQueryValueCache("Pipeline Status", data.at("data"));
                    } else if (request.at("type") == "group")
                        groups_map_[request.at("id")]->populate(data.at("data"));
                    else if (request.at("type") == "sequence") {
                        sequences_map_[request.at("id")]->populate(data.at("data"));
                        sequences_tree_map_[request.at("id")]->setModelData(
                            ShotgunSequenceModel::flatToTree(data.at("data")));
                    } else if (request.at("type") == "shot") {
                        shots_map_[request.at("id")]->populate(data.at("data"));
                        updateQueryValueCache(
                            "Shot", data.at("data"), request.at("id").get<int>());
                    } else if (request.at("type") == "playlist") {
                        playlists_map_[request.at("id")]->populate(data.at("data"));
                        updateQueryValueCache(
                            "Playlist", data.at("data"), request.at("id").get<int>());
                    } else if (request.at("type") == "playlist_result") {
                        handleResult(request, data, "playlistResultsBaseModel", "Playlists");
                    } else if (request.at("type") == "shot_result") {
                        handleResult(request, data, "shotResultsBaseModel", "Versions");
                    } else if (request.at("type") == "shot_tree_result") {
                        handleResult(
                            request, data, "shotTreeResultsBaseModel", "Versions Tree");
                    } else if (request.at("type") == "media_action_result") {
                        handleResult(
                            request, data, "mediaActionResultsBaseModel", "Menu Setup");
                    } else if (request.at("type") == "reference_result") {
                        handleResult(request, data, "referenceResultsBaseModel", "Reference");
                    } else if (request.at("type") == "note_result") {
                        handleResult(request, data, "noteResultsBaseModel", "Notes");
                    } else if (request.at("type") == "note_tree_result") {
                        handleResult(request, data, "noteTreeResultsBaseModel", "Notes Tree");
                    } else if (request.at("type") == "edit_result") {
                        handleResult(request, data, "editResultsBaseModel", "Edits");
                    }
                } catch (const std::exception &err) {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                }
            },

            [=](broadcast::broadcast_down_atom, const caf::actor_addr &) {},
            [=](const group_down_msg & /*msg*/) {
                //      if(msg.source == store_events)
                // unsubscribe();
            }};
    });
}

void ShotgunDataSourceUI::undo() {
    auto tmp = history_.undo();
    if (tmp) {
        auto current = getPresetData((*tmp).first);
        if (current != (*tmp).second) {
            // spdlog::warn("undo {}", (*tmp).first);
            setPreset((*tmp).first, (*tmp).second);
        } else
            undo();
    }
}

void ShotgunDataSourceUI::redo() {
    auto tmp = history_.redo();
    if (tmp) {
        auto current = getPresetData((*tmp).first);
        if (current != (*tmp).second) {
            // spdlog::warn("redo {}", (*tmp).first);
            setPreset((*tmp).first, (*tmp).second);
        } else
            redo();
    }
}

void ShotgunDataSourceUI::snapshot(const QString &preset) {
    auto tmp  = StdFromQString(preset);
    auto data = getPresetData(tmp);

    // spdlog::warn("snapshot {}", tmp);

    history_.push(std::make_pair(tmp, data));
}

void ShotgunDataSourceUI::setPreset(const std::string &preset, const utility::JsonStore &data) {
    if (PresetModelLookup.count(preset))
        qvariant_cast<ShotgunTreeModel *>(
            preset_models_->value(QStringFromStd(PresetModelLookup.at(preset))))
            ->populate(data);
}

utility::JsonStore ShotgunDataSourceUI::getPresetData(const std::string &preset) {
    if (PresetModelLookup.count(preset))
        return qvariant_cast<ShotgunTreeModel *>(
                   preset_models_->value(QStringFromStd(PresetModelLookup.at(preset))))
            ->modelData()
            .at("queries");

    return utility::JsonStore();
}

void ShotgunDataSourceUI::flushPreset(const std::string &preset) {
    if (disable_flush_)
        return;

    preset_update_pending_[preset] = false;
    // flush relevant changes to global dict.
    auto prefs = GlobalStoreHelper(system());

    if (PresetModelLookup.count(preset)) {
        prefs.set_value(
            qvariant_cast<ShotgunTreeModel *>(
                preset_models_->value(QStringFromStd(PresetModelLookup.at(preset))))
                ->modelData()
                .at("queries"),
            "/plugin/data_source/shotgun/" + PresetPreferenceLookup.at(preset));
    }
}

void ShotgunDataSourceUI::setConnected(const bool connected) {
    if (connected_ != connected) {
        connected_ = connected;
        if (connected_) {
            populateCaches();
        }
        emit connectedChanged();
    }
}

void ShotgunDataSourceUI::authenticate(const bool cancel) {
    anon_send(backend_, shotgun_acquire_authentication_atom_v, cancel);
}

void ShotgunDataSourceUI::set_backend(caf::actor backend) {
    backend_ = backend;
    // this forces the use of futures to access datasource
    // if this is not done the application will deadlock if the password
    // is requested, as we'll already be in this class,
    // this is because the UI runs in a single thread.
    // I'm not aware of any solutions to this.
    // we can use timeouts though..
    // but as we're accessing a remote service, we'll need to be careful..

    anon_send(backend_, module::connect_to_ui_atom_v);
    anon_send(backend_, shotgun_authentication_source_atom_v, as_actor());
}

void ShotgunDataSourceUI::setBackendId(const QString &qid) {
    caf::actor_id id = std::stoull(StdFromQString(qid).c_str());

    if (id and id != backend_id_) {
        auto actor = system().registry().template get<caf::actor>(id);
        if (actor) {
            set_backend(actor);
            backend_str_ = QStringFromStd(to_string(backend_));
            backend_id_  = backend_.id();
            emit backendChanged();
            emit backendIdChanged();
        } else {
            spdlog::warn("{} Actor lookup failed", __PRETTY_FUNCTION__);
        }
    }
}

void ShotgunDataSourceUI::populateCaches() {
    getProjectsFuture();
    getUsersFuture();
    getDepartmentsFuture();

    getSchemaFieldsFuture("playlist", "sg_location", "location");
    getSchemaFieldsFuture("playlist", "sg_review_location_1", "review_location");
    getSchemaFieldsFuture("playlist", "sg_type", "playlist_type");

    getSchemaFieldsFuture("shot", "sg_status_list", "shot_status");

    getSchemaFieldsFuture("note", "sg_note_type", "note_type");
    getSchemaFieldsFuture("version", "sg_production_status", "production_status");
    getSchemaFieldsFuture("version", "sg_status_list", "pipeline_status");
}

QFuture<QString> ShotgunDataSourceUI::getProjectsFuture() {
    return QtConcurrent::run([=]() {
        if (backend_) {
            try {
                scoped_actor sys{system()};
                auto projects = request_receive_wait<JsonStore>(
                    *sys, backend_, SHOTGUN_TIMEOUT, shotgun_projects_atom_v);
                // send to self..

                if (not projects.count("data"))
                    throw std::runtime_error(projects.dump(2));

                anon_send(
                    as_actor(),
                    shotgun_info_atom_v,
                    JsonStore(R"({"type": "project"})"_json),
                    projects);

                return QStringFromStd(projects.dump());
            } catch (const XStudioError &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                auto error                = R"({'error':{})"_json;
                error["error"]["source"]  = to_string(err.type());
                error["error"]["message"] = err.what();
                return QStringFromStd(JsonStore(error).dump());

            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                return QStringFromStd(err.what());
            }
        }
        return QString();
    });
}

QFuture<QString> ShotgunDataSourceUI::getSchemaFieldsFuture(
    const QString &entity, const QString &field, const QString &update_name) {
    return QtConcurrent::run([=]() {
        if (backend_) {
            try {
                scoped_actor sys{system()};
                auto data = request_receive_wait<JsonStore>(
                    *sys,
                    backend_,
                    SHOTGUN_TIMEOUT,
                    shotgun_schema_entity_fields_atom_v,
                    StdFromQString(entity),
                    StdFromQString(field),
                    -1);

                if (not update_name.isEmpty()) {
                    auto jsn    = JsonStore(R"({"type": null})"_json);
                    jsn["type"] = StdFromQString(update_name);
                    anon_send(as_actor(), shotgun_info_atom_v, jsn, buildDataFromField(data));
                }

                return QStringFromStd(data.dump());
            } catch (const XStudioError &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                auto error                = R"({'error':{})"_json;
                error["error"]["source"]  = to_string(err.type());
                error["error"]["message"] = err.what();
                return QStringFromStd(JsonStore(error).dump());

            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                return QStringFromStd(err.what());
            }
        }
        return QString();
    });
}

// get json of versions data from shotgun.
QFuture<QString>
ShotgunDataSourceUI::getVersionsFuture(const int project_id, const QVariant &qids) {

    std::vector<int> ids;
    for (const auto &i : qids.toList())
        ids.push_back(i.toInt());

    return QtConcurrent::run([=, project_id = project_id]() {
        if (backend_) {
            try {
                scoped_actor sys{system()};
                auto filter = R"(
                {
                    "logical_operator": "and",
                    "conditions": [
                        ["project", "is", {"type":"Project", "id":0}],
                        ["id", "in", []]
                    ]
                })"_json;

                filter["conditions"][0][2]["id"] = project_id;
                for (const auto i : ids)
                    filter["conditions"][1][2].push_back(i);

                // we've got more that 5000 employees....
                auto data = request_receive_wait<JsonStore>(
                    *sys,
                    backend_,
                    SHOTGUN_TIMEOUT,
                    shotgun_entity_search_atom_v,
                    "Versions",
                    JsonStore(filter),
                    VersionFields,
                    std::vector<std::string>({"id"}),
                    1,
                    4999);

                if (not data.count("data"))
                    throw std::runtime_error(data.dump(2));

                // reorder results based on request order..
                std::map<int, JsonStore> result_items;
                for (const auto &i : data["data"])
                    result_items[i.at("id").get<int>()] = i;

                data["data"].clear();
                for (const auto i : ids) {
                    if (result_items.count(i))
                        data["data"].push_back(result_items[i]);
                }

                return QStringFromStd(data.dump());

            } catch (const XStudioError &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                auto error                = R"({'error':{})"_json;
                error["error"]["source"]  = to_string(err.type());
                error["error"]["message"] = err.what();
                return QStringFromStd(JsonStore(error).dump());

            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                return QStringFromStd(err.what());
            }
        }
        return QString();
    });
}


JsonStore ShotgunDataSourceUI::buildDataFromField(const JsonStore &data) {
    auto result = R"({"data": []})"_json;

    std::map<std::string, std::string> entries;

    for (const auto &i : data["data"]["properties"]["valid_values"]["value"]) {
        auto value = i.get<std::string>();
        auto key   = value;
        if (data["data"]["properties"].count("display_values") and
            data["data"]["properties"]["display_values"]["value"].count(value)) {
            key =
                data["data"]["properties"]["display_values"]["value"][value].get<std::string>();
        }

        entries.insert(std::make_pair(key, value));
    }

    for (const auto &i : entries) {
        auto field                  = R"({"id": null, "attributes": {"name": null}})"_json;
        field["attributes"]["name"] = i.first;
        field["id"]                 = i.second;
        result["data"].push_back(field);
    }

    return JsonStore(result);
}


QFuture<QString> ShotgunDataSourceUI::getPlaylistLinkMediaFuture(const QUuid &playlist) {
    return QtConcurrent::run([=]() {
        if (backend_) {
            try {
                scoped_actor sys{system()};
                auto req             = JsonStore(GetPlaylistLinkMediaJSON);
                req["playlist_uuid"] = to_string(UuidFromQUuid(playlist));

                return QStringFromStd(
                    request_receive_wait<JsonStore>(
                        *sys, backend_, SHOTGUN_TIMEOUT, data_source::get_data_atom_v, req)
                        .dump());
            } catch (const XStudioError &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                auto error                = R"({'error':{})"_json;
                error["error"]["source"]  = to_string(err.type());
                error["error"]["message"] = err.what();
                return QStringFromStd(JsonStore(error).dump());
            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                return QStringFromStd(err.what());
            }
        }
        return QString();
    });
}

QFuture<QString> ShotgunDataSourceUI::getPlaylistValidMediaCountFuture(const QUuid &playlist) {
    return QtConcurrent::run([=]() {
        if (backend_) {
            try {
                scoped_actor sys{system()};
                auto req             = JsonStore(GetPlaylistValidMediaJSON);
                req["playlist_uuid"] = to_string(UuidFromQUuid(playlist));

                return QStringFromStd(
                    request_receive_wait<JsonStore>(
                        *sys, backend_, SHOTGUN_TIMEOUT, data_source::get_data_atom_v, req)
                        .dump());
            } catch (const XStudioError &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                auto error                = R"({'error':{})"_json;
                error["error"]["source"]  = to_string(err.type());
                error["error"]["message"] = err.what();
                return QStringFromStd(JsonStore(error).dump());
            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                return QStringFromStd(err.what());
            }
        }
        return QString();
    });
}

QFuture<QString> ShotgunDataSourceUI::getGroupsFuture(const int project_id) {
    return QtConcurrent::run([=]() {
        if (backend_) {
            try {
                scoped_actor sys{system()};
                auto groups = request_receive_wait<JsonStore>(
                    *sys, backend_, SHOTGUN_TIMEOUT, shotgun_groups_atom_v, project_id);

                if (not groups.count("data"))
                    throw std::runtime_error(groups.dump(2));

                auto request  = R"({"type": "group", "id": 0})"_json;
                request["id"] = project_id;
                anon_send(as_actor(), shotgun_info_atom_v, JsonStore(request), groups);

                return QStringFromStd(groups.dump());
            } catch (const XStudioError &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                auto error                = R"({'error':{})"_json;
                error["error"]["source"]  = to_string(err.type());
                error["error"]["message"] = err.what();
                return QStringFromStd(JsonStore(error).dump());

            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                return QStringFromStd(err.what());
            }
        }
        return QString();
    });
}

QFuture<QString> ShotgunDataSourceUI::getSequencesFuture(const int project_id) {
    return QtConcurrent::run([=]() {
        if (backend_) {
            try {
                scoped_actor sys{system()};

                auto filter = R"(
                {
                    "logical_operator": "and",
                    "conditions": [
                        ["project", "is", {"type":"Project", "id":0}],
                        ["sg_status_list", "not_in", ["na","del"]]
                    ]
                })"_json;

                filter["conditions"][0][2]["id"] = project_id;

                // we've got more that 5000 employees....
                auto data = request_receive_wait<JsonStore>(
                    *sys,
                    backend_,
                    SHOTGUN_TIMEOUT,
                    shotgun_entity_search_atom_v,
                    "Sequences",
                    JsonStore(filter),
                    std::vector<std::string>(
                        {"id", "code", "shots", "type", "sg_parent", "sg_sequence_type"}),
                    std::vector<std::string>({"code"}),
                    1,
                    4999);

                if (not data.count("data"))
                    throw std::runtime_error(data.dump(2));

                if (data.at("data").size() == 4999)
                    spdlog::warn("{} Sequence list truncated.", __PRETTY_FUNCTION__);

                auto request  = R"({"type": "sequence", "id": 0})"_json;
                request["id"] = project_id;
                anon_send(as_actor(), shotgun_info_atom_v, JsonStore(request), data);

                return QStringFromStd(data.dump());
            } catch (const XStudioError &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                auto error                = R"({'error':{})"_json;
                error["error"]["source"]  = to_string(err.type());
                error["error"]["message"] = err.what();
                return QStringFromStd(JsonStore(error).dump());

            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                return QStringFromStd(err.what());
            }
        }
        return QString();
    });
}

QFuture<QString> ShotgunDataSourceUI::getPlaylistsFuture(const int project_id) {

    return QtConcurrent::run([=]() {
        if (backend_) {
            try {
                scoped_actor sys{system()};

                auto filter = R"(
                {
                    "logical_operator": "and",
                    "conditions": [
                        ["project", "is", {"type":"Project", "id":0}],
                        ["versions", "is_not", null]
                    ]
                })"_json;
                // ["updated_at", "in_last", [7, "DAY"]]

                filter["conditions"][0][2]["id"] = project_id;

                // we've got more that 5000 employees....
                auto data = request_receive_wait<JsonStore>(
                    *sys,
                    backend_,
                    SHOTGUN_TIMEOUT,
                    shotgun_entity_search_atom_v,
                    "Playlists",
                    JsonStore(filter),
                    std::vector<std::string>({"id", "code"}),
                    std::vector<std::string>({"-created_at"}),
                    1,
                    4999);

                if (not data.count("data"))
                    throw std::runtime_error(data.dump(2));

                auto request  = R"({"type": "playlist", "id": 0})"_json;
                request["id"] = project_id;
                anon_send(as_actor(), shotgun_info_atom_v, JsonStore(request), data);

                return QStringFromStd(data.dump());
            } catch (const XStudioError &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                auto error                = R"({'error':{})"_json;
                error["error"]["source"]  = to_string(err.type());
                error["error"]["message"] = err.what();
                return QStringFromStd(JsonStore(error).dump());

            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                return QStringFromStd(err.what());
            }
        }
        return QString();
    });
}


QFuture<QString> ShotgunDataSourceUI::getShotsFuture(const int project_id) {
    return QtConcurrent::run([=]() {
        if (backend_) {
            try {
                scoped_actor sys{system()};

                // removed to include all shots.
                // ["sg_status_list", "not_in", ["na","del"]]

                auto filter = R"(
                {
                    "logical_operator": "and",
                    "conditions": [
                        ["project", "is", {"type":"Project", "id":0}]
                    ]
                })"_json;

                filter["conditions"][0][2]["id"] = project_id;

                // we've got more that 5000 employees....
                auto data = request_receive_wait<JsonStore>(
                    *sys,
                    backend_,
                    SHOTGUN_TIMEOUT,
                    shotgun_entity_search_atom_v,
                    "Shots",
                    JsonStore(filter),
                    ShotFields,
                    std::vector<std::string>({"code"}),
                    1,
                    4999);

                if (not data.count("data"))
                    throw std::runtime_error(data.dump(2));

                auto request  = R"({"type": "shot", "id": 0})"_json;
                request["id"] = project_id;
                anon_send(as_actor(), shotgun_info_atom_v, JsonStore(request), data);

                return QStringFromStd(data.dump());
            } catch (const XStudioError &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                auto error                = R"({'error':{})"_json;
                error["error"]["source"]  = to_string(err.type());
                error["error"]["message"] = err.what();
                return QStringFromStd(JsonStore(error).dump());

            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                return QStringFromStd(err.what());
            }
        }
        return QString();
    });
}

QFuture<QString> ShotgunDataSourceUI::getUsersFuture() {
    return QtConcurrent::run([=]() {
        if (backend_) {
            try {
                scoped_actor sys{system()};


                auto filter = R"(
                {
                    "logical_operator": "and",
                    "conditions": [
                        ["sg_status_list", "is", "act"]
                    ]
                })"_json;

                // we've got more that 5000 employees....
                auto data = request_receive_wait<JsonStore>(
                    *sys,
                    backend_,
                    SHOTGUN_TIMEOUT,
                    shotgun_entity_search_atom_v,
                    "HumanUsers",
                    JsonStore(filter),
                    std::vector<std::string>({"name", "id", "login"}),
                    std::vector<std::string>({"name"}),
                    1,
                    4999);

                if (not data.count("data"))
                    throw std::runtime_error(data.dump(2));

                if (data["data"].size() == 4999) {
                    auto data2 = request_receive_wait<JsonStore>(
                        *sys,
                        backend_,
                        SHOTGUN_TIMEOUT,
                        shotgun_entity_search_atom_v,
                        "HumanUsers",
                        JsonStore(filter),
                        std::vector<std::string>({"name", "id", "login"}),
                        std::vector<std::string>({"name"}),
                        2,
                        4999);

                    if (data2["data"].size() == 4999)
                        spdlog::warn("Exceeding user limit..");

                    data["data"].insert(
                        data["data"].end(), data2["data"].begin(), data2["data"].end());
                }

                anon_send(
                    as_actor(),
                    shotgun_info_atom_v,
                    JsonStore(R"({"type": "user"})"_json),
                    data);

                return QStringFromStd(data.dump());
            } catch (const XStudioError &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                auto error                = R"({'error':{})"_json;
                error["error"]["source"]  = to_string(err.type());
                error["error"]["message"] = err.what();
                return QStringFromStd(JsonStore(error).dump());

            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                return QStringFromStd(err.what());
            }
        }
        return QString();
    });
}

QFuture<QString> ShotgunDataSourceUI::getDepartmentsFuture() {
    return QtConcurrent::run([=]() {
        if (backend_) {
            try {
                scoped_actor sys{system()};


                auto filter = R"(
                {
                    "logical_operator": "and",
                    "conditions": [
                    ]
                })"_json;
                // ["sg_status_list", "is", "act"]

                // we've got more that 5000 employees....
                auto data = request_receive_wait<JsonStore>(
                    *sys,
                    backend_,
                    SHOTGUN_TIMEOUT,
                    shotgun_entity_search_atom_v,
                    "Departments",
                    JsonStore(filter),
                    std::vector<std::string>({"name", "id"}),
                    std::vector<std::string>({"name"}),
                    1,
                    4999);

                if (not data.count("data"))
                    throw std::runtime_error(data.dump(2));

                anon_send(
                    as_actor(),
                    shotgun_info_atom_v,
                    JsonStore(R"({"type": "department"})"_json),
                    data);

                return QStringFromStd(data.dump());
            } catch (const XStudioError &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                auto error                = R"({'error':{})"_json;
                error["error"]["source"]  = to_string(err.type());
                error["error"]["message"] = err.what();
                return QStringFromStd(JsonStore(error).dump());

            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                return QStringFromStd(err.what());
            }
        }
        return QString();
    });
}

QFuture<QString> ShotgunDataSourceUI::getCustomEntity24Future(const int project_id) {
    return QtConcurrent::run([=]() {
        if (backend_) {
            try {
                scoped_actor sys{system()};

                auto filter = R"(
                {
                    "logical_operator": "and",
                    "conditions": [
                        ["project", "is", {"type":"Project", "id":0}]
                    ]
                })"_json;

                filter["conditions"][0][2]["id"] = project_id;

                auto data = request_receive_wait<JsonStore>(
                    *sys,
                    backend_,
                    SHOTGUN_TIMEOUT,
                    shotgun_entity_search_atom_v,
                    "CustomEntity24",
                    JsonStore(filter),
                    std::vector<std::string>({"code", "id"}),
                    std::vector<std::string>({"code"}),
                    1,
                    4999);

                if (not data.count("data"))
                    throw std::runtime_error(data.dump(2));

                auto request  = R"({"type": "custom_entity_24", "id": 0})"_json;
                request["id"] = project_id;
                anon_send(as_actor(), shotgun_info_atom_v, JsonStore(request), data);

                return QStringFromStd(data.dump());
            } catch (const XStudioError &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                auto error                = R"({'error':{})"_json;
                error["error"]["source"]  = to_string(err.type());
                error["error"]["message"] = err.what();
                return QStringFromStd(JsonStore(error).dump());

            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                return QStringFromStd(err.what());
            }
        }
        return QString();
    });
}

QString ShotgunDataSourceUI::addVersionToPlaylist(
    const QString &version, const QUuid &playlist, const QUuid &before) {
    return addVersionToPlaylistFuture(version, playlist, before).result();
}

QFuture<QString> ShotgunDataSourceUI::addVersionToPlaylistFuture(
    const QString &version, const QUuid &playlist, const QUuid &before) {

    return QtConcurrent::run([=]() {
        if (backend_) {
            try {
                scoped_actor sys{system()};
                auto media = request_receive<UuidActorVector>(
                    *sys,
                    backend_,
                    playlist::add_media_atom_v,
                    JsonStore(nlohmann::json::parse(StdFromQString(version))),
                    UuidFromQUuid(playlist),
                    caf::actor(),
                    UuidFromQUuid(before));
                auto result = nlohmann::json::array();
                // return uuids..
                for (const auto &i : media) {
                    result.push_back(i.uuid());
                }

                return QStringFromStd(JsonStore(result).dump());
            } catch (const XStudioError &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                auto error                = R"({'error':{})"_json;
                error["error"]["source"]  = to_string(err.type());
                error["error"]["message"] = err.what();
                return QStringFromStd(JsonStore(error).dump());
            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                return QStringFromStd(err.what());
            } catch (...) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, "Unknown exception.");
                return QString();
            }
        }
        return QString();
    });
}

QString ShotgunDataSourceUI::updateEntity(
    const QString &entity, const int record_id, const QString &update_json) {
    return updateEntityFuture(entity, record_id, update_json).result();
}

QFuture<QString> ShotgunDataSourceUI::updateEntityFuture(
    const QString &entity, const int record_id, const QString &update_json) {
    return QtConcurrent::run([=]() {
        if (backend_) {
            try {
                scoped_actor sys{system()};

                auto js = request_receive_wait<JsonStore>(
                    *sys,
                    backend_,
                    SHOTGUN_TIMEOUT,
                    shotgun_update_entity_atom_v,
                    StdFromQString(entity),
                    record_id,
                    utility::JsonStore(nlohmann::json::parse(StdFromQString(update_json))));
                return QStringFromStd(js.dump());
            } catch (const XStudioError &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                auto error                = R"({'error':{})"_json;
                error["error"]["source"]  = to_string(err.type());
                error["error"]["message"] = err.what();
                return QStringFromStd(JsonStore(error).dump());
            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                return QStringFromStd(err.what());
            }
        }
        return QString();
    });
}

QFuture<QString> ShotgunDataSourceUI::preparePlaylistNotesFuture(
    const QUuid &playlist,
    const QList<QUuid> &media,
    const bool notify_owner,
    const std::vector<int> notify_group_ids,
    const bool combine,
    const bool add_time,
    const bool add_playlist_name,
    const bool add_type,
    const bool anno_requires_note,
    const bool skip_already_published,
    const QString &defaultType) {
    return QtConcurrent::run([=]() {
        if (backend_) {
            try {
                scoped_actor sys{system()};
                auto req = JsonStore(PreparePlaylistNotesJSON);

                for (const auto &i : media)
                    req["media_uuids"].push_back(to_string(UuidFromQUuid(i)));

                req["playlist_uuid"]          = to_string(UuidFromQUuid(playlist));
                req["notify_owner"]           = notify_owner;
                req["notify_group_ids"]       = notify_group_ids;
                req["combine"]                = combine;
                req["add_time"]               = add_time;
                req["add_playlist_name"]      = add_playlist_name;
                req["add_type"]               = add_type;
                req["anno_requires_note"]     = anno_requires_note;
                req["skip_already_published"] = skip_already_published;
                req["default_type"]           = StdFromQString(defaultType);

                auto js = request_receive_wait<JsonStore>(
                    *sys, backend_, SHOTGUN_TIMEOUT, data_source::get_data_atom_v, req);
                return QStringFromStd(js.dump());
            } catch (const XStudioError &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                auto error = R"({'error':{})"_json;
                // error["error"]["source"] = to_string(err.type());
                // error["error"]["message"] = err.what();
                return QStringFromStd(JsonStore(error).dump());
            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                return QStringFromStd(err.what());
            }
        }
        return QString();
    });
}


QFuture<QString>
ShotgunDataSourceUI::pushPlaylistNotesFuture(const QString &notes, const QUuid &playlist) {
    return QtConcurrent::run([=]() {
        if (backend_) {
            try {
                scoped_actor sys{system()};
                auto req             = JsonStore(CreatePlaylistNotesJSON);
                req["playlist_uuid"] = to_string(UuidFromQUuid(playlist));
                req["payload"] =
                    JsonStore(nlohmann::json::parse(StdFromQString(notes))["payload"]);

                auto js = request_receive_wait<JsonStore>(
                    *sys, backend_, SHOTGUN_TIMEOUT, data_source::post_data_atom_v, req);
                return QStringFromStd(js.dump());
            } catch (const XStudioError &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                auto error                = R"({'error':{})"_json;
                error["error"]["source"]  = to_string(err.type());
                error["error"]["message"] = err.what();
                return QStringFromStd(JsonStore(error).dump());
            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                return QStringFromStd(err.what());
            }
        }
        return QString();
    });
}


QFuture<QString> ShotgunDataSourceUI::createPlaylistFuture(
    const QUuid &playlist,
    const int project_id,
    const QString &name,
    const QString &location,
    const QString &playlist_type) {
    return QtConcurrent::run([=]() {
        if (backend_) {
            try {
                scoped_actor sys{system()};
                auto req             = JsonStore(CreatePlaylistJSON);
                req["playlist_uuid"] = to_string(UuidFromQUuid(playlist));
                req["project_id"]    = project_id;
                req["code"]          = StdFromQString(name);
                req["location"]      = StdFromQString(location);
                req["playlist_type"] = StdFromQString(playlist_type);

                auto js = request_receive_wait<JsonStore>(
                    *sys, backend_, SHOTGUN_TIMEOUT, data_source::post_data_atom_v, req);
                return QStringFromStd(js.dump());
            } catch (const XStudioError &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                auto error                = R"({'error':{})"_json;
                error["error"]["source"]  = to_string(err.type());
                error["error"]["message"] = err.what();
                return QStringFromStd(JsonStore(error).dump());
            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                return QStringFromStd(err.what());
            }
        }
        return QString();
    });
}

QFuture<QString> ShotgunDataSourceUI::updatePlaylistVersionsFuture(const QUuid &playlist) {
    return QtConcurrent::run([=]() {
        if (backend_) {
            try {
                scoped_actor sys{system()};
                auto req             = JsonStore(UpdatePlaylistJSON);
                req["playlist_uuid"] = to_string(UuidFromQUuid(playlist));
                auto js              = request_receive_wait<JsonStore>(
                    *sys, backend_, SHOTGUN_TIMEOUT, data_source::put_data_atom_v, req);
                return QStringFromStd(js.dump());
            } catch (const XStudioError &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                auto error                = R"({'error':{})"_json;
                error["error"]["source"]  = to_string(err.type());
                error["error"]["message"] = err.what();
                return QStringFromStd(JsonStore(error).dump());
            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                return QStringFromStd(err.what());
            }
        }
        return QString();
    });
}

// find playlist id from playlist
// request versions from shotgun
// add to playlist.
QFuture<QString> ShotgunDataSourceUI::refreshPlaylistVersionsFuture(const QUuid &playlist) {
    return QtConcurrent::run([=]() {
        if (backend_) {
            try {
                scoped_actor sys{system()};
                auto req             = JsonStore(RefreshPlaylistJSON);
                req["playlist_uuid"] = to_string(UuidFromQUuid(playlist));
                auto js              = request_receive_wait<JsonStore>(
                    *sys, backend_, SHOTGUN_TIMEOUT, data_source::use_data_atom_v, req);
                return QStringFromStd(js.dump());
            } catch (const XStudioError &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                auto error                = R"({'error':{})"_json;
                error["error"]["source"]  = to_string(err.type());
                error["error"]["message"] = err.what();
                return QStringFromStd(JsonStore(error).dump());
            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                return QStringFromStd(err.what());
            }
        }
        return QString();
    });
}

QFuture<QString> ShotgunDataSourceUI::refreshPlaylistNotesFuture(const QUuid &playlist) {
    return QtConcurrent::run([=]() {
        if (backend_) {
            try {
                scoped_actor sys{system()};
                auto req             = JsonStore(RefreshPlaylistNotesJSON);
                req["playlist_uuid"] = to_string(UuidFromQUuid(playlist));
                auto js              = request_receive_wait<JsonStore>(
                    *sys, backend_, SHOTGUN_TIMEOUT, data_source::use_data_atom_v, req);
                return QStringFromStd(js.dump());
            } catch (const XStudioError &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                auto error                = R"({'error':{})"_json;
                error["error"]["source"]  = to_string(err.type());
                error["error"]["message"] = err.what();
                return QStringFromStd(JsonStore(error).dump());
            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                return QStringFromStd(err.what());
            }
        }
        return QString();
    });
}

QString ShotgunDataSourceUI::getPlaylistNotes(const int id) {
    return getPlaylistNotesFuture(id).result();
}

QFuture<QString> ShotgunDataSourceUI::getPlaylistNotesFuture(const int id) {
    return QtConcurrent::run([=]() {
        if (backend_) {
            try {
                scoped_actor sys{system()};

                auto note_filter = R"(
                {
                    "logical_operator": "and",
                    "conditions": [
                        ["note_links", "in", {"type":"Playlist", "id":0}]
                    ]
                })"_json;

                note_filter["conditions"][0][2]["id"] = id;

                auto order = request_receive_wait<JsonStore>(
                    *sys,
                    backend_,
                    SHOTGUN_TIMEOUT,
                    shotgun_entity_search_atom_v,
                    "Notes",
                    JsonStore(note_filter),
                    std::vector<std::string>({"*"}),
                    std::vector<std::string>(),
                    1,
                    4999);

                return QStringFromStd(order.dump());
            } catch (const XStudioError &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                auto error                = R"({'error':{})"_json;
                error["error"]["source"]  = to_string(err.type());
                error["error"]["message"] = err.what();
                return QStringFromStd(JsonStore(error).dump());
            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                return QStringFromStd(err.what());
            }
        }
        return QString();
    });
}

QString ShotgunDataSourceUI::getPlaylistVersions(const int id) {
    return getPlaylistVersionsFuture(id).result();
}

QFuture<QString> ShotgunDataSourceUI::getPlaylistVersionsFuture(const int id) {
    return QtConcurrent::run([=]() {
        if (backend_) {
            try {
                scoped_actor sys{system()};

                auto vers = request_receive_wait<JsonStore>(
                    *sys,
                    backend_,
                    SHOTGUN_TIMEOUT,
                    shotgun_entity_atom_v,
                    "Playlists",
                    id,
                    std::vector<std::string>());

                // spdlog::warn("{}", vers.dump(2));

                auto order_filter = R"(
                {
                    "logical_operator": "and",
                    "conditions": [
                        ["playlist", "is", {"type":"Playlist", "id":0}]
                    ]
                })"_json;

                order_filter["conditions"][0][2]["id"] = id;

                auto order = request_receive_wait<JsonStore>(
                    *sys,
                    backend_,
                    SHOTGUN_TIMEOUT,
                    shotgun_entity_search_atom_v,
                    "PlaylistVersionConnection",
                    JsonStore(order_filter),
                    std::vector<std::string>({"sg_sort_order", "version"}),
                    std::vector<std::string>({"sg_sort_order"}),
                    1,
                    4999);

                // should be returned in the correct order..

                // spdlog::warn("{}", order.dump(2));

                std::vector<std::string> version_ids;
                for (const auto &i : order["data"])
                    version_ids.emplace_back(std::to_string(
                        i["relationships"]["version"]["data"].at("id").get<int>()));

                if (version_ids.empty())
                    return QStringFromStd(vers.dump());

                // expand version information..
                // get versions
                auto query       = R"({})"_json;
                auto chunked_ids = split_vector(version_ids, 100);
                auto data        = R"([])"_json;

                for (const auto &chunk : chunked_ids) {
                    query["id"] = join_as_string(chunk, ",");

                    auto js = request_receive_wait<JsonStore>(
                        *sys,
                        backend_,
                        SHOTGUN_TIMEOUT,
                        shotgun_entity_filter_atom_v,
                        "Versions",
                        JsonStore(query),
                        VersionFields,
                        std::vector<std::string>(),
                        1,
                        4999);
                    // reorder list based on playlist..
                    // spdlog::warn("{}", js.dump(2));

                    for (const auto &i : chunk) {
                        for (auto &j : js["data"]) {

                            // spdlog::warn("{} {}", std::to_string(j["id"].get<int>()), i);
                            if (std::to_string(j["id"].get<int>()) == i) {
                                data.push_back(j);
                                break;
                            }
                        }
                    }
                }

                auto data_tmp    = R"({"data":[]})"_json;
                data_tmp["data"] = data;

                // spdlog::warn("{}", js.dump(2));

                // add back in
                vers["data"]["relationships"]["versions"] = data_tmp;

                // create playlist..
                return QStringFromStd(vers.dump());
            } catch (const XStudioError &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                auto error                = R"({'error':{})"_json;
                error["error"]["source"]  = to_string(err.type());
                error["error"]["message"] = err.what();
                return QStringFromStd(JsonStore(error).dump());
            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                return QStringFromStd(err.what());
            }
        }
        return QString();
    });
}

QFuture<QString> ShotgunDataSourceUI::requestFileTransferFuture(
    const QVariantList &uuids,
    const QString &project,
    const QString &src_location,
    const QString &dest_location) {
    return QtConcurrent::run([=]() {
        QString program = "dnenv-do";
        QString result;
        auto show_env = utility::get_env("SHOW");
        auto shot_env = utility::get_env("SHOT");

        QStringList args = {
            QStringFromStd(show_env ? *show_env : "NSFL"),
            QStringFromStd(shot_env ? *shot_env : "ldev_pipe"),
            "--",
            "ft-cp",
            // "-n",
            "-debug",
            "-show",
            project,
            "-e",
            "production",
            "--watchers",
            QStringFromStd(get_login_name()),
            "-priority",
            "medium",
            "-description",
            "File transfer requested by xStudio"};

        std::vector<std::string> dnuuids;
        for (const auto &i : uuids) {
            dnuuids.emplace_back(to_string(UuidFromQUuid(i.toUuid())));
        }
        args.push_back(src_location + ":" + QStringFromStd(join_as_string(dnuuids, ",")));
        args.push_back(dest_location);

        qint64 pid;
        QProcess::startDetached(program, args, "", &pid);

        return result;
    });
}

QFuture<QString> ShotgunDataSourceUI::addDownloadToMediaFuture(const QUuid &media) {
    return QtConcurrent::run([=]() {
        if (backend_) {
            try {
                scoped_actor sys{system()};
                auto req          = JsonStore(DownloadMediaJSON);
                req["media_uuid"] = to_string(UuidFromQUuid(media));

                return QStringFromStd(
                    request_receive_wait<JsonStore>(
                        *sys, backend_, SHOTGUN_TIMEOUT, data_source::get_data_atom_v, req)
                        .dump());
            } catch (const XStudioError &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                auto error                = R"({'error':{})"_json;
                error["error"]["source"]  = to_string(err.type());
                error["error"]["message"] = err.what();
                return QStringFromStd(JsonStore(error).dump());
            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                return QStringFromStd(err.what());
            }
        }
        return QString();
    });
}

// ft-cp -n -debug -show MEG2 -e production  b3ae9497-6218-4124-8f8e-07158e3165dd mum --watchers
// al -priority medium -description "File transfer requested by xStudio


//![plugin]
class QShotgunDataSourceQml : public QQmlExtensionPlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QQmlEngineExtensionInterface_iid)

  public:
    void registerTypes(const char *uri) override {
        qmlRegisterType<ShotgunDataSourceUI>(uri, 1, 0, "ShotgunDataSource");
    }
};
//![plugin]

#include "data_source_shotgun_ui.moc"
