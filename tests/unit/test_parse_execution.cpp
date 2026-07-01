#include "parser/execution.hpp"

#include "sysal/model/raw_store.hpp"
#include "sysal/types/enums.hpp"

#include "test_macros.hpp"
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
        CHECK(result.has_value());

        const auto& ctx = *result;

        // 进程
        CHECK(ctx.process.pid == 1234);
        CHECK(ctx.process.ppid == 1);
        CHECK(ctx.process.uid == 0);
        CHECK(ctx.process.gid == 0);
        CHECK(ctx.process.comm == "testapp");

        // cgroup
        CHECK(ctx.cgroup.version == CgroupVersion::V2);
        CHECK(ctx.cgroup.path == "/user.slice");

        // cpuset
        CHECK(ctx.cpuset.cpus == "0-3,5");
        CHECK(ctx.cpuset.mems == "0-1");
        CHECK(ctx.cpuset.cpus_effective == "0-3,5");
        CHECK(ctx.cpuset.mems_effective == "0-1");

        // 可见逻辑 CPU
        CHECK(ctx.visible_logical_cpu_ids.size() == 5);
        CHECK(ctx.visible_logical_cpu_ids[0] == LogicalCpuId{0});
        CHECK(ctx.visible_logical_cpu_ids[1] == LogicalCpuId{1});
        CHECK(ctx.visible_logical_cpu_ids[2] == LogicalCpuId{2});
        CHECK(ctx.visible_logical_cpu_ids[3] == LogicalCpuId{3});
        CHECK(ctx.visible_logical_cpu_ids[4] == LogicalCpuId{5});

        // 权限
        CHECK(ctx.permission.euid == 0);
        CHECK(ctx.permission.egid == 0);
        CHECK(ctx.permission.is_root == true);
        CHECK(ctx.permission.capabilities.size() == 8);
        CHECK(ctx.permission.capabilities[0] == "CAP_CHOWN");
        CHECK(ctx.permission.capabilities[1] == "CAP_DAC_OVERRIDE");
        CHECK(ctx.permission.capabilities[2] == "CAP_FOWNER");
        CHECK(ctx.permission.capabilities[7] == "CAP_SETPCAP");

        // 容器
        CHECK(ctx.container.has_value());
        CHECK(ctx.container->kind == ContainerKind::Docker);

        // 环境变量
        CHECK(ctx.environment.entries.size() == 3);

        // 可见加速器
        CHECK(ctx.visible_accelerator_ids.size() == 2);
        CHECK(ctx.visible_accelerator_ids[0] == AcceleratorId{0});
        CHECK(ctx.visible_accelerator_ids[1] == AcceleratorId{1});

        // 可见网络接口（v0.0.1 为空）
        CHECK(ctx.visible_network_interface_names.empty());
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
        CHECK(result.has_value());

        const auto& ctx = *result;
        CHECK(ctx.process.pid == 5678);
        CHECK(ctx.process.uid == 1000);
        CHECK(ctx.permission.euid == 1000);
        CHECK(ctx.permission.egid == 1000);
        CHECK(ctx.permission.is_root == false);
        CHECK(!ctx.container.has_value());
        CHECK(ctx.visible_logical_cpu_ids.size() == 192);
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
        CHECK(result.has_value());
        CHECK(result->cgroup.version == CgroupVersion::V1);
        CHECK(result->cgroup.path == "/user.slice");
        CHECK(result->cgroup.controllers.size() == 2);
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
        CHECK(result.has_value());
        CHECK(result->container.has_value());
        CHECK(result->container->kind == ContainerKind::Kubernetes);
    }

    // ---- 测试 5: 无数据 → nullopt ----
    {
        RawStore raw;
        std::vector<std::string> warnings;
        auto result = parse_execution(raw, warnings);
        CHECK(!result.has_value());
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
        CHECK(result.has_value());
        CHECK(result->container.has_value());
        CHECK(result->container->kind == ContainerKind::Podman);
    }

    TEST_SUMMARY();
}
