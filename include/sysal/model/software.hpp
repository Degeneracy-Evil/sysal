/// @file software.hpp
/// @brief 软件栈数据模型
/// @details 定义软件栈的数据结构：SoftwareStack 及其子结构体（Driver、Runtime、
///          Compiler、Library、Cuda、Rocm、LevelZero、Mpi、RdmaStack），
///          描述系统上的驱动、运行时、编译器、库及专用软件栈。

#pragma once

#include "sysal/types/ids.hpp"
#include "sysal/types/value_types.hpp"

#include <optional>
#include <string>
#include <vector>

namespace sysal
{

    /// @brief 单个驱动
    struct Driver
    {
        DriverId id;         ///< 驱动 ID
        std::string name;    ///< 驱动名称
        std::string version; ///< 驱动版本
        bool loaded{};       ///< 是否已加载
        std::string path;    ///< 驱动路径
    };

    /// @brief 单个运行时
    struct Runtime
    {
        std::string name;    ///< 运行时名称
        std::string version; ///< 运行时版本
        std::string path;    ///< 运行时路径
        std::string env_var; ///< 关联环境变量
    };

    /// @brief 单个编译器
    struct Compiler
    {
        std::string name;    ///< 编译器名称
        std::string version; ///< 编译器版本
        std::string path;    ///< 编译器路径
        std::string target;  ///< 目标架构
    };

    /// @brief 单个库
    struct Library
    {
        std::string name;    ///< 库名称
        std::string version; ///< 库版本
        std::string path;    ///< 库路径
        std::string kind;    ///< 库类型（BLAS/FFT/...）
    };

    /// @brief CUDA 栈
    struct Cuda
    {
        std::string version;        ///< CUDA 版本
        std::string driver_version; ///< 驱动版本
        std::string nvcc_path;      ///< nvcc 路径
        std::string home;           ///< CUDA_HOME
    };

    /// @brief ROCm 栈
    struct Rocm
    {
        std::string version;   ///< ROCm 版本
        std::string hip_path;  ///< HIP 路径
        std::string rocm_path; ///< ROCm 路径
    };

    /// @brief Level Zero 栈
    struct LevelZero
    {
        std::string version;     ///< Level Zero 版本
        std::string loader_path; ///< 加载器路径
    };

    /// @brief MPI 栈
    struct Mpi
    {
        std::string implementation; ///< MPI 实现（OpenMPI/MPICH/...）
        std::string version;        ///< MPI 版本
        std::string path;           ///< MPI 路径
    };

    /// @brief RDMA 栈
    struct RdmaStack
    {
        std::string rdma_core_version; ///< rdma-core 版本
        std::string ibverbs_path;      ///< ibverbs 路径
        std::string ucx_version;       ///< UCX 版本
    };

    /// @brief 软件栈
    /// @details 描述系统上的驱动、运行时、编译器、库及专用软件栈。
    struct SoftwareStack
    {
        std::vector<Driver> drivers;     ///< 已安装的驱动
        std::vector<Runtime> runtimes;   ///< 已安装的运行时
        std::vector<Compiler> compilers; ///< 已安装的编译器
        std::vector<Library> libraries;  ///< 已安装的库

        std::optional<Cuda> cuda;            ///< CUDA 栈（若存在）
        std::optional<Rocm> rocm;            ///< ROCm 栈（若存在）
        std::optional<LevelZero> level_zero; ///< Level Zero 栈（若存在）
        std::optional<Mpi> mpi;              ///< MPI 栈（若存在）
        std::optional<RdmaStack> rdma;       ///< RDMA 栈（若存在）
    };

} // namespace sysal
