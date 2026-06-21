#include "resolver.hpp"

#include "parser/parsed_facts.hpp"

#include "sysal/collect_spec.hpp"
#include "sysal/ids.hpp"
#include "sysal/raw_store.hpp"
#include "sysal/system_snapshot.hpp"

#include <chrono>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility>

namespace sysal::detail
{

namespace
{

void fill_meta(SystemSnapshot& snapshot, const CollectSpec& spec,
               std::chrono::system_clock::time_point start_time, const ParsedFacts& facts)
{
    snapshot.meta.collect_time = start_time;
    snapshot.meta.sysal_version = "0.0.1";
    snapshot.meta.collect_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now() - start_time);
    snapshot.meta.requested_spec = spec;

    if(facts.platform)
    {
        snapshot.meta.succeeded_collectors.emplace_back("platform");
    }
    if(facts.cpu)
    {
        snapshot.meta.succeeded_collectors.emplace_back("cpu");
    }
    if(facts.memory)
    {
        snapshot.meta.succeeded_collectors.emplace_back("memory");
    }
    if(facts.network)
    {
        snapshot.meta.succeeded_collectors.emplace_back("network");
    }
    if(facts.pci)
    {
        snapshot.meta.succeeded_collectors.emplace_back("pci");
    }
    if(facts.accelerators)
    {
        snapshot.meta.succeeded_collectors.emplace_back("accelerators");
    }
    if(facts.storage)
    {
        snapshot.meta.succeeded_collectors.emplace_back("storage");
    }
    if(facts.topology)
    {
        snapshot.meta.succeeded_collectors.emplace_back("topology");
    }
    if(facts.software)
    {
        snapshot.meta.succeeded_collectors.emplace_back("software");
    }
    if(facts.execution)
    {
        snapshot.meta.succeeded_collectors.emplace_back("execution");
    }
}

} // namespace

SystemSnapshot resolve(ParsedFacts&& facts, const CollectSpec& spec,
                       std::chrono::system_clock::time_point start_time,
                       const std::optional<RawStore>& raw)
{
    SystemSnapshot snapshot;

    fill_meta(snapshot, spec, start_time, facts);

    if(facts.platform)
    {
        snapshot.platform = std::move(*facts.platform);
    }
    if(facts.cpu)
    {
        snapshot.resources.cpu = std::move(*facts.cpu);
        for(auto& cpu : snapshot.resources.cpu.logical_cpus)
        {
            cpu.visible_to_current_process = true;
        }
    }
    if(facts.memory)
    {
        snapshot.resources.memory = std::move(*facts.memory);
    }
    if(facts.network)
    {
        snapshot.resources.network = std::move(*facts.network);
        for(auto& iface : snapshot.resources.network.interfaces)
        {
            iface.visible_to_current_process = true;
        }
    }
    if(facts.pci)
    {
        snapshot.resources.pci = std::move(*facts.pci);
    }
    if(facts.accelerators)
    {
        snapshot.resources.accelerators = std::move(*facts.accelerators);
        for(auto& dev : snapshot.resources.accelerators.devices)
        {
            dev.visible_to_current_process = true;
        }
    }
    if(facts.storage)
    {
        snapshot.resources.storage = std::move(*facts.storage);
    }
    if(facts.topology)
    {
        snapshot.resources.topology = std::move(*facts.topology);
    }
    if(facts.software)
    {
        snapshot.software = std::move(*facts.software);
    }
    if(facts.execution)
    {
        snapshot.execution = std::move(*facts.execution);
    }

    // Apply cpuset visibility using set lookup (O(n+m) instead of O(n*m)).
    if(!snapshot.execution.cpuset.cpus.empty() && !snapshot.resources.cpu.logical_cpus.empty())
    {
        std::unordered_set<LogicalCpuId> visible_ids(snapshot.execution.cpuset.cpus.begin(),
                                                     snapshot.execution.cpuset.cpus.end());

        for(auto& cpu : snapshot.resources.cpu.logical_cpus)
        {
            cpu.visible_to_current_process = visible_ids.contains(cpu.id);
        }

        // is_restricted is true only when cpuset is a proper subset of all CPUs.
        snapshot.execution.cpuset.is_restricted =
            visible_ids.size() < snapshot.resources.cpu.logical_cpus.size();
    }

    snapshot.execution.visible_logical_cpu_ids.clear();
    for(const auto& cpu : snapshot.resources.cpu.logical_cpus)
    {
        if(cpu.visible_to_current_process)
        {
            snapshot.execution.visible_logical_cpu_ids.push_back(cpu.id);
        }
    }

    snapshot.execution.visible_accelerator_ids.clear();
    for(const auto& dev : snapshot.resources.accelerators.devices)
    {
        if(dev.visible_to_current_process)
        {
            snapshot.execution.visible_accelerator_ids.push_back(dev.id);
        }
    }

    snapshot.execution.visible_network_interface_names.clear();
    for(const auto& iface : snapshot.resources.network.interfaces)
    {
        if(iface.visible_to_current_process)
        {
            snapshot.execution.visible_network_interface_names.push_back(iface.name);
        }
    }

    snapshot.raw = raw;

    return snapshot;
}

} // namespace sysal::detail
