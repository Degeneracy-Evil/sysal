#pragma once

#include "sysal/enums.hpp"
#include "sysal/error.hpp"
#include "sysal/expected.hpp"
#include "sysal/raw_store.hpp"

#include <algorithm>
#include <cassert>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sysal::detail
{

inline std::string escape_string(std::string_view s)
{
    std::string out;
    out.reserve(s.size() + 2);
    out += '"';
    for(char c : s)
    {
        switch(c)
        {
        case '"':
            out += "\\\"";
            break;
        case '\\':
            out += "\\\\";
            break;
        case '\b':
            out += "\\b";
            break;
        case '\f':
            out += "\\f";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            if(static_cast<unsigned char>(c) < 0x20)
            {
                char buf[7];
                std::snprintf(buf, sizeof(buf), "\\u%04x",
                              static_cast<unsigned int>(static_cast<unsigned char>(c)));
                out += buf;
            }
            else
            {
                out += c;
            }
            break;
        }
    }
    out += '"';
    return out;
}

class JsonObj
{
public:
    JsonObj& add(std::string key, std::string value)
    {
        entries_.emplace_back(std::move(key), std::move(value));
        return *this;
    }

    [[nodiscard]] bool empty() const { return entries_.empty(); }

    [[nodiscard]] std::string build(bool pretty, int indent) const
    {
        if(entries_.empty())
        {
            return "{}";
        }
        std::string s = "{";
        for(std::size_t i = 0; i < entries_.size(); ++i)
        {
            if(i > 0)
            {
                s += ',';
            }
            if(pretty)
            {
                s += '\n';
                s += std::string(static_cast<std::size_t>(indent + 1) * 4, ' ');
            }
            s += escape_string(entries_[i].first);
            s += ':';
            if(pretty)
            {
                s += ' ';
            }
            s += entries_[i].second;
        }
        if(pretty)
        {
            s += '\n';
            s += std::string(static_cast<std::size_t>(indent) * 4, ' ');
        }
        s += '}';
        return s;
    }

private:
    std::vector<std::pair<std::string, std::string>> entries_;
};

class JsonArr
{
public:
    JsonArr& add(std::string value)
    {
        items_.push_back(std::move(value));
        return *this;
    }

    [[nodiscard]] bool empty() const { return items_.empty(); }

    [[nodiscard]] std::size_t size() const { return items_.size(); }

    [[nodiscard]] std::string build(bool pretty, int indent) const
    {
        if(items_.empty())
        {
            return "[]";
        }
        std::string s = "[";
        for(std::size_t i = 0; i < items_.size(); ++i)
        {
            if(i > 0)
            {
                s += ',';
            }
            if(pretty)
            {
                s += '\n';
                s += std::string(static_cast<std::size_t>(indent + 1) * 4, ' ');
            }
            s += items_[i];
        }
        if(pretty)
        {
            s += '\n';
            s += std::string(static_cast<std::size_t>(indent) * 4, ' ');
        }
        s += ']';
        return s;
    }

private:
    std::vector<std::string> items_;
};

struct JsonVal
{
    enum class Type
    {
        Null,
        Bool,
        Num,
        Str,
        Arr,
        Obj
    };

    Type type = Type::Null;
    bool bool_val = false;
    std::string str_val;
    std::vector<JsonVal> arr_val;
    std::vector<std::pair<std::string, JsonVal>> obj_val;

    [[nodiscard]] const JsonVal* get(std::string_view key) const
    {
        if(type != Type::Obj)
        {
            return nullptr;
        }
        for(const auto& [k, v] : obj_val)
        {
            if(k == key)
            {
                return &v;
            }
        }
        return nullptr;
    }

    [[nodiscard]] std::optional<bool> as_bool() const
    {
        if(type == Type::Bool)
        {
            return bool_val;
        }
        return std::nullopt;
    }

    [[nodiscard]] std::optional<long long> as_i64() const
    {
        if(type != Type::Num)
        {
            return std::nullopt;
        }
        long long result = 0;
        auto r = std::from_chars(str_val.data(), str_val.data() + str_val.size(), result);
        if(r.ec != std::errc{})
        {
            return std::nullopt;
        }
        return result;
    }

    [[nodiscard]] std::optional<std::uint64_t> as_u64() const
    {
        if(type != Type::Num)
        {
            return std::nullopt;
        }
        std::uint64_t result = 0;
        auto r = std::from_chars(str_val.data(), str_val.data() + str_val.size(), result);
        if(r.ec != std::errc{})
        {
            return std::nullopt;
        }
        return result;
    }

    [[nodiscard]] const std::string* as_str() const
    {
        if(type == Type::Str)
        {
            return &str_val;
        }
        return nullptr;
    }
};

class JsonParser
{
public:
    explicit JsonParser(std::string_view text) : text_(text) {}

    [[nodiscard]] Expected<JsonVal, SysalError> parse()
    {
        skip_ws();
        auto val = parse_value();
        if(!val)
        {
            return make_unexpected(val.error());
        }
        skip_ws();
        if(pos_ != text_.size())
        {
            return make_unexpected(SysalError(ErrorKind::DeserializationError,
                                              "trailing characters after JSON value"));
        }
        return *val;
    }

private:
    std::string_view text_;
    std::size_t pos_ = 0;

    [[nodiscard]] SysalError err(const char* msg) const
    {
        return SysalError(ErrorKind::DeserializationError,
                          std::string(msg) + " at position " + std::to_string(pos_));
    }

    void skip_ws()
    {
        while(pos_ < text_.size())
        {
            char c = text_[pos_];
            if(c == ' ' || c == '\t' || c == '\n' || c == '\r')
            {
                ++pos_;
            }
            else
            {
                break;
            }
        }
    }

    [[nodiscard]] Expected<JsonVal, SysalError> parse_value()
    {
        skip_ws();
        if(pos_ >= text_.size())
        {
            return make_unexpected(err("unexpected end of input"));
        }
        char c = text_[pos_];
        switch(c)
        {
        case '{':
            return parse_object();
        case '[':
            return parse_array();
        case '"':
            return parse_string();
        case 't':
        {
            JsonVal v;
            v.type = JsonVal::Type::Bool;
            v.bool_val = true;
            return parse_literal("true", v);
        }
        case 'f':
        {
            JsonVal v;
            v.type = JsonVal::Type::Bool;
            v.bool_val = false;
            return parse_literal("false", v);
        }
        case 'n':
            return parse_literal("null", JsonVal{});
        default:
            if(c == '-' || (c >= '0' && c <= '9'))
            {
                return parse_number();
            }
            return make_unexpected(err("unexpected character"));
        }
    }

    [[nodiscard]] Expected<JsonVal, SysalError> parse_literal(std::string_view lit, JsonVal val)
    {
        if(pos_ + lit.size() > text_.size() || text_.substr(pos_, lit.size()) != lit)
        {
            return make_unexpected(err("invalid literal"));
        }
        pos_ += lit.size();
        return val;
    }

    [[nodiscard]] Expected<JsonVal, SysalError> parse_string()
    {
        assert(text_[pos_] == '"');
        ++pos_;
        JsonVal val;
        val.type = JsonVal::Type::Str;
        while(pos_ < text_.size())
        {
            char c = text_[pos_];
            if(c == '"')
            {
                ++pos_;
                return val;
            }
            if(c == '\\')
            {
                ++pos_;
                if(pos_ >= text_.size())
                {
                    return make_unexpected(err("unterminated escape"));
                }
                char esc = text_[pos_];
                switch(esc)
                {
                case '"':
                    val.str_val += '"';
                    break;
                case '\\':
                    val.str_val += '\\';
                    break;
                case '/':
                    val.str_val += '/';
                    break;
                case 'b':
                    val.str_val += '\b';
                    break;
                case 'f':
                    val.str_val += '\f';
                    break;
                case 'n':
                    val.str_val += '\n';
                    break;
                case 'r':
                    val.str_val += '\r';
                    break;
                case 't':
                    val.str_val += '\t';
                    break;
                case 'u':
                {
                    if(pos_ + 4 >= text_.size())
                    {
                        return make_unexpected(err("invalid unicode escape"));
                    }
                    auto hex = text_.substr(pos_ + 1, 4);
                    unsigned int code = 0;
                    auto r = std::from_chars(hex.data(), hex.data() + hex.size(), code, 16);
                    if(r.ec != std::errc{})
                    {
                        return make_unexpected(err("invalid unicode escape"));
                    }
                    pos_ += 4;
                    if(code < 0x80)
                    {
                        val.str_val += static_cast<char>(code);
                    }
                    else if(code < 0x800)
                    {
                        val.str_val += static_cast<char>(0xC0 | (code >> 6));
                        val.str_val += static_cast<char>(0x80 | (code & 0x3F));
                    }
                    else
                    {
                        val.str_val += static_cast<char>(0xE0 | (code >> 12));
                        val.str_val += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
                        val.str_val += static_cast<char>(0x80 | (code & 0x3F));
                    }
                    break;
                }
                default:
                    return make_unexpected(err("invalid escape character"));
                }
                ++pos_;
            }
            else
            {
                val.str_val += c;
                ++pos_;
            }
        }
        return make_unexpected(err("unterminated string"));
    }

    [[nodiscard]] Expected<JsonVal, SysalError> parse_number()
    {
        std::size_t start = pos_;
        if(pos_ < text_.size() && text_[pos_] == '-')
        {
            ++pos_;
        }
        while(pos_ < text_.size() && text_[pos_] >= '0' && text_[pos_] <= '9')
        {
            ++pos_;
        }
        if(pos_ < text_.size() && text_[pos_] == '.')
        {
            ++pos_;
            while(pos_ < text_.size() && text_[pos_] >= '0' && text_[pos_] <= '9')
            {
                ++pos_;
            }
        }
        if(pos_ < text_.size() && (text_[pos_] == 'e' || text_[pos_] == 'E'))
        {
            ++pos_;
            if(pos_ < text_.size() && (text_[pos_] == '+' || text_[pos_] == '-'))
            {
                ++pos_;
            }
            while(pos_ < text_.size() && text_[pos_] >= '0' && text_[pos_] <= '9')
            {
                ++pos_;
            }
        }
        if(pos_ == start)
        {
            return make_unexpected(err("invalid number"));
        }
        JsonVal val;
        val.type = JsonVal::Type::Num;
        val.str_val = std::string(text_.substr(start, pos_ - start));
        return val;
    }

    [[nodiscard]] Expected<JsonVal, SysalError> parse_array()
    {
        assert(text_[pos_] == '[');
        ++pos_;
        JsonVal val;
        val.type = JsonVal::Type::Arr;
        skip_ws();
        if(pos_ < text_.size() && text_[pos_] == ']')
        {
            ++pos_;
            return val;
        }
        while(true)
        {
            auto elem = parse_value();
            if(!elem)
            {
                return make_unexpected(elem.error());
            }
            val.arr_val.push_back(*elem);
            skip_ws();
            if(pos_ >= text_.size())
            {
                return make_unexpected(err("unterminated array"));
            }
            if(text_[pos_] == ',')
            {
                ++pos_;
                continue;
            }
            if(text_[pos_] == ']')
            {
                ++pos_;
                return val;
            }
            return make_unexpected(err("expected ',' or ']' in array"));
        }
    }

    [[nodiscard]] Expected<JsonVal, SysalError> parse_object()
    {
        assert(text_[pos_] == '{');
        ++pos_;
        JsonVal val;
        val.type = JsonVal::Type::Obj;
        skip_ws();
        if(pos_ < text_.size() && text_[pos_] == '}')
        {
            ++pos_;
            return val;
        }
        while(true)
        {
            skip_ws();
            if(pos_ >= text_.size() || text_[pos_] != '"')
            {
                return make_unexpected(err("expected string key in object"));
            }
            auto key = parse_string();
            if(!key)
            {
                return make_unexpected(key.error());
            }
            skip_ws();
            if(pos_ >= text_.size() || text_[pos_] != ':')
            {
                return make_unexpected(err("expected ':' after key"));
            }
            ++pos_;
            auto v = parse_value();
            if(!v)
            {
                return make_unexpected(v.error());
            }
            val.obj_val.emplace_back((*key).str_val, *v);
            skip_ws();
            if(pos_ >= text_.size())
            {
                return make_unexpected(err("unterminated object"));
            }
            if(text_[pos_] == ',')
            {
                ++pos_;
                continue;
            }
            if(text_[pos_] == '}')
            {
                ++pos_;
                return val;
            }
            return make_unexpected(err("expected ',' or '}' in object"));
        }
    }
};

inline Expected<JsonVal, SysalError> parse_json(std::string_view text)
{
    JsonParser parser(text);
    return parser.parse();
}

inline std::string time_point_to_ms(std::chrono::system_clock::time_point tp)
{
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()).count();
    return std::to_string(ms);
}

inline std::chrono::system_clock::time_point ms_to_time_point(long long ms)
{
    return std::chrono::system_clock::time_point(std::chrono::milliseconds(ms));
}

inline std::string raw_store_to_json(const RawStore& raw, bool pretty)
{
    JsonArr records;
    for(const auto& rec : raw.records)
    {
        JsonObj o;
        o.add("source", std::to_string(static_cast<int>(rec.source)));
        o.add("path_or_command", escape_string(rec.path_or_command));
        o.add("payload", escape_string(rec.payload));
        o.add("status", std::to_string(static_cast<int>(rec.status)));
        o.add("collected_at", time_point_to_ms(rec.collected_at));
        records.add(o.build(pretty, 1));
    }
    JsonObj root;
    root.add("records", records.build(pretty, 0));
    return root.build(pretty, 0);
}

inline Expected<RawStore, SysalError> raw_store_from_json(const JsonVal& root)
{
    if(root.type != JsonVal::Type::Obj)
    {
        return make_unexpected(
            SysalError(ErrorKind::DeserializationError, "root is not a JSON object"));
    }
    const auto* records = root.get("records");
    if(records == nullptr || records->type != JsonVal::Type::Arr)
    {
        return make_unexpected(
            SysalError(ErrorKind::DeserializationError, "missing or invalid 'records' array"));
    }
    RawStore store;
    for(const auto& rec : records->arr_val)
    {
        if(rec.type != JsonVal::Type::Obj)
        {
            return make_unexpected(
                SysalError(ErrorKind::DeserializationError, "record is not a JSON object"));
        }
        RawRecord record;
        const auto* src = rec.get("source");
        if(src == nullptr)
        {
            return make_unexpected(
                SysalError(ErrorKind::DeserializationError, "missing 'source' field"));
        }
        auto src_val = src->as_i64();
        if(!src_val)
        {
            return make_unexpected(
                SysalError(ErrorKind::DeserializationError, "invalid 'source' field"));
        }
        record.source = static_cast<RawSource>(*src_val);

        const auto* path = rec.get("path_or_command");
        if(path == nullptr || path->as_str() == nullptr)
        {
            return make_unexpected(SysalError(ErrorKind::DeserializationError,
                                              "missing or invalid 'path_or_command' field"));
        }
        record.path_or_command = *path->as_str();

        const auto* payload = rec.get("payload");
        if(payload == nullptr || payload->as_str() == nullptr)
        {
            return make_unexpected(
                SysalError(ErrorKind::DeserializationError, "missing or invalid 'payload' field"));
        }
        record.payload = *payload->as_str();

        const auto* status = rec.get("status");
        if(status == nullptr)
        {
            return make_unexpected(
                SysalError(ErrorKind::DeserializationError, "missing 'status' field"));
        }
        auto status_val = status->as_i64();
        if(!status_val)
        {
            return make_unexpected(
                SysalError(ErrorKind::DeserializationError, "invalid 'status' field"));
        }
        record.status = static_cast<CollectStatus>(*status_val);

        const auto* collected = rec.get("collected_at");
        if(collected == nullptr)
        {
            return make_unexpected(
                SysalError(ErrorKind::DeserializationError, "missing 'collected_at' field"));
        }
        auto collected_val = collected->as_i64();
        if(!collected_val)
        {
            return make_unexpected(
                SysalError(ErrorKind::DeserializationError, "invalid 'collected_at' field"));
        }
        record.collected_at = ms_to_time_point(*collected_val);

        store.records.push_back(std::move(record));
    }
    return store;
}

} // namespace sysal::detail
