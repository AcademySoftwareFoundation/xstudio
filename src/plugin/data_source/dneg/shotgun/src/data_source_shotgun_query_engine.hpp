// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <nlohmann/json.hpp>

#include "xstudio/utility/json_store.hpp"

using namespace xstudio;

const auto TermTemplate = R"({
	"id": null,
	"type": "term",
    "term": "",
    "value": "",
    "dynamic": false,
    "enabled": true,
    "livelink": null,
    "negated": null
})"_json;

const auto PresetTemplate = R"({
	"id": null,
	"type": "preset",
	"name": "PRESET",
    "children": []
})"_json;

const auto GroupTemplate = R"({
	"id": null,
	"type": "group",
	"name": "GROUP",
	"entity": "",
    "children": [
		null,
    	{
    		"id": null,
    		"type": "presets",
    		"children": []
    	}
    ]
})"_json;

const auto RootTemplate = R"({
	"id": null,
	"name": "root",
	"type": "root",
    "children": []
})"_json;


class QueryEngine {
  public:
    QueryEngine()          = default;
    virtual ~QueryEngine() = default;

    static utility::JsonStore build_query(
        const int project_id,
        const std::string &entity,
        const utility::JsonStore &group_terms,
        const utility::JsonStore &terms,
        const utility::JsonStore &context,
        const utility::JsonStore &lookup);

    static utility::JsonStore merge_query(
        const utility::JsonStore &base,
        const utility::JsonStore &override,
        const bool ignore_duplicates = true);

  private:
    utility::JsonStore data_;
};