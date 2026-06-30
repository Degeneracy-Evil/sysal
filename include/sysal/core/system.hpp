/// @file system.hpp
/// @brief System 顶层采集结果载体
/// @details 定义 System 类与 SystemInfo 扁平结构体。System 持有该次采集的
///          全部类型化数据、元数据、警告与可选原始证据，提供静态工厂
///          collect() 与刷新 refresh()。

#pragma once

#include "sysal/core/collect.hpp"
#include "sysal/core/error.hpp"
#include "sysal/model/accelerator.hpp"
#include "sysal/model/cpu.hpp"
#include "sysal/model/execution.hpp"
#include "sysal/model/memory.hpp"
#include "sysal/model/network.hpp"
#include "sysal/model/pci.hpp"
#include "sysal/model/platform.hpp"
#include "sysal/model/raw_store.hpp"
#include "sysal/model/snapshot_meta.hpp"
#include "sysal/model/software.hpp"
#include "sysal/model/storage.hpp"

#include <optional>
#include <string>
#include <vector>

namespace sysal
{

/// @brief 系统信息本体
/// @details 扁平结构，直接包含各子域，不再分 resources 中间层。
struct SystemInfo
{
    Platform platform;          ///< 系统基本标识
    Cpu cpu;                    ///< CPU 资源
    Memory memory;              ///< 内存资源
    Accelerators accelerators;  ///< 加速器资源（GPU 等）
    Network network;            ///< 网络设备
    Storage storage;            ///< 存储设备
    Pci pci;                    ///< PCI 拓扑
    SoftwareStack software;     ///< 软件栈
    ExecutionContext execution; ///< 当前进程的执行上下文
};

/// @brief 顶层采集结果载体
/// @details 调用方持有一个 System 对象即可反复读取该次采集的全部类型化数据。
///          collect() 是静态工厂，refresh() 在已有对象上重新采集。
class System
{
public:
    /// @brief 静态工厂：执行一次完整采集，默认采集全部域
    /// @param flags 采集范围位掩码
    /// @return 采集结果
    static System collect(Collect flags = full);

    /// @brief 在已有对象上重新采集，替换内部状态
    void refresh();

public:
    SystemInfo info;                   ///< 系统信息本体
    SnapshotMeta meta;                 ///< 采集元数据
    std::vector<std::string> warnings; ///< 采集过程中的警告
    std::optional<RawStore> raw;       ///< 可选原始证据
};

} // namespace sysal
