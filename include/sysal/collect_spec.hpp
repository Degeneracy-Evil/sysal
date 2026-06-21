#pragma once

namespace sysal
{

class CollectSpec
{
public:
    static CollectSpec basic()
    {
        CollectSpec spec;
        spec.platform_ = true;
        spec.cpu_ = true;
        spec.memory_ = true;
        spec.execution_context_ = true;
        return spec;
    }

    static CollectSpec full()
    {
        CollectSpec spec;
        spec.raw_ = true;
        spec.platform_ = true;
        spec.cpu_ = true;
        spec.memory_ = true;
        spec.accelerators_ = true;
        spec.network_ = true;
        spec.storage_ = true;
        spec.pci_ = true;
        spec.topology_ = true;
        spec.software_stack_ = true;
        spec.execution_context_ = true;
        return spec;
    }

    static CollectSpec for_operator_dispatch()
    {
        CollectSpec spec;
        spec.platform_ = true;
        spec.cpu_ = true;
        spec.memory_ = true;
        spec.accelerators_ = true;
        spec.network_ = true;
        spec.topology_ = true;
        spec.software_stack_ = true;
        spec.execution_context_ = true;
        return spec;
    }

    CollectSpec& with_raw(bool enabled = true)
    {
        raw_ = enabled;
        return *this;
    }
    CollectSpec& with_platform(bool enabled = true)
    {
        platform_ = enabled;
        return *this;
    }
    CollectSpec& with_cpu(bool enabled = true)
    {
        cpu_ = enabled;
        return *this;
    }
    CollectSpec& with_memory(bool enabled = true)
    {
        memory_ = enabled;
        return *this;
    }
    CollectSpec& with_accelerators(bool enabled = true)
    {
        accelerators_ = enabled;
        return *this;
    }
    CollectSpec& with_network(bool enabled = true)
    {
        network_ = enabled;
        return *this;
    }
    CollectSpec& with_storage(bool enabled = true)
    {
        storage_ = enabled;
        return *this;
    }
    CollectSpec& with_pci(bool enabled = true)
    {
        pci_ = enabled;
        return *this;
    }
    CollectSpec& with_topology(bool enabled = true)
    {
        topology_ = enabled;
        return *this;
    }
    CollectSpec& with_software_stack(bool enabled = true)
    {
        software_stack_ = enabled;
        return *this;
    }
    CollectSpec& with_execution_context(bool enabled = true)
    {
        execution_context_ = enabled;
        return *this;
    }

    bool keep_raw() const { return raw_; }
    bool collect_platform() const { return platform_; }
    bool collect_cpu() const { return cpu_; }
    bool collect_memory() const { return memory_; }
    bool collect_accelerators() const { return accelerators_; }
    bool collect_network() const { return network_; }
    bool collect_storage() const { return storage_; }
    bool collect_pci() const { return pci_; }
    bool collect_topology() const { return topology_; }
    bool collect_software_stack() const { return software_stack_; }
    bool collect_execution_context() const { return execution_context_; }

private:
    bool raw_{false};
    bool platform_{false};
    bool cpu_{false};
    bool memory_{false};
    bool accelerators_{false};
    bool network_{false};
    bool storage_{false};
    bool pci_{false};
    bool topology_{false};
    bool software_stack_{false};
    bool execution_context_{false};
};

} // namespace sysal
