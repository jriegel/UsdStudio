// (c)2024 Juergen Riegel, mail@juergen-riegel.net
#pragma once

#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

// Small hand-written JSON value + writer for the CLI front-end.
//
// Object key order is the order in which keys were set, so output is
// deterministic and easy to assert on in tests (and to diff for an LLM).
// Deliberately independent of pxr::js: no extra link requirements in the
// Qt-free App layer and full control over compact vs. pretty layout.
namespace UsdStudio::Cli
{

class Json
{
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    using Pair = std::pair<std::string, Json>;

    Json() = default; // null

    Json(bool value) : type_(Type::Bool), bool_(value) {}

    template <typename Number, std::enable_if_t<std::is_arithmetic_v<Number> &&
        !std::is_same_v<std::decay_t<Number>, bool>, int> = 0>
    Json(Number value) : type_(Type::Number)
    {
        if constexpr (std::is_integral_v<std::decay_t<Number>>) { int_ = static_cast<long long>(value); }
        else { isInt_ = false; number_ = static_cast<double>(value); }
    }

    Json(std::string value) : type_(Type::String), string_(std::move(value)) {}

    Json(const char* value) : type_(Type::String), string_(value) {}

    Json(std::string_view value) : type_(Type::String), string_(std::string(value)) {}

    Json(std::vector<Json> values) : type_(Type::Array), items_(std::move(values)) {}

    Json(std::initializer_list<Pair> members) : type_(Type::Object), members_(members) {}

    // An empty array/object still serialises as "[]" resp. "{}"; a default
    // constructed Json is null, which is rarely what a caller wants.
    static Json array() { return Json(std::vector<Json>()); }
    static Json object() { Json j; j.type_ = Type::Object; return j; }

    // Objects -------------------------------------------------------------

    // Sets a key, keeping its original position. Values stay present even when
    // null so every response has one stable shape.
    void set(std::string key, Json value)
    {
        type_ = Type::Object;
        for (auto& member : members_)
        {
            if (member.first == key) { member.second = std::move(value); return; }
        }
        members_.emplace_back(std::move(key), std::move(value));
    }

    void setString(std::string key, std::string_view value) { set(std::move(key), Json(std::string(value))); }

    // Arrays --------------------------------------------------------------

    void pushBack(Json value)
    {
        type_ = Type::Array;
        items_.push_back(std::move(value));
    }

    bool isNull() const { return type_ == Type::Null; }
    bool isArray() const { return type_ == Type::Array; }
    bool isObject() const { return type_ == Type::Object; }
    bool isString() const { return type_ == Type::String; }
    std::size_t size() const { return type_ == Type::Array ? items_.size() : members_.size(); }

    Type type() const { return type_; }

    // Empty view for non-strings, so callers can print any value without a
    // type check.
    std::string_view asString() const { return type_ == Type::String ? std::string_view(string_) : std::string_view(); }

    // Direct access to the children. Used by the CLI to apply '--limit' and
    // '--fields' to query results before they are serialized.
    std::vector<Json>& items() { return items_; }
    const std::vector<Json>& items() const { return items_; }
    std::vector<Pair>& members() { return members_; }
    const std::vector<Pair>& members() const { return members_; }

    // Serialization -------------------------------------------------------

    std::string dump(bool compact = true) const
    {
        std::string out;
        write(out, compact, 0);
        return out;
    }

private:
    void write(std::string& out, bool compact, unsigned depth) const
    {
        switch (type_)
        {
        case Type::Null:   out += "null"; break;
        case Type::Bool:   out += bool_ ? "true" : "false"; break;
        case Type::Number: writeNumber(out); break;
        case Type::String: writeString(out, string_); break;
        case Type::Array:
            if (items_.empty()) { out += "[]"; break; }
            out += '[';
            for (std::size_t i = 0; i < items_.size(); ++i)
            {
                if (i) out += ',';
                writeNewline(out, compact, depth + 1);
                items_[i].write(out, compact, depth + 1);
            }
            writeNewline(out, compact, depth);
            out += ']';
            break;
        case Type::Object:
            if (members_.empty()) { out += "{}"; break; }
            out += '{';
            for (std::size_t i = 0; i < members_.size(); ++i)
            {
                if (i) out += ',';
                writeNewline(out, compact, depth + 1);
                writeString(out, members_[i].first);
                out += ':';
                if (!compact) out += ' ';
                members_[i].second.write(out, compact, depth + 1);
            }
            writeNewline(out, compact, depth);
            out += '}';
            break;
        }
    }

    void writeNumber(std::string& out) const
    {
        if (isInt_) { out += std::to_string(int_); return; }
        char buffer[32];
        const int written = std::snprintf(buffer, sizeof(buffer), "%.6g", number_);
        if (written > 0) out.append(buffer, static_cast<std::size_t>(written));
    }

    static void writeNewline(std::string& out, bool compact, unsigned depth)
    {
        if (compact) return;
        out += '\n';
        out.append(2 * depth, ' ');
    }

    static void writeString(std::string& out, std::string_view value)
    {
        out += '"';
        for (const char raw : value)
        {
            const unsigned char c = static_cast<unsigned char>(raw);
            switch (c)
            {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20)
                {
                    char buffer[8];
                    std::snprintf(buffer, sizeof(buffer), "\\u%04x", c);
                    out += buffer;
                }
                else
                {
                    out += raw; // UTF-8 bytes pass through unchanged
                }
            }
        }
        out += '"';
    }

    Type type_ = Type::Null;
    bool bool_ = false;
    bool isInt_ = true;
    long long int_ = 0;
    double number_ = 0.0;
    std::string string_;
    std::vector<Json> items_;
    std::vector<Pair> members_;
};

} // namespace UsdStudio::Cli
