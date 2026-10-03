/// @file serialize.cpp
/// @brief JSON 序列化与反序列化实现
/// @details 实现 RawStore ↔ JSON 与 System ↔ JSON 的转换，以及基于文件的
///          save/load 操作。System 序列化输出顶层对象含 info、meta、warnings、
///          raw 四个字段。使用 nlohmann/json 库进行 JSON 处理。

#include "serialization/accelerator.hpp"
#include "serialization/cpu.hpp"
#include "serialization/execution.hpp"
#include "serialization/hardware.hpp"
#include "serialization/json_values.hpp"
#include "serialization/memory.hpp"
#include "serialization/network.hpp"
#include "serialization/pci.hpp"
#include "serialization/platform.hpp"
#include "serialization/software.hpp"
#include "serialization/storage.hpp"
#include <nlohmann/json.hpp>

#include "sysal/core/error.hpp"
#include "sysal/model/raw_store.hpp"
#include "sysal/serialization/serialization.hpp"
#include "sysal/test/replay.hpp"
#include "sysal/types/enums.hpp"
#include "sysal/version.hpp"

#include <chrono>
#include <cstdint>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sysal
{
    namespace
    {

        using json = nlohmann::json;

        // ───────────────────────────── 辅助工具 ─────────────────────────────

        /// @brief 将时间点转换为 epoch 毫秒
        /// @param tp 系统时钟时间点
        /// @return epoch 毫秒数
        [[nodiscard]] std::int64_t time_point_to_ms(std::chrono::system_clock::time_point tp)
        {
            return std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()).count();
        }

        /// @brief 将 epoch 毫秒数转换为系统时钟时间点
        /// @param ms epoch 毫秒数
        /// @return 对应的 time_point
        [[nodiscard]] std::chrono::system_clock::time_point ms_to_time_point(std::int64_t ms)
        {
            return std::chrono::system_clock::time_point(std::chrono::milliseconds(ms));
        }

        using detail::checked_unsigned;
        using detail::pci_address_from_json;
        using detail::pci_address_to_json;
        using detail::str_array_from_json;
        using detail::uint32_from_json;
        using detail::uint64_from_json;
        using detail::validate_enum;

        // ───────────────────────────── RawRecord / RawStore ─────────────────────────────

        [[nodiscard]] json raw_record_to_json(const RawRecord &rec)
        {
            json j{
                {"source", static_cast<std::uint64_t>(rec.source)},
                {"path_or_command", rec.path_or_command},
                {"payload", rec.payload},
                {"status", static_cast<std::uint64_t>(rec.status)},
                {"collected_at", time_point_to_ms(rec.collected_at)},
            };
            if(rec.failure)
                j["failure"] = static_cast<std::uint32_t>(*rec.failure);
            return j;
        }

        [[nodiscard]] RawRecord raw_record_from_json(const json &j)
        {
            RawRecord rec;
            rec.source =
                validate_enum(uint32_from_json(j.at("source"), "source"), RawSource::SysfsStorageConnections, "source");
            j.at("path_or_command").get_to(rec.path_or_command);
            j.at("payload").get_to(rec.payload);
            rec.status =
                validate_enum(uint32_from_json(j.at("status"), "status"), CollectStatus::NotCollected, "status");
            rec.collected_at = ms_to_time_point(j.at("collected_at").get<std::int64_t>());
            if(j.contains("failure"))
                rec.failure =
                    validate_enum(uint32_from_json(j.at("failure"), "failure"), ReadFailure::LowPower, "failure");
            return rec;
        }

        [[nodiscard]] json raw_store_to_json(const RawStore &store)
        {
            json arr = json::array();
            for(const auto &rec : store.records)
            {
                arr.push_back(raw_record_to_json(rec));
            }
            return json{{"records", std::move(arr)}};
        }

        [[nodiscard]] RawStore raw_store_from_json(const json &root)
        {
            RawStore store;
            for(const auto &elem : root.at("records"))
            {
                store.records.push_back(raw_record_from_json(elem));
            }
            return store;
        }

        [[nodiscard]] json observation_to_json(const CollectionObservation &o)
        {
            json j{{"domain", o.domain},
                   {"source", static_cast<std::uint32_t>(o.source)},
                   {"origin", o.origin},
                   {"status", static_cast<std::uint32_t>(o.status)}};
            if(o.failure)
                j["failure"] = static_cast<std::uint32_t>(*o.failure);
            return j;
        }
        [[nodiscard]] CollectionObservation observation_from_json(const json &j)
        {
            CollectionObservation o;
            o.domain = j.at("domain").get<std::string>();
            o.source =
                validate_enum(uint32_from_json(j.at("source"), "source"), RawSource::SysfsStorageConnections, "source");
            o.origin = j.at("origin").get<std::string>();
            o.status = validate_enum(uint32_from_json(j.at("status"), "status"), CollectStatus::NotCollected, "status");
            if(j.contains("failure"))
                o.failure =
                    validate_enum(uint32_from_json(j.at("failure"), "failure"), ReadFailure::LowPower, "failure");
            return o;
        }

        // ───────────────────────────── SystemInfo ─────────────────────────────

        [[nodiscard]] json system_info_to_json(const SystemInfo &info)
        {
            return json{
                {"platform", detail::platform_to_json(info.platform)},
                {"cpu", detail::cpu_to_json(info.cpu)},
                {"memory", detail::memory_to_json(info.memory)},
                {"accelerators", detail::accelerators_to_json(info.accelerators)},
                {"network", detail::network_to_json(info.network)},
                {"storage", detail::storage_to_json(info.storage)},
                {"pci", detail::pci_to_json(info.pci)},
                {"software", detail::software_to_json(info.software)},
                {"execution", detail::execution_to_json(info.execution)},
                {"sensors", detail::sensors_to_json(info.sensors)},
                {"hardware_health", detail::health_to_json(info.hardware_health)},
            };
        }

        [[nodiscard]] SystemInfo system_info_from_json(const json &j)
        {
            SystemInfo info;
            info.platform = detail::platform_from_json(j.at("platform"));
            info.cpu = detail::cpu_from_json(j.at("cpu"));
            info.memory = detail::memory_from_json(j.at("memory"));
            info.accelerators = detail::accelerators_from_json(j.at("accelerators"));
            info.network = detail::network_from_json(j.at("network"));
            info.storage = detail::storage_from_json(j.at("storage"));
            info.pci = detail::pci_from_json(j.at("pci"));
            info.software = detail::software_from_json(j.at("software"));
            info.execution = detail::execution_from_json(j.at("execution"));
            if(j.contains("sensors"))
                info.sensors = detail::sensors_from_json(j.at("sensors"));
            if(j.contains("hardware_health"))
                info.hardware_health = detail::health_from_json(j.at("hardware_health"));
            return info;
        }

        // ───────────────────────────── SnapshotMeta ─────────────────────────────

        [[nodiscard]] json meta_to_json(const SnapshotMeta &m)
        {
            json j = {
                {"collect_time", time_point_to_ms(m.collect_time)},
                {"sysal_version", m.sysal_version},
                {"collect_duration", m.collect_duration.count()},
                {"requested_flags", static_cast<std::uint32_t>(m.requested_flags)},
            };

            json succ = json::array();
            for(const auto &s : m.succeeded_collectors)
            {
                succ.push_back(s);
            }
            j["succeeded_collectors"] = std::move(succ);

            json fail = json::array();
            for(const auto &f : m.failed_collectors)
            {
                fail.push_back(f);
            }
            j["failed_collectors"] = std::move(fail);

            if(!m.observations.empty())
            {
                j["observations"] = json::array();
                for(const auto &observation : m.observations)
                    j["observations"].push_back(observation_to_json(observation));
            }
            return j;
        }

        [[nodiscard]] SnapshotMeta meta_from_json(const json &j)
        {
            SnapshotMeta m;
            m.collect_time = ms_to_time_point(j.at("collect_time").get<std::int64_t>());
            j.at("sysal_version").get_to(m.sysal_version);
            m.collect_duration = std::chrono::duration<double>(j.at("collect_duration").get<double>());
            m.requested_flags = static_cast<Collect>(uint32_from_json(j.at("requested_flags"), "requested_flags"));
            m.succeeded_collectors = str_array_from_json(j.at("succeeded_collectors"));
            m.failed_collectors = str_array_from_json(j.at("failed_collectors"));
            if(j.contains("observations"))
                for(const auto &observation : j.at("observations"))
                    m.observations.push_back(observation_from_json(observation));
            return m;
        }

    } // namespace

    // ══════════════════════════════════════════════════════════════════════════
    // 公共接口：sysal::test
    // ══════════════════════════════════════════════════════════════════════════

    namespace test
    {

        void save_raw_store(const RawStore &raw, const std::string &path)
        {
            std::ofstream ofs(path);
            if(!ofs)
            {
                throw SysalError(ErrorKind::IoError, "cannot open file for writing: " + path);
            }
            ofs << raw_store_to_json(raw).dump(4);
            if(!ofs)
            {
                throw SysalError(ErrorKind::IoError, "write failed: " + path);
            }
        }

        RawStore load_raw_store(const std::string &path)
        {
            std::ifstream ifs(path);
            if(!ifs)
            {
                throw SysalError(ErrorKind::FileNotFound, "cannot open file for reading: " + path);
            }
            std::ostringstream oss;
            oss << ifs.rdbuf();
            if(!ifs && !ifs.eof())
            {
                throw SysalError(ErrorKind::IoError, "read failed: " + path);
            }

            try
            {
                auto root = json::parse(oss.str());
                if(!root.is_object())
                {
                    throw SysalError(ErrorKind::DeserializationError, "RawStore JSON root must be an object");
                }
                return raw_store_from_json(root);
            }
            catch(const json::exception &e)
            {
                throw SysalError(ErrorKind::DeserializationError, e.what());
            }
        }

    } // namespace test

    // ══════════════════════════════════════════════════════════════════════════
    // 公共接口：sysal
    // ══════════════════════════════════════════════════════════════════════════

    std::string to_json(const System &sys, const SerializationOptions &opts)
    {
        json root;
        root["info"] = system_info_to_json(sys.info);

        if(opts.include_meta)
        {
            root["meta"] = meta_to_json(sys.meta);
        }

        json warnings = json::array();
        for(const auto &w : sys.warnings)
        {
            warnings.push_back(w);
        }
        root["warnings"] = std::move(warnings);

        if(opts.include_raw && sys.raw)
        {
            root["raw"] = raw_store_to_json(*sys.raw);
        }

        return root.dump(opts.pretty_print ? 4 : -1);
    }

    System from_json(std::string_view json_str)
    {
        try
        {
            auto root = json::parse(json_str);
            if(!root.is_object())
            {
                throw SysalError(ErrorKind::DeserializationError, "JSON root must be an object");
            }

            // 版本兼容性检查
            if(root.contains("meta") && root.at("meta").is_object())
            {
                const auto &meta = root.at("meta");
                if(meta.contains("sysal_version") && meta.at("sysal_version").is_string())
                {
                    auto ver = meta.at("sysal_version").get<std::string>();
                    auto prefix = std::to_string(VERSION_MAJOR) + "." + std::to_string(VERSION_MINOR) + ".";
                    if(!ver.starts_with(prefix))
                    {
                        throw SysalError(ErrorKind::DeserializationError, "incompatible version: " + ver);
                    }
                }
            }

            System sys;
            sys.info = system_info_from_json(root.at("info"));

            if(root.contains("meta") && root.at("meta").is_object())
            {
                sys.meta = meta_from_json(root.at("meta"));
            }

            if(root.contains("warnings") && root.at("warnings").is_array())
            {
                sys.warnings = str_array_from_json(root.at("warnings"));
            }

            if(root.contains("raw") && root.at("raw").is_object())
            {
                sys.raw = raw_store_from_json(root.at("raw"));
            }

            return sys;
        }
        catch(const json::exception &e)
        {
            throw SysalError(ErrorKind::DeserializationError, e.what());
        }
    }

} // namespace sysal
