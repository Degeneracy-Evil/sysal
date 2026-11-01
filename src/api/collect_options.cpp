#include "sysal/core/collect_options.hpp"

#include <stdexcept>

namespace sysal
{
    void CollectOptions::validate() const
    {
        switch(filesystems)
        {
        case FilesystemQuery::None:
        case FilesystemQuery::Local:
        case FilesystemQuery::AllMounted:
            break;
        default:
            throw std::invalid_argument("invalid filesystem query policy");
        }
        switch(sensors)
        {
        case SensorDevices::All:
        case SensorDevices::ExcludeDisplay:
            break;
        default:
            throw std::invalid_argument("invalid sensor device policy");
        }
    }

    void CollectionRequest::validate() const
    {
        if((static_cast<std::uint32_t>(flags) & ~static_cast<std::uint32_t>(full)) != 0)
            throw std::invalid_argument("unknown collection flags");
        options.validate();
    }
} // namespace sysal
