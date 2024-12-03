#include "xstudio/conform/conformer.hpp"

using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::conform;

Conformer::Conformer(const utility::JsonStore &prefs) { update_preferences(prefs); }

void Conformer::update_preferences(const utility::JsonStore &prefs) {}

std::vector<std::string> Conformer::conform_tasks() { return std::vector<std::string>(); }

ConformReply
Conformer::conform_request(const std::string &conform_task, const ConformRequest &request) {
    return ConformReply(request);
}

ConformReply Conformer::conform_request(const ConformRequest &request) {
    return ConformReply(request);
}

std::vector<std::optional<std::pair<std::string, caf::uri>>> Conformer::conform_find_timeline(
    const std::vector<std::pair<utility::UuidActor, utility::JsonStore>> &media) {
    return std::vector<std::optional<std::pair<std::string, caf::uri>>>(media.size());
}

bool Conformer::conform_prepare_timeline(
    const UuidActor &timeline, const bool only_create_conform_track) {
    return false;
}

utility::UuidActorVector Conformer::find_matching(
    const std::string &key,
    const std::pair<utility::UuidActor, utility::JsonStore> &needle,
    const std::vector<std::pair<utility::UuidActor, utility::JsonStore>> &haystack) {
    return utility::UuidActorVector();
}
