#ifndef USD_GODOT_MCP_JSON_H
#define USD_GODOT_MCP_JSON_H

#include <string>
#include <sstream>
#include <vector>
#include <map>
#include <iomanip>

namespace mcp {

// Simple JSON builder without external dependencies
// Supports only the subset needed for MCP protocol
class JsonValue {
public:
    enum Type {
        TYPE_NULL,
        TYPE_BOOL,
        TYPE_NUMBER,
        TYPE_STRING,
        TYPE_ARRAY,
        TYPE_OBJECT
    };

    JsonValue() : type_(TYPE_NULL) {}

    static JsonValue null() {
        return JsonValue();
    }

    static JsonValue boolean(bool value) {
        JsonValue v;
        v.type_ = TYPE_BOOL;
        v.bool_value_ = value;
        return v;
    }

    static JsonValue number(int value) {
        JsonValue v;
        v.type_ = TYPE_NUMBER;
        v.number_value_ = value;
        return v;
    }

    static JsonValue number(double value) {
        JsonValue v;
        v.type_ = TYPE_NUMBER;
        v.number_value_ = value;
        return v;
    }

    static JsonValue string(const std::string& value) {
        JsonValue v;
        v.type_ = TYPE_STRING;
        v.string_value_ = value;
        return v;
    }

    static JsonValue array() {
        JsonValue v;
        v.type_ = TYPE_ARRAY;
        return v;
    }

    static JsonValue object() {
        JsonValue v;
        v.type_ = TYPE_OBJECT;
        return v;
    }

    // Array operations
    void push(const JsonValue& value) {
        if (type_ != TYPE_ARRAY) {
            type_ = TYPE_ARRAY;
        }
        array_value_.push_back(value);
    }

    // Object operations
    void set(const std::string& key, const JsonValue& value) {
        if (type_ != TYPE_OBJECT) {
            type_ = TYPE_OBJECT;
        }
        object_value_[key] = value;
    }

    // Serialize to JSON string
    std::string to_string() const {
        std::ostringstream oss;
        write(oss);
        return oss.str();
    }

private:
    void write(std::ostringstream& oss) const {
        switch (type_) {
            case TYPE_NULL:
                oss << "null";
                break;
            case TYPE_BOOL:
                oss << (bool_value_ ? "true" : "false");
                break;
            case TYPE_NUMBER:
                oss << number_value_;
                break;
            case TYPE_STRING:
                write_string(oss, string_value_);
                break;
            case TYPE_ARRAY:
                write_array(oss);
                break;
            case TYPE_OBJECT:
                write_object(oss);
                break;
        }
    }

    void write_string(std::ostringstream& oss, const std::string& str) const {
        oss << '"';
        for (char c : str) {
            switch (c) {
                case '"': oss << "\\\""; break;
                case '\\': oss << "\\\\"; break;
                case '\b': oss << "\\b"; break;
                case '\f': oss << "\\f"; break;
                case '\n': oss << "\\n"; break;
                case '\r': oss << "\\r"; break;
                case '\t': oss << "\\t"; break;
                default:
                    if (c < 32) {
                        oss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
                    } else {
                        oss << c;
                    }
            }
        }
        oss << '"';
    }

    void write_array(std::ostringstream& oss) const {
        oss << '[';
        for (size_t i = 0; i < array_value_.size(); ++i) {
            if (i > 0) oss << ',';
            array_value_[i].write(oss);
        }
        oss << ']';
    }

    void write_object(std::ostringstream& oss) const {
        oss << '{';
        bool first = true;
        for (const auto& pair : object_value_) {
            if (!first) oss << ',';
            first = false;
            write_string(oss, pair.first);
            oss << ':';
            pair.second.write(oss);
        }
        oss << '}';
    }

    Type type_;
    bool bool_value_ = false;
    double number_value_ = 0.0;
    std::string string_value_;
    std::vector<JsonValue> array_value_;
    std::map<std::string, JsonValue> object_value_;
};

} // namespace mcp

#endif // USD_GODOT_MCP_JSON_H
