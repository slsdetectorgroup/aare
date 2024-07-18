#include "aare/file_io/ClusterFile.hpp"
namespace aare {

// ClusterFileHeader functions
std::string ClusterFileHeader::to_json() const {
    std::string json;
    json.reserve(1024);
    json += "{";
    write_str(json, "version", version);
    std::string n_records_str = std::to_string(n_records);
    n_records_str.insert(0, N_RECORDS_CHARS - n_records_str.size(), '0');
    write_str(json, "n_records", n_records_str);

    write_map(json, "metadata", metadata);
    json += "\"header_fields\": [";
    for (auto &f : header_fields) {
        json += f.to_json();
        json += ", ";
    }
    json.pop_back();
    json.pop_back();
    json += "], ";
    json += "\"data_fields\": [";
    for (auto &f : data_fields) {
        json += f.to_json();
        json += ", ";
    }
    json.pop_back();
    json.pop_back();
    json += "]";
    json += "}";
    return json;
}

void ClusterFileHeader::from_json(std::string const &json) {
    simdjson::padded_string const ps(json.c_str(), json.size());
    simdjson::ondemand::parser parser;
    simdjson::ondemand::document doc = parser.iterate(ps);
    simdjson::ondemand::object object = doc.get_object();
    for (auto field : object) {
        std::string_view const key = field.unescaped_key();
        if (key == "version") {
            std::string_view version_sv = field.value().get_string();
            if (version_sv.size() != 3) {
                throw std::runtime_error("Invalid version string");
            }
            this->version = std::string(version_sv.data(), version_sv.size());
        } else if (key == "n_records") {
            std::string_view tmp = field.value().get_string();
            try {

                this->n_records = std::stoi(std::string(tmp.data(), tmp.size()));
            } catch (std::invalid_argument &e) {
                throw std::runtime_error("Invalid n_records value");
            }

        } else if (key == "metadata") {
            simdjson::ondemand::object metadata_obj = field.value().get_object();
            for (auto meta : metadata_obj) {
                std::string_view key_view(meta.unescaped_key());
                std::string const key_str(key_view.data(), key_view.size());
                std::string_view value_view(meta.value().get_string());
                std::string const value_str(value_view.data(), value_view.size());
                this->metadata[key_str] = value_str;
            }
        } else if (key == "header_fields") {
            simdjson::ondemand::array header_fields_arr = field.value().get_array();
            for (auto hf : header_fields_arr) {
                Field f;
                std::string_view const sv = hf.raw_json();
                f.from_json(std::string(sv.data(), sv.size()));
                this->header_fields.push_back(f);
            }
        } else if (key == "data_fields") {
            simdjson::ondemand::array data_fields_arr = field.value().get_array();
            for (auto df : data_fields_arr) {
                Field f;
                std::string_view const sv = df.raw_json();
                f.from_json(std::string(sv.data(), sv.size()));
                this->data_fields.push_back(f);
            }
        }
    }
}
bool ClusterFileHeader::operator==(ClusterFileHeader const &other) const {
    return version == other.version && n_records == other.n_records && metadata == other.metadata &&
           header_fields == other.header_fields && data_fields == other.data_fields;
}

} // namespace aare