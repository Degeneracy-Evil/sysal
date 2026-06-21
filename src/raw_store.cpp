#include "sysal/raw_store.hpp"

#include <algorithm>
#include <ranges>

namespace sysal
{

std::vector<const RawRecord*> RawStore::get_all(RawSource source) const
{
    std::vector<const RawRecord*> result;
    for(const auto& record : records)
    {
        if(record.source == source)
        {
            result.push_back(&record);
        }
    }
    return result;
}

std::vector<const RawRecord*> RawStore::get(RawSource source,
                                            std::string_view path_or_command) const
{
    std::vector<const RawRecord*> result;
    for(const auto& record : records)
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
    return std::ranges::any_of(records,
                               [source](const auto& record) { return record.source == source; });
}

std::size_t RawStore::count(RawSource source) const
{
    return static_cast<std::size_t>(std::ranges::count_if(records, [source](const auto& record)
                                                          { return record.source == source; }));
}

} // namespace sysal
