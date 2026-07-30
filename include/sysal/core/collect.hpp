/// @file collect.hpp
/// @brief 采集范围位掩码
/// @details 定义 Collect 位掩码枚举，用于按域组合采集范围。
///          替代旧有的 builder 模式，提供轻量的值类型组合。

#pragma once

#include <cstdint>

namespace sysal
{

    /// @brief 采集范围位掩码
    /// @details 通过按位或组合多个域，控制 System::collect() 的采集范围。
    enum class Collect : std::uint32_t
    {
        Platform = 1 << 0,
        Cpu = 1 << 1,
        Memory = 1 << 2,
        Accelerator = 1 << 3,
        Network = 1 << 4,
        Storage = 1 << 5,
        Pci = 1 << 6,
        Software = 1 << 7,
        Execution = 1 << 8,
        Raw = 1 << 9,
    };

    /// @brief 按位或，组合多个采集域
    /// @param a 左操作数
    /// @param b 右操作数
    /// @return 组合后的位掩码
    constexpr Collect operator|(Collect a, Collect b)
    {
        return static_cast<Collect>(static_cast<std::uint32_t>(a) | static_cast<std::uint32_t>(b));
    }

    /// @brief 测试 flags 中是否包含 test 位
    /// @param flags 待测位掩码
    /// @param test 待检测位
    /// @return flags 包含 test 时返回 true
    constexpr bool has(Collect flags, Collect test)
    {
        return (static_cast<std::uint32_t>(flags) & static_cast<std::uint32_t>(test)) != 0;
    }

    /// @brief 预设：基本子集
    constexpr Collect basic = Collect::Platform | Collect::Cpu | Collect::Memory | Collect::Execution;

    /// @brief 预设：全部域
    constexpr Collect full = Collect::Platform | Collect::Cpu | Collect::Memory | Collect::Accelerator |
                             Collect::Network | Collect::Storage | Collect::Pci | Collect::Software |
                             Collect::Execution | Collect::Raw;

} // namespace sysal
