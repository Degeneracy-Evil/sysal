/// @file software_stack_info.hpp
/// @brief 软件栈信息数据模型
/// @details 描述驱动、运行时、编译器、库以及 CUDA/ROCm/Level Zero/MPI/RDMA
///          等软件栈组件信息。

#pragma once

#include "sysal/value_types.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace sysal
{

/// @brief 驱动信息
struct DriverInfo
{
    std::string name;    ///< 驱动名称
    std::string version; ///< 驱动版本
    bool loaded{};       ///< 是否已加载
};

/// @brief 运行时信息
struct RuntimeInfo
{
    std::string name;    ///< 运行时名称
    std::string version; ///< 运行时版本
    std::string path;    ///< 运行时路径
};

/// @brief 编译器信息
struct CompilerInfo
{
    std::string name;    ///< 编译器名称
    std::string version; ///< 编译器版本
    std::string path;    ///< 编译器路径
};

/// @brief 库信息
struct LibraryInfo
{
    std::string name;    ///< 库名称
    std::string version; ///< 库版本
    std::string path;    ///< 库路径
};

/// @brief CUDA 环境信息
struct CudaInfo
{
    std::string driver_version;   ///< CUDA 驱动版本
    std::string runtime_version;  ///< CUDA 运行时版本
    std::uint32_t device_count{}; ///< 可见 CUDA 设备数
};

/// @brief ROCm 环境信息
struct RocmInfo
{
    std::string version; ///< ROCm 版本
};

/// @brief Level Zero 环境信息
struct LevelZeroInfo
{
    std::string version; ///< Level Zero 版本
};

/// @brief MPI 环境信息
struct MpiInfo
{
    std::string implementation; ///< MPI 实现（如 OpenMPI、MPICH）
    std::string version;        ///< MPI 版本
};

/// @brief RDMA 软件栈信息
struct RdmaStackInfo
{
    bool ibverbs_available{};              ///< libibverbs 是否可用
    std::vector<std::string> rdma_devices; ///< RDMA 设备名列表
};

/// @brief 软件栈信息聚合
/// @details 汇总驱动、运行时、编译器、库及各并行计算/RDMA 栈信息。
struct SoftwareStackInfo
{
    std::vector<DriverInfo> drivers;         ///< 驱动列表
    std::vector<RuntimeInfo> runtimes;       ///< 运行时列表
    std::vector<CompilerInfo> compilers;     ///< 编译器列表
    std::vector<LibraryInfo> libraries;      ///< 库列表
    std::optional<CudaInfo> cuda;            ///< CUDA 信息（可能不存在）
    std::optional<RocmInfo> rocm;            ///< ROCm 信息（可能不存在）
    std::optional<LevelZeroInfo> level_zero; ///< Level Zero 信息（可能不存在）
    std::optional<MpiInfo> mpi;              ///< MPI 信息（可能不存在）
    std::optional<RdmaStackInfo> rdma;       ///< RDMA 栈信息（可能不存在）
};

} // namespace sysal
