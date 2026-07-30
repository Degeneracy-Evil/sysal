/// @file raw_store.cpp
/// @brief RawStore 查询方法实现
/// @details 实现 RawStore 的 get_all、get、has、count 方法，
///          按来源与次级键过滤原始记录。

#include "sysal/model/raw_store.hpp"

#include <algorithm>

namespace sysal
{

    std::vector<const RawRecord *> RawStore::get_all(RawSource source) const
    {
        std::vector<const RawRecord *> result;
        for(const auto &record : records)
        {
            if(record.source == source)
            {
                result.push_back(&record);
            }
        }
        return result;
    }

    std::vector<const RawRecord *> RawStore::get(RawSource source, std::string_view path_or_command) const
    {
        std::vector<const RawRecord *> result;
        for(const auto &record : records)
        {
            if(record.source == source && record.path_or_command == path_or_command)
            {
                result.push_back(&record);
            }
        }
        return result;
    }

    bool RawStore::has(RawSource source) const
    {
        return std::ranges::any_of(records, [source](const RawRecord &r) { return r.source == source; });
    }

    bool RawStore::has_success(RawSource source) const
    {
        return std::ranges::any_of(records, [source](const RawRecord &r)
                                   { return r.source == source && r.status == CollectStatus::Success; });
    }

    std::size_t RawStore::count(RawSource source) const
    {
        return static_cast<std::size_t>(
            std::ranges::count_if(records, [source](const RawRecord &r) { return r.source == source; }));
    }

} // namespace sysal
