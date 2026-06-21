#include "sysal/collect.hpp"

#include "detail/pipeline.hpp"
#include "reader/linux/procfs_reader.hpp"
#include "reader/linux/sysfs_reader.hpp"

#include <chrono>
#include <utility>

namespace sysal
{

Expected<SystemSnapshot, SysalError> collect(const CollectSpec& spec)
{
    const auto start_time = std::chrono::system_clock::now();

    RawStore raw;

    detail::read_procfs(raw, spec);
    detail::read_sysfs(raw, spec);

    return detail::run_pipeline(raw, spec, start_time);
}

SystemSnapshot collect_or_throw(const CollectSpec& spec)
{
    auto result = collect(spec);
    if(!result)
    {
        throw result.error();
    }
    return std::move(*result);
}

} // namespace sysal
