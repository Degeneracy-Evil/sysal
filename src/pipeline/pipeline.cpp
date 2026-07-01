/// @file pipeline.cpp
/// @brief 采集管线编排实现
/// @details 实现 run_pipeline（Reader → Parser → Resolver）和
///          run_replay（Parser → Resolver），以及 collect_from_raw 公共接口。

#include "pipeline/pipeline.hpp"

#include "parser/accelerator.hpp"
#include "parser/cpu.hpp"
#include "parser/execution.hpp"
#include "parser/memory.hpp"
#include "parser/network.hpp"
#include "parser/pci.hpp"
#include "parser/platform.hpp"
#include "parser/software.hpp"
#include "parser/storage.hpp"
#include "reader/linux/procfs.hpp"
#include "reader/linux/sysfs.hpp"
#include "resolver/resolve.hpp"

#include "sysal/core/collect.hpp"
#include "sysal/core/error.hpp"
#include "sysal/core/system.hpp"
#include "sysal/model/raw_store.hpp"
#include "sysal/test/replay.hpp"
#include "sysal/version.hpp"

#include <chrono>
#include <string>
#include <utility>
#include <vector>

namespace sysal::detail
{

namespace
{

/// @brief 后端初始化占位
/// @details v0.0.1 无外部后端（NVML 等），此函数为空。
///          未来在此处调用 nvmlInit 等后端初始化。
void init_backend()
{
    // v0.0.1: 无外部后端需要初始化
}

/// @brief 后端清理占位
/// @details v0.0.1 无外部后端，此函数为空。
///          未来在此处调用 nvmlShutdown 等后端清理。
void shutdown_backend()
{
    // v0.0.1: 无外部后端需要清理
}

/// @brief 记录成功/失败的采集器
/// @details 根据 ParseResult 各域是否为 nullopt 判断成功与否。
void record_collector_status(const ParseResult& result, std::vector<std::string>& succeeded,
                             std::vector<std::string>& failed)
{
    const auto check = [&](const char* name, const auto& opt)
    {
        if(opt.has_value())
        {
            succeeded.push_back(name);
        }
        else
        {
            failed.push_back(name);
        }
    };

    check("platform", result.platform);
    check("cpu", result.cpu);
    check("memory", result.memory);
    check("pci", result.pci);
    check("network", result.network);
    check("accelerator", result.accelerators);
    check("storage", result.storage);
    check("software", result.software);
    check("execution", result.execution);
}

} // namespace

System run_replay(const RawStore& raw, Collect flags, std::vector<std::string>& warnings)
{
    const auto start = std::chrono::system_clock::now();

    // 后端初始化生命周期：init/shutdown 在 collect 内部配对完成
    init_backend();

    ParseResult result;

    // 按域调用解析器：仅解析 flags 中请求的域
    if(has(flags, Collect::Platform))
    {
        result.platform = parse_platform(raw, warnings);
    }
    if(has(flags, Collect::Cpu))
    {
        result.cpu = parse_cpu(raw, warnings);
    }
    if(has(flags, Collect::Memory))
    {
        result.memory = parse_memory(raw, warnings);
    }
    if(has(flags, Collect::Pci))
    {
        result.pci = parse_pci(raw, warnings);
    }
    if(has(flags, Collect::Network))
    {
        result.network = parse_network(raw, warnings);
    }
    if(has(flags, Collect::Accelerator))
    {
        result.accelerators = parse_accelerator(raw, warnings);
    }
    if(has(flags, Collect::Storage))
    {
        result.storage = parse_storage(raw, warnings);
    }
    if(has(flags, Collect::Software))
    {
        result.software = parse_software(raw, warnings);
    }
    if(has(flags, Collect::Execution))
    {
        result.execution = parse_execution(raw, warnings);
    }

    // 记录成功/失败的采集器（在 resolve 移动之前）
    std::vector<std::string> succeeded_collectors;
    std::vector<std::string> failed_collectors;
    record_collector_status(result, succeeded_collectors, failed_collectors);

    // 全部请求的采集器失败时抛出异常
    if(succeeded_collectors.empty() && !failed_collectors.empty())
    {
        throw SysalError(ErrorKind::CollectionFailed, "all requested collectors failed: " +
                                                          std::to_string(failed_collectors.size()) +
                                                          " collectors");
    }

    // Resolver：合并、冲突解决、可见性计算
    auto info = resolve(std::move(result), warnings);

    const auto end = std::chrono::system_clock::now();

    // 构建 SnapshotMeta
    SnapshotMeta meta;
    meta.collect_time = start;
    meta.sysal_version = sysal::VERSION_STRING;
    meta.collect_duration = end - start;
    meta.requested_flags = flags;
    meta.succeeded_collectors = std::move(succeeded_collectors);
    meta.failed_collectors = std::move(failed_collectors);

    // 后端清理
    shutdown_backend();

    // 组装 System
    System sys;
    sys.info = std::move(info);
    sys.meta = std::move(meta);
    sys.warnings = std::move(warnings);

    return sys;
}

System run_pipeline(Collect flags, std::vector<std::string>& warnings)
{
    // Reader 阶段：采集原始数据
    RawStore raw;
    reader::read_procfs(raw, flags);
    reader::read_sysfs(raw, flags);

    // 回放管线：Parser → Resolver
    auto sys = run_replay(raw, flags, warnings);

    // 仅在请求 Raw 域时保留原始证据
    if(has(flags, Collect::Raw))
    {
        sys.raw = std::move(raw);
    }

    return sys;
}

} // namespace sysal::detail

namespace sysal::test
{

System collect_from_raw(const RawStore& raw, Collect flags)
{
    std::vector<std::string> warnings;
    return detail::run_replay(raw, flags, warnings);
}

} // namespace sysal::test
