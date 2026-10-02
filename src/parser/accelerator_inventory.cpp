#include "parser/accelerator_inventory.hpp"

#include <algorithm>
#include <limits>

namespace sysal::detail
{
    namespace
    {
        bool same_device(const AcceleratorDevice &left, const AcceleratorDevice &right)
        {
            if(left.vendor != right.vendor || left.parent_uuid != right.parent_uuid)
                return false;
            if(left.uuid && right.uuid)
                return left.uuid == right.uuid;
            if(left.parent_uuid)
                return left.gpu_instance_id && right.gpu_instance_id && left.compute_instance_id &&
                       right.compute_instance_id && left.gpu_instance_id == right.gpu_instance_id &&
                       left.compute_instance_id == right.compute_instance_id;
            if(left.pci_address && right.pci_address)
                return left.pci_address == right.pci_address;
            // An ordinal is a fallback only when neither stable identity contradicts it.
            return left.id == right.id;
        }

        template <typename T> void fill_missing(std::optional<T> &target, const std::optional<T> &source)
        {
            if(!target)
                target = source;
        }
    } // namespace

    std::uint32_t AcceleratorInventory::next_id() const
    {
        std::uint32_t next = 0;
        while(std::any_of(devices_.devices.begin(), devices_.devices.end(),
                          [&](const auto &device) { return device.id.value() == next; }))
            ++next;
        return next;
    }

    void AcceleratorInventory::add(AcceleratorDevice device)
    {
        if(device.id.value() == std::numeric_limits<std::uint32_t>::max())
        {
            warnings_.push_back("parse_accelerator: device ordinal out of range");
            return;
        }
        for(auto &existing : devices_.devices)
        {
            if(!same_device(existing, device))
            {
                if(!existing.parent_uuid && !device.parent_uuid && existing.vendor == device.vendor && existing.uuid &&
                   device.uuid && existing.pci_address && existing.pci_address == device.pci_address)
                    warnings_.push_back(
                        "parse_accelerator: conflicting UUIDs at the same PCI address; devices kept separate");
                continue;
            }
            if(existing.pci_address && device.pci_address && existing.pci_address != device.pci_address)
                warnings_.push_back("parse_accelerator: conflicting PCI addresses for the same UUID");
            fill_missing(existing.uuid, device.uuid);
            fill_missing(existing.pci_address, device.pci_address);
            fill_missing(existing.memory_size, device.memory_size);
            fill_missing(existing.nearest_numa_node, device.nearest_numa_node);
            fill_missing(existing.gpu_instance_id, device.gpu_instance_id);
            fill_missing(existing.compute_instance_id, device.compute_instance_id);
            return;
        }
        // A later physical inventory may fill a missing ordinal occupied by a synthetic MIG ID.
        if(!device.parent_uuid)
            for(auto &existing : devices_.devices)
                if(existing.parent_uuid && existing.id == device.id)
                    existing.id = AcceleratorId{next_id()};
        if(device.parent_uuid || std::any_of(devices_.devices.begin(), devices_.devices.end(),
                                             [&](const auto &existing) { return existing.id == device.id; }))
            device.id = AcceleratorId{next_id()};
        devices_.devices.push_back(std::move(device));
    }
} // namespace sysal::detail
