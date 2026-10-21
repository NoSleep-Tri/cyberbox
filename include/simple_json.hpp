#pragma once
#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <iomanip>

namespace json {

enum class Type {
    Null,
    Boolean,
    Number,
    String,
    Array,
    Object
};

class Value {
public:
    Type type = Type::Null;
    bool bool_val = false;
    double num_val = 0.0;
    std::string str_val;
    std::vector<Value> arr_val;
    std::map<std::string, Value> obj_val;

    Value() : type(Type::Null) {}
    Value(std::nullptr_t) : type(Type::Null) {}
    Value(bool b) : type(Type::Boolean), bool_val(b) {}
    Value(int n) : type(Type::Number), num_val(n) {}
    Value(long n) : type(Type::Number), num_val(n) {}
    Value(long long n) : type(Type::Number), num_val(static_cast<double>(n)) {}
    Value(double n) : type(Type::Number), num_val(n) {}
    Value(const char* s) : type(Type::String), str_val(s ? s : "") {}
    Value(const std::string& s) : type(Type::String), str_val(s) {}

    static Value array() {
        Value v;
        v.type = Type::Array;
        return v;
    }

    static Value object() {
        Value v;
        v.type = Type::Object;
        return v;
    }

    bool is_null() const { return type == Type::Null; }
    bool is_bool() const { return type == Type::Boolean; }
    bool is_number() const { return type == Type::Number; }
    bool is_string() const { return type == Type::String; }
    bool is_array() const { return type == Type::Array; }
    bool is_object() const { return type == Type::Object; }

    bool as_bool(bool def = false) const {
        return (type == Type::Boolean) ? bool_val : def;
    }

    int as_int(int def = 0) const {
        return (type == Type::Number) ? static_cast<int>(num_val) : def;
    }

    double as_double(double def = 0.0) const {
        return (type == Type::Number) ? num_val : def;
    }

    std::string as_string(const std::string& def = "") const {
        return (type == Type::String) ? str_val : def;
    }

    // Array operations
    void push_back(const Value& v) {
        if (type != Type::Array) {
            type = Type::Array;
            arr_val.clear();
        }
        arr_val.push_back(v);
    }

    size_t size() const {
        if (type == Type::Array) return arr_val.size();
        if (type == Type::Object) return obj_val.size();
        return 0;
    }

    const Value& operator[](size_t idx) const {
        static Value null_v;
        if (type == Type::Array && idx < arr_val.size()) {
            return arr_val[idx];
        }
        return null_v;
    }

    Value& operator[](size_t idx) {
        if (type != Type::Array) {
            type = Type::Array;
            arr_val.clear();
        }
        if (idx >= arr_val.size()) {
            arr_val.resize(idx + 1);
        }
        return arr_val[idx];
    }

    // Object operations
    bool contains(const std::string& key) const {
        if (type != Type::Object) return false;
        return obj_val.find(key) != obj_val.end();
    }

    const Value& operator[](const std::string& key) const {
        static Value null_v;
        if (type == Type::Object) {
            auto it = obj_val.find(key);
            if (it != obj_val.end()) return it->second;
        }
        return null_v;
    }

    Value& operator[](const std::string& key) {
        if (type != Type::Object) {
            type = Type::Object;
            obj_val.clear();
        }
        return obj_val[key];
    }

    std::string dump(int indent = -1, int current_indent = 0) const {
        std::ostringstream ss;
        std::string ind_str = (indent >= 0) ? std::string(current_indent, ' ') : "";
        std::string next_ind_str = (indent >= 0) ? std::string(current_indent + indent, ' ') : "";
        std::string nl = (indent >= 0) ? "\n" : "";
        std::string sp = (indent >= 0) ? " " : "";

        switch (type) {
            case Type::Null:
                ss << "null";
                break;
            case Type::Boolean:
                ss << (bool_val ? "true" : "false");
                break;
            case Type::Number: {
                if (num_val == static_cast<long long>(num_val)) {
                    ss << static_cast<long long>(num_val);
                } else {
                    ss << std::setprecision(15) << num_val;
                }
                break;
            }
            case Type::String: {
                ss << "\"";
                for (char c : str_val) {
                    if (c == '"') ss << "\\\"";
                    else if (c == '\\') ss << "\\\\";
                    else if (c == '\b') ss << "\\b";
                    else if (c == '\f') ss << "\\f";
                    else if (c == '\n') ss << "\\n";
                    else if (c == '\r') ss << "\\r";
                    else if (c == '\t') ss << "\\t";
                    else if (static_cast<unsigned char>(c) < 0x20) {
                        ss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << (int)(unsigned char)c << std::dec;
                    } else {
                        ss << c;
                    }
                }
                ss << "\"";
                break;
            }
            case Type::Array: {
                if (arr_val.empty()) {
                    ss << "[]";
                } else {
                    ss << "[" << nl;
                    for (size_t i = 0; i < arr_val.size(); ++i) {
                        ss << next_ind_str << arr_val[i].dump(indent, current_indent + (indent >= 0 ? indent : 0));
                        if (i + 1 < arr_val.size()) ss << ",";
                        ss << nl;
                    }
                    ss << ind_str << "]";
                }
                break;
            }
            case Type::Object: {
                if (obj_val.empty()) {
                    ss << "{}";
                } else {
                    ss << "{" << nl;
                    size_t count = 0;
                    for (const auto& pair : obj_val) {
                        ss << next_ind_str << "\"" << pair.first << "\":" << sp;
                        ss << pair.second.dump(indent, current_indent + (indent >= 0 ? indent : 0));
                        if (++count < obj_val.size()) ss << ",";
                        ss << nl;
                    }
                    ss << ind_str << "}";
                }
                break;
            }
        }
        return ss.str();
    }
};

class Parser {
    const std::string& src;
    size_t pos = 0;

    void skip_whitespace() {
        while (pos < src.size()) {
            char c = src[pos];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                pos++;
            } else if (c == '/' && pos + 1 < src.size() && src[pos + 1] == '/') {
                pos += 2;
                while (pos < src.size() && src[pos] != '\n') pos++;
            } else {
                break;
            }
        }
    }

    char peek() {
        skip_whitespace();
        if (pos >= src.size()) return '\0';
        return src[pos];
    }

    char get() {
        skip_whitespace();
        if (pos >= src.size()) return '\0';
        return src[pos++];
    }

public:
    explicit Parser(const std::string& s) : src(s) {}

    Value parse() {
        skip_whitespace();
        Value v = parse_value();
        skip_whitespace();
        return v;
    }

private:
    Value parse_value() {
        char c = peek();
        if (c == 'n') return parse_null();
        if (c == 't' || c == 'f') return parse_bool();
        if (c == '"') return parse_string();
        if (c == '[') return parse_array();
        if (c == '{') return parse_object();
        if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) return parse_number();
        return Value();
    }

    Value parse_null() {
        if (src.compare(pos, 4, "null") == 0) {
            pos += 4;
            return Value();
        }
        return Value();
    }

    Value parse_bool() {
        if (src.compare(pos, 4, "true") == 0) {
            pos += 4;
            return Value(true);
        }
        if (src.compare(pos, 5, "false") == 0) {
            pos += 5;
            return Value(false);
        }
        return Value(false);
    }

    Value parse_string() {
        if (get() != '"') return Value("");
        std::string s;
        while (pos < src.size()) {
            char c = src[pos++];
            if (c == '"') return Value(s);
            if (c == '\\') {
                if (pos >= src.size()) break;
                char esc = src[pos++];
                switch (esc) {
                    case '"': s += '"'; break;
                    case '\\': s += '\\'; break;
                    case '/': s += '/'; break;
                    case 'b': s += '\b'; break;
                    case 'f': s += '\f'; break;
                    case 'n': s += '\n'; break;
                    case 'r': s += '\r'; break;
                    case 't': s += '\t'; break;
                    case 'u': {
                        if (pos + 4 <= src.size()) {
                            std::string hex_code = src.substr(pos, 4);
                            pos += 4;
                            try {
                                unsigned int cp = std::stoul(hex_code, nullptr, 16);
                                // Handle UTF-16 surrogate pairs (\uD800..\uDBFF followed by \uDC00..\uDFFF)
                                if (cp >= 0xD800 && cp <= 0xDBFF && pos + 6 <= src.size() &&
                                    src[pos] == '\\' && src[pos + 1] == 'u') {
                                    std::string low_hex = src.substr(pos + 2, 4);
                                    unsigned int low = std::stoul(low_hex, nullptr, 16);
                                    if (low >= 0xDC00 && low <= 0xDFFF) {
                                        pos += 6;
                                        cp = 0x10000 + (((cp - 0xD800) << 10) | (low - 0xDC00));
                                    }
                                }
                                if (cp < 0x80) {
                                    s += static_cast<char>(cp);
                                } else if (cp < 0x800) {
                                    s += static_cast<char>(0xC0 | ((cp >> 6) & 0x1F));
                                    s += static_cast<char>(0x80 | (cp & 0x3F));
                                } else if (cp < 0x10000) {
                                    s += static_cast<char>(0xE0 | ((cp >> 12) & 0x0F));
                                    s += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                                    s += static_cast<char>(0x80 | (cp & 0x3F));
                                } else {
                                    s += static_cast<char>(0xF0 | ((cp >> 18) & 0x07));
                                    s += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
                                    s += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                                    s += static_cast<char>(0x80 | (cp & 0x3F));
                                }
                            } catch (...) {}
                        }
                        break;
                    }
                    default: s += esc; break;
                }
            } else {
                s += c;
            }
        }
        return Value(s);
    }

    Value parse_number() {
        size_t start = pos;
        if (pos < src.size() && src[pos] == '-') pos++;
        while (pos < src.size() && std::isdigit(static_cast<unsigned char>(src[pos]))) pos++;
        if (pos < src.size() && src[pos] == '.') {
            pos++;
            while (pos < src.size() && std::isdigit(static_cast<unsigned char>(src[pos]))) pos++;
        }
        if (pos < src.size() && (src[pos] == 'e' || src[pos] == 'E')) {
            pos++;
            if (pos < src.size() && (src[pos] == '+' || src[pos] == '-')) pos++;
            while (pos < src.size() && std::isdigit(static_cast<unsigned char>(src[pos]))) pos++;
        }
        std::string num_str = src.substr(start, pos - start);
        try {
            double val = std::stod(num_str);
            return Value(val);
        } catch (...) {
            return Value(0.0);
        }
    }

    Value parse_array() {
        if (get() != '[') return Value::array();
        Value arr = Value::array();
        while (pos < src.size()) {
            if (peek() == ']') {
                get();
                return arr;
            }
            arr.push_back(parse_value());
            char c = peek();
            if (c == ',') {
                get();
            } else if (c == ']') {
                get();
                return arr;
            } else {
                break;
            }
        }
        return arr;
    }

    Value parse_object() {
        if (get() != '{') return Value::object();
        Value obj = Value::object();
        while (pos < src.size()) {
            if (peek() == '}') {
                get();
                return obj;
            }
            Value key = parse_value();
            if (!key.is_string()) break;
            if (peek() != ':') break;
            get(); // skip ':'
            Value val = parse_value();
            obj[key.as_string()] = val;
            char c = peek();
            if (c == ',') {
                get();
            } else if (c == '}') {
                get();
                return obj;
            } else {
                break;
            }
        }
        return obj;
    }
};

inline Value parse(const std::string& str) {
    Parser p(str);
    return p.parse();
}

} // namespace json
