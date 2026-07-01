/// @file software.cpp
/// @brief 软件栈解析器实现
/// @details 从 nvidia-smi/nvcc 命令输出解析驱动和运行时信息。

#include "software.hpp"

#include "parse_utils.hpp"

#include <string_view>

namespace sysal::detail
{

namespace
{

/// @brief 从 nvidia-smi 输出中提取驱动版本
/// @param payload nvidia-smi 命令输出
/// @return 驱动版本字符串（若找到）
std::optional<std::string> extract_nvidia_driver_version(std::string_view payload)
{
    // nvidia-smi 输出格式示例：
    // +-----------------------------------------------------------------------------+
    // | NVIDIA-SMI 535.129.03   Driver Version: 535.129.03   CUDA Version: 12.2     |
    auto lines = split(payload, '\n');
    for(const auto& line : lines)
    {
        auto trimmed_line = trim(line);
        auto pos = trimmed_line.find("Driver Version:");
        if(pos != std::string::npos)
        {
            auto rest = trimmed_line.substr(pos + 15); // "Driver Version:" 长度
            auto version = trim(rest);
            // 版本号后可能还有其他字段，取第一个空格前的部分
            auto space = version.find(' ');
            if(space != std::string::npos)
            {
                version = version.substr(0, space);
            }
            if(!version.empty())
            {
                return std::string(version);
            }
        }
    }
    return std::nullopt;
}

/// @brief 从 nvcc --version 输出中提取 CUDA 版本
/// @param payload nvcc --version 命令输出
/// @return CUDA 版本字符串（若找到）
std::optional<std::string> extract_cuda_version(std::string_view payload)
{
    // nvcc --version 输出格式示例：
    // nvcc: NVIDIA (R) Cuda compiler driver
    // Cuda compilation tools, release 12.4, V12.4.131
    auto lines = split(payload, '\n');
    for(const auto& line : lines)
    {
        auto trimmed_line = trim(line);
        auto pos = trimmed_line.find("release ");
        if(pos != std::string::npos)
        {
            auto rest = trimmed_line.substr(pos + 8); // "release " 长度
            // 版本号后跟逗号，取逗号前的部分
            auto comma = rest.find(',');
            if(comma != std::string::npos)
            {
                auto version = trim(rest.substr(0, comma));
                if(!version.empty())
                {
                    return std::string(version);
                }
            }
        }
    }
    return std::nullopt;
}

} // namespace

std::optional<SoftwareStack> parse_software(const RawStore& raw, std::vector<std::string>& warnings)
{
    // 提取 NVIDIA 驱动版本
    std::optional<std::string> driver_version;
    auto nvidia_records = raw.get_all(RawSource::NvidiaSmi);
    const RawRecord* nvidia_rec = nullptr;
    for(const auto* rec : nvidia_records)
    {
        if(rec->status == CollectStatus::Success)
        {
            nvidia_rec = rec;
            break;
        }
    }
    if(nvidia_rec != nullptr)
    {
        driver_version = extract_nvidia_driver_version(nvidia_rec->payload);
        if(!driver_version.has_value())
        {
            warnings.push_back("parse_software: nvidia-smi 输出中未找到 Driver Version");
        }
    }

    // 提取 CUDA 版本
    std::optional<std::string> cuda_version;
    auto nvcc_records = raw.get_all(RawSource::Nvcc);
    const RawRecord* nvcc_rec = nullptr;
    for(const auto* rec : nvcc_records)
    {
        if(rec->status == CollectStatus::Success)
        {
            nvcc_rec = rec;
            break;
        }
    }
    if(nvcc_rec != nullptr)
    {
        cuda_version = extract_cuda_version(nvcc_rec->payload);
        if(!cuda_version.has_value())
        {
            warnings.push_back("parse_software: nvcc --version 输出中未找到 release 版本");
        }
    }

    // 若既无 nvidia-smi 也无 nvcc 数据源，则无软件栈可解析
    if(nvidia_records.empty() && nvcc_records.empty())
    {
        warnings.push_back("parse_software: 无 nvidia-smi/nvcc 数据");
        return std::nullopt;
    }

    // 数据源存在但驱动版本与 CUDA 版本均解析失败
    if(!driver_version.has_value() && !cuda_version.has_value())
    {
        return std::nullopt;
    }

    SoftwareStack stack;

    // NVIDIA 驱动
    if(driver_version.has_value())
    {
        Driver nvidia_driver;
        nvidia_driver.id = DriverId{0};
        nvidia_driver.name = "nvidia";
        nvidia_driver.version = *driver_version;
        nvidia_driver.loaded = true;
        nvidia_driver.path = "";
        stack.drivers.push_back(std::move(nvidia_driver));
    }

    // CUDA 运行时
    if(cuda_version.has_value())
    {
        Runtime cuda_runtime;
        cuda_runtime.name = "cuda";
        cuda_runtime.version = *cuda_version;
        cuda_runtime.path = "";
        cuda_runtime.env_var = "CUDA_HOME";
        stack.runtimes.push_back(std::move(cuda_runtime));
    }

    // CUDA 栈
    if(cuda_version.has_value() || driver_version.has_value())
    {
        Cuda cuda;
        cuda.version = cuda_version.value_or("");
        cuda.driver_version = driver_version.value_or("");
        cuda.nvcc_path = "";
        cuda.home = "";
        stack.cuda = std::move(cuda);
    }

    // v0.0.1：编译器、库、ROCm、Level Zero、MPI、RDMA 均不实现
    // stack.compilers 保持空
    // stack.libraries 保持空
    // stack.rocm 保持 nullopt
    // stack.level_zero 保持 nullopt
    // stack.mpi 保持 nullopt
    // stack.rdma 保持 nullopt

    return stack;
}

} // namespace sysal::detail
