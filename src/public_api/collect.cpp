/// @file collect.cpp
/// @brief 公共采集 API 实现
/// @details 实现 sysal 公共入口 collect() 与 collect_or_throw()：前者返回
///          Expected 以避免抛异常，后者在失败时直接抛出 SysalError。

#include "sysal/collect.hpp"

#include "detail/pipeline.hpp"
#include "reader/linux/procfs_reader.hpp"
#include "reader/linux/sysfs_reader.hpp"

#include <chrono>
#include <utility>

namespace sysal
{

/// @brief 按采集规格采集系统信息并返回快照
/// @details 记录起始时刻后依次执行 procfs 与 sysfs 读取，将原始证据填入
///          RawStore，再交由内部流水线解析构建 SystemSnapshot。
///          失败时通过返回的 Expected 携带 SysalError，不抛异常。
/// @param spec 采集规格，控制采集哪些子系统及是否保留原始证据
/// @return 成功返回 SystemSnapshot，失败返回 SysalError
Expected<SystemSnapshot, SysalError> collect(const CollectSpec& spec)
{
    const auto start_time = std::chrono::system_clock::now();

    RawStore raw;

    detail::read_procfs(raw, spec);
    detail::read_sysfs(raw, spec);

    return detail::run_pipeline(raw, spec, start_time);
}

/// @brief 采集系统信息，失败时抛出异常
/// @details 等价于调用 collect()，但当结果为失败时直接抛出对应的 SysalError，
///          适用于调用方偏好异常风格的场景。
/// @param spec 采集规格
/// @return 成功返回 SystemSnapshot
/// @throws SysalError 当采集失败时抛出
SystemSnapshot collect_or_throw(const CollectSpec& spec)
{
    auto result = collect(spec);
    if(!result)
    {
        throw result.error();
    }
    return std::move(*result);
}

} // namespace sysal
