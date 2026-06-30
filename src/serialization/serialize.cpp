/// @file serialize.cpp
/// @brief RawStore JSON 序列化与反序列化实现
/// @details 实现 RawStore ↔ JSON 的转换，以及基于文件的 save/load 操作。
///          JSON 格式：顶层对象含 "records" 数组，每条记录含 source（整数）、
///          path_or_command（字符串）、payload（字符串）、status（整数）、
///          collected_at（epoch 毫秒整数）。

#include "serialization/json.hpp"

#include <sysal/core/error.hpp>
#include <sysal/model/raw_store.hpp>
#include <sysal/test/replay.hpp>
#include <sysal/types/enums.hpp>

#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>

namespace sysal::test
{

namespace
{

/// @brief 将 RawRecord 序列化为 JSON 对象文本
/// @param rec 原始记录
/// @return JSON 对象字符串
[[nodiscard]] std::string raw_record_to_json(const RawRecord& rec)
{
    detail::JsonObj obj;
    obj.add("source", std::to_string(static_cast<std::uint64_t>(rec.source)));
    obj.add("path_or_command", detail::escape_string(rec.path_or_command));
    obj.add("payload", detail::escape_string(rec.payload));
    obj.add("status", std::to_string(static_cast<std::uint64_t>(rec.status)));
    obj.add("collected_at", detail::time_point_to_ms(rec.collected_at));
    return obj.build(true, 2);
}

/// @brief 将 RawStore 序列化为 JSON 文本
/// @param store 原始证据存储
/// @return JSON 文本字符串
[[nodiscard]] std::string raw_store_to_json(const RawStore& store)
{
    detail::JsonArr arr;
    for(const auto& rec : store.records)
    {
        arr.add(raw_record_to_json(rec));
    }
    detail::JsonObj root;
    root.add("records", arr.build(true, 1));
    return root.build(true, 0);
}

/// @brief 从 JsonVal 解析 RawRecord
/// @param obj JSON 对象值
/// @return 解析后的 RawRecord
/// @throws SysalError 字段缺失或类型错误时抛出
[[nodiscard]] RawRecord raw_record_from_json(const detail::JsonVal& obj)
{
    RawRecord rec;

    // source
    const auto* source_val = obj.get("source");
    if(!source_val)
    {
        throw SysalError(ErrorKind::DeserializationError, "missing 'source' field in record");
    }
    auto source_int = source_val->as_u64();
    if(!source_int)
    {
        throw SysalError(ErrorKind::DeserializationError, "'source' must be an integer");
    }
    rec.source = static_cast<RawSource>(*source_int);

    // path_or_command
    const auto* path_val = obj.get("path_or_command");
    if(!path_val)
    {
        throw SysalError(ErrorKind::DeserializationError,
                         "missing 'path_or_command' field in record");
    }
    const auto* path_str = path_val->as_str();
    if(!path_str)
    {
        throw SysalError(ErrorKind::DeserializationError, "'path_or_command' must be a string");
    }
    rec.path_or_command = *path_str;

    // payload
    const auto* payload_val = obj.get("payload");
    if(!payload_val)
    {
        throw SysalError(ErrorKind::DeserializationError, "missing 'payload' field in record");
    }
    const auto* payload_str = payload_val->as_str();
    if(!payload_str)
    {
        throw SysalError(ErrorKind::DeserializationError, "'payload' must be a string");
    }
    rec.payload = *payload_str;

    // status
    const auto* status_val = obj.get("status");
    if(!status_val)
    {
        throw SysalError(ErrorKind::DeserializationError, "missing 'status' field in record");
    }
    auto status_int = status_val->as_u64();
    if(!status_int)
    {
        throw SysalError(ErrorKind::DeserializationError, "'status' must be an integer");
    }
    rec.status = static_cast<CollectStatus>(*status_int);

    // collected_at
    const auto* time_val = obj.get("collected_at");
    if(!time_val)
    {
        throw SysalError(ErrorKind::DeserializationError, "missing 'collected_at' field in record");
    }
    auto time_int = time_val->as_i64();
    if(!time_int)
    {
        throw SysalError(ErrorKind::DeserializationError, "'collected_at' must be an integer");
    }
    rec.collected_at = detail::ms_to_time_point(*time_int);

    return rec;
}

/// @brief 从 JsonVal 解析 RawStore
/// @param root JSON 根对象
/// @return 解析后的 RawStore
/// @throws SysalError 结构错误时抛出
[[nodiscard]] RawStore raw_store_from_json(const detail::JsonVal& root)
{
    const auto* records_val = root.get("records");
    if(!records_val || records_val->type != detail::JsonVal::Type::Arr)
    {
        throw SysalError(ErrorKind::DeserializationError,
                         "missing or invalid 'records' array in RawStore JSON");
    }

    RawStore store;
    for(const auto& elem : records_val->arr_val)
    {
        if(elem.type != detail::JsonVal::Type::Obj)
        {
            throw SysalError(ErrorKind::DeserializationError,
                             "each element in 'records' must be a JSON object");
        }
        store.records.push_back(raw_record_from_json(elem));
    }
    return store;
}

} // namespace

void save_raw_store(const RawStore& raw, const std::string& path)
{
    std::ofstream ofs(path);
    if(!ofs)
    {
        throw SysalError(ErrorKind::IoError, "cannot open file for writing: " + path);
    }
    ofs << raw_store_to_json(raw);
    if(!ofs)
    {
        throw SysalError(ErrorKind::IoError, "write failed: " + path);
    }
}

RawStore load_raw_store(const std::string& path)
{
    std::ifstream ifs(path);
    if(!ifs)
    {
        throw SysalError(ErrorKind::FileNotFound, "cannot open file for reading: " + path);
    }
    std::ostringstream oss;
    oss << ifs.rdbuf();
    if(!ifs && !ifs.eof())
    {
        throw SysalError(ErrorKind::IoError, "read failed: " + path);
    }

    try
    {
        auto root = detail::parse_json(oss.str());
        if(root.type != detail::JsonVal::Type::Obj)
        {
            throw SysalError(ErrorKind::DeserializationError,
                             "RawStore JSON root must be an object");
        }
        return raw_store_from_json(root);
    }
    catch(const detail::JsonError& e)
    {
        throw SysalError(ErrorKind::DeserializationError, e.what());
    }
}

} // namespace sysal::test
