#include "sysal/core/system.hpp"

#include <cassert>
#include <string>

using namespace sysal;

int main()
{
    // ---- 测试 1: System::collect 基本冒烟测试 ----
    {
        auto sys = System::collect(Collect::Cpu | Collect::Memory | Collect::Platform);

        // CPU 逻辑 CPU 列表不为空
        assert(!sys.info.cpu.logical_cpus.empty());

        // 成功采集器列表不为空
        assert(!sys.meta.succeeded_collectors.empty());

        // 平台 OS 名称不为空
        assert(!sys.info.platform.os.name.empty());

        // 内存总量大于 0
        assert(sys.info.memory.total_memory.value > 0);
    }

    // ---- 测试 2: System::refresh 保持同一对象 ----
    {
        auto sys = System::collect(Collect::Cpu | Collect::Memory);
        auto original_cpu_count = sys.info.cpu.logical_cpus.size();

        sys.refresh();

        // 刷新后 CPU 数量应与之前一致
        assert(sys.info.cpu.logical_cpus.size() == original_cpu_count);
    }

    // ---- 测试 3: 采集元数据正确 ----
    {
        auto sys = System::collect(Collect::Platform | Collect::Execution);

        // sysal 版本
        assert(sys.meta.sysal_version == "0.0.1");

        // 请求的 flags
        assert(has(sys.meta.requested_flags, Collect::Platform));
        assert(has(sys.meta.requested_flags, Collect::Execution));

        // 采集耗时非负
        assert(sys.meta.collect_duration.count() >= 0.0);
    }

    return 0;
}
