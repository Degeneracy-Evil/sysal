#include "reader/linux/accelerator.hpp"
#include "reader/linux/accelerator_runtime.hpp"
#include "reader/linux/file_utils.hpp"
#include "reader/linux/shared_library.hpp"

#include <nlohmann/json.hpp>

#include <array>
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

        struct NvmlFunctions
        {
            using StringQuery = int (*)(Device, char *, unsigned);
            explicit NvmlFunctions(const SharedLibrary &library)
                : init(library.symbol<int (*)()>("nvmlInit_v2")), shutdown(library.symbol<int (*)()>("nvmlShutdown")),
                  count(library.symbol<int (*)(unsigned *)>("nvmlDeviceGetCount_v2")),
                  handle(library.symbol<int (*)(unsigned, Device *)>("nvmlDeviceGetHandleByIndex_v2")),
                  name(library.symbol<StringQuery>("nvmlDeviceGetName")),
                  uuid(library.symbol<StringQuery>("nvmlDeviceGetUUID")),
                  pci(library.symbol<int (*)(Device, PciInfo *)>("nvmlDeviceGetPciInfo_v3")),
                  memory(library.symbol<int (*)(Device, MemoryInfo *)>("nvmlDeviceGetMemoryInfo")),
                  mig_count(library.symbol<int (*)(Device, unsigned *)>("nvmlDeviceGetMaxMigDeviceCount")),
                  mig_handle(
                      library.symbol<int (*)(Device, unsigned, Device *)>("nvmlDeviceGetMigDeviceHandleByIndex")),
                  gpu_instance(library.symbol<int (*)(Device, unsigned *)>("nvmlDeviceGetGpuInstanceId")),
                  compute_instance(library.symbol<int (*)(Device, unsigned *)>("nvmlDeviceGetComputeInstanceId")),
                  driver(library.symbol<int (*)(char *, unsigned)>("nvmlSystemGetDriverVersion"))
            {
            }
            int (*init)();
            int (*shutdown)();
            int (*count)(unsigned *);
            int (*handle)(unsigned, Device *);
            StringQuery name, uuid;
            int (*pci)(Device, PciInfo *);
            int (*memory)(Device, MemoryInfo *);
            int (*mig_count)(Device, unsigned *);
            int (*mig_handle)(Device, unsigned, Device *);
            int (*gpu_instance)(Device, unsigned *);
            int (*compute_instance)(Device, unsigned *);
            int (*driver)(char *, unsigned);
        };

        class Nvml
        {
        public:
            Nvml()
                : functions_(library_),
                  initialized_(functions_.init != nullptr && functions_.shutdown != nullptr && functions_.init() == 0)
            {
            }
            ~Nvml()
            {
                if(initialized_)
                    functions_.shutdown();
            }
            Nvml(const Nvml &) = delete;
            Nvml &operator=(const Nvml &) = delete;
            Nvml(Nvml &&) = delete;
            Nvml &operator=(Nvml &&) = delete;
            [[nodiscard]] bool available() const
            {
                return initialized_;
            }
            const NvmlFunctions &functions() const
            {
                return functions_;
            }

        private:
            SharedLibrary library_{"libnvidia-ml.so.1"};
            NvmlFunctions functions_;
            bool initialized_{};
        };

        nlohmann::json nvml_device(const NvmlFunctions &api, Device handle, unsigned index)
        {
            nlohmann::json record{{"index", index}};
            for(const auto &[query, key] : std::array<std::pair<NvmlFunctions::StringQuery, const char *>, 2>{
                    {{api.name, "name"}, {api.uuid, "uuid"}}})
            {
                std::array<char, 256> buffer{};
                if(query != nullptr && query(handle, buffer.data(), buffer.size()) == 0)
                    record[key] = buffer.data();
            }
            PciInfo pci;
            if(api.pci != nullptr && api.pci(handle, &pci) == 0)
                record["pci_bus_id"] = pci.bus_id.data();
            MemoryInfo memory;
            if(api.memory != nullptr && api.memory(handle, &memory) == 0)
                record["memory_total"] = memory.total;
            return record;
        }

        void read_nvml(RawStore &raw)
        {
            const Nvml session;
            if(!session.available())
                return;
            const auto &api = session.functions();
            unsigned count{};
            if(api.count == nullptr || api.handle == nullptr || api.count(&count) != 0 || count > 65536)
                return;
            auto evidence = nlohmann::json{{"devices", nlohmann::json::array()}};
            for(unsigned index = 0; index < count; ++index)
            {
                Device handle{};
                if(api.handle(index, &handle) != 0)
                    continue;
                auto device = nvml_device(api, handle, index);
                evidence["devices"].push_back(device);
                if(!device.contains("uuid"))
                    continue; // A MIG instance without a stable parent must not become a physical device.
                unsigned instances{};
                if(api.mig_count == nullptr || api.mig_handle == nullptr || api.mig_count(handle, &instances) != 0 ||
                   instances > 1024)
                    continue;
                for(unsigned instance = 0; instance < instances; ++instance)
                {
                    Device child{};
                    if(api.mig_handle(handle, instance, &child) != 0)
                        continue;
                    auto entry = nvml_device(api, child, index);
                    if(device.contains("uuid"))
                        entry["parent_uuid"] = device["uuid"];
                    entry["mig_index"] = instance;
                    for(const auto &[query, key] : std::array<std::pair<int (*)(Device, unsigned *), const char *>, 2>{
                            {{api.gpu_instance, "gpu_instance_id"}, {api.compute_instance, "compute_instance_id"}}})
                    {
                        unsigned id{};
                        if(query != nullptr && query(child, &id) == 0)
                            entry[key] = id;
                    }
                    evidence["devices"].push_back(std::move(entry));
                }
            }
            std::array<char, 128> version{};
            if(api.driver != nullptr && api.driver(version.data(), version.size()) == 0)
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
            const auto cards = directory_entries(root, error);
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
