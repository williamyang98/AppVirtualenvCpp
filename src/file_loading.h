#pragma once

#include <ostream>
#include <optional>

#include <rapidjson/document.h>

namespace app {

std::optional<rapidjson::Document> load_document_from_filename(const char *fn);

void write_json_to_stream(const rapidjson::Document &doc, std::ostream &os);
bool write_document_to_file(const char *fn, const rapidjson::Document &doc);

}