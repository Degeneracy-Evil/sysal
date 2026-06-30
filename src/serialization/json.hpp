/// @file json.hpp
/// @brief 手写 JSON 序列化与反序列化（无外部依赖）
/// @details 提供轻量级 JSON 文本生成器（JsonObj / JsonArr）、JSON 解析器（JsonParser）、
///          以及 JsonVal 到 JSON 文本的发射函数 dump_json。用于将数据持久化为 JSON 格式
///          或从 JSON 还原。时间工具函数 time_point_to_ms / ms_to_time_point 用于
///          时间戳与 epoch 毫秒的互转。

#pragma once

#include <algorithm>
#include <cassert>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sysal::detail
{

/// @brief JSON 解析/发射错误
/// @details 继承 std::exception，携带错误描述与位置信息，用于替代 SysalError/Expected。
class JsonError : public std::exception
{
public:
    /// @brief 构造 JsonError
    /// @param msg 错误描述
    explicit JsonError(std::string msg) : msg_(std::move(msg)) {}

    /// @brief 返回错误描述
    /// @return C 风格错误字符串
    [[nodiscard]] const char* what() const noexcept override { return msg_.c_str(); }

private:
    std::string msg_;
};

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

/// @brief 将 JsonVal 发射为 JSON 文本
/// @param v 要发射的 JSON 值
/// @param pretty 是否启用美化输出（换行与缩进）
/// @param indent 当前缩进层级（每层 4 空格）
/// @return JSON 文本字符串
inline std::string dump_json(const JsonVal& v, bool pretty = false, int indent = 0)
{
    switch(v.type)
    {
    case JsonVal::Type::Null:
        return "null";
    case JsonVal::Type::Bool:
        return v.bool_val ? "true" : "false";
    case JsonVal::Type::Num:
        return v.str_val;
    case JsonVal::Type::Str:
        return escape_string(v.str_val);
    case JsonVal::Type::Arr:
    {
        JsonArr arr;
        for(const auto& elem : v.arr_val)
        {
            arr.add(dump_json(elem, pretty, indent + 1));
        }
        return arr.build(pretty, indent);
    }
    case JsonVal::Type::Obj:
    {
        JsonObj obj;
        for(const auto& [k, val] : v.obj_val)
        {
            obj.add(k, dump_json(val, pretty, indent + 1));
        }
        return obj.build(pretty, indent);
    }
    }
    // 所有分支已覆盖，此处不可达
    return "";
}

/// @brief 递归下降式 JSON 解析器
/// @details 从字符串视图逐字符解析，支持对象、数组、字符串、数字、布尔、null。
///          解析失败抛出 JsonError，包含错误位置信息。
class JsonParser
{
public:
    /// @brief 构造解析器
    /// @param text 待解析的 JSON 文本
    explicit JsonParser(std::string_view text) : text_(text) {}

    /// @brief 解析顶层 JSON 值
    /// @return 解析结果
    /// @throws JsonError 解析失败时抛出
    [[nodiscard]] JsonVal parse()
    {
        skip_ws();
        auto val = parse_value();
        skip_ws();
        // 确保值后无多余字符
        if(pos_ != text_.size())
        {
            throw JsonError("trailing characters after JSON value at position " +
                            std::to_string(pos_));
        }
        return val;
    }

private:
    std::string_view text_; ///< 待解析文本
    std::size_t pos_ = 0;   ///< 当前解析位置

    /// @brief 构造包含当前位置的错误消息
    /// @param msg 错误描述
    /// @return 带位置信息的错误消息字符串
    [[nodiscard]] std::string err(const char* msg) const
    {
        return std::string(msg) + " at position " + std::to_string(pos_);
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
    /// @return 解析结果
    /// @throws JsonError 解析失败时抛出
    [[nodiscard]] JsonVal parse_value()
    {
        skip_ws();
        if(pos_ >= text_.size())
        {
            throw JsonError(err("unexpected end of input"));
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
            throw JsonError(err("unexpected character"));
        }
    }

    /// @brief 解析字面量（true / false / null）
    /// @param lit 期望的字面量文本
    /// @param val 匹配成功时返回的 JsonVal
    /// @return 匹配成功返回 val
    /// @throws JsonError 匹配失败时抛出
    [[nodiscard]] JsonVal parse_literal(std::string_view lit, JsonVal val)
    {
        if(pos_ + lit.size() > text_.size() || text_.substr(pos_, lit.size()) != lit)
        {
            throw JsonError(err("invalid literal"));
        }
        pos_ += lit.size();
        return val;
    }

    /// @brief 解析 JSON 字符串（含转义序列处理与 UTF-8 编码）
    /// @return Str 类型 JsonVal
    /// @throws JsonError 解析失败时抛出
    [[nodiscard]] JsonVal parse_string()
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
                    throw JsonError(err("unterminated escape"));
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
                        throw JsonError(err("invalid unicode escape"));
                    }
                    auto hex = text_.substr(pos_ + 1, 4);
                    unsigned int code = 0;
                    auto r = std::from_chars(hex.data(), hex.data() + hex.size(), code, 16);
                    if(r.ec != std::errc{})
                    {
                        throw JsonError(err("invalid unicode escape"));
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
                    throw JsonError(err("invalid escape character"));
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
        throw JsonError(err("unterminated string"));
    }

    /// @brief 解析 JSON 数字（整数、小数、科学计数法）
    /// @return Num 类型 JsonVal（str_val 保存原始文本）
    /// @throws JsonError 解析失败时抛出
    [[nodiscard]] JsonVal parse_number()
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
            throw JsonError(err("invalid number"));
        }
        JsonVal val;
        val.type = JsonVal::Type::Num;
        // 保留原始数字文本，按需再转换
        val.str_val = std::string(text_.substr(start, pos_ - start));
        return val;
    }

    /// @brief 解析 JSON 数组
    /// @return Arr 类型 JsonVal
    /// @throws JsonError 解析失败时抛出
    [[nodiscard]] JsonVal parse_array()
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
            val.arr_val.push_back(std::move(elem));
            skip_ws();
            if(pos_ >= text_.size())
            {
                throw JsonError(err("unterminated array"));
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
            throw JsonError(err("expected ',' or ']' in array"));
        }
    }

    /// @brief 解析 JSON 对象
    /// @return Obj 类型 JsonVal
    /// @throws JsonError 解析失败时抛出
    [[nodiscard]] JsonVal parse_object()
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
                throw JsonError(err("expected string key in object"));
            }
            auto key = parse_string();
            skip_ws();
            // 键后必须有冒号
            if(pos_ >= text_.size() || text_[pos_] != ':')
            {
                throw JsonError(err("expected ':' after key"));
            }
            ++pos_;
            auto v = parse_value();
            val.obj_val.emplace_back(std::move(key.str_val), std::move(v));
            skip_ws();
            if(pos_ >= text_.size())
            {
                throw JsonError(err("unterminated object"));
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
            throw JsonError(err("expected ',' or '}' in object"));
        }
    }
};

/// @brief 解析 JSON 文本为 JsonVal
/// @param text JSON 文本
/// @return 解析结果
/// @throws JsonError 解析失败时抛出
inline JsonVal parse_json(std::string_view text)
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

} // namespace sysal::detail
