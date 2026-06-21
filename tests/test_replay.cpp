#include "sysal/collect.hpp"
#include "sysal/test/replay.hpp"

#include <iostream>

int main()
{
    std::cout << "=== Step 1: Capture ===\n";
    auto snapshot = sysal::collect(sysal::CollectSpec::full().with_raw());
    if(!snapshot)
    {
        std::cerr << "collect failed: " << snapshot.error().what() << "\n";
        return 1;
    }
    const auto& snap = *snapshot;
    if(!snap.raw)
    {
        std::cerr << "snapshot has no raw store\n";
        return 1;
    }
    std::cout << "Captured snapshot with " << snap.raw->records.size() << " raw records\n";
    std::cout << "CPU packages: " << snap.resources.cpu.packages.size() << "\n";
    std::cout << "Logical CPUs: " << snap.resources.cpu.logical_cpus.size() << "\n";
    std::cout << "Memory total: " << snap.resources.memory.total_memory.value << " bytes\n";

    std::cout << "\n=== Step 2: Save raw store ===\n";
    auto save_result = sysal::test::save_raw_store(*snap.raw, "output/test_raw.json");
    if(!save_result)
    {
        std::cerr << "save_raw_store failed: " << save_result.error().what() << "\n";
        return 1;
    }
    std::cout << "Saved to output/test_raw.json\n";

    std::cout << "\n=== Step 3: Load raw store ===\n";
    auto raw2 = sysal::test::load_raw_store("output/test_raw.json");
    if(!raw2)
    {
        std::cerr << "load_raw_store failed: " << raw2.error().what() << "\n";
        return 1;
    }
    std::cout << "Loaded " << (*raw2).records.size() << " raw records\n";

    std::cout << "\n=== Step 4: Replay ===\n";
    auto snapshot2 = sysal::test::collect_from_raw(*raw2, sysal::CollectSpec::full());
    if(!snapshot2)
    {
        std::cerr << "collect_from_raw failed: " << snapshot2.error().what() << "\n";
        return 1;
    }
    const auto& snap2 = *snapshot2;
    std::cout << "Replayed snapshot:\n";
    std::cout << "CPU packages: " << snap2.resources.cpu.packages.size() << "\n";
    std::cout << "Logical CPUs: " << snap2.resources.cpu.logical_cpus.size() << "\n";
    std::cout << "Memory total: " << snap2.resources.memory.total_memory.value << " bytes\n";

    std::cout << "\n=== Step 5: Verify ===\n";
    int failures = 0;

    auto check = [&failures](const char* name, bool ok)
    {
        std::cout << name << ": " << (ok ? "PASS" : "FAIL") << "\n";
        if(!ok)
        {
            ++failures;
        }
    };

    check("raw record count matches", snap.raw->records.size() == (*raw2).records.size());
    check("CPU package count matches",
          snap.resources.cpu.packages.size() == snap2.resources.cpu.packages.size());
    check("logical CPU count matches",
          snap.resources.cpu.logical_cpus.size() == snap2.resources.cpu.logical_cpus.size());
    check("memory total matches",
          snap.resources.memory.total_memory.value == snap2.resources.memory.total_memory.value);
    check("NUMA node count matches",
          snap.resources.cpu.numa_nodes.size() == snap2.resources.cpu.numa_nodes.size());
    check("network interface count matches",
          snap.resources.network.interfaces.size() == snap2.resources.network.interfaces.size());
    check("PCI device count matches",
          snap.resources.pci.devices.size() == snap2.resources.pci.devices.size());
    check("storage device count matches",
          snap.resources.storage.devices.size() == snap2.resources.storage.devices.size());
    check("accelerator count matches", snap.resources.accelerators.devices.size() ==
                                           snap2.resources.accelerators.devices.size());

    std::cout << "\n=== Result ===\n";
    if(failures == 0)
    {
        std::cout << "All checks passed.\n";
        return 0;
    }
    std::cout << failures << " checks failed.\n";
    return 1;
}
