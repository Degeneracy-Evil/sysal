# SoftwareStackInfo

Describes whether system resources can be used by software.

```cpp
struct SoftwareStackInfo
{
    std::vector<DriverInfo> drivers;
    std::vector<RuntimeInfo> runtimes;
    std::vector<CompilerInfo> compilers;
    std::vector<LibraryInfo> libraries;

    std::optional<CudaInfo> cuda;
    std::optional<RocmInfo> rocm;
    std::optional<LevelZeroInfo> level_zero;
    std::optional<MpiInfo> mpi;
    std::optional<RdmaStackInfo> rdma;
};
```

Hardware resources and software stack are separated:

```txt
NVIDIA H20 GPU       → ResourceInfo
CUDA driver/runtime  → SoftwareStackInfo
```

Examples:

* NVIDIA driver version
* CUDA runtime version
* ROCm version
* Level Zero availability
* MPI implementation and version
* UCX version
* OpenBLAS / BLIS / MKL / cuBLAS availability
* compiler versions
