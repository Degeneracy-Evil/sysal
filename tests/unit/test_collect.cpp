#include "sysal/core/error.hpp"
#include "sysal/core/system.hpp"
#include "sysal/model/raw_store.hpp"
#include "sysal/test/replay.hpp"
#include "sysal/version.hpp"

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
        assert(sys.meta.sysal_version == sysal::VERSION_STRING);

        // 请求的 flags
        assert(has(sys.meta.requested_flags, Collect::Platform));
        assert(has(sys.meta.requested_flags, Collect::Execution));

        // 采集耗时非负
        assert(sys.meta.collect_duration.count() >= 0.0);
    }

    // ---- 测试 4: 全部采集器失败时抛出 SysalError ----
    {
        RawStore empty_raw;
        bool threw = false;
        try
        {
            auto sys = sysal::test::collect_from_raw(empty_raw, Collect::Cpu);
            (void)sys;
        }
        catch(const SysalError& e)
        {
            assert(e.kind() == ErrorKind::CollectionFailed);
            threw = true;
        }
        assert(threw);
    }

    return 0;
}
