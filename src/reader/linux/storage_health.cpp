#include "reader/linux/storage_health.hpp"
#include "reader/linux/file_utils.hpp"

#include <filesystem>
#include <map>
#include <nlohmann/json.hpp>

namespace sysal::reader
{
    namespace
    {
        namespace fs = std::filesystem;
        void query(RawStore &raw, const std::string &target, const std::string &protocol)
        {
            const auto argument = shell_argument("/dev/" + target);
            const auto power = protocol == "ata" ? " -n standby,3,5" : protocol == "scsi" ? " -n standby,3" : "";
            const auto data = protocol == "scsi" ? " -H -l error" : " -H -A";
            const auto result = execute_command("LC_ALL=C smartctl --json=v" + std::string(data) + " -d " + protocol +
                                                power + " " + argument);
            auto failure = command_failure(result);
            auto status = CollectStatus::Failed;
            const auto json = nlohmann::json::parse(result.output, nullptr, false);
            // smartctl encodes device findings in upper exit bits, not process failure.
            if(json.is_object() && result.exit_code && result.status != CommandStatus::TimedOut &&
               (result.status == CommandStatus::Success || result.status == CommandStatus::Failed))
            {
                const auto code = *result.exit_code;
                failure = (code & 7) == 0 ? std::nullopt : std::optional{ReadFailure::IoError};
                status = (code & 7) == 0   ? CollectStatus::Success
                         : (code & 3) != 0 ? CollectStatus::Failed
                                           : CollectStatus::Partial;
                if(code & 1)
                    failure = ReadFailure::Unsupported;
                if(protocol != "nvme" && code == 3)
                {
                    failure = ReadFailure::LowPower;
                    status = CollectStatus::NotCollected;
                }
                else if(protocol == "ata" && code == 5)
                {
                    failure = ReadFailure::Unsupported;
                    status = CollectStatus::NotCollected;
                }
            }
            if(result.output.find("Permission denied") != std::string::npos ||
               result.output.find("Operation not permitted") != std::string::npos)
                failure = ReadFailure::PermissionDenied;
            add_record(raw, RawSource::Smartctl, "smartctl/" + target, result.output, status, failure);
            if(protocol == "nvme" && failure == ReadFailure::ToolUnavailable)
            {
                const auto fallback = execute_command("LC_ALL=C nvme smart-log " + argument + " -o json");
                add_record(raw, RawSource::NvmeSmartLog, "nvme-smart-log/" + target, fallback.output,
                           fallback.successful() ? CollectStatus::Success : CollectStatus::Failed,
                           command_failure(fallback));
            }
        }
        bool numbered(std::string_view name, std::string_view prefix)
        {
            return name.starts_with(prefix) && name.size() > prefix.size() &&
                   name.find_first_not_of("0123456789", prefix.size()) == std::string_view::npos;
        }
        void associate(RawStore &raw, const fs::path &block, const std::string &target)
        {
            add_record(raw, RawSource::StorageHealthSysfs, (block / "health_target").string(), target,
                       CollectStatus::Success);
        }
    } // namespace
    void read_storage_health(RawStore &raw)
    {
        std::map<fs::path, std::string> controllers;
        std::map<std::string, std::string> targets;
        std::error_code ec;
        for(const auto &entry : directory_entries("/sys/class/nvme", ec))
        {
            const auto name = entry.path().filename().string();
            if(!numbered(name, "nvme"))
                continue;
            std::error_code canonical_ec;
            const auto path = fs::canonical(entry.path(), canonical_ec);
            if(!canonical_ec)
            {
                controllers.emplace(path, name);
                targets.emplace(name, "nvme");
            }
        }
        record_directory_failure(raw, RawSource::StorageHealthSysfs, "/sys/class/nvme", ec);
        ec.clear();
        for(const auto &entry : directory_entries("/sys/class/block", ec))
        {
            std::error_code entry_ec;
            if(fs::exists(entry.path() / "partition", entry_ec) || entry_ec)
                continue;
            const auto path = fs::canonical(entry.path(), entry_ec);
            if(entry_ec || path.string().find("/virtual/") != std::string::npos)
                continue;
            bool associated = false;
            for(auto parent = path; parent != parent.root_path(); parent = parent.parent_path())
                if(const auto found = controllers.find(parent); found != controllers.end())
                {
                    associate(raw, entry.path(), found->second);
                    associated = true;
                    break;
                }
            if(associated)
                continue;
            const auto type = read_file((entry.path() / "device/type").string());
            if(!type || type->find_first_not_of("0\n \t") != std::string::npos || type->find('0') == std::string::npos)
                continue;
            bool ata = false;
            bool usb = false;
            for(const auto &part : path)
            {
                ata |= numbered(part.string(), "ata");
                usb |= numbered(part.string(), "usb");
            }
            const auto name = entry.path().filename().string();
            if(usb || name.empty() || name.front() == '-')
            {
                add_record(raw, RawSource::StorageHealthSysfs, (entry.path() / "health_target").string(), "",
                           CollectStatus::NotCollected, ReadFailure::Unsupported);
                continue;
            }
            associate(raw, entry.path(), name);
            targets.emplace(name, ata ? "ata" : "scsi");
        }
        if(ec)
            add_record(raw, RawSource::StorageHealthSysfs, "/sys/class/block", "", CollectStatus::Failed,
                       file_failure(ec.value()));
        for(const auto &[name, protocol] : targets)
        {
            add_record(raw, RawSource::StorageHealthSysfs, "/dev/" + name + "/protocol", protocol,
                       CollectStatus::Success);
            query(raw, name, protocol);
        }
        if(targets.empty())
            add_record(raw, RawSource::StorageHealthSysfs, "storage-health", "", CollectStatus::NotCollected,
                       ReadFailure::NotPresent);
    }
} // namespace sysal::reader
