/// @file resource_info.hpp
/// @brief 资源信息数据模型
/// @details 定义 CPU、内存、加速器、网络、存储、PCI 子系统的强类型数据结构，
///          以及聚合这些子系统的 ResourceInfo。

#pragma once

#include "sysal/enums.hpp"
#include "sysal/ids.hpp"
#include "sysal/topology_info.hpp"
#include "sysal/units.hpp"
#include "sysal/value_types.hpp"

#include <optional>
#include <vector>

namespace sysal
{

/// @brief NUMA 节点
struct NumaNode
{
    NumaNodeId id;                          ///< NUMA 节点 ID
    std::optional<MemorySize> local_memory; ///< 节点本地内存大小（可能未知）
};

/// @brief CPU 物理封装（socket）
struct CpuPackage
{
    CpuPackageId id;                         ///< 封装 ID
    Vendor vendor;                           ///< 厂商
    DeviceName model_name;                   ///< 型号名称
    std::uint32_t physical_cores{};          ///< 物理核数
    std::uint32_t logical_threads{};         ///< 逻辑线程数
    std::optional<Frequency> base_frequency; ///< 基础频率（可能未知）
    std::optional<Frequency> max_frequency;  ///< 最大频率（可能未知）
};

/// @brief CPU 物理核
struct CpuCore
{
    CpuCoreId id;                        ///< 物理核 ID
    CpuPackageId package_id;             ///< 所属封装 ID
    std::uint32_t logical_threads{};     ///< 该核上的逻辑线程数
    std::optional<NumaNodeId> numa_node; ///< 所属 NUMA 节点（可能未知）
};

/// @brief 逻辑 CPU（硬件线程）
struct LogicalCpu
{
    LogicalCpuId id;                     ///< 逻辑 CPU ID
    CpuCoreId core_id;                   ///< 所属物理核 ID
    CpuPackageId package_id;             ///< 所属封装 ID
    std::optional<NumaNodeId> numa_node; ///< 所属 NUMA 节点（可能未知）
    bool visible_to_current_process{};   ///< 当前进程是否可见
};

/// @brief CPU 子系统
/// @details 汇总封装、物理核、逻辑 CPU、NUMA 节点与 ISA 扩展信息，并提供
///          层级关系查询与可见性筛选接口。
struct CpuSubsystem
{
    Architecture arch{};                      ///< CPU 架构
    std::vector<CpuPackage> packages;         ///< 物理封装列表
    std::vector<CpuCore> cores;               ///< 物理核列表
    std::vector<LogicalCpu> logical_cpus;     ///< 逻辑 CPU 列表
    std::vector<NumaNode> numa_nodes;         ///< NUMA 节点列表
    std::vector<IsaExtension> isa_extensions; ///< 支持的 ISA 扩展列表

    /// @brief 按封装 ID 查找封装
    /// @param id 封装 ID
    /// @return 找到返回指针，否则返回 nullptr
    const CpuPackage* find_package(CpuPackageId id) const;
    /// @brief 按物理核 ID 查找物理核
    /// @param id 物理核 ID
    /// @return 找到返回指针，否则返回 nullptr
    const CpuCore* find_core(CpuCoreId id) const;
    /// @brief 按逻辑 CPU ID 查找逻辑 CPU
    /// @param id 逻辑 CPU ID
    /// @return 找到返回指针，否则返回 nullptr
    const LogicalCpu* find_logical_cpu(LogicalCpuId id) const;
    /// @brief 获取指定封装下的全部逻辑 CPU
    /// @param id 封装 ID
    /// @return 指向逻辑 CPU 的指针列表
    std::vector<const LogicalCpu*> logical_cpus_of_package(CpuPackageId id) const;
    /// @brief 获取指定物理核上的全部逻辑 CPU
    /// @param id 物理核 ID
    /// @return 指向逻辑 CPU 的指针列表
    std::vector<const LogicalCpu*> logical_cpus_of_core(CpuCoreId id) const;
    /// @brief 获取指定封装下的全部物理核
    /// @param id 封装 ID
    /// @return 指向物理核的指针列表
    std::vector<const CpuCore*> cores_of_package(CpuPackageId id) const;
    /// @brief 获取当前进程可见的全部逻辑 CPU
    /// @return 指向逻辑 CPU 的指针列表
    std::vector<const LogicalCpu*> visible_logical_cpus() const;
};

/// @brief 单个 NUMA 节点的内存信息
struct NumaMemoryInfo
{
    NumaNodeId node;                     ///< NUMA 节点 ID
    MemorySize total;                    ///< 节点内存总量
    std::optional<MemorySize> available; ///< 可用内存（可能未知）
};

/// @brief 内存子系统
struct MemorySubsystem
{
    MemorySize total_memory;                    ///< 系统内存总量
    std::optional<MemorySize> available_memory; ///< 可用内存（可能未知）
    std::vector<NumaMemoryInfo> numa_memory;    ///< 各 NUMA 节点内存信息
};

/// @brief 加速器设备
struct AcceleratorDevice
{
    AcceleratorId id;                            ///< 加速器 ID
    AcceleratorKind kind{};                      ///< 加速器类型
    Vendor vendor;                               ///< 厂商
    DeviceName name;                             ///< 设备名称
    std::optional<PciAddress> pci_address;       ///< PCI 地址（可能无）
    std::optional<NumaNodeId> nearest_numa_node; ///< 最近 NUMA 节点（可能未知）
    std::optional<MemorySize> memory_size;       ///< 设备显存/内存（可能未知）
    std::optional<DriverId> driver;              ///< 关联驱动 ID（可能无）
    bool visible_to_current_process{};           ///< 当前进程是否可见
};

/// @brief 加速器子系统
/// @details 持有全部加速器设备并提供按类型筛选、可见性筛选与查找接口。
struct AcceleratorSubsystem
{
    std::vector<AcceleratorDevice> devices; ///< 加速器设备列表

    /// @brief 按类型筛选加速器
    /// @param kind 加速器类型
    /// @return 指向匹配设备的指针列表
    std::vector<const AcceleratorDevice*> by_kind(AcceleratorKind kind) const;
    /// @brief 获取全部 GPU
    /// @return 指向 GPU 的指针列表
    std::vector<const AcceleratorDevice*> gpus() const;
    /// @brief 获取全部 NPU
    /// @return 指向 NPU 的指针列表
    std::vector<const AcceleratorDevice*> npus() const;
    /// @brief 获取全部 FPGA
    /// @return 指向 FPGA 的指针列表
    std::vector<const AcceleratorDevice*> fpgas() const;
    /// @brief 获取当前进程可见的加速器
    /// @return 指向可见设备的指针列表
    std::vector<const AcceleratorDevice*> visible() const;
    /// @brief 按加速器 ID 查找设备
    /// @param id 加速器 ID
    /// @return 找到返回指针，否则返回 nullptr
    const AcceleratorDevice* find(AcceleratorId id) const;
};

/// @brief 网络接口
struct NetworkInterface
{
    InterfaceName name;                      ///< 接口名称
    MacAddress mac;                          ///< MAC 地址
    InterfaceState state{};                  ///< 链路状态
    std::optional<Bandwidth> speed;          ///< 链路速率（可能未知）
    std::vector<IpAddress> addresses;        ///< 绑定的 IP 地址列表
    std::optional<PciAddress> pci_address;   ///< PCI 地址（可能无）
    std::optional<RdmaDeviceId> rdma_device; ///< 关联的 RDMA 设备 ID（可能无）
    bool visible_to_current_process{};       ///< 当前进程是否可见
};

/// @brief 网络子系统
/// @details 持有全部网络接口并提供可见性筛选与按名查找接口。
struct NetworkSubsystem
{
    std::vector<NetworkInterface> interfaces; ///< 网络接口列表

    /// @brief 获取当前进程可见的接口
    /// @return 指向可见接口的指针列表
    std::vector<const NetworkInterface*> visible() const;
    /// @brief 按接口名查找
    /// @param name 接口名
    /// @return 找到返回指针，否则返回 nullptr
    const NetworkInterface* find(const InterfaceName& name) const;
};

/// @brief PCI 设备
struct PciDevice
{
    PciAddress address;                  ///< PCI 地址
    Vendor vendor;                       ///< 厂商
    DeviceName device_name;              ///< 设备名称
    std::string device_class;            ///< 设备类别
    std::optional<NumaNodeId> numa_node; ///< 所属 NUMA 节点（可能未知）
};

/// @brief PCI 子系统
struct PciSubsystem
{
    std::vector<PciDevice> devices; ///< PCI 设备列表

    /// @brief 按 PCI 地址查找设备
    /// @param addr PCI 地址
    /// @return 找到返回指针，否则返回 nullptr
    const PciDevice* find(PciAddress addr) const;
};

/// @brief 存储设备
struct StorageDevice
{
    StorageId id;                          ///< 存储设备 ID
    DeviceName name;                       ///< 设备名称
    std::optional<MemorySize> capacity;    ///< 容量（可能未知）
    std::optional<PciAddress> pci_address; ///< PCI 地址（可能无）
    StorageKind kind{};                    ///< 存储类型
};

/// @brief 存储子系统
struct StorageSubsystem
{
    std::vector<StorageDevice> devices; ///< 存储设备列表
};

/// @brief 资源信息聚合
/// @details 汇总 CPU、内存、加速器、网络、存储、PCI 与拓扑等全部资源子系统。
struct ResourceInfo
{
    CpuSubsystem cpu;                  ///< CPU 子系统
    MemorySubsystem memory;            ///< 内存子系统
    AcceleratorSubsystem accelerators; ///< 加速器子系统
    NetworkSubsystem network;          ///< 网络子系统
    StorageSubsystem storage;          ///< 存储子系统
    PciSubsystem pci;                  ///< PCI 子系统
    TopologyInfo topology;             ///< 拓扑信息
};

} // namespace sysal
