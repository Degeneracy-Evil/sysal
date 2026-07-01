#include "sysal/core/collect.hpp"
#include "sysal/core/error.hpp"
#include "sysal/types/enums.hpp"
#include "sysal/types/ids.hpp"
#include "sysal/types/strong_id.hpp"
#include "sysal/types/units.hpp"
#include "sysal/types/value_types.hpp"

#include "test_macros.hpp"
#include <cstdint>

int main()
{
    // Collect 位掩码
    static_assert(sysal::has(sysal::full, sysal::Collect::Cpu));
    static_assert(sysal::has(sysal::full, sysal::Collect::Raw));
    static_assert(sysal::has(sysal::basic, sysal::Collect::Platform));
    static_assert(!sysal::has(sysal::basic, sysal::Collect::Accelerator));

    auto flags = sysal::Collect::Cpu | sysal::Collect::Memory | sysal::Collect::Raw;
    CHECK(sysal::has(flags, sysal::Collect::Cpu));
    CHECK(sysal::has(flags, sysal::Collect::Memory));
    CHECK(sysal::has(flags, sysal::Collect::Raw));
    CHECK(!sysal::has(flags, sysal::Collect::Network));

    // Arch 枚举
    auto arch = sysal::Arch::X86_64;
    CHECK(arch == sysal::Arch::X86_64);

    // StrongId
    sysal::CpuPackageId pkg{0};
    sysal::CpuCoreId core{1};
    sysal::LogicalCpuId cpu{2};
    CHECK(pkg.value() == 0);
    CHECK(core.value() == 1);
    CHECK(cpu.value() == 2);
    CHECK(pkg == sysal::CpuPackageId{0});

    // ScalarUnit
    sysal::MemorySize mem{1024};
    sysal::Frequency freq{2400000000};
    CHECK(mem.value == 1024);
    CHECK(freq.value == 2400000000);

    // NamedString
    sysal::Vendor vendor{"GenuineIntel"};
    sysal::DeviceName name{"Intel(R) Xeon(R) Gold 5320"};
    sysal::PciClass cls{"030000"};
    CHECK(vendor.value == "GenuineIntel");
    CHECK(cls.value == "030000");

    // PciAddress
    sysal::PciAddress addr{0, 65, 0, 0};
    CHECK(addr.domain == 0);
    CHECK(addr.bus == 65);

    // SysalError
    sysal::SysalError err(sysal::ErrorKind::CollectionFailed, "test error");
    CHECK(err.kind() == sysal::ErrorKind::CollectionFailed);

    TEST_SUMMARY();
}
