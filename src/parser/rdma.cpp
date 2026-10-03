#include "parser/rdma.hpp"

#include "parser/parse_utils.hpp"

#include <algorithm>
#include <limits>
#include <map>

namespace sysal::detail
{
    namespace
    {
        constexpr std::string_view root = "/sys/class/infiniband";
        constexpr std::string_view prefix = "/sys/class/infiniband/";

        template <typename Enum> Enum reported_enum(std::string_view payload, Enum maximum)
        {
            const auto number = parse_uint(payload.substr(0, payload.find(':')));
            return number && *number <= static_cast<std::uint64_t>(maximum) ? static_cast<Enum>(*number) : Enum{};
        }

        void append_interface(std::vector<InterfaceName> &interfaces, std::string_view payload)
        {
            const auto name = trim(payload);
            if(name.empty() || name.find('/') != std::string::npos || name == "." || name == "..")
                return;
            const InterfaceName interface{name};
            if(std::find(interfaces.begin(), interfaces.end(), interface) == interfaces.end())
                interfaces.push_back(interface);
        }

        std::optional<Bandwidth> parse_rate(std::string_view report)
        {
            const auto unit = report.find(" Gb/sec");
            if(unit == std::string_view::npos)
                return std::nullopt;
            const auto decimal = report.substr(0, unit);
            const auto dot = decimal.find('.');
            const auto whole = parse_uint(decimal.substr(0, dot));
            constexpr std::uint64_t scale = 1000000000;
            if(!whole || *whole > std::numeric_limits<std::uint64_t>::max() / scale)
                return std::nullopt;
            std::uint64_t fraction = 0;
            if(dot != std::string_view::npos)
            {
                const auto digits = decimal.substr(dot + 1);
                const auto value = parse_uint(digits);
                if(!value || digits.size() > 9 || digits.find_first_not_of("0123456789") != std::string_view::npos)
                    return std::nullopt;
                fraction = *value;
                for(std::size_t size = digits.size(); size < 9; ++size)
                    fraction *= 10;
            }
            if(fraction > std::numeric_limits<std::uint64_t>::max() - *whole * scale)
                return std::nullopt;
            return Bandwidth{*whole * scale + fraction};
        }

        void parse_port(RdmaPort &port, std::string_view field, std::string_view payload)
        {
            if(field == "state")
                port.state = reported_enum(payload, RdmaPortState::ActiveDeferred);
            else if(field == "phys_state")
                port.physical_state = reported_enum(payload, RdmaPhysicalState::PhyTest);
            else if(field == "link_layer")
            {
                const auto value = trim(payload);
                if(value == "InfiniBand")
                    port.link_layer = RdmaLinkLayer::InfiniBand;
                else if(value == "Ethernet")
                    port.link_layer = RdmaLinkLayer::Ethernet;
            }
            else if(field == "rate")
            {
                port.rate_report = hardware_text(payload);
                port.rate = parse_rate(port.rate_report);
            }
            else if(field == "lid" || field == "sm_lid" || field == "cap_mask")
            {
                const auto value = trim(payload);
                const auto digits = std::string_view{value};
                const auto number =
                    parse_hex(digits.starts_with("0x") || digits.starts_with("0X") ? digits.substr(2) : digits);
                if(!number || *number > std::numeric_limits<std::uint32_t>::max())
                    return;
                if(field == "cap_mask")
                    port.capability_mask = static_cast<std::uint32_t>(*number);
                else
                {
                    if(field == "lid")
                        port.lid = RdmaLid{static_cast<std::uint32_t>(*number)};
                    else
                        port.subnet_manager_lid = RdmaLid{static_cast<std::uint32_t>(*number)};
                }
            }
            else if(field == "sm_sl" || field == "lid_mask_count")
            {
                const auto number = parse_uint(trim(payload));
                const std::uint64_t maximum = field == "sm_sl" ? 15 : 7;
                if(number && *number <= maximum)
                {
                    if(field == "sm_sl")
                        port.subnet_manager_sl = static_cast<std::uint8_t>(*number);
                    else
                        port.lid_mask_count = static_cast<std::uint8_t>(*number);
                }
            }
            else if(field.starts_with("gid_attrs/ndevs/") && parse_uint(field.substr(16)))
                append_interface(port.network_interfaces, payload);
        }

        RdmaDevice parse_device(const std::string &name, const std::vector<const RawRecord *> &records, RdmaDeviceId id)
        {
            RdmaDevice device;
            device.id = id;
            device.name = RdmaDeviceName{name};
            std::map<std::uint32_t, RdmaPort> ports;
            const auto base = std::string(prefix) + name;
            for(const auto *record : records)
            {
                if(record->path_or_command.size() <= base.size())
                    continue;
                const std::string_view field{record->path_or_command.data() + base.size() + 1,
                                             record->path_or_command.size() - base.size() - 1};
                const auto &payload = record->payload;
                if(field == "node_type")
                    device.node_type = reported_enum(payload, RdmaNodeType::UsnicUdp);
                else if(field == "node_guid")
                    device.node_guid = RdmaGuid{hardware_text(payload)};
                else if(field == "sys_image_guid")
                    device.system_image_guid = RdmaGuid{hardware_text(payload)};
                else if(field == "node_desc")
                    device.description = hardware_text(payload);
                else if(field == "fw_ver")
                    device.firmware_version = hardware_text(payload);
                else if(field == "device/driver")
                    device.driver = extract_filename(trim(payload));
                else if(field == "sysfs_path")
                {
                    for(const auto &component : split(trim(payload), '/'))
                        if(auto address = parse_pci_address(component))
                            device.pci_address = address;
                }
                else if(field.starts_with("device/net/") && field.find('/', 11) == std::string_view::npos)
                    append_interface(device.network_interfaces, field.substr(11));
                else if(field.starts_with("ports/"))
                {
                    const auto tail = field.substr(6);
                    const auto slash = tail.find('/');
                    const auto number = parse_uint(tail.substr(0, slash));
                    if(!number || *number > std::numeric_limits<std::uint32_t>::max())
                        continue;
                    const auto port_number = static_cast<std::uint32_t>(*number);
                    auto &port = ports[port_number];
                    port.number = RdmaPortNumber{port_number};
                    if(slash != std::string_view::npos)
                        parse_port(port, tail.substr(slash + 1), payload);
                }
            }
            for(auto &[number, port] : ports)
            {
                (void)number;
                device.ports.push_back(std::move(port));
            }
            return device;
        }
    } // namespace

    RdmaInventory parse_rdma(const RawStore &raw)
    {
        RdmaInventory inventory;
        std::map<std::string, std::vector<const RawRecord *>> devices;
        for(const auto *record : raw.get_all(RawSource::SysfsRdma))
        {
            if(record->path_or_command == root)
            {
                inventory.status = record->status;
                inventory.failure = record->failure;
            }
            if(record->status != CollectStatus::Success || !record->path_or_command.starts_with(prefix))
                continue;
            const auto tail = std::string_view{record->path_or_command}.substr(prefix.size());
            const auto name = tail.substr(0, tail.find('/'));
            if(!name.empty() && name != "." && name != "..")
                devices[std::string(name)].push_back(record);
        }
        for(const auto &[name, records] : devices)
            inventory.devices.push_back(
                parse_device(name, records, RdmaDeviceId{static_cast<std::uint32_t>(inventory.devices.size())}));
        return inventory;
    }
} // namespace sysal::detail
