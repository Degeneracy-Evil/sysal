#pragma once

#include "sysal/model/accelerator.hpp"

namespace sysal::detail
{
    // Identity resolution and snapshot ID allocation live here, independent of backend formats.
    class AcceleratorInventory
    {
    public:
        explicit AcceleratorInventory(std::vector<std::string> &warnings) : warnings_(warnings) {}
        void add(AcceleratorDevice device);
        [[nodiscard]] std::uint32_t next_id() const;
        Accelerators &devices()
        {
            return devices_;
        }
        Accelerators release()
        {
            return std::move(devices_);
        }

    private:
        Accelerators devices_;
        std::vector<std::string> &warnings_;
    };
} // namespace sysal::detail
