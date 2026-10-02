#include "reader/linux/cgroup.hpp"
#include "reader/linux/file_utils.hpp"

#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

namespace sysal::reader
{
    namespace
    {
        std::vector<std::string> fields(const std::string &line)
        {
            std::istringstream input(line);
            std::vector<std::string> result;
            for(std::string value; input >> value;)
                result.push_back(value);
            return result;
        }

        std::string unescape(std::string value)
        {
            for(const auto &[escaped, decoded] : {std::pair{"\\040", " "}, {"\\011", "\t"}, {"\\134", "\\"}})
            {
                for(auto position = value.find(escaped); position != std::string::npos;
                    position = value.find(escaped, position + 1))
                    value.replace(position, 4, decoded);
            }
            return value;
        }

        bool contains_controller(const std::string &list, const std::string &name)
        {
            return ("," + list + ",").find("," + name + ",") != std::string::npos;
        }

        void read_hierarchy(RawStore &raw, const std::filesystem::path &mount, std::filesystem::path leaf,
                            const std::vector<std::string> &names)
        {
            for(bool current = true;; current = false)
            {
                for(const auto &name : names)
                {
                    if(!current && name == "memory.limit_in_bytes")
                    {
                        if(const auto hierarchy = read_file((leaf / "memory.use_hierarchy").string());
                           hierarchy && hierarchy->starts_with("0"))
                            continue;
                    }
                    if(!current && (name == "memory.current" || name == "memory.usage_in_bytes"))
                        continue;
                    const auto path = (leaf / name).string();
                    const auto payload = read_file(path);
                    add_record(raw, RawSource::CgroupFile, path, payload.value_or(""),
                               payload ? CollectStatus::Success : CollectStatus::Failed);
                }
                if(leaf == mount)
                    break;
                const auto parent = leaf.parent_path();
                if(parent == leaf)
                    break;
                leaf = parent;
            }
        }
    } // namespace

    void read_cgroup_limits(RawStore &raw)
    {
        const auto mounts = read_file("/proc/self/mountinfo");
        const auto membership = read_file("/proc/self/cgroup");
        if(!mounts || !membership)
            return;
        add_record(raw, RawSource::CgroupMountInfo, "/proc/self/mountinfo", *mounts, CollectStatus::Success);
        std::istringstream lines(*mounts);
        for(std::string line; std::getline(lines, line);)
        {
            const auto separator = line.find(" - ");
            if(separator == std::string::npos)
                continue;
            const auto left = fields(line.substr(0, separator));
            const auto right = fields(line.substr(separator + 3));
            if(left.size() < 5 || right.size() < 3 || (right[0] != "cgroup" && right[0] != "cgroup2"))
                continue;
            const auto root = std::filesystem::path(unescape(left[3])).lexically_normal();
            const auto mount = std::filesystem::path(unescape(left[4])).lexically_normal();
            std::istringstream groups(*membership);
            for(std::string group; std::getline(groups, group);)
            {
                const auto first = group.find(':');
                const auto second = first == std::string::npos ? first : group.find(':', first + 1);
                if(second == std::string::npos)
                    continue;
                const auto controllers = group.substr(first + 1, second - first - 1);
                const bool unified = right[0] == "cgroup2";
                if(unified != controllers.empty())
                    continue;
                std::vector<std::string> names;
                if(unified)
                    names = {"cpu.max", "memory.max", "memory.current", "cpuset.cpus.effective",
                             "cpuset.mems.effective"};
                else
                {
                    if(contains_controller(controllers, "cpu") && contains_controller(right[2], "cpu"))
                        names.insert(names.end(), {"cpu.cfs_quota_us", "cpu.cfs_period_us"});
                    if(contains_controller(controllers, "memory") && contains_controller(right[2], "memory"))
                        names.insert(names.end(), {"memory.limit_in_bytes", "memory.usage_in_bytes"});
                    if(contains_controller(controllers, "cpuset") && contains_controller(right[2], "cpuset"))
                        names.insert(names.end(), {"cpuset.cpus", "cpuset.mems"});
                }
                if(names.empty())
                    continue;
                const auto path = std::filesystem::path(group.substr(second + 1)).lexically_normal();
                auto relative = path.lexically_relative(root);
                // A cgroup namespace reports '/' even when the mounted hierarchy starts at a host subtree.
                if(path == "/")
                    relative.clear();
                if(!relative.empty() && *relative.begin() == "..")
                    continue;
                if(relative.empty() && path != "/" && path != root)
                    continue;
                const auto leaf = (mount / relative).lexically_normal();
                read_hierarchy(raw, mount, leaf, names);
            }
        }
    }
} // namespace sysal::reader
