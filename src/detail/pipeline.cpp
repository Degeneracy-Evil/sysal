/// @file pipeline.cpp
/// @brief 收集流水线实现
/// @details 实现 run_pipeline：依次调用各类 parser 将 RawStore 解析为 ParsedFacts，
///          再交由 resolver 组装为最终的 SystemSnapshot，并附加诊断信息。

#include "pipeline.hpp"

#include "parser/accelerator_parser.hpp"
#include "parser/cpu_parser.hpp"
#include "parser/execution_parser.hpp"
#include "parser/memory_parser.hpp"
#include "parser/network_parser.hpp"
#include "parser/parsed_facts.hpp"
#include "parser/pci_parser.hpp"
#include "parser/platform_parser.hpp"
#include "parser/software_parser.hpp"
#include "parser/storage_parser.hpp"
#include "parser/topology_parser.hpp"
#include "resolver/resolver.hpp"

#include <optional>
#include <utility>

namespace sysal::detail
{

/// @brief 运行解析-解析-解析流水线，生成系统快照
/// @param raw 已采集的原始数据存储
/// @param spec 采集规格，决定解析哪些类别
/// @param start_time 流水线起始时间点，用于填充快照元数据
/// @return 组装完成的 SystemSnapshot
SystemSnapshot run_pipeline(const RawStore& raw, const CollectSpec& spec,
                            std::chrono::system_clock::time_point start_time)
{
    Diagnostics diag;

    ParsedFacts facts;
    // 按采集规格逐类解析原始记录，每类独立可选
    if(spec.collect_platform())
    {
        facts.platform = parse_platform(raw, diag);
    }
    if(spec.collect_cpu())
    {
        facts.cpu = parse_cpu(raw, diag);
    }
    if(spec.collect_memory())
    {
        facts.memory = parse_memory(raw, diag);
    }
    if(spec.collect_pci())
    {
        facts.pci = parse_pci(raw, diag);
    }
    if(spec.collect_network())
    {
        facts.network = parse_network(raw, diag);
    }
    if(spec.collect_accelerators())
    {
        facts.accelerators = parse_accelerators(raw, diag);
    }
    if(spec.collect_storage())
    {
        facts.storage = parse_storage(raw, diag);
    }
    if(spec.collect_topology())
    {
        facts.topology = parse_topology(raw, diag);
    }
    if(spec.collect_software_stack())
    {
        facts.software = parse_software_stack(raw, diag);
    }
    if(spec.collect_execution_context())
    {
        facts.execution = parse_execution_context(raw, diag);
    }

    // 仅在用户要求保留原始数据时才将其放入快照
    std::optional<RawStore> raw_opt;
    if(spec.keep_raw())
    {
        raw_opt = raw;
    }

    // resolver 阶段：将 ParsedFacts 组装为最终快照
    auto snapshot = resolve(std::move(facts), spec, start_time, raw_opt);
    // 填入解析阶段收集的诊断信息
    snapshot.diagnostics = std::move(diag);

    return snapshot;
}

} // namespace sysal::detail
