/// @file cpu.cpp
/// @brief CPU 解析器实现
/// @details 从 /proc/cpuinfo 和 sysfs cpufreq 数据中解析 CPU 拓扑、频率和 NUMA 映射。

#include "cpu.hpp"

#include "parse_utils.hpp"

#include <algorithm>
#include <cctype>
#include <map>
#include <set>
#include <string_view>
#include <unordered_set>

namespace sysal::detail
{

    namespace
    {

        /// @brief 单个 /proc/cpuinfo 条目的中间表示
        struct CpuInfoEntry
        {
            std::uint32_t processor{};    ///< 逻辑 CPU 编号
            std::uint32_t physical_id{0}; ///< 物理封装 ID（缺失默认 0）
            std::uint32_t core_id{0};     ///< 物理核 ID（缺失默认为 processor）
            std::string model_name;       ///< 型号名称
            std::string vendor_id;        ///< 厂商 ID
            std::string flags;            ///< CPU flags 字符串
            bool core_id_set{false};      ///< core_id 是否已设置
        };

        /// @brief 解析 /proc/cpuinfo 内容
        /// @param payload /proc/cpuinfo 文件内容
        /// @param warnings 警告列表
        /// @return CpuInfoEntry 列表
        std::vector<CpuInfoEntry> parse_cpuinfo(std::string_view payload, std::vector<std::string> &warnings)
        {
            std::vector<CpuInfoEntry> entries;
            CpuInfoEntry current;
            bool has_entry = false;

            auto lines = split(payload, '\n');
            for(const auto &line : lines)
            {
                auto trimmed = trim(line);
                if(trimmed.empty())
                {
                    if(has_entry)
                    {
                        if(!current.core_id_set)
                        {
                            current.core_id = current.processor;
                        }
                        entries.push_back(std::move(current));
                        current = CpuInfoEntry{};
                        has_entry = false;
                    }
                    continue;
                }

                auto [key, value] = parse_kv(trimmed, ':');
                has_entry = true;

                if(key == "processor")
                {
                    auto v = parse_uint(value);
                    if(v.has_value())
                    {
                        current.processor = static_cast<std::uint32_t>(*v);
                    }
                    else
                    {
                        warnings.push_back("parse_cpu: /proc/cpuinfo processor 解析失败: " + std::string(value));
                    }
                }
                else if(key == "physical id")
                {
                    auto v = parse_uint(value);
                    if(v.has_value())
                    {
                        current.physical_id = static_cast<std::uint32_t>(*v);
                    }
                }
                else if(key == "core id")
                {
                    auto v = parse_uint(value);
                    if(v.has_value())
                    {
                        current.core_id = static_cast<std::uint32_t>(*v);
                        current.core_id_set = true;
                    }
                }
                else if(key == "model name")
                {
                    current.model_name = value;
                }
                else if(key == "vendor_id")
                {
                    current.vendor_id = value;
                }
                else if(key == "flags")
                {
                    current.flags = value;
                }
            }

            // 处理最后一个条目（文件末尾无空行的情况）
            if(has_entry)
            {
                if(!current.core_id_set)
                {
                    current.core_id = current.processor;
                }
                entries.push_back(std::move(current));
            }

            return entries;
        }

        /// @brief 从 flags 字符串解析 ISA 扩展列表
        /// @param flags CPU flags 字符串
        /// @return ISA 扩展列表
        std::vector<IsaExtension> parse_isa_extensions(const std::string &flags)
        {
            std::vector<IsaExtension> extensions;
            auto parts = split(flags, ' ');

            std::set<std::string> flag_set;
            for(const auto &f : parts)
            {
                flag_set.insert(trim(f));
            }

            static const std::vector<std::pair<std::string, IsaExtension>> isa_map = {
                {"sse", IsaExtension::Sse},
                {"sse2", IsaExtension::Sse2},
                {"sse3", IsaExtension::Sse3},
                {"ssse3", IsaExtension::Ssse3},
                {"sse4_1", IsaExtension::Sse41},
                {"sse4_2", IsaExtension::Sse42},
                {"avx", IsaExtension::Avx},
                {"avx2", IsaExtension::Avx2},
                {"avx512f", IsaExtension::Avx512f},
                {"avx512cd", IsaExtension::Avx512cd},
                {"avx512bw", IsaExtension::Avx512bw},
                {"avx512dq", IsaExtension::Avx512dq},
                {"avx512vl", IsaExtension::Avx512vl},
                {"aes", IsaExtension::Aes},
                {"fma", IsaExtension::Fma},
                {"f16c", IsaExtension::F16c},
                {"pclmulqdq", IsaExtension::Pclmulqdq},
            };

            for(const auto &[flag, ext] : isa_map)
            {
                if(flag_set.count(flag))
                {
                    extensions.push_back(ext);
                }
            }

            return extensions;
        }

        /// @brief 从路径中提取 CPU 编号
        /// @param path sysfs 路径，如 "cpu/cpu0/cpufreq/base_frequency"
        /// @return CPU 编号，若模式未找到则返回 nullopt
        std::optional<std::uint32_t> extract_cpu_number_from_path(std::string_view path)
        {
            auto pos = path.find("cpu/cpu");
            if(pos == std::string_view::npos)
            {
                return std::nullopt;
            }
            auto digits_start = pos + 7; // "cpu/cpu" 长度
            auto digits_end = digits_start;
            while(digits_end < path.size() && std::isdigit(static_cast<unsigned char>(path[digits_end])))
            {
                ++digits_end;
            }
            if(digits_end == digits_start)
            {
                return std::nullopt;
            }
            auto v = parse_uint(path.substr(digits_start, digits_end - digits_start));
            if(!v.has_value())
            {
                return std::nullopt;
            }
            return static_cast<std::uint32_t>(*v);
        }

        /// @brief 从 sysfs cpufreq 记录中读取频率信息
        /// @param raw 原始证据存储
        /// @param package_id 封装 ID
        /// @param package_cpu_ids 属于该封装的逻辑 CPU 编号集合
        /// @param warnings 警告列表
        /// @return pair<base_frequency, max_frequency>
        std::pair<std::optional<Frequency>, std::optional<Frequency>>
        read_cpufreq(const RawStore &raw, [[maybe_unused]] std::uint32_t package_id,
                     const std::unordered_set<std::uint32_t> &package_cpu_ids,
                     [[maybe_unused]] std::vector<std::string> &warnings)
        {
            std::optional<Frequency> base_freq;
            std::optional<Frequency> max_freq;

            auto cpu_records = raw.get_all(RawSource::SysfsCpu);
            for(const auto *rec : cpu_records)
            {
                if(rec->status != CollectStatus::Success)
                {
                    continue;
                }

                const auto &path = rec->path_or_command;

                // 提取路径中的 CPU 编号，仅处理属于该封装的 CPU
                auto cpu_num = extract_cpu_number_from_path(path);
                if(!cpu_num.has_value() || package_cpu_ids.count(*cpu_num) == 0)
                {
                    continue;
                }

                // 查找 base_frequency: cpu/cpuN/cpufreq/base_frequency
                if(path.find("base_frequency") != std::string::npos && !base_freq.has_value())
                {
                    auto v = parse_uint(rec->payload);
                    if(v.has_value())
                    {
                        // sysfs 频率单位为 kHz，转换为 Hz
                        base_freq = Frequency{*v * 1000};
                    }
                }
                // 查找 scaling_max_freq: cpu/cpuN/cpufreq/scaling_max_freq
                if(path.find("scaling_max_freq") != std::string::npos && !max_freq.has_value())
                {
                    auto v = parse_uint(rec->payload);
                    if(v.has_value())
                    {
                        max_freq = Frequency{*v * 1000};
                    }
                }
            }

            return {base_freq, max_freq};
        }

        /// @brief 从 sysfs NUMA 记录中构建 CPU → NUMA 节点映射
        /// @param raw 原始证据存储
        /// @param warnings 警告列表
        /// @return CPU 编号 → NUMA 节点 ID 的映射
        std::map<std::uint32_t, std::uint32_t> build_numa_mapping(const RawStore &raw,
                                                                  [[maybe_unused]] std::vector<std::string> &warnings)
        {
            std::map<std::uint32_t, std::uint32_t> cpu_to_numa;

            auto numa_records = raw.get_all(RawSource::SysfsNuma);
            for(const auto *rec : numa_records)
            {
                if(rec->status != CollectStatus::Success)
                {
                    continue;
                }

                const auto &path = rec->path_or_command;
                // 查找 cpulist: node/nodeN/cpulist
                if(path.find("cpulist") == std::string::npos)
                {
                    continue;
                }

                // 从路径提取节点 ID: node/nodeN/cpulist
                // 查找 "node/node" 模式
                auto node_node_pos = path.find("node/node");
                if(node_node_pos == std::string::npos)
                {
                    continue;
                }
                auto after_node_node = path.substr(node_node_pos + 9); // "node/node" 长度
                auto slash_pos = after_node_node.find('/');
                if(slash_pos == std::string::npos)
                {
                    continue;
                }
                auto node_id_str = after_node_node.substr(0, slash_pos);
                auto node_id = parse_uint(node_id_str);
                if(!node_id.has_value())
                {
                    continue;
                }

                // 解析 cpulist: 格式如 "0-3,8-11" 或 "0,1,2,3"
                constexpr std::size_t MAX_CPUS = 1024;
                const auto &cpulist = rec->payload;
                auto parts = split(cpulist, ',');
                for(const auto &part : parts)
                {
                    auto trimmed = trim(part);
                    auto dash_pos = trimmed.find('-');
                    if(dash_pos != std::string::npos)
                    {
                        // 范围: "0-3"
                        auto start = parse_uint(trimmed.substr(0, dash_pos));
                        auto end = parse_uint(trimmed.substr(dash_pos + 1));
                        if(start.has_value() && end.has_value())
                        {
                            for(auto cpu = *start; cpu <= *end && cpu_to_numa.size() < MAX_CPUS; ++cpu)
                            {
                                cpu_to_numa[static_cast<std::uint32_t>(cpu)] = static_cast<std::uint32_t>(*node_id);
                            }
                            if(cpu_to_numa.size() >= MAX_CPUS && *end > cpu_to_numa.rbegin()->first)
                            {
                                break;
                            }
                        }
                    }
                    else
                    {
                        // 单个: "5"
                        auto cpu = parse_uint(trimmed);
                        if(cpu.has_value())
                        {
                            if(cpu_to_numa.size() >= MAX_CPUS)
                            {
                                break;
                            }
                            cpu_to_numa[static_cast<std::uint32_t>(*cpu)] = static_cast<std::uint32_t>(*node_id);
                        }
                    }
                }
            }

            return cpu_to_numa;
        }

        /// @brief 从 NUMA 映射构建 NumaNode 列表
        /// @param cpu_to_numa CPU → NUMA 节点映射
        /// @return NumaNode 列表（按节点 ID 排序）
        std::vector<NumaNode> build_numa_nodes(const std::map<std::uint32_t, std::uint32_t> &cpu_to_numa)
        {
            // 收集所有 NUMA 节点 ID 及其 CPU 列表
            std::map<std::uint32_t, std::vector<LogicalCpuId>> node_cpus;
            for(const auto &[cpu, node] : cpu_to_numa)
            {
                node_cpus[node].push_back(LogicalCpuId{cpu});
            }

            std::vector<NumaNode> nodes;
            nodes.reserve(node_cpus.size());
            for(const auto &[node_id, cpus] : node_cpus)
            {
                nodes.push_back(NumaNode{NumaNodeId{node_id}, cpus});
            }
            return nodes;
        }

    } // namespace

    std::optional<Cpu> parse_cpu(const RawStore &raw, std::vector<std::string> &warnings)
    {
        // 解析 /proc/cpuinfo
        auto cpuinfo_records = raw.get_all(RawSource::ProcCpuInfo);
        const RawRecord *cpuinfo_rec = nullptr;
        for(const auto *rec : cpuinfo_records)
        {
            if(rec->status == CollectStatus::Success)
            {
                cpuinfo_rec = rec;
                break;
            }
        }
        if(cpuinfo_rec == nullptr)
        {
            warnings.push_back("parse_cpu: 缺少 /proc/cpuinfo 数据");
            return std::nullopt;
        }

        auto entries = parse_cpuinfo(cpuinfo_rec->payload, warnings);
        if(entries.empty())
        {
            warnings.push_back("parse_cpu: /proc/cpuinfo 无有效条目");
            return std::nullopt;
        }

        Cpu cpu;

        // 收集唯一的 package ID 和 (package_id, core_id) 对
        std::set<std::uint32_t> package_ids;
        std::set<std::pair<std::uint32_t, std::uint32_t>> core_keys;

        for(const auto &e : entries)
        {
            package_ids.insert(e.physical_id);
            core_keys.insert({e.physical_id, e.core_id});
        }

        // 构建 CpuPackage 列表
        std::map<std::uint32_t, CpuPackageId> package_id_map;
        std::uint32_t pkg_seq = 0;
        for(auto pid : package_ids)
        {
            auto id = CpuPackageId{pkg_seq};
            package_id_map[pid] = id;

            CpuPackage pkg;
            pkg.id = id;
            // 从第一个属于该封装的条目获取型号和厂商
            for(const auto &e : entries)
            {
                if(e.physical_id == pid)
                {
                    pkg.model_name = DeviceName{e.model_name};
                    pkg.vendor = Vendor{e.vendor_id};
                    break;
                }
            }
            cpu.packages.push_back(std::move(pkg));
            ++pkg_seq;
        }

        // 构建 CpuCore 列表
        std::map<std::pair<std::uint32_t, std::uint32_t>, CpuCoreId> core_key_map;
        std::uint32_t core_seq = 0;
        for(const auto &key : core_keys)
        {
            auto id = CpuCoreId{core_seq};
            core_key_map[key] = id;

            CpuCore core;
            core.id = id;
            core.package_id = package_id_map[key.first];
            cpu.cores.push_back(core);
            ++core_seq;
        }

        // 构建 LogicalCpu 列表
        for(const auto &e : entries)
        {
            auto core_it = core_key_map.find({e.physical_id, e.core_id});
            if(core_it == core_key_map.end())
            {
                warnings.push_back("parse_cpu: 逻辑 CPU " + std::to_string(e.processor) +
                                   " 的 (physical_id=" + std::to_string(e.physical_id) +
                                   ", core_id=" + std::to_string(e.core_id) + ") 未找到匹配核");
                continue;
            }
            auto pkg_it = package_id_map.find(e.physical_id);
            if(pkg_it == package_id_map.end())
            {
                warnings.push_back("parse_cpu: 逻辑 CPU " + std::to_string(e.processor) +
                                   " 的 physical_id=" + std::to_string(e.physical_id) + " 未找到匹配封装");
                continue;
            }
            LogicalCpu lc;
            lc.id = LogicalCpuId{e.processor};
            lc.core_id = core_it->second;
            lc.package_id = pkg_it->second;
            lc.visible_to_current_process = true;
            cpu.logical_cpus.push_back(lc);
        }

        // 统计每个封装的逻辑线程数和物理核数
        for(auto &pkg : cpu.packages)
        {
            std::uint32_t threads = 0;
            std::uint32_t cores_count = 0;
            for(const auto &core : cpu.cores)
            {
                if(core.package_id == pkg.id)
                {
                    ++cores_count;
                }
            }
            for(const auto &lc : cpu.logical_cpus)
            {
                if(lc.package_id == pkg.id)
                {
                    ++threads;
                }
            }
            pkg.physical_cores = cores_count;
            pkg.logical_threads = threads;
        }

        // 统计每个物理核的逻辑线程数
        for(auto &core : cpu.cores)
        {
            std::uint32_t threads = 0;
            for(const auto &lc : cpu.logical_cpus)
            {
                if(lc.core_id == core.id)
                {
                    ++threads;
                }
            }
            core.logical_threads = threads;
        }

        // 解析 ISA 扩展（从第一个条目的 flags）
        if(!entries.empty() && !entries[0].flags.empty())
        {
            cpu.isa_extensions = parse_isa_extensions(entries[0].flags);
        }

        // 读取 cpufreq 频率信息（每个封装从属于该封装的 CPU 读取）
        for(auto &pkg : cpu.packages)
        {
            // 找到属于该封装的原始 physical_id
            std::uint32_t raw_pkg_id = 0;
            for(const auto &[raw_id, mapped_id] : package_id_map)
            {
                if(mapped_id == pkg.id)
                {
                    raw_pkg_id = raw_id;
                    break;
                }
            }
            // 收集属于该封装的逻辑 CPU 编号
            std::unordered_set<std::uint32_t> package_cpu_ids;
            for(const auto &e : entries)
            {
                if(e.physical_id == raw_pkg_id)
                {
                    package_cpu_ids.insert(e.processor);
                }
            }
            auto [base_freq, max_freq] = read_cpufreq(raw, raw_pkg_id, package_cpu_ids, warnings);
            pkg.base_frequency = base_freq;
            pkg.max_frequency = max_freq;
        }

        // 构建 NUMA 映射
        auto cpu_to_numa = build_numa_mapping(raw, warnings);

        // 填充 CpuCore 和 LogicalCpu 的 numa_node
        for(auto &core : cpu.cores)
        {
            // 从属于该核的第一个逻辑 CPU 获取 NUMA 节点
            for(const auto &lc : cpu.logical_cpus)
            {
                if(lc.core_id == core.id)
                {
                    auto it = cpu_to_numa.find(lc.id.value());
                    if(it != cpu_to_numa.end())
                    {
                        core.numa_node = NumaNodeId{it->second};
                    }
                    break;
                }
            }
        }
        for(auto &lc : cpu.logical_cpus)
        {
            auto it = cpu_to_numa.find(lc.id.value());
            if(it != cpu_to_numa.end())
            {
                lc.numa_node = NumaNodeId{it->second};
            }
        }

        // 构建 NumaNode 列表
        cpu.numa_nodes = build_numa_nodes(cpu_to_numa);

        // 设置架构（从第一个条目的 vendor_id 推断，或默认 Other）
        if(!entries.empty())
        {
            const auto &vendor = entries[0].vendor_id;
            if(vendor.find("GenuineIntel") != std::string::npos || vendor.find("AuthenticAMD") != std::string::npos)
            {
                cpu.arch = Arch::X86_64;
            }
            // ARM 和 RISC-V 通常不在 /proc/cpuinfo 中有 vendor_id
            // 架构信息由 platform parser 更准确地提供
        }

        return cpu;
    }

} // namespace sysal::detail
