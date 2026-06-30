/// @file json.hpp
/// @brief 手写 JSON 序列化与反序列化（无外部依赖）
/// @details 提供轻量级 JSON 文本生成器（JsonObj / JsonArr）、JSON 解析器（JsonParser），
///          以及 RawStore 与 JSON 互转的工具函数。用于将原始采集数据持久化为 JSON 格式
///          或从 JSON 还原。数据格式：
///          - RawStore 序列化为 {"records": [ {source, path_or_command, payload, status,
///          collected_at}, ... ]}
///          - 时间戳 collected_at 以 epoch 毫秒整数表示。

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

/// @brief 将字符串转义为 JSON 字符串字面量（含首尾引号）
/// @param s 原始字符串
/// @return 带引号的 JSON 字符串，控制字符转为 \uXXXX 或对应转义序列
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
            // 控制字符（0x00-0x1F）编码为 \uXXXX
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

/// @brief JSON 对象构建器，按插入顺序保留键值对
/// @details 值以已序列化的 JSON 字符串形式存储，支持链式 add 调用。
class JsonObj
{
public:
    /// @brief 添加一个键值对（value 为已序列化的 JSON 文本）
    /// @param key 键名
    /// @param value 已序列化的 JSON 值字符串
    /// @return 返回自身引用以支持链式调用
    JsonObj& add(std::string key, std::string value)
    {
        entries_.emplace_back(std::move(key), std::move(value));
        return *this;
    }

    /// @brief 判断对象是否无任何键值对
    /// @return 为空返回 true
    [[nodiscard]] bool empty() const { return entries_.empty(); }

    /// @brief 构建 JSON 对象文本
    /// @param pretty 是否启用美化输出（换行与缩进）
    /// @param indent 当前缩进层级（每层 4 空格）
    /// @return JSON 对象字符串，空对象返回 "{}"
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
                // 美化模式：换行并缩进到子层级
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
            // 闭合花括号前回到当前缩进层级
            s += '\n';
            s += std::string(static_cast<std::size_t>(indent) * 4, ' ');
        }
        s += '}';
        return s;
    }

private:
    std::vector<std::pair<std::string, std::string>> entries_;
};

/// @brief JSON 数组构建器，按插入顺序保留元素
/// @details 元素以已序列化的 JSON 字符串形式存储，支持链式 add 调用。
class JsonArr
{
public:
    /// @brief 添加一个已序列化的 JSON 元素
    /// @param value 已序列化的 JSON 值字符串
    /// @return 返回自身引用以支持链式调用
    JsonArr& add(std::string value)
    {
        items_.push_back(std::move(value));
        return *this;
    }

    /// @brief 判断数组是否为空
    /// @return 为空返回 true
    [[nodiscard]] bool empty() const { return items_.empty(); }

    /// @brief 返回数组元素个数
    /// @return 元素数量
    [[nodiscard]] std::size_t size() const { return items_.size(); }

    /// @brief 构建 JSON 数组文本
    /// @param pretty 是否启用美化输出（换行与缩进）
    /// @param indent 当前缩进层级（每层 4 空格）
    /// @return JSON 数组字符串，空数组返回 "[]"
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
                // 美化模式：换行并缩进到子层级
                s += '\n';
                s += std::string(static_cast<std::size_t>(indent + 1) * 4, ' ');
            }
            s += items_[i];
        }
        if(pretty)
        {
            // 闭合方括号前回到当前缩进层级
            s += '\n';
            s += std::string(static_cast<std::size_t>(indent) * 4, ' ');
        }
        s += ']';
        return s;
    }

private:
    std::vector<std::string> items_;
};

/// @brief JSON 值的运行时表示（解析后的中间结构）
/// @details 支持 Null / Bool / Num / Str / Arr / Obj 六种类型，
///          数值以字符串形式保存原始文本，按需转换为整数。
struct JsonVal
{
    /// @brief JSON 值类型枚举
    enum class Type
    {
        Null,
        Bool,
        Num,
        Str,
        Arr,
        Obj
    };

    Type type = Type::Null;                               ///< 当前值类型
    bool bool_val = false;                                ///< Bool 类型时的布尔值
    std::string str_val;                                  ///< Str 或 Num 类型时的原始文本
    std::vector<JsonVal> arr_val;                         ///< Arr 类型时的元素列表
    std::vector<std::pair<std::string, JsonVal>> obj_val; ///< Obj 类型时的键值对列表

    /// @brief 在对象中按键查找子值
    /// @param key 要查找的键名
    /// @return 找到返回指向子值的指针，非对象或未找到返回 nullptr
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

    /// @brief 尝试将值转为布尔
    /// @return Bool 类型返回对应值，否则 std::nullopt
    [[nodiscard]] std::optional<bool> as_bool() const
    {
        if(type == Type::Bool)
        {
            return bool_val;
        }
        return std::nullopt;
    }

    /// @brief 尝试将值转为有符号 64 位整数
    /// @return Num 类型且可解析为整数时返回值，否则 std::nullopt
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

    /// @brief 尝试将值转为无符号 64 位整数
    /// @return Num 类型且可解析为无符号整数时返回值，否则 std::nullopt
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

    /// @brief 尝试获取字符串值的指针
    /// @return Str 类型返回指向内部字符串的指针，否则 nullptr
    [[nodiscard]] const std::string* as_str() const
    {
        if(type == Type::Str)
        {
            return &str_val;
        }
        return nullptr;
    }
};

/// @brief 递归下降式 JSON 解析器
/// @details 从字符串视图逐字符解析，支持对象、数组、字符串、数字、布尔、null。
///          解析失败返回 SysalError，包含错误位置信息。
class JsonParser
{
public:
    /// @brief 构造解析器
    /// @param text 待解析的 JSON 文本
    explicit JsonParser(std::string_view text) : text_(text) {}

    /// @brief 解析顶层 JSON 值
    /// @return 成功返回解析结果，失败返回 SysalError
    [[nodiscard]] Expected<JsonVal, SysalError> parse()
    {
        skip_ws();
        auto val = parse_value();
        if(!val)
        {
            return make_unexpected(val.error());
        }
        skip_ws();
        // 确保值后无多余字符
        if(pos_ != text_.size())
        {
            return make_unexpected(SysalError(ErrorKind::DeserializationError,
                                              "trailing characters after JSON value"));
        }
        return *val;
    }

private:
    std::string_view text_; ///< 待解析文本
    std::size_t pos_ = 0;   ///< 当前解析位置

    /// @brief 构造包含当前位置的错误对象
    /// @param msg 错误描述
    /// @return 带位置信息的 SysalError
    [[nodiscard]] SysalError err(const char* msg) const
    {
        return SysalError(ErrorKind::DeserializationError,
                          std::string(msg) + " at position " + std::to_string(pos_));
    }

    /// @brief 跳过空白字符（空格、制表、换行、回车）
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

    /// @brief 根据首字符分发解析对应类型的 JSON 值
    /// @return 解析结果或错误
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
            // 数字以 '-' 或 0-9 开头
            if(c == '-' || (c >= '0' && c <= '9'))
            {
                return parse_number();
            }
            return make_unexpected(err("unexpected character"));
        }
    }

    /// @brief 解析字面量（true / false / null）
    /// @param lit 期望的字面量文本
    /// @param val 匹配成功时返回的 JsonVal
    /// @return 成功返回 val，失败返回错误
    [[nodiscard]] Expected<JsonVal, SysalError> parse_literal(std::string_view lit, JsonVal val)
    {
        if(pos_ + lit.size() > text_.size() || text_.substr(pos_, lit.size()) != lit)
        {
            return make_unexpected(err("invalid literal"));
        }
        pos_ += lit.size();
        return val;
    }

    /// @brief 解析 JSON 字符串（含转义序列处理与 UTF-8 编码）
    /// @return 成功返回 Str 类型 JsonVal，失败返回错误
    [[nodiscard]] Expected<JsonVal, SysalError> parse_string()
    {
        assert(text_[pos_] == '"');
        ++pos_; // 跳过开头的引号
        JsonVal val;
        val.type = JsonVal::Type::Str;
        while(pos_ < text_.size())
        {
            char c = text_[pos_];
            if(c == '"')
            {
                ++pos_; // 跳过结尾的引号
                return val;
            }
            if(c == '\\')
            {
                ++pos_; // 跳过反斜杠
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
                    // \uXXXX：解析 4 位十六进制码点并编码为 UTF-8
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
                        // 单字节 UTF-8
                        val.str_val += static_cast<char>(code);
                    }
                    else if(code < 0x800)
                    {
                        // 双字节 UTF-8
                        val.str_val += static_cast<char>(0xC0 | (code >> 6));
                        val.str_val += static_cast<char>(0x80 | (code & 0x3F));
                    }
                    else
                    {
                        // 三字节 UTF-8（BMP 范围内）
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
                // 普通字符直接追加
                val.str_val += c;
                ++pos_;
            }
        }
        return make_unexpected(err("unterminated string"));
    }

    /// @brief 解析 JSON 数字（整数、小数、科学计数法）
    /// @return 成功返回 Num 类型 JsonVal（str_val 保存原始文本），失败返回错误
    [[nodiscard]] Expected<JsonVal, SysalError> parse_number()
    {
        std::size_t start = pos_;
        // 可选负号
        if(pos_ < text_.size() && text_[pos_] == '-')
        {
            ++pos_;
        }
        // 整数部分
        while(pos_ < text_.size() && text_[pos_] >= '0' && text_[pos_] <= '9')
        {
            ++pos_;
        }
        // 小数部分
        if(pos_ < text_.size() && text_[pos_] == '.')
        {
            ++pos_;
            while(pos_ < text_.size() && text_[pos_] >= '0' && text_[pos_] <= '9')
            {
                ++pos_;
            }
        }
        // 指数部分
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
        // 保留原始数字文本，按需再转换
        val.str_val = std::string(text_.substr(start, pos_ - start));
        return val;
    }

    /// @brief 解析 JSON 数组
    /// @return 成功返回 Arr 类型 JsonVal，失败返回错误
    [[nodiscard]] Expected<JsonVal, SysalError> parse_array()
    {
        assert(text_[pos_] == '[');
        ++pos_; // 跳过 '['
        JsonVal val;
        val.type = JsonVal::Type::Arr;
        skip_ws();
        // 空数组直接返回
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

    /// @brief 解析 JSON 对象
    /// @return 成功返回 Obj 类型 JsonVal，失败返回错误
    [[nodiscard]] Expected<JsonVal, SysalError> parse_object()
    {
        assert(text_[pos_] == '{');
        ++pos_; // 跳过 '{'
        JsonVal val;
        val.type = JsonVal::Type::Obj;
        skip_ws();
        // 空对象直接返回
        if(pos_ < text_.size() && text_[pos_] == '}')
        {
            ++pos_;
            return val;
        }
        while(true)
        {
            skip_ws();
            // 键必须是字符串
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
            // 键后必须有冒号
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

/// @brief 解析 JSON 文本为 JsonVal
/// @param text JSON 文本
/// @return 成功返回 JsonVal，失败返回 SysalError
inline Expected<JsonVal, SysalError> parse_json(std::string_view text)
{
    JsonParser parser(text);
    return parser.parse();
}

/// @brief 将时间点转换为 epoch 毫秒整数字符串
/// @param tp 系统时钟时间点
/// @return 毫秒数字符串
inline std::string time_point_to_ms(std::chrono::system_clock::time_point tp)
{
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()).count();
    return std::to_string(ms);
}

/// @brief 将 epoch 毫秒数转换为系统时钟时间点
/// @param ms epoch 毫秒数
/// @return 对应的 time_point
inline std::chrono::system_clock::time_point ms_to_time_point(long long ms)
{
    return std::chrono::system_clock::time_point(std::chrono::milliseconds(ms));
}

/// @brief 将 RawStore 序列化为 JSON 文本
/// @details 输出格式：{"records": [ {"source": int, "path_or_command": str, "payload": str,
/// "status": int, "collected_at": ms}, ... ]}
/// @param raw 原始数据存储
/// @param pretty 是否启用美化输出
/// @return JSON 字符串
inline std::string raw_store_to_json(const RawStore& raw, bool pretty)
{
    JsonArr records;
    for(const auto& rec : raw.records)
    {
        JsonObj o;
        // source / status 以整数编码存储
        o.add("source", std::to_string(static_cast<int>(rec.source)));
        o.add("path_or_command", escape_string(rec.path_or_command));
        o.add("payload", escape_string(rec.payload));
        o.add("status", std::to_string(static_cast<int>(rec.status)));
        // collected_at 以 epoch 毫秒整数存储
        o.add("collected_at", time_point_to_ms(rec.collected_at));
        records.add(o.build(pretty, 1));
    }
    JsonObj root;
    root.add("records", records.build(pretty, 0));
    return root.build(pretty, 0);
}

/// @brief 从 JsonVal 反序列化为 RawStore
/// @details 期望输入为包含 "records" 数组的对象，每个元素含 source、path_or_command、
///          payload、status、collected_at 字段。字段缺失或类型不符返回错误。
/// @param root 已解析的 JSON 根值
/// @return 成功返回 RawStore，失败返回 SysalError
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

        // 解析 source 字段（整数枚举）
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

        // 解析 path_or_command 字段（字符串）
        const auto* path = rec.get("path_or_command");
        if(path == nullptr || path->as_str() == nullptr)
        {
            return make_unexpected(SysalError(ErrorKind::DeserializationError,
                                              "missing or invalid 'path_or_command' field"));
        }
        record.path_or_command = *path->as_str();

        // 解析 payload 字段（字符串）
        const auto* payload = rec.get("payload");
        if(payload == nullptr || payload->as_str() == nullptr)
        {
            return make_unexpected(
                SysalError(ErrorKind::DeserializationError, "missing or invalid 'payload' field"));
        }
        record.payload = *payload->as_str();

        // 解析 status 字段（整数枚举）
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

        // 解析 collected_at 字段（epoch 毫秒整数）
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
