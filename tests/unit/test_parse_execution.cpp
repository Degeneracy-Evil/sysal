#include "parser/execution.hpp"

#include "sysal/model/raw_store.hpp"
#include "sysal/types/enums.hpp"

#include <cassert>
#include <chrono>
#include <string>
#include <vector>

using namespace sysal;
using namespace sysal::detail;

static RawRecord make_record(RawSource source, const std::string& path, const std::string& payload)
{
    return RawRecord{source, path, payload, CollectStatus::Success,
                     std::chrono::system_clock::now()};
}

int main()
{
    // ---- 测试 1: 完整执行上下文解析 ----
    {
        RawStore raw;
        raw.records.push_back(make_record(RawSource::ProcSelfStatus, "/proc/self/status",
                                          "Name:\ttestapp\n"
                                          "Pid:\t1234\n"
                                          "PPid:\t1\n"
                                          "Uid:\t0\t0\t0\t0\n"
                                          "Gid:\t0\t0\t0\t0\n"
                                          "Cpus_allowed_list:\t0-3,5\n"
                                          "Mems_allowed_list:\t0-1\n"
                                          "CapEff:\t000001ff\n"));
        raw.records.push_back(
            make_record(RawSource::ProcSelfCgroup, "/proc/self/cgroup", "0::/user.slice\n"));
        raw.records.push_back(make_record(RawSource::RootDockerenv, "/.dockerenv", ""));
        raw.records.push_back(make_record(RawSource::Environment, "environ",
                                          "CUDA_VISIBLE_DEVICES=0,1\n"
                                          "OMP_NUM_THREADS=8\n"
                                          "HOME=/root\n"));

        std::vector<std::string> warnings;
        auto result = parse_execution(raw, warnings);
        assert(result.has_value());

        const auto& ctx = *result;

        // 进程
        assert(ctx.process.pid == 1234);
        assert(ctx.process.ppid == 1);
        assert(ctx.process.uid == 0);
        assert(ctx.process.gid == 0);
        assert(ctx.process.comm == "testapp");

        // cgroup
        assert(ctx.cgroup.version == CgroupVersion::V2);
        assert(ctx.cgroup.path == "/user.slice");

        // cpuset
        assert(ctx.cpuset.cpus == "0-3,5");
        assert(ctx.cpuset.mems == "0-1");
        assert(ctx.cpuset.cpus_effective == "0-3,5");
        assert(ctx.cpuset.mems_effective == "0-1");

        // 可见逻辑 CPU
        assert(ctx.visible_logical_cpu_ids.size() == 5);
        assert(ctx.visible_logical_cpu_ids[0] == LogicalCpuId{0});
        assert(ctx.visible_logical_cpu_ids[1] == LogicalCpuId{1});
        assert(ctx.visible_logical_cpu_ids[2] == LogicalCpuId{2});
        assert(ctx.visible_logical_cpu_ids[3] == LogicalCpuId{3});
        assert(ctx.visible_logical_cpu_ids[4] == LogicalCpuId{5});

        // 权限
        assert(ctx.permission.euid == 0);
        assert(ctx.permission.egid == 0);
        assert(ctx.permission.is_root == true);
        assert(ctx.permission.capabilities.size() == 8);
        assert(ctx.permission.capabilities[0] == "CAP_CHOWN");
        assert(ctx.permission.capabilities[1] == "CAP_DAC_OVERRIDE");
        assert(ctx.permission.capabilities[2] == "CAP_FOWNER");
        assert(ctx.permission.capabilities[7] == "CAP_SETPCAP");

        // 容器
        assert(ctx.container.has_value());
        assert(ctx.container->kind == ContainerKind::Docker);

        // 环境变量
        assert(ctx.environment.entries.size() == 3);

        // 可见加速器
        assert(ctx.visible_accelerator_ids.size() == 2);
        assert(ctx.visible_accelerator_ids[0] == AcceleratorId{0});
        assert(ctx.visible_accelerator_ids[1] == AcceleratorId{1});

        // 可见网络接口（v0.0.1 为空）
        assert(ctx.visible_network_interface_names.empty());
    }

    // ---- 测试 2: 非特权用户 ----
    {
        RawStore raw;
        raw.records.push_back(make_record(RawSource::ProcSelfStatus, "/proc/self/status",
                                          "Name:\tmyapp\n"
                                          "Pid:\t5678\n"
                                          "PPid:\t100\n"
                                          "Uid:\t1000\t1000\t1000\t1000\n"
                                          "Gid:\t1000\t1000\t1000\t1000\n"
                                          "Cpus_allowed_list:\t0-191\n"
                                          "Mems_allowed_list:\t0-7\n"));

        std::vector<std::string> warnings;
        auto result = parse_execution(raw, warnings);
        assert(result.has_value());

        const auto& ctx = *result;
        assert(ctx.process.pid == 5678);
        assert(ctx.process.uid == 1000);
        assert(ctx.permission.euid == 1000);
        assert(ctx.permission.egid == 1000);
        assert(ctx.permission.is_root == false);
        assert(!ctx.container.has_value());
        assert(ctx.visible_logical_cpu_ids.size() == 192);
    }

    // ---- 测试 3: cgroup v1 ----
    {
        RawStore raw;
        raw.records.push_back(make_record(RawSource::ProcSelfStatus, "/proc/self/status",
                                          "Pid:\t1\n"
                                          "Uid:\t0\t0\t0\t0\n"
                                          "Gid:\t0\t0\t0\t0\n"));
        raw.records.push_back(make_record(RawSource::ProcSelfCgroup, "/proc/self/cgroup",
                                          "1:cpu,cpuacct:/user.slice\n"));

        std::vector<std::string> warnings;
        auto result = parse_execution(raw, warnings);
        assert(result.has_value());
        assert(result->cgroup.version == CgroupVersion::V1);
        assert(result->cgroup.path == "/user.slice");
        assert(result->cgroup.controllers.size() == 2);
    }

    // ---- 测试 4: Kubernetes 容器检测 ----
    {
        RawStore raw;
        raw.records.push_back(make_record(RawSource::ProcSelfStatus, "/proc/self/status",
                                          "Pid:\t1\n"
                                          "Uid:\t0\t0\t0\t0\n"
                                          "Gid:\t0\t0\t0\t0\n"));
        raw.records.push_back(make_record(RawSource::ProcOneCgroup, "/proc/1/cgroup",
                                          "0::/kubepods/besteffort/pod1234\n"));
        raw.records.push_back(
            make_record(RawSource::Environment, "environ", "KUBERNETES_SERVICE_HOST=10.0.0.1\n"));

        std::vector<std::string> warnings;
        auto result = parse_execution(raw, warnings);
        assert(result.has_value());
        assert(result->container.has_value());
        assert(result->container->kind == ContainerKind::Kubernetes);
    }

    // ---- 测试 5: 无数据 → nullopt ----
    {
        RawStore raw;
        std::vector<std::string> warnings;
        auto result = parse_execution(raw, warnings);
        assert(!result.has_value());
    }

    // ---- 测试 6: Podman 容器检测（环境变量） ----
    {
        RawStore raw;
        raw.records.push_back(make_record(RawSource::ProcSelfStatus, "/proc/self/status",
                                          "Pid:\t1\n"
                                          "Uid:\t0\t0\t0\t0\n"
                                          "Gid:\t0\t0\t0\t0\n"));
        raw.records.push_back(make_record(RawSource::Environment, "environ", "container=podman\n"));

        std::vector<std::string> warnings;
        auto result = parse_execution(raw, warnings);
        assert(result.has_value());
        assert(result->container.has_value());
        assert(result->container->kind == ContainerKind::Podman);
    }

    return 0;
}
