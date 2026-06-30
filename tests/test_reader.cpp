#include "reader/linux/file_utils.hpp"
#include "reader/linux/procfs.hpp"
#include "reader/linux/sysfs.hpp"

#include <sysal/core/collect.hpp>
#include <sysal/model/raw_store.hpp>
#include <sysal/types/enums.hpp>

#include <cassert>
#include <iostream>

int main()
{
    // ---- file_utils 测试 ----

    // read_file 成功
    auto cpuinfo = sysal::reader::read_file("/proc/cpuinfo");
    assert(cpuinfo.has_value());
    assert(!cpuinfo->empty());

    // read_file 失败
    auto nonexistent = sysal::reader::read_file("/nonexistent_file_for_test");
    assert(!nonexistent.has_value());

    // file_exists
    assert(sysal::reader::file_exists("/proc/cpuinfo"));
    assert(!sysal::reader::file_exists("/nonexistent_file_for_test"));

    // ---- procfs 采集测试 ----

    sysal::RawStore raw;
    sysal::reader::read_procfs(raw, sysal::full);

    // ProcCpuInfo
    assert(raw.has(sysal::RawSource::ProcCpuInfo));
    assert(raw.count(sysal::RawSource::ProcCpuInfo) > 0);

    // ProcMemInfo
    assert(raw.has(sysal::RawSource::ProcMemInfo));

    // ProcVersion
    assert(raw.has(sysal::RawSource::ProcVersion));

    // EtcOsRelease
    assert(raw.has(sysal::RawSource::EtcOsRelease));

    // Uname
    assert(raw.has(sysal::RawSource::Uname));

    // ProcSelfCgroup
    assert(raw.has(sysal::RawSource::ProcSelfCgroup));

    // ProcSelfStatus
    assert(raw.has(sysal::RawSource::ProcSelfStatus));

    // ProcOneCgroup
    assert(raw.has(sysal::RawSource::ProcOneCgroup));

    // Environment
    assert(raw.has(sysal::RawSource::Environment));

    // RootDockerenv（可能 NotCollected，但记录应存在）
    assert(raw.has(sysal::RawSource::RootDockerenv));

    // ---- sysfs 采集测试 ----

    sysal::RawStore raw2;
    sysal::reader::read_sysfs(raw2, sysal::full);

    // SysfsCpu
    assert(raw2.has(sysal::RawSource::SysfsCpu));

    // SysfsNuma（可能无 NUMA，但记录应存在）
    assert(raw2.has(sysal::RawSource::SysfsNuma));

    // SysfsNet
    assert(raw2.has(sysal::RawSource::SysfsNet));

    // SysfsPci
    assert(raw2.has(sysal::RawSource::SysfsPci));

    // SysfsBlock
    assert(raw2.has(sysal::RawSource::SysfsBlock));

    // SysfsDmi（虚拟机可能无 DMI，但记录应存在）
    assert(raw2.has(sysal::RawSource::SysfsDmi));

    // ---- 合并采集测试 ----

    sysal::RawStore raw3;
    sysal::reader::read_procfs(raw3, sysal::full);
    sysal::reader::read_sysfs(raw3, sysal::full);

    // 验证合并后记录数
    assert(raw3.count(sysal::RawSource::ProcCpuInfo) > 0);
    assert(raw3.count(sysal::RawSource::SysfsCpu) > 0);

    // 验证采集状态
    auto cpuinfo_records = raw3.get_all(sysal::RawSource::ProcCpuInfo);
    assert(!cpuinfo_records.empty());
    assert(cpuinfo_records[0]->status == sysal::CollectStatus::Success);

    std::cout << "test_reader: all assertions passed\n";
    return 0;
}
