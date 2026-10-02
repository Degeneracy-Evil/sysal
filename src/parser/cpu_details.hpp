#pragma once

#include "sysal/model/cpu.hpp"
#include "sysal/model/raw_store.hpp"

#include <optional>
#include <string_view>

namespace sysal::detail
{
    /// @brief 解析内核 CPU 列表（逗号/空白分隔和范围）；异常或过大列表返回未知。
    std::optional<std::vector<LogicalCpuId>> parse_cpu_id_list(std::string_view value);
    void read_cpu_details(const RawStore &raw, Cpu &cpu, std::vector<std::string> &warnings);
} // namespace sysal::detail
