#include "parser/software.hpp"

#include "sysal/model/raw_store.hpp"
#include "sysal/types/enums.hpp"

#include "test_macros.hpp"
#include <chrono>
#include <string>
#include <vector>

using namespace sysal;
using namespace sysal::detail;

static RawRecord make_record(RawSource source, const std::string &path, const std::string &payload)
{
    return RawRecord{source, path, payload, CollectStatus::Success, std::chrono::system_clock::now()};
}

int main()
{
    // ---- 测试 1: 完整软件栈解析（NVIDIA 驱动 + CUDA） ----
    {
        RawStore raw;
        raw.records.push_back(make_record(RawSource::NvidiaSmi, "nvidia-smi",
                                          "0, NVIDIA A100 80GB PCIe, 81920 MiB, 00000000:65:00.0, 535.129.03\n"
                                          "1, NVIDIA A100 80GB PCIe, 81920 MiB, 00000000:ca:00.0, 535.129.03\n"));
        raw.records.push_back(make_record(RawSource::Nvcc, "nvcc --version",
                                          "nvcc: NVIDIA (R) Cuda compiler driver\n"
                                          "Cuda compilation tools, release 12.4, V12.4.131\n"));

        std::vector<std::string> warnings;
        auto result = parse_software(raw, warnings);
        CHECK(result.has_value());

        const auto &s = *result;

        // 驱动
        CHECK(s.drivers.size() == 1);
        CHECK(s.drivers[0].name == "nvidia");
        CHECK(s.drivers[0].version == "535.129.03");
        CHECK(s.drivers[0].loaded == true);
        CHECK(s.drivers[0].id == DriverId{0});

        // 运行时
        CHECK(s.runtimes.size() == 1);
        CHECK(s.runtimes[0].name == "cuda");
        CHECK(s.runtimes[0].version == "12.4");
        CHECK(s.runtimes[0].env_var == "CUDA_HOME");

        // CUDA 栈
        CHECK(s.cuda.has_value());
        CHECK(s.cuda->version == "12.4");
        CHECK(s.cuda->driver_version == "535.129.03");

        // v0.0.1 不实现的部分
        CHECK(s.compilers.empty());
        CHECK(s.libraries.empty());
        CHECK(!s.rocm.has_value());
        CHECK(!s.level_zero.has_value());
        CHECK(!s.mpi.has_value());
        CHECK(!s.rdma.has_value());
    }

    // ---- 测试 2: 仅 NVIDIA 驱动（无 CUDA） ----
    {
        RawStore raw;
        raw.records.push_back(make_record(RawSource::NvidiaSmi, "nvidia-smi",
                                          "0, NVIDIA GeForce RTX 3090, 24576 MiB, 00000000:01:00.0, 470.42\n"));

        std::vector<std::string> warnings;
        auto result = parse_software(raw, warnings);
        CHECK(result.has_value());
        CHECK(result->drivers.size() == 1);
        CHECK(result->drivers[0].version == "470.42");
        CHECK(result->runtimes.empty());
        CHECK(result->cuda.has_value());
        CHECK(result->cuda->version.empty());
        CHECK(result->cuda->driver_version == "470.42");
    }

    // ---- 测试 3: 仅 CUDA（无 NVIDIA 驱动） ----
    {
        RawStore raw;
        raw.records.push_back(
            make_record(RawSource::Nvcc, "nvcc --version", "Cuda compilation tools, release 11.8, V11.8.89\n"));

        std::vector<std::string> warnings;
        auto result = parse_software(raw, warnings);
        CHECK(result.has_value());
        CHECK(result->drivers.empty());
        CHECK(result->runtimes.size() == 1);
        CHECK(result->runtimes[0].version == "11.8");
        CHECK(result->cuda.has_value());
        CHECK(result->cuda->version == "11.8");
        CHECK(result->cuda->driver_version.empty());
    }

    // ---- 测试 4: 无数据 → nullopt ----
    {
        RawStore raw;
        std::vector<std::string> warnings;
        auto result = parse_software(raw, warnings);
        CHECK(!result.has_value());
    }

    // ---- 测试 6: 单编译器探测（gcc）----
    {
        RawStore raw;
        raw.records.push_back(make_record(RawSource::CompilerVersion, "gcc --version",
                                          "gcc (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0\n"
                                          "Copyright (C) 2023 Free Software Foundation, Inc.\n"));
        raw.records.push_back(make_record(RawSource::CompilerPath, "command -v gcc", "/usr/bin/gcc\n"));
        raw.records.push_back(make_record(RawSource::CompilerTarget, "gcc -dumpmachine", "x86_64-linux-gnu\n"));

        // 缺失的 clang 等不产生记录 → 静默跳过
        std::vector<std::string> warnings;
        auto result = parse_software(raw, warnings);
        CHECK(result.has_value());
        CHECK(warnings.empty());
        CHECK(result->drivers.empty());
        CHECK(result->runtimes.empty());

        CHECK(result->compilers.size() == 1);
        CHECK(result->compilers[0].name == "gcc");
        CHECK(result->compilers[0].version == "13.3.0");
        CHECK(result->compilers[0].path == "/usr/bin/gcc");
        CHECK(result->compilers[0].target == "x86_64-linux-gnu");
    }

    // ---- 测试 7: 多编译器混合格式（clang 格式不同）----
    {
        RawStore raw;
        raw.records.push_back(
            make_record(RawSource::CompilerVersion, "gcc --version", "gcc (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0\n"));
        raw.records.push_back(make_record(RawSource::CompilerVersion, "clang --version",
                                          "Ubuntu clang version 18.1.3 (1ubuntu1)\nTarget: x86_64-pc-linux-gnu\n"));

        std::vector<std::string> warnings;
        auto result = parse_software(raw, warnings);
        CHECK(result.has_value());

        CHECK(result->compilers.size() == 2);
        CHECK(result->compilers[0].name == "gcc");
        CHECK(result->compilers[0].version == "13.3.0");
        CHECK(result->compilers[1].name == "clang");
        CHECK(result->compilers[1].version == "18.1.3");
    }

    // ---- 测试 8: gfortran 版本提取 ----
    {
        RawStore raw;
        raw.records.push_back(make_record(RawSource::CompilerVersion, "gfortran --version",
                                          "GNU Fortran (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0\n"));

        std::vector<std::string> warnings;
        auto result = parse_software(raw, warnings);
        CHECK(result.has_value());
        CHECK(result->compilers.size() == 1);
        CHECK(result->compilers[0].name == "gfortran");
        CHECK(result->compilers[0].version == "13.3.0");
    }

    // ---- 测试 9: 编译器版本格式异常 → 静默跳过，不产生警告 ----
    {
        RawStore raw;
        raw.records.push_back(make_record(RawSource::CompilerVersion, "gcc --version", "garbled output\n"));

        std::vector<std::string> warnings;
        auto result = parse_software(raw, warnings);
        CHECK(!result.has_value());
        CHECK(warnings.empty());
    }

    // ---- 测试 10: 仅编译器，无 nvidia/nvcc → 返回编译器栈 ----
    {
        RawStore raw;
        raw.records.push_back(
            make_record(RawSource::CompilerVersion, "g++ --version", "g++ (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0\n"));

        std::vector<std::string> warnings;
        auto result = parse_software(raw, warnings);
        CHECK(result.has_value());
        CHECK(result->compilers.size() == 1);
        CHECK(result->compilers[0].name == "g++");
        CHECK(warnings.empty());
    }

    // ---- 测试 5: nvidia-smi 格式异常 → 警告 ----
    {
        RawStore raw;
        raw.records.push_back(make_record(RawSource::NvidiaSmi, "nvidia-smi", "garbage output\n"));

        std::vector<std::string> warnings;
        auto result = parse_software(raw, warnings);
        CHECK(!result.has_value());
        CHECK(!warnings.empty());
    }

    TEST_SUMMARY();
}
