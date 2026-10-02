#include "reader/linux/accelerator.hpp"
#include "reader/linux/accelerator_runtime.hpp"
#include "reader/linux/file_utils.hpp"

#include <nlohmann/json.hpp>

#include <array>
#include <bit>
#include <dlfcn.h>
#include <filesystem>
#include <string>

namespace sysal::reader
{
    namespace
    {
        // ABI subset of NVIDIA's public nvml.h. No NVIDIA SDK or link-time library is required.
        struct NvmlDevice;
        using Device = NvmlDevice *;
        struct PciInfo
        {
            std::array<char, 16> legacy_bus_id{};
            unsigned int domain{}, bus{}, device{}, device_id{}, subsystem_id{};
            std::array<char, 32> bus_id{};
        };
        struct MemoryInfo
        {
            unsigned long long total{}, free{}, used{};
        };

        class Nvml
        {
        public:
            Nvml() : library_(dlopen("libnvidia-ml.so.1", RTLD_LOCAL | RTLD_LAZY))
            {
                const auto init = symbol<int (*)()>("nvmlInit_v2");
                shutdown_ = symbol<int (*)()>("nvmlShutdown");
                initialized_ = init != nullptr && shutdown_ != nullptr && init() == 0;
            }
            ~Nvml()
            {
                if(initialized_)
                    shutdown_();
                if(library_ != nullptr)
                    dlclose(library_);
            }
            Nvml(const Nvml &) = delete;
            Nvml &operator=(const Nvml &) = delete;
            Nvml(Nvml &&) = delete;
            Nvml &operator=(Nvml &&) = delete;
            [[nodiscard]] bool available() const
            {
                return initialized_;
            }
            template <typename Function> Function symbol(const char *name) const
            {
                return library_ == nullptr ? nullptr : std::bit_cast<Function>(dlsym(library_, name));
            }

        private:
            void *library_{};
            int (*shutdown_)(){};
            bool initialized_{};
        };

        nlohmann::json nvml_device(const Nvml &api, Device handle, unsigned index)
        {
            nlohmann::json record{{"index", index}};
            using StringQuery = int (*)(Device, char *, unsigned);
            for(const auto &[symbol, key] : std::array<std::pair<const char *, const char *>, 2>{
                    {{"nvmlDeviceGetName", "name"}, {"nvmlDeviceGetUUID", "uuid"}}})
            {
                std::array<char, 256> buffer{};
                const auto query = api.symbol<StringQuery>(symbol);
                if(query != nullptr && query(handle, buffer.data(), buffer.size()) == 0)
                    record[key] = buffer.data();
            }
            PciInfo pci;
            const auto pci_query = api.symbol<int (*)(Device, PciInfo *)>("nvmlDeviceGetPciInfo_v3");
            if(pci_query != nullptr && pci_query(handle, &pci) == 0)
                record["pci_bus_id"] = pci.bus_id.data();
            MemoryInfo memory;
            const auto memory_query = api.symbol<int (*)(Device, MemoryInfo *)>("nvmlDeviceGetMemoryInfo");
            if(memory_query != nullptr && memory_query(handle, &memory) == 0)
                record["memory_total"] = memory.total;
            return record;
        }

        void read_nvml(RawStore &raw)
        {
            const Nvml api;
            if(!api.available())
                return;
            const auto count_query = api.symbol<int (*)(unsigned *)>("nvmlDeviceGetCount_v2");
            const auto handle_query = api.symbol<int (*)(unsigned, Device *)>("nvmlDeviceGetHandleByIndex_v2");
            unsigned count{};
            if(count_query == nullptr || handle_query == nullptr || count_query(&count) != 0 || count > 65536)
                return;
            auto evidence = nlohmann::json{{"devices", nlohmann::json::array()}};
            for(unsigned index = 0; index < count; ++index)
            {
                Device handle{};
                if(handle_query(index, &handle) != 0)
                    continue;
                auto device = nvml_device(api, handle, index);
                evidence["devices"].push_back(device);
                const auto mig_count = api.symbol<int (*)(Device, unsigned *)>("nvmlDeviceGetMaxMigDeviceCount");
                const auto mig_handle =
                    api.symbol<int (*)(Device, unsigned, Device *)>("nvmlDeviceGetMigDeviceHandleByIndex");
                unsigned instances{};
                if(mig_count == nullptr || mig_handle == nullptr || mig_count(handle, &instances) != 0 ||
                   instances > 1024)
                    continue;
                for(unsigned instance = 0; instance < instances; ++instance)
                {
                    Device child{};
                    if(mig_handle(handle, instance, &child) != 0)
                        continue;
                    auto entry = nvml_device(api, child, index);
                    if(device.contains("uuid"))
                        entry["parent_uuid"] = device["uuid"];
                    entry["mig_index"] = instance;
                    for(const auto &[symbol, key] : std::array<std::pair<const char *, const char *>, 2>{
                            {{"nvmlDeviceGetGpuInstanceId", "gpu_instance_id"},
                             {"nvmlDeviceGetComputeInstanceId", "compute_instance_id"}}})
                    {
                        const auto query = api.symbol<int (*)(Device, unsigned *)>(symbol);
                        unsigned id{};
                        if(query != nullptr && query(child, &id) == 0)
                            entry[key] = id;
                    }
                    evidence["devices"].push_back(std::move(entry));
                }
            }
            std::array<char, 128> version{};
            const auto driver_query = api.symbol<int (*)(char *, unsigned)>("nvmlSystemGetDriverVersion");
            if(driver_query != nullptr && driver_query(version.data(), version.size()) == 0)
                evidence["driver_version"] = version.data();
            add_record(raw, RawSource::Nvml, "libnvidia-ml.so.1", evidence.dump(), CollectStatus::Success);
        }

        void read_drm(RawStore &raw)
        {
            namespace fs = std::filesystem;
            std::error_code error;
            const fs::path root{"/sys/class/drm"};
            if(!fs::exists(root, error))
                return;
            const fs::directory_iterator cards(root, error);
            add_record(raw, RawSource::SysfsDrm, root.string(), "{}",
                       error ? CollectStatus::Failed : CollectStatus::Success);
            for(const auto &entry : cards)
            {
                const auto name = entry.path().filename().string();
                if(!name.starts_with("card") || name.size() <= 4 ||
                   name.find_first_not_of("0123456789", 4) != std::string::npos)
                    continue;
                const auto device = fs::canonical(entry.path() / "device", error);
                if(error)
                {
                    error.clear();
                    continue;
                }
                auto record = nlohmann::json{{"card", name}, {"pci_bus_id", device.filename().string()}};
                for(const auto *field :
                    {"vendor", "device", "product_name", "unique_id", "mem_info_vram_total", "numa_node"})
                {
                    if(const auto value = read_file((device / field).string()))
                        record[field] = *value;
                }
                const auto driver = fs::read_symlink(device / "driver", error);
                if(!error)
                    record["driver"] = driver.filename().string();
                error.clear();
                add_record(raw, RawSource::SysfsDrm, entry.path().string(), record.dump(), CollectStatus::Success);
            }
        }
    } // namespace

    void read_accelerator_backends(RawStore &raw)
    {
        read_nvml(raw);
        read_drm(raw);
        read_accelerator_runtimes(raw);
    }
} // namespace sysal::reader
