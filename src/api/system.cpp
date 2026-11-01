/// @file system.cpp
/// @brief System 公共 API 实现
/// @details 实现 System::collect() 和 System::refresh()，
///          委托给 pipeline 模块执行完整采集管线。

#include "pipeline/pipeline.hpp"

#include "sysal/core/system.hpp"

#include <stdexcept>
#include <utility>
#include <vector>

namespace sysal
{

    System System::collect(Collect flags)
    {
        return collect(flags, CollectOptions{});
    }

    System System::collect(Collect flags, const CollectOptions &options)
    {
        CollectionRequest{flags, options}.validate();
        std::vector<std::string> warnings;
        return detail::run_pipeline(flags, options, warnings);
    }

    void System::refresh()
    {
        if(!meta.collection_request)
            throw std::invalid_argument("snapshot acquisition options are unknown; collect with an explicit request");
        auto new_sys = collect(meta.requested_flags, meta.collection_request->options);
        *this = std::move(new_sys);
    }

} // namespace sysal
