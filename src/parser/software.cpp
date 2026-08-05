/// @file software.cpp
/// @brief 软件栈解析器实现
/// @details 从 nvidia-smi/nvcc 命令输出解析驱动和运行时信息。

#include "software.hpp"

#include "parse_utils.hpp"

#include <cctype>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sysal::detail
{

    namespace
    {

        /// @brief 从 nvidia-smi CSV 输出中提取驱动版本
        /// @param payload nvidia-smi --query-gpu=...,driver_version --format=csv,noheader 输出
        /// @return 驱动版本字符串（若找到）
        std::optional<std::string> extract_nvidia_driver_version(std::string_view payload)
        {
            // CSV 格式: index,name,memory.total,pci.bus_id,driver_version
            // 示例: 0, NVIDIA A100 80GB PCIe, 81920 MiB, 00000000:65:00.0, 595.58.03
            auto lines = split(payload, '\n');
            for(const auto &line : lines)
            {
                auto trimmed_line = trim(line);
                if(trimmed_line.empty())
                {
                    continue;
                }
                auto fields = split(trimmed_line, ',');
                if(fields.size() >= 5)
                {
                    auto version = trim(fields[4]);
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
            for(const auto &line : lines)
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

        /// @brief 从编译器 --version 首行提取 X.Y.Z 版本号
        /// @param payload 编译器 --version 命令输出
        /// @return 版本号字符串（若找到）
        /// @details 兼容 gcc/g++/clang/gfortran 格式：
        ///          gcc:    "gcc (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0"
        ///          clang:  "Ubuntu clang version 18.1.3 (1ubuntu1)"
        ///          gfortran: "GNU Fortran (Ubuntu 13.3.0) 13.3.0"
        ///          统一策略：首个形如 X.Y.Z（纯数字段）的 token 即为版本。
        std::optional<std::string> extract_compiler_version(std::string_view payload)
        {
            auto lines = split(payload, '\n');
            if(lines.empty())
            {
                return std::nullopt;
            }
            const auto first_line = split(trim(lines.front()), ' ');
            for(const auto &token : first_line)
            {
                auto t = trim(token);
                if(t.empty())
                {
                    continue;
                }
                // 按 '.' 拆分，验证段数=3 且每段为纯数字
                auto parts = split(t, '.');
                if(parts.size() != 3)
                {
                    continue;
                }
                bool valid = true;
                for(const auto &part : parts)
                {
                    if(part.empty())
                    {
                        valid = false;
                        break;
                    }
                    for(char ch : part)
                    {
                        if(!std::isdigit(static_cast<unsigned char>(ch)))
                        {
                            valid = false;
                            break;
                        }
                    }
                    if(!valid)
                    {
                        break;
                    }
                }
                if(valid)
                {
                    return std::string(t);
                }
            }
            return std::nullopt;
        }

        /// @brief 从 RawStore 中取某来源的首条 Success 记录
        /// @param raw 原始证据存储
        /// @param source 原始数据来源
        /// @param match 次级键前缀（命令字符串），用于区分同来源多个命令
        /// @return 匹配的首条 Success 记录；无则返回 nullopt
        const RawRecord *first_success(const RawStore &raw, RawSource source, std::string_view match)
        {
            for(const auto *rec : raw.get_all(source))
            {
                if(rec->status == CollectStatus::Success && rec->path_or_command.find(match) != std::string::npos)
                {
                    return rec;
                }
            }
            return nullptr;
        }

    } // namespace

    std::optional<SoftwareStack> parse_software(const RawStore &raw, std::vector<std::string> &warnings)
    {
        // 提取 NVIDIA 驱动版本
        std::optional<std::string> driver_version;
        auto nvidia_records = raw.get_all(RawSource::NvidiaSmi);
        const RawRecord *nvidia_rec = nullptr;
        for(const auto *rec : nvidia_records)
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
        const RawRecord *nvcc_rec = nullptr;
        for(const auto *rec : nvcc_records)
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

        SoftwareStack stack;

        // 编译器：逐个探测 gcc/g++/clang/clang++/gfortran
        // 缺失的命令在 reader 层静默记 Failed，此处仅收集 Success 记录，不产生 warning
        static const char *const compiler_names[] = {"gcc", "g++", "clang", "clang++", "gfortran"};
        auto compiler_version_records = raw.get_all(RawSource::CompilerVersion);
        const bool has_compiler_data = !compiler_version_records.empty();
        for(const char *cc : compiler_names)
        {
            auto ver_rec = first_success(raw, RawSource::CompilerVersion, cc);
            if(ver_rec == nullptr)
            {
                continue;
            }
            auto version = extract_compiler_version(ver_rec->payload);
            if(!version.has_value())
            {
                continue;
            }

            Compiler compiler;
            compiler.name = cc;
            compiler.version = *version;
            if(auto path_rec = first_success(raw, RawSource::CompilerPath, cc))
            {
                compiler.path = trim(path_rec->payload);
            }
            if(auto target_rec = first_success(raw, RawSource::CompilerTarget, cc))
            {
                compiler.target = trim(target_rec->payload);
            }
            stack.compilers.push_back(std::move(compiler));
        }
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

        // 无任何可解析的软件（无 nvidia-smi、无 nvcc、无编译器）
        if(stack.drivers.empty() && stack.runtimes.empty() && stack.compilers.empty())
        {
            // 仅当完全不采集到任何软件数据时才告警；数据存在但解析失败属静默场景不告警
            if(nvidia_records.empty() && nvcc_records.empty() && !has_compiler_data)
            {
                warnings.push_back("parse_software: 无 nvidia-smi/nvcc/编译器数据");
            }
            return std::nullopt;
        }

        // v0.0.1：库、ROCm、Level Zero、MPI、RDMA 均不实现
        // stack.libraries 保持空
        // stack.rocm 保持 nullopt
        // stack.level_zero 保持 nullopt
        // stack.mpi 保持 nullopt
        // stack.rdma 保持 nullopt

        return stack;
    }

} // namespace sysal::detail
