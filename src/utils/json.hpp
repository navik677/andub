#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <cctype>
#include <cstdint>

namespace anime::json {

enum class Type { Null, Boolean, Number, String, Array, Object };

class Value {
public:
    Type type = Type::Null;
    bool bool_val = false;
    double num_val = 0.0;
    std::string str_val;
    std::vector<Value> arr_val;
    std::map<std::string, Value> obj_val;

    Value() = default;
    Value(std::nullptr_t) : type(Type::Null) {}
    Value(bool b) : type(Type::Boolean), bool_val(b) {}
    Value(int n) : type(Type::Number), num_val(n) {}
    Value(double n) : type(Type::Number), num_val(n) {}
    Value(const char* s) : type(Type::String), str_val(s ? s : "") {}
    Value(std::string s) : type(Type::String), str_val(std::move(s)) {}

    bool is_null() const { return type == Type::Null; }
    bool is_bool() const { return type == Type::Boolean; }
    bool is_number() const { return type == Type::Number; }
    bool is_string() const { return type == Type::String; }
    bool is_array() const { return type == Type::Array; }
    bool is_object() const { return type == Type::Object; }

    std::string get_str(const std::string& def = "") const {
        return is_string() ? str_val : def;
    }

    int get_int(int def = 0) const {
        return is_number() ? static_cast<int>(num_val) : def;
    }

    double get_double(double def = 0.0) const {
        return is_number() ? num_val : def;
    }

    bool get_bool(bool def = false) const {
        return is_bool() ? bool_val : def;
    }

    bool contains(const std::string& key) const {
        if (!is_object()) return false;
        return obj_val.find(key) != obj_val.end();
    }

    const Value& operator[](const std::string& key) const {
        static const Value null_val;
        if (!is_object()) return null_val;
        auto it = obj_val.find(key);
        return it != obj_val.end() ? it->second : null_val;
    }

    Value& operator[](const std::string& key) {
        if (type != Type::Object) {
            type = Type::Object;
            obj_val.clear();
        }
        return obj_val[key];
    }

    const Value& operator[](size_t index) const {
        static const Value null_val;
        if (!is_array() || index >= arr_val.size()) return null_val;
        return arr_val[index];
    }

    Value& operator[](size_t index) {
        if (type != Type::Array) {
            type = Type::Array;
            arr_val.clear();
        }
        if (index >= arr_val.size()) {
            arr_val.resize(index + 1);
        }
        return arr_val[index];
    }

    size_t size() const {
        if (is_array()) return arr_val.size();
        if (is_object()) return obj_val.size();
        return 0;
    }

    void push_back(Value v) {
        if (type != Type::Array) {
            type = Type::Array;
            arr_val.clear();
        }
        arr_val.push_back(std::move(v));
    }

    std::string dump(int indent = 0, int current_indent = 0) const {
        std::string ind(current_indent, ' ');
        std::string next_ind(current_indent + indent, ' ');
        switch (type) {
            case Type::Null: return "null";
            case Type::Boolean: return bool_val ? "true" : "false";
            case Type::Number: {
                if (num_val == static_cast<long long>(num_val)) {
                    return std::to_string(static_cast<long long>(num_val));
                }
                return std::to_string(num_val);
            }
            case Type::String: {
                std::ostringstream ss;
                ss << '"';
                for (char c : str_val) {
                    if (c == '"') ss << "\\\"";
                    else if (c == '\\') ss << "\\\\";
                    else if (c == '\b') ss << "\\b";
                    else if (c == '\f') ss << "\\f";
                    else if (c == '\n') ss << "\\n";
                    else if (c == '\r') ss << "\\r";
                    else if (c == '\t') ss << "\\t";
                    else ss << c;
                }
                ss << '"';
                return ss.str();
            }
            case Type::Array: {
                if (arr_val.empty()) return "[]";
                std::string res = "[";
                if (indent > 0) res += "\n";
                for (size_t i = 0; i < arr_val.size(); ++i) {
                    if (indent > 0) res += next_ind;
                    res += arr_val[i].dump(indent, current_indent + indent);
                    if (i + 1 < arr_val.size()) res += ",";
                    if (indent > 0) res += "\n";
                    else if (i + 1 < arr_val.size()) res += " ";
                }
                if (indent > 0) res += ind;
                res += "]";
                return res;
            }
            case Type::Object: {
                if (obj_val.empty()) return "{}";
                std::string res = "{";
                if (indent > 0) res += "\n";
                size_t i = 0;
                for (const auto& [k, v] : obj_val) {
                    if (indent > 0) res += next_ind;
                    res += "\"" + k + "\": " + v.dump(indent, current_indent + indent);
                    if (i + 1 < obj_val.size()) res += ",";
                    if (indent > 0) res += "\n";
                    else if (i + 1 < obj_val.size()) res += " ";
                    i++;
                }
                if (indent > 0) res += ind;
                res += "}";
                return res;
            }
        }
        return "null";
    }

    static Value parse(const std::string& src) {
        size_t pos = 0;
        return parse_value(src, pos);
    }

private:
    static void skip_whitespace(const std::string& s, size_t& pos) {
        while (pos < s.size() && std::isspace(static_cast<unsigned char>(s[pos]))) {
            pos++;
        }
    }

    static Value parse_value(const std::string& s, size_t& pos) {
        skip_whitespace(s, pos);
        if (pos >= s.size()) return Value();

        char c = s[pos];
        if (c == 'n') { pos += 4; return Value(); } // null
        if (c == 't') { pos += 4; return Value(true); } // true
        if (c == 'f') { pos += 5; return Value(false); } // false
        if (c == '"') return parse_string(s, pos);
        if (c == '[') return parse_array(s, pos);
        if (c == '{') return parse_object(s, pos);
        if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) return parse_number(s, pos);

        return Value();
    }

    static int hex_val(char c) {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    }

    static uint32_t parse_hex4(const std::string& s, size_t pos) {
        uint32_t val = 0;
        for (size_t i = 0; i < 4; ++i) {
            int h = hex_val(s[pos + i]);
            if (h < 0) return 0;
            val = (val << 4) | h;
        }
        return val;
    }

    static void append_utf8(std::string& out, uint32_t cp) {
        if (cp <= 0x7F) {
            out += static_cast<char>(cp);
        } else if (cp <= 0x7FF) {
            out += static_cast<char>(0xC0 | ((cp >> 6) & 0x1F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp <= 0xFFFF) {
            out += static_cast<char>(0xE0 | ((cp >> 12) & 0x0F));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp <= 0x10FFFF) {
            out += static_cast<char>(0xF0 | ((cp >> 18) & 0x07));
            out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        }
    }

    static Value parse_string(const std::string& s, size_t& pos) {
        pos++; // skip "
        std::string res;
        while (pos < s.size()) {
            char c = s[pos++];
            if (c == '"') break;
            if (c == '\\' && pos < s.size()) {
                char esc = s[pos++];
                if (esc == '"') res += '"';
                else if (esc == '\\') res += '\\';
                else if (esc == '/') res += '/';
                else if (esc == 'b') res += '\b';
                else if (esc == 'f') res += '\f';
                else if (esc == 'n') res += '\n';
                else if (esc == 'r') res += '\r';
                else if (esc == 't') res += '\t';
                else if (esc == 'u' && pos + 4 <= s.size()) {
                    uint32_t cp = parse_hex4(s, pos);
                    pos += 4;
                    // Handle surrogate pairs for astral planes
                    if (cp >= 0xD800 && cp <= 0xDBFF && pos + 6 <= s.size() && s[pos] == '\\' && s[pos + 1] == 'u') {
                        uint32_t low = parse_hex4(s, pos + 2);
                        if (low >= 0xDC00 && low <= 0xDFFF) {
                            cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                            pos += 6;
                        }
                    }
                    append_utf8(res, cp);
                }
            } else {
                res += c;
            }
        }
        return Value(std::move(res));
    }

    static Value parse_number(const std::string& s, size_t& pos) {
        size_t start = pos;
        if (s[pos] == '-') pos++;
        while (pos < s.size() && (std::isdigit(static_cast<unsigned char>(s[pos])) || s[pos] == '.' || s[pos] == 'e' || s[pos] == 'E' || s[pos] == '+' || s[pos] == '-')) {
            pos++;
        }
        double val = 0.0;
        try {
            val = std::stod(s.substr(start, pos - start));
        } catch (...) {}
        return Value(val);
    }

    static Value parse_array(const std::string& s, size_t& pos) {
        pos++; // skip [
        Value arr;
        arr.type = Type::Array;
        while (pos < s.size()) {
            skip_whitespace(s, pos);
            if (pos < s.size() && s[pos] == ']') {
                pos++;
                break;
            }
            arr.push_back(parse_value(s, pos));
            skip_whitespace(s, pos);
            if (pos < s.size() && s[pos] == ',') pos++;
        }
        return arr;
    }

    static Value parse_object(const std::string& s, size_t& pos) {
        pos++; // skip {
        Value obj;
        obj.type = Type::Object;
        while (pos < s.size()) {
            skip_whitespace(s, pos);
            if (pos < s.size() && s[pos] == '}') {
                pos++;
                break;
            }
            if (s[pos] != '"') break;
            Value key_val = parse_string(s, pos);
            skip_whitespace(s, pos);
            if (pos < s.size() && s[pos] == ':') pos++;
            Value val = parse_value(s, pos);
            obj.obj_val[key_val.str_val] = std::move(val);
            skip_whitespace(s, pos);
            if (pos < s.size() && s[pos] == ',') pos++;
        }
        return obj;
    }
};

} // namespace anime::json
