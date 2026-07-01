#include "parser/platform.hpp"

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
    // ---- 测试 1: 完整平台信息解析 ----
    {
        RawStore raw;
        raw.records.push_back(make_record(RawSource::EtcOsRelease, "/etc/os-release",
                                          "NAME=\"Ubuntu\"\n"
                                          "VERSION=\"22.04.3 LTS (Jammy Jellyfish)\"\n"
                                          "ID=ubuntu\n"
                                          "VERSION_ID=\"22.04\"\n"
                                          "VERSION_CODENAME=jammy\n"));
        raw.records.push_back(
            make_record(RawSource::ProcVersion, "/proc/version",
                        "Linux version 5.15.0-91-generic (buildd@lcy02-amd64-003) "
                        "(gcc (Ubuntu 11.4.0-1ubuntu1~22.04) 11.4.0, GNU ld "
                        "(GNU Binutils for Ubuntu) 2.38) "
                        "#101-Ubuntu SMP Mon Nov 13 18:02:07 UTC 2023\n"));
        raw.records.push_back(make_record(RawSource::Uname, "uname -m", "x86_64\n"));
        raw.records.push_back(make_record(RawSource::SysfsDmi, "/sys/class/dmi/id/bios_vendor",
                                          "American Megatrends Inc.\n"));
        raw.records.push_back(
            make_record(RawSource::SysfsDmi, "/sys/class/dmi/id/bios_version", "1.0.0\n"));
        raw.records.push_back(
            make_record(RawSource::SysfsDmi, "/sys/class/dmi/id/bios_date", "01/01/2023\n"));
        raw.records.push_back(
            make_record(RawSource::SysfsDmi, "/sys/class/dmi/id/product_name", "PowerEdge R750\n"));
        raw.records.push_back(
            make_record(RawSource::SysfsDmi, "/sys/class/dmi/id/sys_vendor", "Dell Inc.\n"));
        raw.records.push_back(
            make_record(RawSource::SysfsDmi, "/sys/class/dmi/id/product_serial", "ABCD1234\n"));

        std::vector<std::string> warnings;
        auto result = parse_platform(raw, warnings);
        assert(result.has_value());

        const auto& p = *result;

        // Os
        assert(p.os.name == "Ubuntu");
        assert(p.os.version == "22.04");
        assert(p.os.distribution == "ubuntu");
        assert(p.os.codename == "jammy");

        // Kernel
        assert(p.kernel.release == "5.15.0-91-generic");
        assert(p.kernel.version == "#101-Ubuntu SMP");
        assert(p.kernel.compiled_at.find("Mon Nov 13") != std::string::npos);
        assert(p.kernel.architecture == "x86_64");

        // Architecture
        assert(p.architecture.name == "x86_64");
        assert(p.architecture.bits == 64);
        assert(p.architecture.byte_order == "little");

        // Host (DMI)
        assert(p.host.product_name == "PowerEdge R750");
        assert(p.host.vendor.value == "Dell Inc.");
        assert(p.host.serial == "ABCD1234");

        // Firmware
        assert(p.firmware.has_value());
        assert(p.firmware->bios_vendor == "American Megatrends Inc.");
        assert(p.firmware->bios_version == "1.0.0");
        assert(p.firmware->bios_date == "01/01/2023");

        // Virtualization: 无数据，应为 nullopt
        assert(!p.virtualization.has_value());
    }

    // ---- 测试 2: 容器环境不再产生 Virtualization（容器信息由 ExecutionContext 承载） ----
    {
        RawStore raw;
        raw.records.push_back(
            make_record(RawSource::EtcOsRelease, "/etc/os-release", "NAME=\"Ubuntu\"\n"));
        raw.records.push_back(make_record(RawSource::ProcVersion, "/proc/version",
                                          "Linux version 5.15.0-91-generic #101-Ubuntu SMP ...\n"));
        raw.records.push_back(make_record(RawSource::Uname, "uname -m", "x86_64\n"));
        raw.records.push_back(
            make_record(RawSource::ProcOneCgroup, "/proc/1/cgroup", "0::/docker/abc123\n"));
        raw.records.push_back(make_record(RawSource::RootDockerenv, "/.dockerenv", ""));

        std::vector<std::string> warnings;
        auto result = parse_platform(raw, warnings);
        assert(result.has_value());
        // detect_virtualization 仅检测硬件虚拟化，容器不再产生 Virtualization
        assert(!result->virtualization.has_value());
    }

    // ---- 测试 3: 虚拟化检测（KVM） ----
    {
        RawStore raw;
        raw.records.push_back(
            make_record(RawSource::EtcOsRelease, "/etc/os-release", "NAME=\"Ubuntu\"\n"));
        raw.records.push_back(make_record(RawSource::ProcVersion, "/proc/version",
                                          "Linux version 5.15.0-91-generic #101-Ubuntu SMP ...\n"));
        raw.records.push_back(make_record(RawSource::Uname, "uname -m", "x86_64\n"));
        raw.records.push_back(
            make_record(RawSource::ProcOneCgroup, "/proc/1/cgroup", "0::/kvm/xyz\n"));

        std::vector<std::string> warnings;
        auto result = parse_platform(raw, warnings);
        assert(result.has_value());
        assert(result->virtualization.has_value());
        assert(result->virtualization->kind == VirtualizationKind::Kvm);
        assert(result->virtualization->hypervisor == "KVM");
    }

    // ---- 测试 4: aarch64 架构 ----
    {
        RawStore raw;
        raw.records.push_back(
            make_record(RawSource::EtcOsRelease, "/etc/os-release", "NAME=\"Ubuntu\"\n"));
        raw.records.push_back(make_record(RawSource::ProcVersion, "/proc/version",
                                          "Linux version 5.15.0-91-generic #101-Ubuntu SMP ...\n"));
        raw.records.push_back(make_record(RawSource::Uname, "uname -m", "aarch64\n"));

        std::vector<std::string> warnings;
        auto result = parse_platform(raw, warnings);
        assert(result.has_value());
        assert(result->architecture.name == "aarch64");
        assert(result->architecture.bits == 64);
    }

    // ---- 测试 5: riscv64 架构 ----
    {
        RawStore raw;
        raw.records.push_back(
            make_record(RawSource::EtcOsRelease, "/etc/os-release", "NAME=\"Ubuntu\"\n"));
        raw.records.push_back(make_record(RawSource::ProcVersion, "/proc/version",
                                          "Linux version 5.15.0-91-generic #101-Ubuntu SMP ...\n"));
        raw.records.push_back(make_record(RawSource::Uname, "uname -m", "riscv64\n"));

        std::vector<std::string> warnings;
        auto result = parse_platform(raw, warnings);
        assert(result.has_value());
        assert(result->architecture.name == "riscv64");
        assert(result->architecture.bits == 64);
    }

    // ---- 测试 6: 缺少数据时产生警告 ----
    {
        RawStore raw;
        // 不添加任何记录

        std::vector<std::string> warnings;
        auto result = parse_platform(raw, warnings);
        // 即使缺少数据，仍返回默认 Platform
        assert(result.has_value());
        // 应有警告
        assert(!warnings.empty());
    }

    return 0;
}
