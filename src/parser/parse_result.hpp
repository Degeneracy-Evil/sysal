/// @file parse_result.hpp
/// @brief 解析结果内部契约
/// @details 定义 ParseResult 结构体，作为 Parser 与 Resolver 之间的内部契约。
///          每个字段为 optional，表示该域可能失败或未被请求。

#pragma once

#include "sysal/model/accelerator.hpp"
#include "sysal/model/cpu.hpp"
#include "sysal/model/execution.hpp"
#include "sysal/model/memory.hpp"
#include "sysal/model/network.hpp"
#include "sysal/model/pci.hpp"
#include "sysal/model/platform.hpp"
#include "sysal/model/software.hpp"
#include "sysal/model/storage.hpp"

#include <optional>

namespace sysal::detail
{

/// @brief 解析结果
/// @details 各域独立解析，每个字段为 optional —— 该域可能失败或未被请求。
struct ParseResult
{
    std::optional<Platform> platform;
    std::optional<Cpu> cpu;
    std::optional<Memory> memory;
    std::optional<Pci> pci;
    std::optional<Network> network;
    std::optional<Accelerators> accelerators;
    std::optional<Storage> storage;
    std::optional<SoftwareStack> software;
    std::optional<ExecutionContext> execution;
};

} // namespace sysal::detail
