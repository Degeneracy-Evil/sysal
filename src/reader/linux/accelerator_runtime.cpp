#include "reader/linux/accelerator_runtime.hpp"
#include "reader/linux/file_utils.hpp"

#include <nlohmann/json.hpp>

#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <dlfcn.h>
#include <vector>

namespace sysal::reader
{
    namespace
    {
        class Library
        {
        public:
            explicit Library(const char *name) : handle_(dlopen(name, RTLD_LOCAL | RTLD_LAZY)) {}
            ~Library()
            {
                if(handle_ != nullptr)
                    dlclose(handle_);
            }
            Library(const Library &) = delete;
            Library &operator=(const Library &) = delete;
            Library(Library &&) = delete;
            Library &operator=(Library &&) = delete;
            [[nodiscard]] bool available() const
            {
                return handle_ != nullptr;
            }
            template <typename Function> Function symbol(const char *name) const
            {
                return handle_ == nullptr ? nullptr : std::bit_cast<Function>(dlsym(handle_, name));
            }

        private:
            void *handle_{};
        };

        void read_hip(RawStore &raw, const Library &library)
        {
            const auto count_query = library.symbol<int (*)(int *)>("hipGetDeviceCount");
            const auto pci_query = library.symbol<int (*)(char *, int, int)>("hipDeviceGetPCIBusId");
            if(count_query == nullptr || pci_query == nullptr)
                return;
            int count{};
            const int status = count_query(&count);
            if((status != 0 && status != 100) || count < 0 || count > 65536)
                return;
            if(status == 100)
                count = 0; // hipErrorNoDevice is a successful empty process-visible inventory.
            auto evidence = nlohmann::json{{"vendor", "AMD"}, {"devices", nlohmann::json::array()}};
            for(int index = 0; index < count; ++index)
            {
                std::array<char, 64> pci{};
                if(pci_query(pci.data(), pci.size(), index) != 0)
                    return; // Partial enumeration cannot establish exclusions.
                auto device = nlohmann::json{{"pci_bus_id", pci.data()}, {"index", index}};
                std::array<char, 256> name{};
                const auto name_query = library.symbol<int (*)(char *, int, int)>("hipDeviceGetName");
                if(name_query != nullptr && name_query(name.data(), name.size(), index) == 0)
                    device["name"] = name.data();
                std::size_t memory{};
                const auto memory_query = library.symbol<int (*)(std::size_t *, int)>("hipDeviceTotalMem");
                if(memory_query != nullptr && memory_query(&memory, index) == 0)
                    device["memory_total"] = memory;
                evidence["devices"].push_back(std::move(device));
            }
            int version{};
            const auto version_query = library.symbol<int (*)(int *)>("hipRuntimeGetVersion");
            if(version_query != nullptr && version_query(&version) == 0 && version > 0)
                evidence["runtime_version"] = std::to_string(version / 10000000) + "." +
                                              std::to_string((version / 100000) % 100) + "." +
                                              std::to_string(version % 100000);
            add_record(raw, RawSource::AcceleratorRuntime, "HIP", evidence.dump(), CollectStatus::Success);
        }

        void read_cuda(RawStore &raw)
        {
            const Library library("libcuda.so.1");
            const auto init = library.symbol<int (*)(unsigned)>("cuInit");
            const auto count_query = library.symbol<int (*)(int *)>("cuDeviceGetCount");
            const auto device_query = library.symbol<int (*)(int *, int)>("cuDeviceGet");
            const auto pci_query = library.symbol<int (*)(char *, int, int)>("cuDeviceGetPCIBusId");
            auto uuid_query = library.symbol<int (*)(std::array<char, 16> *, int)>("cuDeviceGetUuid_v2");
            if(uuid_query == nullptr)
                uuid_query = library.symbol<int (*)(std::array<char, 16> *, int)>("cuDeviceGetUuid");
            if(init == nullptr || count_query == nullptr || device_query == nullptr || pci_query == nullptr ||
               uuid_query == nullptr)
                return;
            const int status = init(0);
            int count{};
            if((status != 0 && status != 100) || (status == 0 && count_query(&count) != 0) || count < 0 ||
               count > 65536)
                return;
            auto evidence = nlohmann::json{{"vendor", "NVIDIA"}, {"devices", nlohmann::json::array()}};
            for(int index = 0; index < count; ++index)
            {
                std::array<char, 64> pci{};
                std::array<char, 16> uuid{};
                int device{};
                if(device_query(&device, index) != 0 || pci_query(pci.data(), pci.size(), device) != 0 ||
                   uuid_query(&uuid, device) != 0)
                    return;
                std::string hex;
                for(const auto byte : uuid)
                {
                    char encoded[3];
                    std::snprintf(encoded, sizeof(encoded), "%02x",
                                  static_cast<unsigned>(static_cast<unsigned char>(byte)));
                    hex += encoded;
                }
                auto entry = nlohmann::json{{"pci_bus_id", pci.data()}, {"uuid_hex", hex}};
                std::array<char, 256> name{};
                const auto name_query = library.symbol<int (*)(char *, int, int)>("cuDeviceGetName");
                if(name_query != nullptr && name_query(name.data(), name.size(), device) == 0)
                    entry["name"] = name.data();
                std::size_t memory{};
                const auto memory_query = library.symbol<int (*)(std::size_t *, int)>("cuDeviceTotalMem_v2");
                if(memory_query != nullptr && memory_query(&memory, device) == 0)
                    entry["memory_total"] = memory;
                evidence["devices"].push_back(std::move(entry));
            }
            add_record(raw, RawSource::AcceleratorRuntime, "CUDA Driver", evidence.dump(), CollectStatus::Success);
        }

        // ABI subset from the MIT-licensed public Level Zero ze_api.h (PCI extension).
        struct ZeDriver;
        struct ZeDevice;
        struct ZePci
        {
            std::uint32_t type{0x10008};
            void *next{};
            std::uint32_t domain{}, bus{}, device{}, function{};
            std::int32_t generation{}, width{};
            std::int64_t bandwidth{};
        };

        void read_level_zero(RawStore &raw)
        {
            const Library library("libze_loader.so.1");
            const auto init = library.symbol<int (*)(std::uint32_t)>("zeInit");
            const auto drivers_query = library.symbol<int (*)(std::uint32_t *, ZeDriver **)>("zeDriverGet");
            const auto devices_query = library.symbol<int (*)(ZeDriver *, std::uint32_t *, ZeDevice **)>("zeDeviceGet");
            const auto pci_query = library.symbol<int (*)(ZeDevice *, ZePci *)>("zeDevicePciGetPropertiesExt");
            if(init == nullptr || drivers_query == nullptr || devices_query == nullptr || pci_query == nullptr ||
               init(1) != 0)
                return;
            std::uint32_t count{};
            if(drivers_query(&count, nullptr) != 0 || count > 1024)
                return;
            std::vector<ZeDriver *> drivers(count);
            if(count > 0 && drivers_query(&count, drivers.data()) != 0)
                return;
            if(count > drivers.size())
                return;
            drivers.resize(count);
            auto evidence = nlohmann::json{{"vendor", "Intel"}, {"devices", nlohmann::json::array()}};
            for(auto *driver : drivers)
            {
                std::uint32_t devices_count{};
                if(devices_query(driver, &devices_count, nullptr) != 0 || devices_count > 65536)
                    return;
                std::vector<ZeDevice *> devices(devices_count);
                if(devices_count > 0 && devices_query(driver, &devices_count, devices.data()) != 0)
                    return;
                if(devices_count > devices.size())
                    return;
                devices.resize(devices_count);
                for(auto *device : devices)
                {
                    ZePci pci;
                    if(pci_query(device, &pci) != 0)
                        return;
                    char address[64];
                    std::snprintf(address, sizeof(address), "%04x:%02x:%02x.%x", pci.domain, pci.bus, pci.device,
                                  pci.function);
                    evidence["devices"].push_back(nlohmann::json{{"pci_bus_id", address}});
                }
                std::uint32_t version{};
                const auto version_query =
                    library.symbol<int (*)(ZeDriver *, std::uint32_t *)>("zeDriverGetApiVersion");
                if(version_query != nullptr && version_query(driver, &version) == 0)
                    evidence["api_version"] = std::to_string(version >> 16) + "." + std::to_string(version & 0xffff);
            }
            add_record(raw, RawSource::AcceleratorRuntime, "Level Zero", evidence.dump(), CollectStatus::Success);
        }
    } // namespace

    void read_accelerator_runtimes(RawStore &raw)
    {
        read_cuda(raw);
        for(const auto *name : {"libamdhip64.so", "libamdhip64.so.7", "libamdhip64.so.6", "libamdhip64.so.5"})
        {
            const Library library(name);
            if(!library.available())
                continue;
            read_hip(raw, library);
            break;
        }
        read_level_zero(raw);
    }
} // namespace sysal::reader
