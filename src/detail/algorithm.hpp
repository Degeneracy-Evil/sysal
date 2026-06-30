/// @file algorithm.hpp
/// @brief 通用算法工具
/// @details 提供轻量级线性查找与过滤模板函数，作用于 std::vector，
///          返回指向元素的指针以避免拷贝。

#pragma once

#include <vector>

namespace sysal::detail
{

/// @brief 在 vector 中查找第一个满足谓词的元素
/// @tparam T 元素类型
/// @tparam Pred 一元谓词类型
/// @param items 待查找的 vector
/// @param pred 判断条件的谓词
/// @return 找到则返回指向元素的指针，否则返回 nullptr
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

/// @brief 过滤出 vector 中所有满足谓词的元素
/// @tparam T 元素类型
/// @tparam Pred 一元谓词类型
/// @param items 待过滤的 vector
/// @param pred 判断条件的谓词
/// @return 包含所有匹配元素指针的 vector（不拷贝元素本身）
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
