#include "xstudio/conform/conformer.hpp"

using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::conform;

Conformer::Conformer(const utility::JsonStore &prefs) { update_preferences(prefs); }

void Conformer::update_preferences(const utility::JsonStore &prefs) {}

std::vector<std::string> Conformer::conform_tasks() { return std::vector<std::string>(); }

ConformReply Conformer::conform_request(
    const std::string &conform_task,
    const utility::JsonStore &conform_detail,
    const ConformRequest &request) {
    return ConformReply();
}
