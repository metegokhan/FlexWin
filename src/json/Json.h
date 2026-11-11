#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace flexwin::json {

enum class Type {
    Null,
    Bool,
    Number,
    String,
    Array,
    Object
};

class Value {
public:
    using Array = std::vector<Value>;
    using Object = std::vector<std::pair<std::string, Value>>;

    Value() : m_type(Type::Null) {}
    Value(std::nullptr_t) : m_type(Type::Null) {}
    Value(bool b) : m_type(Type::Bool), m_bool(b) {}
    Value(int i) : m_type(Type::Number), m_number(static_cast<double>(i)) {}
    Value(int64_t i) : m_type(Type::Number), m_number(static_cast<double>(i)) {}
    Value(double d) : m_type(Type::Number), m_number(d) {}
    Value(const char* s) : m_type(Type::String), m_string(s ? s : "") {}
    Value(std::string s) : m_type(Type::String), m_string(std::move(s)) {}
    Value(Array arr) : m_type(Type::Array), m_array(std::move(arr)) {}
    Value(Object obj) : m_type(Type::Object), m_object(std::move(obj)) {}

    static Value MakeArray() { return Value(Array{}); }
    static Value MakeObject() { return Value(Object{}); }

    Type GetType() const noexcept { return m_type; }
    bool IsNull() const noexcept { return m_type == Type::Null; }
    bool IsBool() const noexcept { return m_type == Type::Bool; }
    bool IsNumber() const noexcept { return m_type == Type::Number; }
    bool IsString() const noexcept { return m_type == Type::String; }
    bool IsArray() const noexcept { return m_type == Type::Array; }
    bool IsObject() const noexcept { return m_type == Type::Object; }

    bool AsBool(bool def = false) const noexcept { return m_type == Type::Bool ? m_bool : def; }
    double AsDouble(double def = 0.0) const noexcept { return m_type == Type::Number ? m_number : def; }
    int AsInt(int def = 0) const noexcept { return m_type == Type::Number ? static_cast<int>(m_number) : def; }
    int64_t AsInt64(int64_t def = 0) const noexcept { return m_type == Type::Number ? static_cast<int64_t>(m_number) : def; }
    const std::string& AsString() const noexcept { return m_string; }
    std::string AsStringOr(const std::string& def) const { return m_type == Type::String ? m_string : def; }

    const Array& AsArray() const noexcept { return m_array; }
    Array& AsArray() noexcept { return m_array; }
    const Object& AsObject() const noexcept { return m_object; }
    Object& AsObject() noexcept { return m_object; }

    void PushBack(Value v) {
        if (m_type != Type::Array) {
            m_type = Type::Array;
            m_array.clear();
        }
        m_array.push_back(std::move(v));
    }

    void Set(const std::string& key, Value v) {
        if (m_type != Type::Object) {
            m_type = Type::Object;
            m_object.clear();
        }
        for (auto& kv : m_object) {
            if (kv.first == key) {
                kv.second = std::move(v);
                return;
            }
        }
        m_object.emplace_back(key, std::move(v));
    }

    const Value* Find(const std::string& key) const {
        if (m_type != Type::Object) return nullptr;
        for (const auto& kv : m_object) {
            if (kv.first == key) return &kv.second;
        }
        return nullptr;
    }

    bool GetBool(const std::string& key, bool def = false) const {
        const Value* v = Find(key);
        return v ? v->AsBool(def) : def;
    }

    int GetInt(const std::string& key, int def = 0) const {
        const Value* v = Find(key);
        return v ? v->AsInt(def) : def;
    }

    double GetDouble(const std::string& key, double def = 0.0) const {
        const Value* v = Find(key);
        return v ? v->AsDouble(def) : def;
    }

    std::string GetString(const std::string& key, const std::string& def = {}) const {
        const Value* v = Find(key);
        return v ? v->AsStringOr(def) : def;
    }

    std::string Dump(int indent = 2) const;

private:
    Type m_type = Type::Null;
    bool m_bool = false;
    double m_number = 0.0;
    std::string m_string;
    Array m_array;
    Object m_object;
};

bool Parse(const std::string& text, Value& outValue, std::string* outError = nullptr);

} // namespace flexwin::json
