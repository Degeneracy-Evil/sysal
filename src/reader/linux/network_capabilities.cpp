#include "reader/linux/network_capabilities.hpp"
#include "reader/linux/file_utils.hpp"

#include <string_view>
#include <utility>

namespace sysal::reader
{
    void read_network_capabilities(RawStore &raw, const std::string &interface)
    {
        // Quote kernel-provided names as one shell argument; no network configuration is changed.
        if(interface.empty() || interface.front() == '-')
        {
            add_record(raw, RawSource::Ethtool, "ethtool/" + interface, "", CollectStatus::NotCollected,
                       ReadFailure::Unsupported);
            return;
        }
        const auto argument = shell_argument(interface);
        const std::pair<std::string_view, std::string_view> queries[] = {
            {"settings", ""}, {"driver", "-i "}, {"permanent_address", "-P "}};
        for(const auto &[field, option] : queries)
        {
            const auto result = execute_command("LC_ALL=C ethtool " + std::string(option) + argument);
            const auto failure = command_failure(result);
            const auto status = result.successful()     ? (failure ? CollectStatus::Partial : CollectStatus::Success)
                                : result.output.empty() ? CollectStatus::Failed
                                                        : CollectStatus::Partial;
            add_record(raw, RawSource::Ethtool, "ethtool/" + interface + "/" + std::string(field), result.output,
                       status, failure);
            if(failure == ReadFailure::ToolUnavailable)
                break;
        }
    }
} // namespace sysal::reader
