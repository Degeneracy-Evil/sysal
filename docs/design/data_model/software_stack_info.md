# SoftwareStackInfo

描述系统资源能否被软件使用。

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

硬件资源与软件栈分离：

```txt
NVIDIA H20 GPU       → ResourceInfo
CUDA driver/runtime  → SoftwareStackInfo
```

示例：

* NVIDIA 驱动版本
* CUDA 运行时版本
* ROCm 版本
* Level Zero 可用性
* MPI 实现及版本
* UCX 版本
* OpenBLAS / BLIS / MKL / cuBLAS 可用性
* 编译器版本
