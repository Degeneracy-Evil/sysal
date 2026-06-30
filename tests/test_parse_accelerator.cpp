#include "parser/accelerator.hpp"

#include "sysal/model/raw_store.hpp"
#include "sysal/types/enums.hpp"

#include <cassert>
#include <chrono>
#include <string>
#include <vector>

using namespace sysal;
using namespace sysal::detail;

/// @brief 创建一条 RawRecord
static RawRecord make_record(RawSource source, const std::string& path, const std::string& payload)
{
    return RawRecord{source, path, payload, CollectStatus::Success,
                     std::chrono::system_clock::now()};
}

int main()
{
    // ---- 测试 1: 2 块 NVIDIA H20 GPU ----
    {
        RawStore raw;
        raw.records.push_back(make_record(RawSource::NvidiaSmi, "nvidia-smi",
                                          "index, name, pci.bus_id, memory.total\n"
                                          "0, NVIDIA H20 96GB, 00000000:41:00.0, 97536 MiB\n"
                                          "1, NVIDIA H20 96GB, 00000000:42:00.0, 97536 MiB\n"));

        std::vector<std::string> warnings;
        auto result = parse_accelerator(raw, warnings);
        assert(result.has_value());

        const auto& acc = *result;
        assert(acc.devices.size() == 2);

        // 设备 0
        assert(acc.devices[0].id == AcceleratorId{0});
        assert(acc.devices[0].kind == AcceleratorKind::Gpu);
        assert(acc.devices[0].vendor.value == "NVIDIA");
        assert(acc.devices[0].name.value == "NVIDIA H20 96GB");
        assert(acc.devices[0].pci_address.has_value());
        assert(acc.devices[0].pci_address->domain == 0);
        assert(acc.devices[0].pci_address->bus == 0x41);
        assert(acc.devices[0].pci_address->device == 0x00);
        assert(acc.devices[0].pci_address->function == 0x0);
        assert(acc.devices[0].memory_size.has_value());
        assert(acc.devices[0].memory_size->value == 97536ULL * 1024ULL * 1024ULL);
        assert(acc.devices[0].visible_to_current_process == true);
        assert(!acc.devices[0].driver.has_value());

        // 设备 1
        assert(acc.devices[1].id == AcceleratorId{1});
        assert(acc.devices[1].kind == AcceleratorKind::Gpu);
        assert(acc.devices[1].name.value == "NVIDIA H20 96GB");
        assert(acc.devices[1].pci_address.has_value());
        assert(acc.devices[1].pci_address->bus == 0x42);
        assert(acc.devices[1].memory_size.has_value());
        assert(acc.devices[1].memory_size->value == 97536ULL * 1024ULL * 1024ULL);
    }

    // ---- 测试 2: NUMA 节点查找（D-4 修正） ----
    {
        RawStore raw;
        raw.records.push_back(make_record(RawSource::NvidiaSmi, "nvidia-smi",
                                          "index, name, pci.bus_id, memory.total\n"
                                          "0, NVIDIA H20 96GB, 00000000:41:00.0, 97536 MiB\n"));
        raw.records.push_back(
            make_record(RawSource::SysfsPci, "/sys/bus/pci/devices/0000:41:00.0/numa_node", "0\n"));

        std::vector<std::string> warnings;
        auto result = parse_accelerator(raw, warnings);
        assert(result.has_value());

        const auto& acc = *result;
        assert(acc.devices.size() == 1);
        assert(acc.devices[0].nearest_numa_node.has_value());
        assert(*acc.devices[0].nearest_numa_node == NumaNodeId{0});
    }

    // ---- 测试 3: 空 NvidiaSmi → nullopt ----
    {
        RawStore raw;
        std::vector<std::string> warnings;
        auto result = parse_accelerator(raw, warnings);
        assert(!result.has_value());
    }

    // ---- 测试 4: GiB 单位解析 ----
    {
        RawStore raw;
        raw.records.push_back(make_record(RawSource::NvidiaSmi, "nvidia-smi",
                                          "index, name, pci.bus_id, memory.total\n"
                                          "0, NVIDIA A100 80GB, 00000000:3b:00.0, 80 GiB\n"));

        std::vector<std::string> warnings;
        auto result = parse_accelerator(raw, warnings);
        assert(result.has_value());

        const auto& acc = *result;
        assert(acc.devices.size() == 1);
        assert(acc.devices[0].memory_size.has_value());
        assert(acc.devices[0].memory_size->value == 80ULL * 1024ULL * 1024ULL * 1024ULL);
    }

    return 0;
}
