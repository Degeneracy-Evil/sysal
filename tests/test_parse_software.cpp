#include "parser/software.hpp"

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
    // ---- 测试 1: 完整软件栈解析（NVIDIA 驱动 + CUDA） ----
    {
        RawStore raw;
        raw.records.push_back(make_record(
            RawSource::NvidiaSmi, "nvidia-smi",
            "+-----------------------------------------------------------------------------+\n"
            "| NVIDIA-SMI 535.129.03   Driver Version: 535.129.03   CUDA Version: 12.2     |\n"
            "+-----------------------------------------------------------------------------+\n"));
        raw.records.push_back(make_record(RawSource::Nvcc, "nvcc --version",
                                          "nvcc: NVIDIA (R) Cuda compiler driver\n"
                                          "Cuda compilation tools, release 12.4, V12.4.131\n"));

        std::vector<std::string> warnings;
        auto result = parse_software(raw, warnings);
        assert(result.has_value());

        const auto& s = *result;

        // 驱动
        assert(s.drivers.size() == 1);
        assert(s.drivers[0].name == "nvidia");
        assert(s.drivers[0].version == "535.129.03");
        assert(s.drivers[0].loaded == true);
        assert(s.drivers[0].id == DriverId{0});

        // 运行时
        assert(s.runtimes.size() == 1);
        assert(s.runtimes[0].name == "cuda");
        assert(s.runtimes[0].version == "12.4");
        assert(s.runtimes[0].env_var == "CUDA_HOME");

        // CUDA 栈
        assert(s.cuda.has_value());
        assert(s.cuda->version == "12.4");
        assert(s.cuda->driver_version == "535.129.03");

        // v0.0.1 不实现的部分
        assert(s.compilers.empty());
        assert(s.libraries.empty());
        assert(!s.rocm.has_value());
        assert(!s.level_zero.has_value());
        assert(!s.mpi.has_value());
        assert(!s.rdma.has_value());
    }

    // ---- 测试 2: 仅 NVIDIA 驱动（无 CUDA） ----
    {
        RawStore raw;
        raw.records.push_back(
            make_record(RawSource::NvidiaSmi, "nvidia-smi",
                        "| NVIDIA-SMI 470.42   Driver Version: 470.42   CUDA Version: 11.4 |\n"));

        std::vector<std::string> warnings;
        auto result = parse_software(raw, warnings);
        assert(result.has_value());
        assert(result->drivers.size() == 1);
        assert(result->drivers[0].version == "470.42");
        assert(result->runtimes.empty());
        assert(result->cuda.has_value());
        assert(result->cuda->version.empty());
        assert(result->cuda->driver_version == "470.42");
    }

    // ---- 测试 3: 仅 CUDA（无 NVIDIA 驱动） ----
    {
        RawStore raw;
        raw.records.push_back(make_record(RawSource::Nvcc, "nvcc --version",
                                          "Cuda compilation tools, release 11.8, V11.8.89\n"));

        std::vector<std::string> warnings;
        auto result = parse_software(raw, warnings);
        assert(result.has_value());
        assert(result->drivers.empty());
        assert(result->runtimes.size() == 1);
        assert(result->runtimes[0].version == "11.8");
        assert(result->cuda.has_value());
        assert(result->cuda->version == "11.8");
        assert(result->cuda->driver_version.empty());
    }

    // ---- 测试 4: 无数据 → nullopt ----
    {
        RawStore raw;
        std::vector<std::string> warnings;
        auto result = parse_software(raw, warnings);
        assert(!result.has_value());
    }

    // ---- 测试 5: nvidia-smi 格式异常 → 警告 ----
    {
        RawStore raw;
        raw.records.push_back(make_record(RawSource::NvidiaSmi, "nvidia-smi", "garbage output\n"));

        std::vector<std::string> warnings;
        auto result = parse_software(raw, warnings);
        assert(!result.has_value());
        assert(!warnings.empty());
    }

    return 0;
}
