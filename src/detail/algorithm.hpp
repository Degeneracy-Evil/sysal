#pragma once

#include <vector>

namespace sysal::detail
{

template <typename T, typename Pred> const T* find_if(const std::vector<T>& items, Pred pred)
{
    for(const auto& item : items)
    {
        if(pred(item))
        {
            return &item;
        }
    }
    return nullptr;
}

template <typename T, typename Pred>
std::vector<const T*> filter_if(const std::vector<T>& items, Pred pred)
{
    std::vector<const T*> result;
    for(const auto& item : items)
    {
        if(pred(item))
        {
            result.push_back(&item);
        }
    }
    return result;
}

} // namespace sysal::detail
