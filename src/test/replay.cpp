/// @file replay.cpp
/// @brief 测试用原始证据回放实现
/// @details 提供将 RawStore 序列化为 JSON 文件、从文件反序列化为 RawStore，
///          以及直接基于已有 RawStore 走采集流水线构建 SystemSnapshot 的工具，
///          供测试在不接触真实系统的情况下复现采集行为。

#include "sysal/test/replay.hpp"

#include "detail/json.hpp"
#include "detail/pipeline.hpp"

#include <chrono>
#include <fstream>
#include <sstream>
#include <utility>

namespace sysal::test
{

/// @brief 将 RawStore 以 JSON 形式保存到文件
/// @details 使用 pretty 格式将 RawStore 序列化为 JSON 字符串后整体写入指定路径。
///          打开或写入失败均返回 IoError。
/// @param raw 待保存的原始证据存储
/// @param path 目标文件路径
/// @return 成功返回空值，失败返回 SysalError
Expected<void, SysalError> save_raw_store(const RawStore& raw, const std::string& path)
{
    std::ofstream out(path, std::ios::binary);
    if(!out.is_open())
    {
        return make_unexpected(
            SysalError(ErrorKind::IoError, "cannot open file for writing: " + path));
    }
    const std::string json = detail::raw_store_to_json(raw, true);
    out.write(json.data(), static_cast<std::streamsize>(json.size()));
    if(!out)
    {
        return make_unexpected(SysalError(ErrorKind::IoError, "write failed: " + path));
    }
    return {};
}

/// @brief 从文件加载 RawStore
/// @details 以二进制方式读取整个文件内容，先解析为 JSON 再转换为 RawStore。
///          打开失败返回 FileNotFound，JSON 解析或转换失败透传对应错误。
/// @param path 源文件路径
/// @return 成功返回 RawStore，失败返回 SysalError
Expected<RawStore, SysalError> load_raw_store(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    if(!in.is_open())
    {
        return make_unexpected(SysalError(ErrorKind::FileNotFound, "cannot open file: " + path));
    }
    std::stringstream ss;
    ss << in.rdbuf();
    const std::string content = ss.str();

    auto parsed = detail::parse_json(content);
    if(!parsed)
    {
        return make_unexpected(parsed.error());
    }
    return detail::raw_store_from_json(*parsed);
}

/// @brief 基于已有 RawStore 走采集流水线构建快照
/// @details 以当前时刻作为采集起始时间，跳过真实系统读取，直接将给定
///          RawStore 与采集规格交由内部流水线解析构建 SystemSnapshot，
///          用于从回放数据复现采集结果。
/// @param raw 已加载的原始证据存储
/// @param spec 采集规格
/// @return 成功返回 SystemSnapshot，失败返回 SysalError
Expected<SystemSnapshot, SysalError> collect_from_raw(const RawStore& raw, const CollectSpec& spec)
{
    const auto start_time = std::chrono::system_clock::now();
    return detail::run_pipeline(raw, spec, start_time);
}

} // namespace sysal::test
