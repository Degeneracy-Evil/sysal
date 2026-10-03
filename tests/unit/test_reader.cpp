#include "reader/linux/file_utils.hpp"
#include "reader/linux/procfs.hpp"
#include "reader/linux/sysfs.hpp"

#include "sysal/core/collect.hpp"
#include "sysal/model/raw_store.hpp"
#include "sysal/types/enums.hpp"

#include <doctest/doctest.h>
#include <iostream>

TEST_CASE("test_reader")
{
    // ---- file_utils 测试 ----

    // read_file 成功
    auto cpuinfo = sysal::reader::read_file("/proc/cpuinfo");
    REQUIRE(cpuinfo.has_value());
    if(!cpuinfo.has_value())
    {
        return;
    }
    CHECK(!cpuinfo->empty());

    // read_file 失败
    auto nonexistent = sysal::reader::read_file("/nonexistent_file_for_test");
    CHECK(!nonexistent.has_value());

    // file_exists
    CHECK(sysal::reader::file_exists("/proc/cpuinfo"));
    CHECK(!sysal::reader::file_exists("/nonexistent_file_for_test"));

    // ---- procfs 采集测试 ----

    sysal::RawStore raw;
    sysal::reader::read_procfs(raw, sysal::full);

    // ProcCpuInfo
    CHECK(raw.has(sysal::RawSource::ProcCpuInfo));
    CHECK(raw.count(sysal::RawSource::ProcCpuInfo) > 0);

    // ProcMemInfo
    CHECK(raw.has(sysal::RawSource::ProcMemInfo));

    // ProcVersion
    CHECK(raw.has(sysal::RawSource::ProcVersion));

    // EtcOsRelease
    CHECK(raw.has(sysal::RawSource::EtcOsRelease));

    // Uname
    CHECK(raw.has(sysal::RawSource::Uname));

    // ProcSelfCgroup
    CHECK(raw.has(sysal::RawSource::ProcSelfCgroup));

    // ProcSelfStatus
    CHECK(raw.has(sysal::RawSource::ProcSelfStatus));

    // ProcOneCgroup
    CHECK(raw.has(sysal::RawSource::ProcOneCgroup));

    // Environment
    CHECK(raw.has(sysal::RawSource::Environment));

    // RootDockerenv（可能 NotCollected，但记录应存在）
    CHECK(raw.has(sysal::RawSource::RootDockerenv));

    // ProcHostname（gethostname 系统调用，始终可用）
    CHECK(raw.has(sysal::RawSource::ProcHostname));

    // IfAddrs（getifaddrs 系统调用，始终可用）
    CHECK(raw.has(sysal::RawSource::IfAddrs));

    // StorageMountInfo（当前进程命名空间的挂载清单）
    CHECK(raw.has(sysal::RawSource::StorageMountInfo));

    // ---- sysfs 采集测试 ----

    sysal::RawStore raw2;
    sysal::reader::read_sysfs(raw2, sysal::full);

    // SysfsCpu
    CHECK(raw2.has(sysal::RawSource::SysfsCpu));

    // SysfsNuma（可能无 NUMA，但记录应存在）
    CHECK(raw2.has(sysal::RawSource::SysfsNuma));

    // SysfsNet
    CHECK(raw2.has(sysal::RawSource::SysfsNet));

    // SysfsPci
    CHECK(raw2.has(sysal::RawSource::SysfsPci));

    // SysfsBlock
    CHECK(raw2.has(sysal::RawSource::SysfsBlock));

    // SysfsDmi（虚拟机可能无 DMI，但记录应存在）
    CHECK(raw2.has(sysal::RawSource::SysfsDmi));

    // ---- 合并采集测试 ----

    sysal::RawStore raw3;
    sysal::reader::read_procfs(raw3, sysal::full);
    sysal::reader::read_sysfs(raw3, sysal::full);

    // 验证合并后记录数
    CHECK(raw3.count(sysal::RawSource::ProcCpuInfo) > 0);
    CHECK(raw3.count(sysal::RawSource::SysfsCpu) > 0);

    // 验证采集状态
    auto cpuinfo_records = raw3.get_all(sysal::RawSource::ProcCpuInfo);
    CHECK(!cpuinfo_records.empty());
    CHECK(cpuinfo_records[0]->status == sysal::CollectStatus::Success);
}
