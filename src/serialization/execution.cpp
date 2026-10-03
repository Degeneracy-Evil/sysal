#include "serialization/execution.hpp"
#include "serialization/json_values.hpp"

#include <limits>
#include <utility>

namespace sysal::detail
{
    using json = nlohmann::json;

    namespace
    {

        [[nodiscard]] json process_to_json(const Process &p)
        {
            return json{
                {"pid", p.pid},   {"ppid", p.ppid}, {"uid", p.uid}, {"gid", p.gid},
                {"comm", p.comm}, {"exe", p.exe},   {"cwd", p.cwd},
            };
        }

        [[nodiscard]] Process process_from_json(const json &j)
        {
            Process p;
            p.pid = j.at("pid").get<std::int32_t>();
            p.ppid = j.at("ppid").get<std::int32_t>();
            p.uid = uint32_from_json(j.at("uid"), "uid");
            p.gid = uint32_from_json(j.at("gid"), "gid");
            j.at("comm").get_to(p.comm);
            j.at("exe").get_to(p.exe);
            j.at("cwd").get_to(p.cwd);
            return p;
        }

        [[nodiscard]] json environment_to_json(const Environment &e)
        {
            json arr = json::array();
            for(const auto &[k, v] : e.entries)
            {
                arr.push_back(json{{"key", k}, {"value", v}});
            }
            return json{{"entries", std::move(arr)}};
        }

        [[nodiscard]] Environment environment_from_json(const json &j)
        {
            Environment e;
            for(const auto &elem : j.at("entries"))
            {
                e.entries.emplace_back(elem.at("key").get<std::string>(), elem.at("value").get<std::string>());
            }
            return e;
        }

        [[nodiscard]] json cgroup_to_json(const Cgroup &c)
        {
            json j = {
                {"version", static_cast<std::uint32_t>(c.version)},
                {"path", c.path},
            };
            json arr = json::array();
            for(const auto &ctrl : c.controllers)
            {
                arr.push_back(ctrl);
            }
            j["controllers"] = std::move(arr);
            j["cpu_limit_known"] = c.cpu_limit_known;
            j["memory_limit_known"] = c.memory_limit_known;
            if(c.cpu_quota_us)
                j["cpu_quota_us"] = c.cpu_quota_us->value;
            if(c.cpu_period_us)
                j["cpu_period_us"] = c.cpu_period_us->value;
            if(c.memory_limit)
                j["memory_limit"] = c.memory_limit->value;
            if(c.memory_current)
                j["memory_current"] = c.memory_current->value;
            return j;
        }

        [[nodiscard]] Cgroup cgroup_from_json(const json &j)
        {
            Cgroup c;
            c.version = validate_enum(uint32_from_json(j.at("version"), "version"), CgroupVersion::V2, "version");
            j.at("path").get_to(c.path);
            if(j.contains("controllers"))
            {
                c.controllers = str_array_from_json(j.at("controllers"));
            }
            c.cpu_limit_known = j.value("cpu_limit_known", false);
            c.memory_limit_known = j.value("memory_limit_known", false);
            if(j.contains("cpu_quota_us"))
                c.cpu_quota_us = Microseconds{uint64_from_json(j.at("cpu_quota_us"), "cpu_quota_us")};
            if(j.contains("cpu_period_us"))
                c.cpu_period_us = Microseconds{uint64_from_json(j.at("cpu_period_us"), "cpu_period_us")};
            if(j.contains("memory_limit"))
                c.memory_limit = MemorySize{uint64_from_json(j.at("memory_limit"), "memory_limit")};
            if(j.contains("memory_current"))
                c.memory_current = MemorySize{uint64_from_json(j.at("memory_current"), "memory_current")};
            return c;
        }

        [[nodiscard]] json cpuset_to_json(const Cpuset &cs)
        {
            return json{
                {"cpus", cs.cpus},
                {"mems", cs.mems},
                {"cpus_effective", cs.cpus_effective},
                {"mems_effective", cs.mems_effective},
            };
        }

        [[nodiscard]] Cpuset cpuset_from_json(const json &j)
        {
            Cpuset cs;
            j.at("cpus").get_to(cs.cpus);
            j.at("mems").get_to(cs.mems);
            j.at("cpus_effective").get_to(cs.cpus_effective);
            j.at("mems_effective").get_to(cs.mems_effective);
            return cs;
        }

        [[nodiscard]] json permission_to_json(const Permission &p)
        {
            json j = {
                {"euid", p.euid},
                {"egid", p.egid},
                {"is_root", p.is_root},
            };
            json arr = json::array();
            for(const auto &cap : p.capabilities)
            {
                arr.push_back(cap);
            }
            j["capabilities"] = std::move(arr);
            return j;
        }

        [[nodiscard]] Permission permission_from_json(const json &j)
        {
            Permission p;
            p.euid = uint32_from_json(j.at("euid"), "euid");
            p.egid = uint32_from_json(j.at("egid"), "egid");
            if(j.contains("capabilities"))
            {
                p.capabilities = str_array_from_json(j.at("capabilities"));
            }
            p.is_root = j.at("is_root").get<bool>();
            return p;
        }

        [[nodiscard]] json container_to_json(const Container &c)
        {
            return json{
                {"kind", static_cast<std::uint32_t>(c.kind)},
                {"id", c.id},
                {"runtime", c.runtime},
            };
        }

        [[nodiscard]] Container container_from_json(const json &j)
        {
            Container c;
            c.kind = validate_enum(uint32_from_json(j.at("kind"), "kind"), ContainerKind::Other, "kind");
            j.at("id").get_to(c.id);
            j.at("runtime").get_to(c.runtime);
            return c;
        }

    } // namespace

    [[nodiscard]] json execution_to_json(const ExecutionContext &e)
    {
        json j = {
            {"process", process_to_json(e.process)},
            {"environment", environment_to_json(e.environment)},
            {"cgroup", cgroup_to_json(e.cgroup)},
            {"cpuset", cpuset_to_json(e.cpuset)},
            {"permission", permission_to_json(e.permission)},
        };
        if(e.container)
        {
            j["container"] = container_to_json(*e.container);
        }

        json vcpu = json::array();
        for(const auto &id : e.visible_logical_cpu_ids)
        {
            vcpu.push_back(id.value());
        }
        j["visible_logical_cpu_ids"] = std::move(vcpu);

        json vacc = json::array();
        for(const auto &id : e.visible_accelerator_ids)
        {
            vacc.push_back(id.value());
        }
        j["visible_accelerator_ids"] = std::move(vacc);
        j["accelerator_visibility_restricted"] = e.accelerator_visibility_restricted;

        json vnet = json::array();
        for(const auto &name : e.visible_network_interface_names)
        {
            vnet.push_back(name.value);
        }
        j["visible_network_interface_names"] = std::move(vnet);

        return j;
    }

    [[nodiscard]] ExecutionContext execution_from_json(const json &j)
    {
        ExecutionContext e;
        e.process = process_from_json(j.at("process"));
        e.environment = environment_from_json(j.at("environment"));
        e.accelerator_visibility_restricted = j.value("accelerator_visibility_restricted", false);
        e.cgroup = cgroup_from_json(j.at("cgroup"));
        e.cpuset = cpuset_from_json(j.at("cpuset"));
        e.permission = permission_from_json(j.at("permission"));

        if(j.contains("container"))
        {
            e.container = container_from_json(j.at("container"));
        }

        for(const auto &elem : j.at("visible_logical_cpu_ids"))
        {
            e.visible_logical_cpu_ids.push_back(LogicalCpuId(uint32_from_json(elem, "array item")));
        }
        for(const auto &elem : j.at("visible_accelerator_ids"))
        {
            e.visible_accelerator_ids.push_back(AcceleratorId(uint32_from_json(elem, "array item")));
        }
        for(const auto &elem : j.at("visible_network_interface_names"))
        {
            InterfaceName in;
            in.value = elem.get<std::string>();
            e.visible_network_interface_names.push_back(std::move(in));
        }

        return e;
    }

} // namespace sysal::detail
