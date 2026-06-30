/// @file system.cpp
/// @brief System 公共 API 实现
/// @details 实现 System::collect() 和 System::refresh()，
///          委托给 pipeline 模块执行完整采集管线。

#include "pipeline/pipeline.hpp"

#include "sysal/core/system.hpp"

#include <utility>
#include <vector>

namespace sysal
{

System System::collect(Collect flags)
{
    std::vector<std::string> warnings;
    return detail::run_pipeline(flags, warnings);
}

void System::refresh()
{
    auto flags = meta.requested_flags;
    std::vector<std::string> warnings;
    auto new_sys = detail::run_pipeline(flags, warnings);
    *this = std::move(new_sys);
}

} // namespace sysal
