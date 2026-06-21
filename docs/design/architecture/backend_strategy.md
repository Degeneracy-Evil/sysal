# Backend Strategy

sysal may depend on external libraries, but public APIs must remain independent.

```txt
Topology:                  Prefer hwloc.
Generic hardware inventory: procfs + sysfs + pci.
GPU:                       NVML for NVIDIA, ROCm SMI for AMD, Level Zero for Intel.
RDMA:                      sysfs + ibverbs.
Software stack:            version queries, dynamic library detection, command output parsing.
```

External backends are information sources only; their types do not appear in the public API.
