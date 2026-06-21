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

SystemSnapshot run_pipeline(const RawStore& raw, const CollectSpec& spec,
                            std::chrono::system_clock::time_point start_time)
{
    Diagnostics diag;

    ParsedFacts facts;
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

    std::optional<RawStore> raw_opt;
    if(spec.keep_raw())
    {
        raw_opt = raw;
    }

    auto snapshot = resolve(std::move(facts), spec, start_time, raw_opt);
    snapshot.diagnostics = std::move(diag);

    return snapshot;
}

} // namespace sysal::detail
