#include "serialization/cpu.hpp"
#include "serialization/json_values.hpp"

#include <limits>
#include <utility>

namespace sysal::detail
{
    using json = nlohmann::json;

    namespace
    {

        [[nodiscard]] json cpu_package_to_json(const CpuPackage &pkg)
        {
            json j = {
                {"id", pkg.id.value()},
                {"vendor", pkg.vendor.value},
                {"model_name", pkg.model_name.value},
                {"physical_cores", pkg.physical_cores},
                {"logical_threads", pkg.logical_threads},
            };
            if(pkg.base_frequency)
            {
                j["base_frequency"] = pkg.base_frequency->value;
            }
            if(pkg.max_frequency)
            {
                j["max_frequency"] = pkg.max_frequency->value;
            }
            return j;
        }

        [[nodiscard]] CpuPackage cpu_package_from_json(const json &j)
        {
            CpuPackage pkg;
            pkg.id = CpuPackageId(uint32_from_json(j.at("id"), "id"));
            j.at("vendor").get_to(pkg.vendor.value);
            j.at("model_name").get_to(pkg.model_name.value);
            pkg.physical_cores = uint32_from_json(j.at("physical_cores"), "physical_cores");
            pkg.logical_threads = uint32_from_json(j.at("logical_threads"), "logical_threads");
            if(j.contains("base_frequency"))
            {
                pkg.base_frequency = Frequency{uint64_from_json(j.at("base_frequency"), "base_frequency")};
            }
            if(j.contains("max_frequency"))
            {
                pkg.max_frequency = Frequency{uint64_from_json(j.at("max_frequency"), "max_frequency")};
            }
            return pkg;
        }

        [[nodiscard]] json cpu_core_to_json(const CpuCore &c)
        {
            json j = {
                {"id", c.id.value()},
                {"package_id", c.package_id.value()},
                {"logical_threads", c.logical_threads},
            };
            if(c.numa_node)
            {
                j["numa_node"] = c.numa_node->value();
            }
            return j;
        }

        [[nodiscard]] CpuCore cpu_core_from_json(const json &j)
        {
            CpuCore c;
            c.id = CpuCoreId(uint32_from_json(j.at("id"), "id"));
            c.package_id = CpuPackageId(uint32_from_json(j.at("package_id"), "package_id"));
            c.logical_threads = uint32_from_json(j.at("logical_threads"), "logical_threads");
            if(j.contains("numa_node"))
            {
                c.numa_node = NumaNodeId(uint32_from_json(j.at("numa_node"), "numa_node"));
            }
            return c;
        }

        [[nodiscard]] json cpu_ids_to_json(const std::vector<LogicalCpuId> &ids)
        {
            json result = json::array();
            for(auto id : ids)
                result.push_back(id.value());
            return result;
        }

        [[nodiscard]] std::vector<LogicalCpuId> cpu_ids_from_json(const json &j)
        {
            std::vector<LogicalCpuId> result;
            for(const auto &value : j)
                result.emplace_back(uint32_from_json(value, "array item"));
            return result;
        }

        [[nodiscard]] json cpu_identification_to_json(const CpuIdentification &identity)
        {
            json j = json::object();
            if(identity.family)
                j["family"] = *identity.family;
            if(identity.model)
                j["model"] = *identity.model;
            if(identity.stepping)
                j["stepping"] = *identity.stepping;
            if(identity.implementer)
                j["implementer"] = *identity.implementer;
            if(identity.part)
                j["part"] = *identity.part;
            if(identity.variant)
                j["variant"] = *identity.variant;
            j["revision"] = identity.revision;
            j["architecture"] = identity.architecture;
            j["microcode"] = identity.microcode;
            j["features"] = identity.features;
            return j;
        }

        [[nodiscard]] CpuIdentification cpu_identification_from_json(const json &j)
        {
            CpuIdentification identity;
            if(j.contains("family"))
                identity.family = uint32_from_json(j.at("family"), "family");
            if(j.contains("model"))
                identity.model = uint32_from_json(j.at("model"), "model");
            if(j.contains("stepping"))
                identity.stepping = uint32_from_json(j.at("stepping"), "stepping");
            if(j.contains("implementer"))
                identity.implementer = uint32_from_json(j.at("implementer"), "implementer");
            if(j.contains("part"))
                identity.part = uint32_from_json(j.at("part"), "part");
            if(j.contains("variant"))
                identity.variant = uint32_from_json(j.at("variant"), "variant");
            identity.revision = j.value("revision", std::string{});
            identity.architecture = j.value("architecture", std::string{});
            identity.microcode = j.value("microcode", std::string{});
            identity.features = j.value("features", std::vector<std::string>{});
            return identity;
        }

        [[nodiscard]] json cpu_policy_to_json(const CpuFrequencyPolicy &policy)
        {
            json j = {{"index", policy.index},
                      {"related_cpus", cpu_ids_to_json(policy.related_cpus)},
                      {"affected_cpus", cpu_ids_to_json(policy.affected_cpus)}};
            if(policy.base_frequency)
                j["base_frequency"] = policy.base_frequency->value;
            if(policy.hardware_min_frequency)
                j["hardware_min_frequency"] = policy.hardware_min_frequency->value;
            if(policy.hardware_max_frequency)
                j["hardware_max_frequency"] = policy.hardware_max_frequency->value;
            if(policy.scaling_min_frequency)
                j["scaling_min_frequency"] = policy.scaling_min_frequency->value;
            if(policy.scaling_max_frequency)
                j["scaling_max_frequency"] = policy.scaling_max_frequency->value;
            if(policy.scaling_current_frequency)
                j["scaling_current_frequency"] = policy.scaling_current_frequency->value;
            if(policy.hardware_current_frequency)
                j["hardware_current_frequency"] = policy.hardware_current_frequency->value;
            j["driver"] = policy.driver;
            j["governor"] = policy.governor;
            j["energy_performance_preference"] = policy.energy_performance_preference;
            return j;
        }

        [[nodiscard]] CpuFrequencyPolicy cpu_policy_from_json(const json &j)
        {
            CpuFrequencyPolicy policy;
            policy.index = uint32_from_json(j.at("index"), "index");
            policy.related_cpus = cpu_ids_from_json(j.at("related_cpus"));
            policy.affected_cpus = cpu_ids_from_json(j.at("affected_cpus"));
            if(j.contains("base_frequency"))
                policy.base_frequency = Frequency{uint64_from_json(j.at("base_frequency"), "base_frequency")};
            if(j.contains("hardware_min_frequency"))
                policy.hardware_min_frequency =
                    Frequency{uint64_from_json(j.at("hardware_min_frequency"), "hardware_min_frequency")};
            if(j.contains("hardware_max_frequency"))
                policy.hardware_max_frequency =
                    Frequency{uint64_from_json(j.at("hardware_max_frequency"), "hardware_max_frequency")};
            if(j.contains("scaling_min_frequency"))
                policy.scaling_min_frequency =
                    Frequency{uint64_from_json(j.at("scaling_min_frequency"), "scaling_min_frequency")};
            if(j.contains("scaling_max_frequency"))
                policy.scaling_max_frequency =
                    Frequency{uint64_from_json(j.at("scaling_max_frequency"), "scaling_max_frequency")};
            if(j.contains("scaling_current_frequency"))
                policy.scaling_current_frequency =
                    Frequency{uint64_from_json(j.at("scaling_current_frequency"), "scaling_current_frequency")};
            if(j.contains("hardware_current_frequency"))
                policy.hardware_current_frequency =
                    Frequency{uint64_from_json(j.at("hardware_current_frequency"), "hardware_current_frequency")};
            policy.driver = j.value("driver", std::string{});
            policy.governor = j.value("governor", std::string{});
            policy.energy_performance_preference = j.value("energy_performance_preference", std::string{});
            return policy;
        }

        [[nodiscard]] json logical_cpu_to_json(const LogicalCpu &lc)
        {
            json j = {
                {"id", lc.id.value()},
                {"core_id", lc.core_id.value()},
                {"package_id", lc.package_id.value()},
            };
            if(lc.visible_to_current_process)
                j["visible_to_current_process"] = *lc.visible_to_current_process;
            if(lc.numa_node)
            {
                j["numa_node"] = lc.numa_node->value();
            }
            j["identification"] = cpu_identification_to_json(lc.identification);
            if(lc.online)
                j["online"] = *lc.online;
            return j;
        }

        [[nodiscard]] LogicalCpu logical_cpu_from_json(const json &j)
        {
            LogicalCpu lc;
            lc.id = LogicalCpuId(uint32_from_json(j.at("id"), "id"));
            lc.core_id = CpuCoreId(uint32_from_json(j.at("core_id"), "core_id"));
            lc.package_id = CpuPackageId(uint32_from_json(j.at("package_id"), "package_id"));
            if(j.contains("numa_node"))
            {
                lc.numa_node = NumaNodeId(uint32_from_json(j.at("numa_node"), "numa_node"));
            }
            if(j.contains("visible_to_current_process"))
                lc.visible_to_current_process = j.at("visible_to_current_process").get<bool>();
            if(j.contains("identification"))
                lc.identification = cpu_identification_from_json(j.at("identification"));
            if(j.contains("online"))
                lc.online = j.at("online").get<bool>();
            return lc;
        }

        [[nodiscard]] json numa_node_to_json(const NumaNode &n)
        {
            json cpus = json::array();
            for(const auto &cpu_id : n.cpus)
            {
                cpus.push_back(cpu_id.value());
            }
            return json{{"id", n.id.value()}, {"cpus", std::move(cpus)}};
        }

        [[nodiscard]] NumaNode numa_node_from_json(const json &j)
        {
            NumaNode n;
            n.id = NumaNodeId(uint32_from_json(j.at("id"), "id"));
            for(const auto &elem : j.at("cpus"))
            {
                n.cpus.push_back(LogicalCpuId(uint32_from_json(elem, "array item")));
            }
            return n;
        }

        [[nodiscard]] json cpu_cache_to_json(const CpuCache &c)
        {
            json j{
                {"level", c.level},         {"type", static_cast<std::uint32_t>(c.type)},
                {"size", c.size.value},     {"ways", c.ways},
                {"line_size", c.line_size}, {"cpu_number", c.cpu_number},
            };
            if(c.cache_id)
                j["cache_id"] = *c.cache_id;
            if(c.sets)
                j["sets"] = *c.sets;
            j["shared_cpus"] = cpu_ids_to_json(c.shared_cpus);
            return j;
        }

        [[nodiscard]] CpuCache cpu_cache_from_json(const json &j)
        {
            CpuCache c;
            c.level = uint32_from_json(j.at("level"), "level");
            c.type = validate_enum(uint32_from_json(j.at("type"), "type"), CacheType::Other, "cache.type");
            c.size = MemorySize{uint64_from_json(j.at("size"), "size")};
            c.ways = uint32_from_json(j.at("ways"), "ways");
            c.line_size = uint32_from_json(j.at("line_size"), "line_size");
            c.cpu_number = uint32_from_json(j.at("cpu_number"), "cpu_number");
            if(j.contains("cache_id"))
                c.cache_id = uint32_from_json(j.at("cache_id"), "cache_id");
            if(j.contains("sets"))
                c.sets = uint32_from_json(j.at("sets"), "sets");
            if(j.contains("shared_cpus"))
                c.shared_cpus = cpu_ids_from_json(j.at("shared_cpus"));
            return c;
        }

        [[nodiscard]] json thermal_zone_to_json(const ThermalZone &t)
        {
            return json{
                {"name", t.name},
                {"type", t.type},
                {"temp", t.temp.value},
            };
        }

        [[nodiscard]] ThermalZone thermal_zone_from_json(const json &j)
        {
            ThermalZone t;
            t.name = j.at("name").get<std::string>();
            t.type = j.at("type").get<std::string>();
            t.temp = Temperature{uint64_from_json(j.at("temp"), "temp")};
            return t;
        }

    } // namespace

    [[nodiscard]] json cpu_to_json(const Cpu &c)
    {
        json packages = json::array();
        for(const auto &pkg : c.packages)
        {
            packages.push_back(cpu_package_to_json(pkg));
        }
        json cores = json::array();
        for(const auto &core : c.cores)
        {
            cores.push_back(cpu_core_to_json(core));
        }
        json logical_cpus = json::array();
        for(const auto &lc : c.logical_cpus)
        {
            logical_cpus.push_back(logical_cpu_to_json(lc));
        }
        json numa_nodes = json::array();
        for(const auto &n : c.numa_nodes)
        {
            numa_nodes.push_back(numa_node_to_json(n));
        }
        json isa = json::array();
        for(const auto &ext : c.isa_extensions)
        {
            isa.push_back(static_cast<std::uint32_t>(ext));
        }
        json caches = json::array();
        for(const auto &cache : c.caches)
        {
            caches.push_back(cpu_cache_to_json(cache));
        }
        json thermal = json::array();
        for(const auto &zone : c.thermal_zones)
        {
            thermal.push_back(thermal_zone_to_json(zone));
        }
        json j{
            {"arch", static_cast<std::uint32_t>(c.arch)},
            {"packages", std::move(packages)},
            {"cores", std::move(cores)},
            {"logical_cpus", std::move(logical_cpus)},
            {"numa_nodes", std::move(numa_nodes)},
            {"isa_extensions", std::move(isa)},
            {"caches", std::move(caches)},
            {"governor", c.governor},
            {"thermal_zones", std::move(thermal)},
        };
        j["frequency_policies"] = json::array();
        for(const auto &policy : c.frequency_policies)
            j["frequency_policies"].push_back(cpu_policy_to_json(policy));
        if(c.present_cpu_ids)
            j["present_cpu_ids"] = cpu_ids_to_json(*c.present_cpu_ids);
        if(c.online_cpu_ids)
            j["online_cpu_ids"] = cpu_ids_to_json(*c.online_cpu_ids);
        if(c.smt_active)
            j["smt_active"] = *c.smt_active;
        if(c.boost_enabled)
            j["boost_enabled"] = *c.boost_enabled;
        j["smt_control"] = c.smt_control;
        return j;
    }

    [[nodiscard]] Cpu cpu_from_json(const json &j)
    {
        Cpu c;
        c.arch = validate_enum(uint32_from_json(j.at("arch"), "arch"), Arch::Other, "arch");
        for(const auto &elem : j.at("packages"))
        {
            c.packages.push_back(cpu_package_from_json(elem));
        }
        for(const auto &elem : j.at("cores"))
        {
            c.cores.push_back(cpu_core_from_json(elem));
        }
        for(const auto &elem : j.at("logical_cpus"))
        {
            c.logical_cpus.push_back(logical_cpu_from_json(elem));
        }
        for(const auto &elem : j.at("numa_nodes"))
        {
            c.numa_nodes.push_back(numa_node_from_json(elem));
        }
        for(const auto &elem : j.at("isa_extensions"))
        {
            c.isa_extensions.push_back(
                validate_enum(uint32_from_json(elem, "array item"), IsaExtension::Pclmulqdq, "isa_extensions"));
        }
        if(j.contains("caches"))
        {
            for(const auto &elem : j.at("caches"))
            {
                c.caches.push_back(cpu_cache_from_json(elem));
            }
        }
        if(j.contains("governor"))
        {
            c.governor = j.at("governor").get<std::string>();
        }
        if(j.contains("thermal_zones"))
        {
            for(const auto &elem : j.at("thermal_zones"))
            {
                c.thermal_zones.push_back(thermal_zone_from_json(elem));
            }
        }
        if(j.contains("frequency_policies"))
        {
            for(const auto &policy : j.at("frequency_policies"))
                c.frequency_policies.push_back(cpu_policy_from_json(policy));
        }
        if(j.contains("present_cpu_ids"))
            c.present_cpu_ids = cpu_ids_from_json(j.at("present_cpu_ids"));
        if(j.contains("online_cpu_ids"))
            c.online_cpu_ids = cpu_ids_from_json(j.at("online_cpu_ids"));
        if(j.contains("smt_active"))
            c.smt_active = j.at("smt_active").get<bool>();
        if(j.contains("boost_enabled"))
            c.boost_enabled = j.at("boost_enabled").get<bool>();
        c.smt_control = j.value("smt_control", std::string{});
        return c;
    }

} // namespace sysal::detail
