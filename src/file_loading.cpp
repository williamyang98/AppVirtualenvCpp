#include <optional>
#include <ostream>
#include <fstream>

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/error/en.h>

#include <spdlog/spdlog.h>
#include <fmt/core.h>

namespace app {

std::optional<rapidjson::Document> load_document_from_filename(const char *fn) {
    std::ifstream file(fn);
    if (!file.is_open()) {
        spdlog::error(fmt::format("Failed to open document file ({})", fn));
        return {};
    }

    std::stringstream ss;
    ss << file.rdbuf();
    file.close();

    rapidjson::Document doc;
    rapidjson::ParseResult ok = doc.Parse(ss.str().c_str());
    if (!ok) {
        spdlog::error(fmt::format("JSON parse error: {} ({})", 
            rapidjson::GetParseError_En(ok.Code()), ok.Offset()));
        return {};
    }

    return doc;
}

void write_json_to_stream(const rapidjson::Document &doc, std::ostream &os) {
    rapidjson::StringBuffer sb;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(sb);
    writer.SetIndent(' ', 1);

    doc.Accept(writer);
    os << sb.GetString() << std::endl;
}

bool write_document_to_file(const char *fn, const rapidjson::Document &doc) {
    std::ofstream file(fn);
    if (!file.is_open()) {
        return false;
    }

    write_json_to_stream(doc, file);
    file.close();
    return true;
}

}