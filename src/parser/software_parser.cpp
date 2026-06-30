/// @file software_parser.cpp
/// @brief 软件栈解析器实现
/// @details 解析 NVIDIA 相关的命令输出，提取 NVRM 内核驱动版本、
///          nvcc 报告的 CUDA Toolkit 版本以及 nvidia-smi 统计的设备数量，
///          并据此组装 SoftwareStackInfo（驱动列表、运行时列表、CudaInfo）。

#include "software_parser.hpp"
#include "parse_utils.hpp"
#include "parsed_facts.hpp"

#include "sysal/diagnostics.hpp"
#include "sysal/raw_store.hpp"

#include <string>

namespace sysal::detail
{

namespace
{

/// @brief 判断一个 token 是否构成版本号字符串
/// @param token 待判断的子串
/// @return 仅由数字和点号组成且至少含一个数字时返回 true
/// @details 用于从 NVRM version 行中识别形如 "535.183.06" 的版本号片段。
bool is_version_token(const std::string& token)
{
    if(token.empty())
    {
        return false;
    }
    bool has_digit = false;
    for(char c : token)
    {
        if(c >= '0' && c <= '9')
        {
            has_digit = true;
        }
        else if(c != '.')
        {
            // 出现非数字、非点号字符则不是版本号
            return false;
        }
    }
    return has_digit;
}

/// @brief 从 /proc/driver/nvidia/version 输出中提取 NVRM 驱动版本
/// @param payload 文件内容，包含以 "NVRM version" 开头的行
/// @return 提取到的版本号字符串；未找到时返回 std::nullopt
/// @details 找到 "NVRM version" 行后，按空格切分并取第一个符合版本号格式的 token。
std::optional<std::string> extract_nvrm_driver_version(const std::string& payload)
{
    auto lines = split(payload, '\n');
    for(const auto& line : lines)
    {
        auto trimmed = trim(line);
        if(!trimmed.starts_with("NVRM version"))
        {
            continue;
        }
        // 在该行中逐个 token 寻找版本号
        auto parts = split(trimmed, ' ');
        for(const auto& part : parts)
        {
            if(is_version_token(part))
            {
                return part;
            }
        }
    }
    return std::nullopt;
}

/// @brief 从 nvcc --version 输出中提取 CUDA 发布版本
/// @param payload nvcc --version 的输出文本
/// @return 形如 "12.4" 的版本字符串；未找到时返回 std::nullopt
/// @details 典型行包含 "release 12.4, V12.4.131"，解析时定位 "release "
///          之后、逗号之前的内容作为版本号。
std::optional<std::string> extract_cuda_release_version(const std::string& payload)
{
    auto lines = split(payload, '\n');
    for(const auto& line : lines)
    {
        auto trimmed = trim(line);
        auto pos = trimmed.find("release ");
        if(pos == std::string::npos)
        {
            continue;
        }
        // 跳过 "release " 8 个字符
        auto rest = std::string_view(trimmed).substr(pos + 8);
        auto comma = rest.find(',');
        if(comma == std::string_view::npos)
        {
            continue;
        }
        // 取逗号之前的部分作为版本号
        auto version = trim(rest.substr(0, comma));
        if(!version.empty())
        {
            return version;
        }
    }
    return std::nullopt;
}

/// @brief 从 nvidia-smi CSV 输出中提取驱动版本
/// @param payload nvidia-smi --query-gpu=...,driver_version --format=csv 的输出
/// @return CSV 行最后一个字段的值；无有效行时返回 std::nullopt
/// @details CSV 行至少 5 列时才视为有效数据行，驱动版本位于最后一列。
std::optional<std::string> extract_smi_driver_version(const std::string& payload)
{
    auto lines = split(payload, '\n');
    for(const auto& line : lines)
    {
        auto trimmed = trim(line);
        if(trimmed.empty())
        {
            continue;
        }
        auto fields = split(trimmed, ',');
        // 列数不足视为非数据行
        if(fields.size() < 5)
        {
            continue;
        }
        auto version = trim(fields.back());
        if(!version.empty())
        {
            return version;
        }
    }
    return std::nullopt;
}

/// @brief 统计 nvidia-smi CSV 输出中的设备数量
/// @param payload nvidia-smi CSV 输出文本
/// @return 有效数据行数（字段数 >= 5 的行），即 GPU 数量
std::uint32_t count_smi_devices(const std::string& payload)
{
    std::uint32_t count = 0;
    auto lines = split(payload, '\n');
    for(const auto& line : lines)
    {
        auto trimmed = trim(line);
        if(trimmed.empty())
        {
            continue;
        }
        auto fields = split(trimmed, ',');
        // 每个字段数 >= 5 的行对应一个 GPU
        if(fields.size() >= 5)
        {
            ++count;
        }
    }
    return count;
}

} // namespace

/// @brief 从 RawStore 解析软件栈信息
/// @param raw 原始数据存储
/// @param diag 诊断信息容器，用于记录解析过程中的告警
/// @return 解析出驱动或 CUDA 版本时返回 SoftwareStackInfo；否则返回 std::nullopt
/// @details 遍历所有 RawSource::NvidiaSmi 记录，按 path_or_command 分派到
///          不同的提取器。采集失败的记录会产生告警。
///          最终将驱动版本、CUDA 版本、设备数量汇总为 SoftwareStackInfo。
std::optional<SoftwareStackInfo> parse_software_stack(const RawStore& raw, Diagnostics& diag)
{
    std::optional<std::string> driver_version;
    std::optional<std::string> cuda_version;
    std::uint32_t device_count = 0;

    for(const auto& record : raw.records)
    {
        // 仅处理 NVIDIA 相关采集记录
        if(record.source != RawSource::NvidiaSmi)
        {
            continue;
        }
        if(record.status != CollectStatus::Success)
        {
            // 采集失败时记录告警
            add_warning(diag, "NVIDIA data collection failed for: " + record.path_or_command,
                        RawSource::NvidiaSmi);
            continue;
        }

        if(record.path_or_command == "/proc/driver/nvidia/version")
        {
            // 从 NVRM version 文件提取内核驱动版本
            auto v = extract_nvrm_driver_version(record.payload);
            if(v && !driver_version)
            {
                driver_version = v;
            }
        }
        else if(record.path_or_command == "nvcc --version")
        {
            // 从 nvcc 输出提取 CUDA Toolkit 版本
            auto v = extract_cuda_release_version(record.payload);
            if(v && !cuda_version)
            {
                cuda_version = v;
            }
        }
        else if(record.path_or_command.starts_with("nvidia-smi"))
        {
            // 从 nvidia-smi CSV 统计设备数量并提取驱动版本
            device_count += count_smi_devices(record.payload);
            auto v = extract_smi_driver_version(record.payload);
            if(v && !driver_version)
            {
                driver_version = v;
            }
        }
    }

    // 没有任何可用版本信息时认为不存在软件栈
    if(!driver_version && !cuda_version)
    {
        return std::nullopt;
    }

    SoftwareStackInfo info{};

    // 填充驱动列表
    if(driver_version)
    {
        info.drivers.push_back({
            .name = "nvidia",
            .version = *driver_version,
            .loaded = true,
        });
    }

    // 填充运行时列表
    if(cuda_version)
    {
        info.runtimes.push_back({
            .name = "cuda",
            .version = *cuda_version,
            .path = "",
        });
    }

    // 汇总 CUDA 相关信息：驱动版本、运行时版本、可见设备数
    info.cuda = CudaInfo{
        .driver_version = driver_version.value_or(""),
        .runtime_version = cuda_version.value_or(""),
        .device_count = device_count,
    };

    return info;
}

} // namespace sysal::detail
