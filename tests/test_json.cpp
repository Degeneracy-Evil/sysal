/// @file test_json.cpp
/// @brief JSON 解析/发射往返测试

#include "serialization/json.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>

using namespace sysal::detail;

/// @brief 比较两个 JsonVal 是否结构相等
/// @param a 第一个值
/// @param b 第二个值
/// @return 相等返回 true
[[nodiscard]] bool json_equal(const JsonVal& a, const JsonVal& b)
{
    if(a.type != b.type)
    {
        return false;
    }
    switch(a.type)
    {
    case JsonVal::Type::Null:
        return true;
    case JsonVal::Type::Bool:
        return a.bool_val == b.bool_val;
    case JsonVal::Type::Num:
    case JsonVal::Type::Str:
        return a.str_val == b.str_val;
    case JsonVal::Type::Arr:
    {
        if(a.arr_val.size() != b.arr_val.size())
        {
            return false;
        }
        for(std::size_t i = 0; i < a.arr_val.size(); ++i)
        {
            if(!json_equal(a.arr_val[i], b.arr_val[i]))
            {
                return false;
            }
        }
        return true;
    }
    case JsonVal::Type::Obj:
    {
        if(a.obj_val.size() != b.obj_val.size())
        {
            return false;
        }
        for(std::size_t i = 0; i < a.obj_val.size(); ++i)
        {
            if(a.obj_val[i].first != b.obj_val[i].first ||
               !json_equal(a.obj_val[i].second, b.obj_val[i].second))
            {
                return false;
            }
        }
        return true;
    }
    }
    return false;
}

/// @brief 测试基本类型解析
void test_basic_types()
{
    auto n = parse_json("null");
    assert(n.type == JsonVal::Type::Null);

    auto t = parse_json("true");
    assert(t.type == JsonVal::Type::Bool && t.bool_val == true);

    auto f = parse_json("false");
    assert(f.type == JsonVal::Type::Bool && f.bool_val == false);

    auto num = parse_json("42");
    assert(num.type == JsonVal::Type::Num && num.str_val == "42");
    assert(num.as_i64() && *num.as_i64() == 42);

    auto neg = parse_json("-7");
    assert(neg.type == JsonVal::Type::Num && neg.str_val == "-7");
    assert(neg.as_i64() && *neg.as_i64() == -7);

    auto fp = parse_json("3.14");
    assert(fp.type == JsonVal::Type::Num && fp.str_val == "3.14");

    auto sci = parse_json("1e10");
    assert(sci.type == JsonVal::Type::Num && sci.str_val == "1e10");

    auto str = parse_json("\"hello\"");
    assert(str.type == JsonVal::Type::Str && str.str_val == "hello");

    std::cout << "  basic types: OK\n";
}

/// @brief 测试字符串转义
void test_string_escapes()
{
    auto q = parse_json("\"quote\\\"inside\"");
    assert(q.str_val == "quote\"inside");

    auto bs = parse_json("\"back\\\\slash\"");
    assert(bs.str_val == "back\\slash");

    auto ctrl = parse_json("\"line1\\nline2\\ttab\"");
    assert(ctrl.str_val == "line1\nline2\ttab");

    auto uni = parse_json("\"\\u0041\"");
    assert(uni.str_val == "A");

    auto cn = parse_json("\"\\u4e2d\"");
    assert(cn.str_val.size() == 3); // 三字节 UTF-8

    std::cout << "  string escapes: OK\n";
}

/// @brief 测试数组和对象解析
void test_containers()
{
    auto arr = parse_json("[1, 2, 3]");
    assert(arr.type == JsonVal::Type::Arr && arr.arr_val.size() == 3);
    assert(arr.arr_val[0].str_val == "1");
    assert(arr.arr_val[2].str_val == "3");

    auto empty_arr = parse_json("[]");
    assert(empty_arr.type == JsonVal::Type::Arr && empty_arr.arr_val.empty());

    auto obj = parse_json("{\"a\":1,\"b\":2}");
    assert(obj.type == JsonVal::Type::Obj && obj.obj_val.size() == 2);
    assert(obj.get("a") && obj.get("a")->as_i64() && *obj.get("a")->as_i64() == 1);
    assert(obj.get("b") && obj.get("b")->as_i64() && *obj.get("b")->as_i64() == 2);

    auto empty_obj = parse_json("{}");
    assert(empty_obj.type == JsonVal::Type::Obj && empty_obj.obj_val.empty());

    std::cout << "  containers: OK\n";
}

/// @brief 测试嵌套结构
void test_nested()
{
    auto v = parse_json("{\"name\":\"sysal\",\"version\":1,\"active\":true,"
                        "\"tags\":[\"cpp\",\"linux\"],"
                        "\"nested\":{\"x\":0,\"y\":null}}");

    assert(v.type == JsonVal::Type::Obj);
    assert(v.get("name") && v.get("name")->str_val == "sysal");
    assert(v.get("version") && *v.get("version")->as_i64() == 1);
    assert(v.get("active") && *v.get("active")->as_bool() == true);

    const auto* tags = v.get("tags");
    assert(tags && tags->type == JsonVal::Type::Arr && tags->arr_val.size() == 2);
    assert(tags->arr_val[0].str_val == "cpp");
    assert(tags->arr_val[1].str_val == "linux");

    const auto* nested = v.get("nested");
    assert(nested && nested->type == JsonVal::Type::Obj);
    assert(nested->get("x") && *nested->get("x")->as_i64() == 0);
    assert(nested->get("y") && nested->get("y")->type == JsonVal::Type::Null);

    std::cout << "  nested: OK\n";
}

/// @brief 测试 dump_json 发射
void test_dump()
{
    auto v = parse_json("{\"a\":[1,2],\"b\":null}");
    auto compact = dump_json(v);
    assert(compact == "{\"a\":[1,2],\"b\":null}");

    auto pretty = dump_json(v, true);
    assert(pretty.find('\n') != std::string::npos);

    std::cout << "  dump: OK\n";
}

/// @brief 测试往返一致性（parse → dump → parse → 比较）
void test_roundtrip()
{
    const char* src = "{\"name\":\"sysal\",\"version\":1,\"active\":true,"
                      "\"tags\":[\"cpp\",\"linux\",\"gpu\"],"
                      "\"nested\":{\"x\":0,\"y\":null,\"z\":[3.14,-7,1e10]},"
                      "\"empty_arr\":[],"
                      "\"empty_obj\":{}}";

    auto v1 = parse_json(src);
    auto emitted = dump_json(v1);
    auto v2 = parse_json(emitted);
    assert(json_equal(v1, v2));

    // 美化输出也必须往返一致
    auto pretty = dump_json(v1, true);
    auto v3 = parse_json(pretty);
    assert(json_equal(v1, v3));

    std::cout << "  roundtrip: OK\n";
}

/// @brief 测试 escape_string 函数
void test_escape_string()
{
    assert(escape_string("") == "\"\"");
    assert(escape_string("hello") == "\"hello\"");
    assert(escape_string("a\"b") == "\"a\\\"b\"");
    assert(escape_string("a\\b") == "\"a\\\\b\"");
    assert(escape_string("a\nb") == "\"a\\nb\"");
    assert(escape_string("a\tb") == "\"a\\tb\"");

    std::cout << "  escape_string: OK\n";
}

/// @brief 测试时间工具函数
void test_time_utils()
{
    auto now = std::chrono::system_clock::now();
    auto ms_str = time_point_to_ms(now);
    assert(!ms_str.empty());

    long long ms_val = 0;
    for(char c : ms_str)
    {
        ms_val = ms_val * 10 + (c - '0');
    }
    auto restored = ms_to_time_point(ms_val);
    auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch() -
                                                                      restored.time_since_epoch())
                    .count();
    assert(diff == 0);

    std::cout << "  time utils: OK\n";
}

/// @brief 测试错误处理（畸形 JSON 抛出 JsonError）
void test_errors()
{
    auto expect_throw = [](const char* json, const char* label)
    {
        try
        {
            parse_json(json);
            std::cerr << "  FAIL: expected JsonError for " << label << "\n";
            assert(false);
        }
        catch(const JsonError&)
        {
            // 预期行为
        }
    };

    expect_throw("", "empty input");
    expect_throw("{", "unterminated object");
    expect_throw("[", "unterminated array");
    expect_throw("\"unterminated", "unterminated string");
    expect_throw("{]", "mismatched braces");
    expect_throw("[1,]", "trailing comma in array");
    expect_throw("{\"a\":}", "missing value in object");
    expect_throw("xyz", "invalid literal");
    expect_throw("true extra", "trailing characters");

    std::cout << "  errors: OK\n";
}

/// @brief 测试 JsonObj / JsonArr 构建器
void test_builders()
{
    JsonObj obj;
    obj.add("key1", "42");
    obj.add("key2", escape_string("value"));
    auto result = obj.build(false, 0);
    assert(result == "{\"key1\":42,\"key2\":\"value\"}");

    JsonArr arr;
    arr.add("1");
    arr.add("2");
    arr.add("3");
    auto arr_result = arr.build(false, 0);
    assert(arr_result == "[1,2,3]");

    std::cout << "  builders: OK\n";
}

int main()
{
    std::cout << "test_json:\n";
    test_basic_types();
    test_string_escapes();
    test_containers();
    test_nested();
    test_dump();
    test_roundtrip();
    test_escape_string();
    test_time_utils();
    test_errors();
    test_builders();
    std::cout << "  all passed!\n";
    return 0;
}
