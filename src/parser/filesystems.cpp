#include "parser/filesystems.hpp"
#include "common/filesystem.hpp"
#include <limits>
#include <nlohmann/json.hpp>
#include <set>

namespace sysal::detail
{
    namespace
    {
        std::optional<std::uint64_t> number(const nlohmann::json &value)
        {
            if(!value.is_number_integer() || (!value.is_number_unsigned() && value.get<std::int64_t>() < 0))
                return std::nullopt;
            return value.get<std::uint64_t>();
        }
        std::optional<std::uint64_t> field(const nlohmann::json &value, const char *name)
        {
            const auto item = value.find(name);
            return item == value.end() ? std::nullopt : number(*item);
        }
        void statistics(FilesystemReport &report, const nlohmann::json &stats)
        {
            const auto size = field(stats, "block_size");
            const auto blocks = field(stats, "blocks");
            if(!size || !*size || !blocks || *blocks > std::numeric_limits<std::uint64_t>::max() / *size)
            {
                report.status = CollectStatus::Failed;
                report.failure = ReadFailure::IoError;
                report.note = "Invalid filesystem capacity statistics";
                return;
            }
            FilesystemUsage usage;
            usage.total = MemorySize{*blocks * *size};
            const auto free = field(stats, "free_blocks");
            const auto available = field(stats, "available_blocks");
            if(free && *free <= *blocks)
            {
                usage.free = MemorySize{*free * *size};
                if(available && *available <= *free)
                    usage.available = MemorySize{*available * *size};
            }
            if(!usage.free || !usage.available)
            {
                report.status = CollectStatus::Partial;
                report.note = "Some space statistics were missing or inconsistent";
            }
            const auto inodes = field(stats, "inodes");
            if(inodes && *inodes > 0 && *inodes != std::numeric_limits<std::uint64_t>::max())
            {
                usage.inodes = inodes;
                const auto free_inodes = field(stats, "free_inodes");
                const auto available_inodes = field(stats, "available_inodes");
                if(free_inodes && *free_inodes <= *inodes)
                {
                    usage.free_inodes = free_inodes;
                    if(available_inodes && *available_inodes <= *free_inodes)
                        usage.available_inodes = available_inodes;
                }
            }
            if(const auto id = stats.find("filesystem_id"); id != stats.end() && id->is_string())
                usage.filesystem_id = id->get<std::string>();
            report.usage = std::move(usage);
        }
    } // namespace

    void apply_filesystems(Storage &storage, const RawStore &raw, std::vector<std::string> &warnings)
    {
        std::set<std::uint32_t> mounts;
        for(const auto &mount : storage.mounts)
            mounts.insert(mount.mount_id);
        for(const auto *record : raw.get_all(RawSource::FilesystemUsage))
        {
            const auto value = nlohmann::json::parse(record->payload, nullptr, false);
            if(!value.is_object() || !value.contains("mount_ids") || !value["mount_ids"].is_array())
            {
                warnings.push_back("parse_storage: invalid filesystem report in " + record->path_or_command);
                continue;
            }
            FilesystemReport report;
            for(const auto &id : value["mount_ids"])
                if(const auto parsed = number(id); parsed && *parsed <= std::numeric_limits<std::uint32_t>::max() &&
                                                   mounts.contains(static_cast<std::uint32_t>(*parsed)))
                    report.mount_ids.push_back(static_cast<std::uint32_t>(*parsed));
            if(report.mount_ids.empty())
                continue;
            for(const auto &mount : storage.mounts)
                if(mount.mount_id == report.mount_ids.front())
                {
                    report.kind = filesystem_kind(mount.filesystem.value);
                    break;
                }
            report.skip = record->skip;
            report.status = record->status;
            report.failure = record->failure;
            if(const auto note = value.find("note"); note != value.end() && note->is_string())
                report.note = note->get<std::string>();
            if(record->status == CollectStatus::Success)
            {
                const auto stats = value.find("stats");
                statistics(report, stats != value.end() && stats->is_object() ? *stats : nlohmann::json::object());
            }
            storage.filesystems.push_back(std::move(report));
        }
    }
} // namespace sysal::detail
