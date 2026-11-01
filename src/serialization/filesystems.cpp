#include "serialization/filesystems.hpp"
#include "serialization/json_values.hpp"

namespace sysal::detail
{
    namespace
    {
        constexpr std::pair<const char *, std::optional<MemorySize> FilesystemUsage::*> byte_fields[] = {
            {"free", &FilesystemUsage::free}, {"available", &FilesystemUsage::available}};
        constexpr std::pair<const char *, std::optional<std::uint64_t> FilesystemUsage::*> inode_fields[] = {
            {"inodes", &FilesystemUsage::inodes},
            {"free_inodes", &FilesystemUsage::free_inodes},
            {"available_inodes", &FilesystemUsage::available_inodes}};
    } // namespace
    void filesystems_to_json(nlohmann::json &j, const Storage &storage)
    {
        if(storage.filesystems.empty())
            return;
        j["filesystems"] = nlohmann::json::array();
        for(const auto &report : storage.filesystems)
        {
            nlohmann::json value{{"mount_ids", report.mount_ids},
                                 {"kind", static_cast<unsigned>(report.kind)},
                                 {"status", static_cast<unsigned>(report.status)},
                                 {"note", report.note}};
            if(report.failure)
                value["failure"] = static_cast<unsigned>(*report.failure);
            if(report.skip)
                value["skip"] = static_cast<unsigned>(*report.skip);
            if(report.usage)
            {
                const auto &usage = *report.usage;
                nlohmann::json data{{"total", usage.total.value}, {"filesystem_id", usage.filesystem_id}};
                for(const auto &[name, member] : byte_fields)
                    if(const auto &amount = usage.*member)
                        data[name] = amount->value;
                for(const auto &[name, member] : inode_fields)
                    if(const auto &amount = usage.*member)
                        data[name] = *amount;
                value["usage"] = std::move(data);
            }
            j["filesystems"].push_back(std::move(value));
        }
    }
    void filesystems_from_json(const nlohmann::json &j, Storage &storage)
    {
        if(!j.contains("filesystems"))
            return;
        for(const auto &value : j.at("filesystems"))
        {
            FilesystemReport report;
            for(const auto &id : value.at("mount_ids"))
                report.mount_ids.push_back(uint32_from_json(id, "filesystem mount id"));
            report.kind = validate_enum(uint32_from_json(value.at("kind"), "filesystem kind"), FilesystemKind::Unknown,
                                        "filesystem kind");
            report.status = validate_enum(uint32_from_json(value.at("status"), "filesystem status"),
                                          CollectStatus::NotCollected, "filesystem status");
            if(value.contains("failure"))
                report.failure = validate_enum(uint32_from_json(value.at("failure"), "filesystem failure"),
                                               ReadFailure::LowPower, "filesystem failure");
            if(value.contains("skip"))
                report.skip = validate_enum(uint32_from_json(value.at("skip"), "filesystem skip"),
                                            SkipReason::NotApplicable, "filesystem skip");
            report.note = value.value("note", std::string{});
            if(value.contains("usage"))
            {
                const auto &data = value.at("usage");
                FilesystemUsage usage;
                usage.total = MemorySize{uint64_from_json(data.at("total"), "filesystem total")};
                usage.filesystem_id = data.value("filesystem_id", std::string{});
                for(const auto &[name, member] : byte_fields)
                    if(data.contains(name))
                        usage.*member = MemorySize{uint64_from_json(data.at(name), name)};
                for(const auto &[name, member] : inode_fields)
                    if(data.contains(name))
                        usage.*member = uint64_from_json(data.at(name), name);
                report.usage = std::move(usage);
            }
            storage.filesystems.push_back(std::move(report));
        }
    }
} // namespace sysal::detail
