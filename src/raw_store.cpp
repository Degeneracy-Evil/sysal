/// @file raw_store.cpp
/// @brief RawStore 成员函数实现
/// @details 提供 RawStore 中按来源、按路径过滤原始记录的查询方法。

#include "sysal/raw_store.hpp"

#include <algorithm>
#include <ranges>

namespace sysal
{

/// @brief 获取指定来源的全部原始记录指针
/// @param source 需要筛选的原始数据来源
/// @return 指向内部 records 中所有匹配来源记录的指针列表
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

/// @brief 获取指定来源且匹配路径或命令的原始记录指针
/// @param source 需要筛选的原始数据来源
/// @param path_or_command 需要匹配的文件路径或命令字符串
/// @return 指向内部 records 中同时匹配来源与路径/命令的记录指针列表
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

/// @brief 判断是否存在指定来源的原始记录
/// @param source 需要查询的原始数据来源
/// @return 存在匹配记录返回 true，否则返回 false
bool RawStore::has(RawSource source) const
{
    return std::ranges::any_of(records,
                               [source](const auto& record) { return record.source == source; });
}

/// @brief 统计指定来源的原始记录数量
/// @param source 需要统计的原始数据来源
/// @return 匹配来源的记录条数
std::size_t RawStore::count(RawSource source) const
{
    return static_cast<std::size_t>(std::ranges::count_if(records, [source](const auto& record)
                                                          { return record.source == source; }));
}

} // namespace sysal
