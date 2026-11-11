#include "Json.h"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <sstream>

namespace flexwin::json {

namespace {

void AppendUtf8(std::string& out, uint32_t cp) {
    if (cp <= 0x7Fu) {
        out.push_back(static_cast<char>(cp));
    } else if (cp <= 0x7FFu) {
        out.push_back(static_cast<char>(0xC0u | (cp >> 6)));
        out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
    } else if (cp <= 0xFFFFu) {
        out.push_back(static_cast<char>(0xE0u | (cp >> 12)));
        out.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
    } else if (cp <= 0x10FFFFu) {
        out.push_back(static_cast<char>(0xF0u | (cp >> 18)));
        out.push_back(static_cast<char>(0x80u | ((cp >> 12) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
    }
}

void WriteEscapedString(std::string& out, const std::string& s) {
    out.push_back('"');
    for (unsigned char c : s) {
        switch (c) {
        case '"':  out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\b': out += "\\b"; break;
        case '\f': out += "\\f"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (c < 0x20) {
                char buf[8]{};
                std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned int>(c));
                out += buf;
            } else {
                out.push_back(static_cast<char>(c));
            }
            break;
        }
    }
    out.push_back('"');
}

void DumpImpl(const Value& v, std::string& out, int indent, int level) {
    switch (v.GetType()) {
    case Type::Null:
        out += "null";
        break;
    case Type::Bool:
        out += v.AsBool() ? "true" : "false";
        break;
    case Type::Number: {
        double d = v.AsDouble();
        if (!std::isfinite(d)) {
            out += "null";
        } else if (std::floor(d) == d && d >= -9007199254740991.0 && d <= 9007199254740991.0) {
            char buf[64]{};
            std::snprintf(buf, sizeof(buf), "%.0f", d);
            out += buf;
        } else {
            std::ostringstream oss;
            oss << d;
            out += oss.str();
        }
        break;
    }
    case Type::String:
        WriteEscapedString(out, v.AsString());
        break;
    case Type::Array: {
        const auto& arr = v.AsArray();
        if (arr.empty()) {
            out += "[]";
            break;
        }
        out.push_back('[');
        for (size_t i = 0; i < arr.size(); ++i) {
            if (indent > 0) {
                out.push_back('\n');
                out.append(static_cast<size_t>((level + 1) * indent), ' ');
            }
            DumpImpl(arr[i], out, indent, level + 1);
            if (i + 1 < arr.size()) {
                out.push_back(',');
            }
        }
        if (indent > 0) {
            out.push_back('\n');
            out.append(static_cast<size_t>(level * indent), ' ');
        }
        out.push_back(']');
        break;
    }
    case Type::Object: {
        const auto& obj = v.AsObject();
        if (obj.empty()) {
            out += "{}";
            break;
        }
        out.push_back('{');
        for (size_t i = 0; i < obj.size(); ++i) {
            if (indent > 0) {
                out.push_back('\n');
                out.append(static_cast<size_t>((level + 1) * indent), ' ');
            }
            WriteEscapedString(out, obj[i].first);
            out += (indent > 0) ? ": " : ":";
            DumpImpl(obj[i].second, out, indent, level + 1);
            if (i + 1 < obj.size()) {
                out.push_back(',');
            }
        }
        if (indent > 0) {
            out.push_back('\n');
            out.append(static_cast<size_t>(level * indent), ' ');
        }
        out.push_back('}');
        break;
    }
    }
}

class Parser {
public:
    explicit Parser(const std::string& s) : m_s(s) {}

    bool Run(Value& out, std::string* err) {
        // Skip optional UTF-8 BOM
        if (m_s.size() >= 3 &&
            static_cast<unsigned char>(m_s[0]) == 0xEF &&
            static_cast<unsigned char>(m_s[1]) == 0xBB &&
            static_cast<unsigned char>(m_s[2]) == 0xBF) {
            m_pos = 3;
        }
        SkipWs();
        if (!ParseValue(out, 0)) {
            if (err) *err = m_error;
            return false;
        }
        SkipWs();
        if (m_pos != m_s.size()) {
            m_error = "Trailing characters after JSON root";
            if (err) *err = m_error;
            return false;
        }
        return true;
    }

private:
    const std::string& m_s;
    size_t m_pos = 0;
    std::string m_error;

    static constexpr int kMaxDepth = 64;

    void SkipWs() {
        while (m_pos < m_s.size()) {
            unsigned char c = static_cast<unsigned char>(m_s[m_pos]);
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
                ++m_pos;
            } else {
                break;
            }
        }
    }

    bool Fail(const std::string& msg) {
        if (m_error.empty()) {
            m_error = msg + " at offset " + std::to_string(m_pos);
        }
        return false;
    }

    bool ParseValue(Value& out, int depth) {
        if (depth > kMaxDepth) return Fail("Maximum JSON nesting depth exceeded");
        SkipWs();
        if (m_pos >= m_s.size()) return Fail("Unexpected end of input");

        char c = m_s[m_pos];
        if (c == '{') return ParseObject(out, depth);
        if (c == '[') return ParseArray(out, depth);
        if (c == '"') {
            std::string str;
            if (!ParseString(str)) return false;
            out = Value(std::move(str));
            return true;
        }
        if (c == 't' && m_s.compare(m_pos, 4, "true") == 0) {
            m_pos += 4;
            out = Value(true);
            return true;
        }
        if (c == 'f' && m_s.compare(m_pos, 5, "false") == 0) {
            m_pos += 5;
            out = Value(false);
            return true;
        }
        if (c == 'n' && m_s.compare(m_pos, 4, "null") == 0) {
            m_pos += 4;
            out = Value(nullptr);
            return true;
        }
        if (c == '-' || (c >= '0' && c <= '9')) {
            return ParseNumber(out);
        }
        return Fail("Unexpected token");
    }

    bool ParseHex4(uint32_t& code) {
        if (m_pos + 4 > m_s.size()) return Fail("Incomplete unicode escape");
        code = 0;
        for (int i = 0; i < 4; ++i) {
            char c = m_s[m_pos++];
            code <<= 4;
            if (c >= '0' && c <= '9') code |= static_cast<uint32_t>(c - '0');
            else if (c >= 'a' && c <= 'f') code |= static_cast<uint32_t>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') code |= static_cast<uint32_t>(c - 'A' + 10);
            else return Fail("Invalid hex digit in unicode escape");
        }
        return true;
    }

    bool ParseString(std::string& out) {
        if (m_pos >= m_s.size() || m_s[m_pos] != '"') return Fail("Expected '\"'");
        ++m_pos;
        out.clear();
        while (m_pos < m_s.size()) {
            unsigned char c = static_cast<unsigned char>(m_s[m_pos++]);
            if (c == '"') return true;
            if (c < 0x20) return Fail("Unescaped control character in string");
            if (c == '\\') {
                if (m_pos >= m_s.size()) return Fail("Incomplete escape sequence");
                char esc = m_s[m_pos++];
                switch (esc) {
                case '"': out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                case '/': out.push_back('/'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                case 'u': {
                    uint32_t cp = 0;
                    if (!ParseHex4(cp)) return false;
                    if (cp >= 0xD800u && cp <= 0xDBFFu) {
                        if (m_pos + 2 <= m_s.size() && m_s[m_pos] == '\\' && m_s[m_pos + 1] == 'u') {
                            m_pos += 2;
                            uint32_t low = 0;
                            if (!ParseHex4(low)) return false;
                            if (low >= 0xDC00u && low <= 0xDFFFu) {
                                cp = 0x10000u + (((cp - 0xD800u) << 10) | (low - 0xDC00u));
                            } else {
                                return Fail("Invalid UTF-16 low surrogate");
                            }
                        } else {
                            return Fail("Missing UTF-16 low surrogate");
                        }
                    }
                    AppendUtf8(out, cp);
                    break;
                }
                default:
                    return Fail("Invalid escape character");
                }
            } else {
                out.push_back(static_cast<char>(c));
            }
        }
        return Fail("Unterminated string");
    }

    bool ParseNumber(Value& out) {
        size_t start = m_pos;
        if (m_s[m_pos] == '-') ++m_pos;
        if (m_pos >= m_s.size() || !std::isdigit(static_cast<unsigned char>(m_s[m_pos]))) {
            return Fail("Invalid number");
        }
        if (m_s[m_pos] == '0') {
            ++m_pos;
        } else {
            while (m_pos < m_s.size() && std::isdigit(static_cast<unsigned char>(m_s[m_pos]))) ++m_pos;
        }
        if (m_pos < m_s.size() && m_s[m_pos] == '.') {
            ++m_pos;
            if (m_pos >= m_s.size() || !std::isdigit(static_cast<unsigned char>(m_s[m_pos]))) {
                return Fail("Invalid fraction in number");
            }
            while (m_pos < m_s.size() && std::isdigit(static_cast<unsigned char>(m_s[m_pos]))) ++m_pos;
        }
        if (m_pos < m_s.size() && (m_s[m_pos] == 'e' || m_s[m_pos] == 'E')) {
            ++m_pos;
            if (m_pos < m_s.size() && (m_s[m_pos] == '+' || m_s[m_pos] == '-')) ++m_pos;
            if (m_pos >= m_s.size() || !std::isdigit(static_cast<unsigned char>(m_s[m_pos]))) {
                return Fail("Invalid exponent in number");
            }
            while (m_pos < m_s.size() && std::isdigit(static_cast<unsigned char>(m_s[m_pos]))) ++m_pos;
        }

        std::string sub = m_s.substr(start, m_pos - start);
        char* endPtr = nullptr;
        double d = std::strtod(sub.c_str(), &endPtr);
        if (endPtr != sub.c_str() + sub.size() || !std::isfinite(d)) {
            return Fail("Number out of range");
        }
        out = Value(d);
        return true;
    }

    bool ParseArray(Value& out, int depth) {
        ++m_pos; // '['
        Value::Array arr;
        SkipWs();
        if (m_pos < m_s.size() && m_s[m_pos] == ']') {
            ++m_pos;
            out = Value(std::move(arr));
            return true;
        }
        while (true) {
            Value elem;
            if (!ParseValue(elem, depth + 1)) return false;
            arr.push_back(std::move(elem));
            SkipWs();
            if (m_pos >= m_s.size()) return Fail("Unterminated array");
            if (m_s[m_pos] == ']') {
                ++m_pos;
                out = Value(std::move(arr));
                return true;
            }
            if (m_s[m_pos] != ',') return Fail("Expected ',' or ']' in array");
            ++m_pos;
        }
    }

    bool ParseObject(Value& out, int depth) {
        ++m_pos; // '{'
        Value::Object obj;
        SkipWs();
        if (m_pos < m_s.size() && m_s[m_pos] == '}') {
            ++m_pos;
            out = Value(std::move(obj));
            return true;
        }
        while (true) {
            SkipWs();
            std::string key;
            if (!ParseString(key)) return false;
            SkipWs();
            if (m_pos >= m_s.size() || m_s[m_pos] != ':') return Fail("Expected ':' after key");
            ++m_pos;
            Value val;
            if (!ParseValue(val, depth + 1)) return false;

            bool updated = false;
            for (auto& kv : obj) {
                if (kv.first == key) {
                    kv.second = std::move(val);
                    updated = true;
                    break;
                }
            }
            if (!updated) {
                obj.emplace_back(std::move(key), std::move(val));
            }

            SkipWs();
            if (m_pos >= m_s.size()) return Fail("Unterminated object");
            if (m_s[m_pos] == '}') {
                ++m_pos;
                out = Value(std::move(obj));
                return true;
            }
            if (m_s[m_pos] != ',') return Fail("Expected ',' or '}' in object");
            ++m_pos;
        }
    }
};

} // namespace

std::string Value::Dump(int indent) const {
    std::string out;
    out.reserve(512);
    DumpImpl(*this, out, indent, 0);
    return out;
}

bool Parse(const std::string& text, Value& outValue, std::string* outError) {
    Parser p(text);
    return p.Run(outValue, outError);
}

} // namespace flexwin::json
