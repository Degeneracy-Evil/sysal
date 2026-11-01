#include "resolver/inventory.hpp"

#include "parser/parse_utils.hpp"

#include <iomanip>
#include <limits>
#include <set>
#include <sstream>

namespace sysal::detail
{
    namespace
    {
        template <typename Entries, typename Name>
        InventoryReport report(const RawStore &raw, InventoryKind kind, const Entries &entries, Name name,
                               const std::string &origin)
        {
            InventoryReport result{
                kind, entries.empty() ? InventoryCompleteness::Unknown : InventoryCompleteness::Partial, origin};
            const auto *enumeration = raw.enumeration(kind);
            if(enumeration == nullptr)
                return result;
            result.origin = enumeration->origin;
            if(enumeration->status != CollectStatus::Success || enumeration->failure)
                return result;
            const std::set<std::string> expected(enumeration->entries.begin(), enumeration->entries.end());
            std::set<std::string> actual;
            for(const auto &entry : entries)
                actual.insert(name(entry));
            // Duplicate or discarded identities cannot establish a complete inventory.
            if(expected.size() == enumeration->entries.size() && actual.size() == entries.size() && expected == actual)
                result.completeness = InventoryCompleteness::Complete;
            return result;
        }

        bool topology_known(const Cpu &cpu, const RawStore &raw)
        {
            if(cpu.logical_cpus.empty())
                return false;
            for(const auto &logical : cpu.logical_cpus)
                for(const auto *field : {"physical_package_id", "core_id"})
                {
                    const auto path =
                        "/sys/devices/system/cpu/cpu" + std::to_string(logical.id.value()) + "/topology/" + field;
                    const auto records = raw.get(RawSource::SysfsCpu, path);
                    if(records.size() != 1 || records.front()->status != CollectStatus::Success)
                        return false;
                    const auto number = parse_uint(trim(records.front()->payload));
                    if(!number || *number > std::numeric_limits<std::uint32_t>::max())
                        return false;
                }
            return true;
        }

        InventoryReport observed(InventoryKind kind, bool empty, const std::string &origin)
        {
            return {kind, empty ? InventoryCompleteness::Unknown : InventoryCompleteness::Partial, origin};
        }
    } // namespace

    std::vector<InventoryReport> inventory_reports(const SystemInfo &info, const RawStore &raw, Collect flags)
    {
        std::vector<InventoryReport> reports;
        if(has(flags, Collect::Cpu))
        {
            auto logical = report(
                raw, InventoryKind::LogicalCpus, info.cpu.logical_cpus,
                [](const auto &entry) { return "cpu" + std::to_string(entry.id.value()); }, "/sys/devices/system/cpu");
            reports.push_back(logical);
            auto packages = observed(InventoryKind::CpuPackages, info.cpu.packages.empty(), logical.origin);
            auto cores = observed(InventoryKind::CpuCores, info.cpu.cores.empty(), logical.origin);
            if(topology_known(info.cpu, raw))
                packages.completeness = cores.completeness = logical.completeness;
            reports.push_back(std::move(packages));
            reports.push_back(std::move(cores));
            reports.push_back(report(
                raw, InventoryKind::NumaNodes, info.cpu.numa_nodes, [](const auto &entry)
                { return "node" + std::to_string(entry.id.value()); }, "/sys/devices/system/node"));
            // Cache deduplication and backend unions need their own enumeration proof.
            reports.push_back(observed(InventoryKind::CpuCaches, info.cpu.caches.empty(), "observed CPU caches"));
        }
        if(has(flags, Collect::Network))
            reports.push_back(report(
                raw, InventoryKind::NetworkInterfaces, info.network.interfaces,
                [](const auto &entry) { return entry.name.value; }, "/sys/class/net"));
        if(has(flags, Collect::Storage))
            reports.push_back(report(
                raw, InventoryKind::BlockDevices, info.storage.devices,
                [](const auto &entry) { return entry.name.value; }, "/sys/class/block"));
        if(has(flags, Collect::Pci))
            reports.push_back(report(
                raw, InventoryKind::PciDevices, info.pci.devices,
                [](const auto &entry)
                {
                    const auto &a = entry.address;
                    std::ostringstream name;
                    name << std::hex << std::setfill('0') << std::setw(4) << a.domain << ':' << std::setw(2)
                         << static_cast<unsigned>(a.bus) << ':' << std::setw(2) << static_cast<unsigned>(a.device)
                         << '.' << static_cast<unsigned>(a.function);
                    return name.str();
                },
                "/sys/bus/pci/devices"));
        if(has(flags, Collect::Accelerator))
            reports.push_back(observed(InventoryKind::AcceleratorDevices, info.accelerators.devices.empty(),
                                       "observed accelerator backends"));
        return reports;
    }
} // namespace sysal::detail
