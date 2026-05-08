#pragma once
#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <stdexcept>
#include <cctype>
#include <cmath>
#include <limits>

struct JsonValue {
    enum class Type { Null, Bool, Number, String, Array, Object } type = Type::Null;

    bool        b = false;
    double      n = 0.0;
    std::string s;
    std::vector<JsonValue>           arr;
    std::map<std::string, JsonValue> obj;

    JsonValue() = default;
    JsonValue(bool v)               : type(Type::Bool),   b(v) {}
    JsonValue(int v)                : type(Type::Number), n((double)v) {}
    JsonValue(double v)             : type(Type::Number), n(v) {}
    JsonValue(const std::string& v) : type(Type::String), s(v) {}
    JsonValue(const char* v)        : type(Type::String), s(v) {}

    static JsonValue makeArray()  { JsonValue v; v.type = Type::Array;  return v; }
    static JsonValue makeObject() { JsonValue v; v.type = Type::Object; return v; }

    bool isNull()   const { return type == Type::Null;   }
    bool isBool()   const { return type == Type::Bool;   }
    bool isNumber() const { return type == Type::Number; }
    bool isString() const { return type == Type::String; }
    bool isArray()  const { return type == Type::Array;  }
    bool isObject() const { return type == Type::Object; }

    bool               asBool()   const { return b; }
    double             asDouble() const { return n; }
    int                asInt()    const { return (int)n; }
    const std::string& asString() const { return s; }

    JsonValue& operator[](const std::string& key) { return obj[key]; }
    const JsonValue& operator[](const std::string& key) const { return obj.at(key); }
    bool contains(const std::string& key) const {
        return isObject() && obj.count(key) > 0;
    }

    JsonValue& operator[](size_t i) { return arr[i]; }
    const JsonValue& operator[](size_t i) const { return arr[i]; }
    size_t size() const {
        if (isArray())  return arr.size();
        if (isObject()) return obj.size();
        return 0;
    }
    void push(JsonValue v) { arr.push_back(std::move(v)); }

    std::string dump(int indent = 2, int depth = 0) const {
        switch (type) {
        case Type::Null:   return "null";
        case Type::Bool:   return b ? "true" : "false";
        case Type::Number: {
            if (n == std::floor(n) && std::abs(n) < 1e15) {
                std::ostringstream o; o << (long long)n; return o.str();
            }
            std::ostringstream o;
            o << std::setprecision(std::numeric_limits<double>::digits10) << n;
            return o.str();
        }
        case Type::String: {
            std::string r = "\"";
            for (unsigned char c : s) {
                switch (c) {
                case '"':  r += "\\\""; break;
                case '\\': r += "\\\\"; break;
                case '\n': r += "\\n";  break;
                case '\r': r += "\\r";  break;
                case '\t': r += "\\t";  break;
                default:   r += (char)c; break;
                }
            }
            return r + "\"";
        }
        case Type::Array: {
            if (arr.empty()) return "[]";
            std::string p0(static_cast<size_t>(depth       * indent), ' ');
            std::string p1(static_cast<size_t>((depth + 1) * indent), ' ');
            std::string r = "[\n";
            for (size_t i = 0; i < arr.size(); ++i) {
                r += p1 + arr[i].dump(indent, depth + 1);
                if (i + 1 < arr.size()) r += ",";
                r += "\n";
            }
            return r + p0 + "]";
        }
        case Type::Object: {
            if (obj.empty()) return "{}";
            std::string p0(static_cast<size_t>(depth       * indent), ' ');
            std::string p1(static_cast<size_t>((depth + 1) * indent), ' ');
            std::string r = "{\n";
            size_t i = 0;
            for (const auto& kv : obj) {
                r += p1 + "\"" + kv.first + "\": " + kv.second.dump(indent, depth + 1);
                if (++i < obj.size()) r += ",";
                r += "\n";
            }
            return r + p0 + "}";
        }
        }
        return "null";
    }

    static JsonValue parse(const std::string& text) {
        size_t p = 0;
        return pVal(text, p);
    }

    static JsonValue loadFile(const std::string& path) {
        std::ifstream f(path);
        if (!f.is_open()) return makeObject();
        std::string txt((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());
        return parse(txt);
    }

    void saveFile(const std::string& path, int indent = 2) const {
        std::ofstream f(path);
        if (!f) throw std::runtime_error("Cannot write: " + path);
        f << dump(indent) << "\n";
    }

private:
    static void ws(const std::string& s, size_t& p) {
        while (p < s.size() && (unsigned char)s[p] <= ' ') ++p;
    }

    static JsonValue pVal(const std::string& s, size_t& p) {
        ws(s, p);
        if (p >= s.size()) throw std::runtime_error("Unexpected EOF");
        char c = s[p];
        if (c == 'n') { p += 4; return {}; }
        if (c == 't') { p += 4; return JsonValue(true); }
        if (c == 'f') { p += 5; return JsonValue(false); }
        if (c == '"') return pStr(s, p);
        if (c == '[') return pArr(s, p);
        if (c == '{') return pObj(s, p);
        if (c == '-' || isdigit((unsigned char)c)) return pNum(s, p);
        throw std::runtime_error(std::string("Unexpected char: ") + c);
    }

    static JsonValue pNum(const std::string& s, size_t& p) {
        size_t start = p;
        if (s[p] == '-') ++p;
        while (p < s.size() && isdigit((unsigned char)s[p])) ++p;
        if (p < s.size() && s[p] == '.') {
            ++p;
            while (p < s.size() && isdigit((unsigned char)s[p])) ++p;
        }
        if (p < s.size() && (s[p] == 'e' || s[p] == 'E')) {
            ++p;
            if (p < s.size() && (s[p] == '+' || s[p] == '-')) ++p;
            while (p < s.size() && isdigit((unsigned char)s[p])) ++p;
        }
        return JsonValue(std::stod(s.substr(start, p - start)));
    }

    static std::string pRaw(const std::string& s, size_t& p) {
        ++p; // skip opening "
        std::string r;
        while (p < s.size() && s[p] != '"') {
            if (s[p] == '\\') {
                ++p;
                switch (s[p]) {
                case '"':  r += '"';  break;
                case '\\': r += '\\'; break;
                case '/':  r += '/';  break;
                case 'n':  r += '\n'; break;
                case 'r':  r += '\r'; break;
                case 't':  r += '\t'; break;
                default:   r += s[p]; break;
                }
            } else {
                r += s[p];
            }
            ++p;
        }
        if (p >= s.size()) throw std::runtime_error("Unterminated string");
        ++p; // skip closing "
        return r;
    }

    static JsonValue pStr(const std::string& s, size_t& p) {
        return JsonValue(pRaw(s, p));
    }

    static JsonValue pArr(const std::string& s, size_t& p) {
        ++p; // skip [
        auto a = makeArray();
        ws(s, p);
        if (p < s.size() && s[p] == ']') { ++p; return a; }
        while (true) {
            a.arr.push_back(pVal(s, p));
            ws(s, p);
            if (p >= s.size()) throw std::runtime_error("Unterminated array");
            char c = s[p++];
            if (c == ']') return a;
            if (c != ',') throw std::runtime_error("Expected ',' or ']' in array");
        }
    }

    static JsonValue pObj(const std::string& s, size_t& p) {
        ++p; // skip {
        auto o = makeObject();
        ws(s, p);
        if (p < s.size() && s[p] == '}') { ++p; return o; }
        while (true) {
            ws(s, p);
            if (p >= s.size() || s[p] != '"') throw std::runtime_error("Expected key string");
            std::string key = pRaw(s, p);
            ws(s, p);
            if (p >= s.size() || s[p] != ':') throw std::runtime_error("Expected ':'");
            ++p;
            o.obj[key] = pVal(s, p);
            ws(s, p);
            if (p >= s.size()) throw std::runtime_error("Unterminated object");
            char c = s[p++];
            if (c == '}') return o;
            if (c != ',') throw std::runtime_error("Expected ',' or '}' in object");
        }
    }
};
