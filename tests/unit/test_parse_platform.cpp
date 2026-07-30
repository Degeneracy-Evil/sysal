#include "parser/platform.hpp"

#include "sysal/model/raw_store.hpp"
#include "sysal/types/enums.hpp"

#include "test_macros.hpp"
#include <chrono>
#include <string>
#include <vector>

using namespace sysal;
using namespace sysal::detail;

/// @brief 创建一条 RawRecord
static RawRecord make_record(RawSource source, const std::string &path, const std::string &payload)
{
    return RawRecord{source, path, payload, CollectStatus::Success, std::chrono::system_clock::now()};
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
        raw.records.push_back(make_record(RawSource::ProcVersion, "uname",
                                          "5.15.0-91-generic\n#101-Ubuntu SMP Mon Nov 13 18:02:07 UTC 2023"));
        raw.records.push_back(make_record(RawSource::Uname, "uname", "x86_64"));
        raw.records.push_back(
            make_record(RawSource::SysfsDmi, "/sys/class/dmi/id/bios_vendor", "American Megatrends Inc.\n"));
        raw.records.push_back(make_record(RawSource::SysfsDmi, "/sys/class/dmi/id/bios_version", "1.0.0\n"));
        raw.records.push_back(make_record(RawSource::SysfsDmi, "/sys/class/dmi/id/bios_date", "01/01/2023\n"));
        raw.records.push_back(make_record(RawSource::SysfsDmi, "/sys/class/dmi/id/product_name", "PowerEdge R750\n"));
        raw.records.push_back(make_record(RawSource::SysfsDmi, "/sys/class/dmi/id/sys_vendor", "Dell Inc.\n"));
        raw.records.push_back(make_record(RawSource::SysfsDmi, "/sys/class/dmi/id/product_serial", "ABCD1234\n"));

        std::vector<std::string> warnings;
        auto result = parse_platform(raw, warnings);
        CHECK(result.has_value());

        const auto &p = *result;

        // Os
        CHECK(p.os.name == "Ubuntu");
        CHECK(p.os.version == "22.04");
        CHECK(p.os.distribution == "ubuntu");
        CHECK(p.os.codename == "jammy");

        // Kernel
        CHECK(p.kernel.release == "5.15.0-91-generic");
        CHECK(p.kernel.version == "5.15.0");
        CHECK(p.kernel.compiled_at == "#101-Ubuntu SMP Mon Nov 13 18:02:07 UTC 2023");
        CHECK(p.kernel.architecture == "x86_64");

        // Architecture
        CHECK(p.architecture.name == "x86_64");
        CHECK(p.architecture.bits == 64);
        CHECK(p.architecture.byte_order == "little");

        // Host (DMI)
        CHECK(p.host.product_name == "PowerEdge R750");
        CHECK(p.host.vendor.value == "Dell Inc.");
        CHECK(p.host.serial == "ABCD1234");

        // Firmware
        CHECK(p.firmware.has_value());
        CHECK(p.firmware->bios_vendor.value == "American Megatrends Inc.");
        CHECK(p.firmware->bios_version == "1.0.0");
        CHECK(p.firmware->bios_date == "01/01/2023");

        // Virtualization: 无数据，应为 nullopt
        CHECK(!p.virtualization.has_value());
    }

    // ---- 测试 2: 容器环境不再产生 Virtualization（容器信息由 ExecutionContext 承载） ----
    {
        RawStore raw;
        raw.records.push_back(make_record(RawSource::EtcOsRelease, "/etc/os-release", "NAME=\"Ubuntu\"\n"));
        raw.records.push_back(make_record(RawSource::ProcVersion, "uname", "5.15.0-91-generic\n#101-Ubuntu SMP ..."));
        raw.records.push_back(make_record(RawSource::Uname, "uname", "x86_64"));
        raw.records.push_back(make_record(RawSource::ProcOneCgroup, "/proc/1/cgroup", "0::/docker/abc123\n"));
        raw.records.push_back(make_record(RawSource::RootDockerenv, "/.dockerenv", ""));

        std::vector<std::string> warnings;
        auto result = parse_platform(raw, warnings);
        CHECK(result.has_value());
        // detect_virtualization 仅检测硬件虚拟化，容器不再产生 Virtualization
        CHECK(!result->virtualization.has_value());
    }

    // ---- 测试 3: 虚拟化检测（KVM，通过 DMI sys_vendor） ----
    {
        RawStore raw;
        raw.records.push_back(make_record(RawSource::EtcOsRelease, "/etc/os-release", "NAME=\"Ubuntu\"\n"));
        raw.records.push_back(make_record(RawSource::ProcVersion, "uname", "5.15.0-91-generic\n#101-Ubuntu SMP ..."));
        raw.records.push_back(make_record(RawSource::Uname, "uname", "x86_64"));
        raw.records.push_back(make_record(RawSource::SysfsDmi, "/sys/class/dmi/id/sys_vendor", "KVM\n"));
        raw.records.push_back(
            make_record(RawSource::SysfsDmi, "/sys/class/dmi/id/product_name", "KVM Virtual Machine\n"));

        std::vector<std::string> warnings;
        auto result = parse_platform(raw, warnings);
        CHECK(result.has_value());
        CHECK(result->virtualization.has_value());
        CHECK(result->virtualization->kind == VirtualizationKind::Kvm);
        CHECK(result->virtualization->hypervisor == "KVM");
    }

    // ---- 测试 4: 虚拟化检测（Xen，通过 /sys/hypervisor/type） ----
    {
        RawStore raw;
        raw.records.push_back(make_record(RawSource::EtcOsRelease, "/etc/os-release", "NAME=\"Ubuntu\"\n"));
        raw.records.push_back(make_record(RawSource::ProcVersion, "uname", "5.15.0-91-generic\n#101-Ubuntu SMP ..."));
        raw.records.push_back(make_record(RawSource::Uname, "uname", "x86_64"));
        raw.records.push_back(make_record(RawSource::SysHypervisor, "/sys/hypervisor/type", "xen\n"));

        std::vector<std::string> warnings;
        auto result = parse_platform(raw, warnings);
        CHECK(result.has_value());
        CHECK(result->virtualization.has_value());
        CHECK(result->virtualization->kind == VirtualizationKind::Xen);
        CHECK(result->virtualization->hypervisor == "Xen");
    }

    // ---- 测试 5: 虚拟化检测（VMware，通过 DMI sys_vendor） ----
    {
        RawStore raw;
        raw.records.push_back(make_record(RawSource::EtcOsRelease, "/etc/os-release", "NAME=\"Ubuntu\"\n"));
        raw.records.push_back(make_record(RawSource::ProcVersion, "uname", "5.15.0-91-generic\n#101-Ubuntu SMP ..."));
        raw.records.push_back(make_record(RawSource::Uname, "uname", "x86_64"));
        raw.records.push_back(make_record(RawSource::SysfsDmi, "/sys/class/dmi/id/sys_vendor", "VMware, Inc.\n"));
        raw.records.push_back(make_record(RawSource::SysfsDmi, "/sys/class/dmi/id/product_name", "VMware7,1\n"));

        std::vector<std::string> warnings;
        auto result = parse_platform(raw, warnings);
        CHECK(result.has_value());
        CHECK(result->virtualization.has_value());
        CHECK(result->virtualization->kind == VirtualizationKind::Vmware);
        CHECK(result->virtualization->hypervisor == "VMware");
    }

    // ---- 测试 6: 虚拟化检测（Hyper-V，通过 DMI sys_vendor + product_name） ----
    {
        RawStore raw;
        raw.records.push_back(make_record(RawSource::EtcOsRelease, "/etc/os-release", "NAME=\"Ubuntu\"\n"));
        raw.records.push_back(make_record(RawSource::ProcVersion, "uname", "5.15.0-91-generic\n#101-Ubuntu SMP ..."));
        raw.records.push_back(make_record(RawSource::Uname, "uname", "x86_64"));
        raw.records.push_back(
            make_record(RawSource::SysfsDmi, "/sys/class/dmi/id/sys_vendor", "Microsoft Corporation\n"));
        raw.records.push_back(make_record(RawSource::SysfsDmi, "/sys/class/dmi/id/product_name", "Virtual Machine\n"));

        std::vector<std::string> warnings;
        auto result = parse_platform(raw, warnings);
        CHECK(result.has_value());
        CHECK(result->virtualization.has_value());
        CHECK(result->virtualization->kind == VirtualizationKind::HyperV);
        CHECK(result->virtualization->hypervisor == "Hyper-V");
    }

    // ---- 测试 7: 虚拟化检测（QEMU，通过 DMI sys_vendor） ----
    {
        RawStore raw;
        raw.records.push_back(make_record(RawSource::EtcOsRelease, "/etc/os-release", "NAME=\"Ubuntu\"\n"));
        raw.records.push_back(make_record(RawSource::ProcVersion, "uname", "5.15.0-91-generic\n#101-Ubuntu SMP ..."));
        raw.records.push_back(make_record(RawSource::Uname, "uname", "x86_64"));
        raw.records.push_back(make_record(RawSource::SysfsDmi, "/sys/class/dmi/id/sys_vendor", "QEMU\n"));

        std::vector<std::string> warnings;
        auto result = parse_platform(raw, warnings);
        CHECK(result.has_value());
        CHECK(result->virtualization.has_value());
        CHECK(result->virtualization->kind == VirtualizationKind::Qemu);
        CHECK(result->virtualization->hypervisor == "QEMU");
    }

    // ---- 测试 8: 虚拟化检测（VirtualBox，通过 DMI product_name） ----
    {
        RawStore raw;
        raw.records.push_back(make_record(RawSource::EtcOsRelease, "/etc/os-release", "NAME=\"Ubuntu\"\n"));
        raw.records.push_back(make_record(RawSource::ProcVersion, "uname", "5.15.0-91-generic\n#101-Ubuntu SMP ..."));
        raw.records.push_back(make_record(RawSource::Uname, "uname", "x86_64"));
        raw.records.push_back(make_record(RawSource::SysfsDmi, "/sys/class/dmi/id/sys_vendor", "innotek GmbH\n"));
        raw.records.push_back(make_record(RawSource::SysfsDmi, "/sys/class/dmi/id/product_name", "VirtualBox\n"));

        std::vector<std::string> warnings;
        auto result = parse_platform(raw, warnings);
        CHECK(result.has_value());
        CHECK(result->virtualization.has_value());
        CHECK(result->virtualization->kind == VirtualizationKind::VirtualBox);
        CHECK(result->virtualization->hypervisor == "VirtualBox");
    }

    // ---- 测试 9: 虚拟化检测（Parallels，通过 DMI sys_vendor） ----
    {
        RawStore raw;
        raw.records.push_back(make_record(RawSource::EtcOsRelease, "/etc/os-release", "NAME=\"Ubuntu\"\n"));
        raw.records.push_back(make_record(RawSource::ProcVersion, "uname", "5.15.0-91-generic\n#101-Ubuntu SMP ..."));
        raw.records.push_back(make_record(RawSource::Uname, "uname", "x86_64"));
        raw.records.push_back(make_record(RawSource::SysfsDmi, "/sys/class/dmi/id/sys_vendor",
                                          "Parallels Software International Inc.\n"));

        std::vector<std::string> warnings;
        auto result = parse_platform(raw, warnings);
        CHECK(result.has_value());
        CHECK(result->virtualization.has_value());
        CHECK(result->virtualization->kind == VirtualizationKind::Parallels);
        CHECK(result->virtualization->hypervisor == "Parallels");
    }

    // ---- 测试 10: 虚拟化检测（Xen HVM，通过 DMI sys_vendor） ----
    {
        RawStore raw;
        raw.records.push_back(make_record(RawSource::EtcOsRelease, "/etc/os-release", "NAME=\"Ubuntu\"\n"));
        raw.records.push_back(make_record(RawSource::ProcVersion, "uname", "5.15.0-91-generic\n#101-Ubuntu SMP ..."));
        raw.records.push_back(make_record(RawSource::Uname, "uname", "x86_64"));
        raw.records.push_back(make_record(RawSource::SysfsDmi, "/sys/class/dmi/id/sys_vendor", "Xen\n"));
        raw.records.push_back(make_record(RawSource::SysfsDmi, "/sys/class/dmi/id/product_name", "HVM domU\n"));

        std::vector<std::string> warnings;
        auto result = parse_platform(raw, warnings);
        CHECK(result.has_value());
        CHECK(result->virtualization.has_value());
        CHECK(result->virtualization->kind == VirtualizationKind::Xen);
        CHECK(result->virtualization->hypervisor == "Xen");
    }

    // ---- 测试 11: 虚拟化检测（cpuinfo hypervisor flag → Other + warning） ----
    {
        RawStore raw;
        raw.records.push_back(make_record(RawSource::EtcOsRelease, "/etc/os-release", "NAME=\"Ubuntu\"\n"));
        raw.records.push_back(make_record(RawSource::ProcVersion, "uname", "5.15.0-91-generic\n#101-Ubuntu SMP ..."));
        raw.records.push_back(make_record(RawSource::Uname, "uname", "x86_64"));
        raw.records.push_back(
            make_record(RawSource::ProcCpuInfo, "/proc/cpuinfo",
                        "processor\t: 0\nvendor_id\t: GenuineIntel\nflags\t\t: fpu vme de hypervisor\n"));

        std::vector<std::string> warnings;
        auto result = parse_platform(raw, warnings);
        CHECK(result.has_value());
        CHECK(result->virtualization.has_value());
        CHECK(result->virtualization->kind == VirtualizationKind::Other);
        CHECK(result->virtualization->hypervisor == "Unknown");
        bool has_virt_warning = false;
        for(const auto &w : warnings)
        {
            if(w.find("detect_virtualization") != std::string::npos)
            {
                has_virt_warning = true;
                break;
            }
        }
        CHECK(has_virt_warning);
    }

    // ---- 测试 12: 物理机（无虚拟化检测，无 warning） ----
    {
        RawStore raw;
        raw.records.push_back(make_record(RawSource::EtcOsRelease, "/etc/os-release", "NAME=\"Ubuntu\"\n"));
        raw.records.push_back(make_record(RawSource::ProcVersion, "uname", "5.15.0-91-generic\n#101-Ubuntu SMP ..."));
        raw.records.push_back(make_record(RawSource::Uname, "uname", "x86_64"));
        raw.records.push_back(make_record(RawSource::SysfsDmi, "/sys/class/dmi/id/sys_vendor", "Inspur\n"));
        raw.records.push_back(make_record(RawSource::SysfsDmi, "/sys/class/dmi/id/product_name", "NF5280M6\n"));
        raw.records.push_back(make_record(RawSource::ProcCpuInfo, "/proc/cpuinfo",
                                          "processor\t: 0\nvendor_id\t: GenuineIntel\nflags\t\t: fpu vme de aes\n"));

        std::vector<std::string> warnings;
        auto result = parse_platform(raw, warnings);
        CHECK(result.has_value());
        CHECK(!result->virtualization.has_value());
    }

    // ---- 测试 13: Xen 优先级高于 DMI（/sys/hypervisor/type 先命中） ----
    {
        RawStore raw;
        raw.records.push_back(make_record(RawSource::EtcOsRelease, "/etc/os-release", "NAME=\"Ubuntu\"\n"));
        raw.records.push_back(make_record(RawSource::ProcVersion, "uname", "5.15.0-91-generic\n#101-Ubuntu SMP ..."));
        raw.records.push_back(make_record(RawSource::Uname, "uname", "x86_64"));
        raw.records.push_back(make_record(RawSource::SysHypervisor, "/sys/hypervisor/type", "xen\n"));
        raw.records.push_back(make_record(RawSource::SysfsDmi, "/sys/class/dmi/id/sys_vendor", "KVM\n"));

        std::vector<std::string> warnings;
        auto result = parse_platform(raw, warnings);
        CHECK(result.has_value());
        CHECK(result->virtualization.has_value());
        CHECK(result->virtualization->kind == VirtualizationKind::Xen);
    }

    // ---- 测试 14: aarch64 架构 ----
    {
        RawStore raw;
        raw.records.push_back(make_record(RawSource::EtcOsRelease, "/etc/os-release", "NAME=\"Ubuntu\"\n"));
        raw.records.push_back(make_record(RawSource::ProcVersion, "uname", "5.15.0-91-generic\n#101-Ubuntu SMP ..."));
        raw.records.push_back(make_record(RawSource::Uname, "uname", "aarch64"));

        std::vector<std::string> warnings;
        auto result = parse_platform(raw, warnings);
        CHECK(result.has_value());
        CHECK(result->architecture.name == "aarch64");
        CHECK(result->architecture.bits == 64);
    }

    // ---- 测试 15: riscv64 架构 ----
    {
        RawStore raw;
        raw.records.push_back(make_record(RawSource::EtcOsRelease, "/etc/os-release", "NAME=\"Ubuntu\"\n"));
        raw.records.push_back(make_record(RawSource::ProcVersion, "uname", "5.15.0-91-generic\n#101-Ubuntu SMP ..."));
        raw.records.push_back(make_record(RawSource::Uname, "uname", "riscv64"));

        std::vector<std::string> warnings;
        auto result = parse_platform(raw, warnings);
        CHECK(result.has_value());
        CHECK(result->architecture.name == "riscv64");
        CHECK(result->architecture.bits == 64);
    }

    // ---- 测试 16: 缺少数据时产生警告 ----
    {
        RawStore raw;
        // 不添加任何记录

        std::vector<std::string> warnings;
        auto result = parse_platform(raw, warnings);
        // 即使缺少数据，仍返回默认 Platform
        CHECK(result.has_value());
        // 应有警告
        CHECK(!warnings.empty());
    }

    TEST_SUMMARY();
}
