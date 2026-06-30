# SoftwareStack

描述系统上的软件栈：哪些驱动、运行时、编译器、库已安装可用，以及是否有
CUDA / ROCm / Level Zero / MPI / RDMA 栈。`SoftwareStack` 回答
"硬件资源能否被软件使用"——同一台机器，软件栈不同，可用能力不同。

## SoftwareStack 结构体

```cpp
namespace sysal
{

struct SoftwareStack
{
    std::vector<Driver>   drivers;    // 已安装的驱动
    std::vector<Runtime>  runtimes;   // 已安装的运行时
    std::vector<Compiler> compilers;  // 已安装的编译器
    std::vector<Library>  libraries;  // 已安装的库

    std::optional<Cuda>      cuda;      // CUDA 栈（若存在）
    std::optional<Rocm>      rocm;      // ROCm 栈（若存在）
    std::optional<LevelZero> level_zero; // Level Zero 栈（若存在）
    std::optional<Mpi>       mpi;       // MPI 栈（若存在）
    std::optional<RdmaStack> rdma;      // RDMA 栈（若存在）
};

}  // namespace sysal
```

通用项（`drivers` / `runtimes` / `compilers` / `libraries`）为向量，
因为同一类软件可能存在多个版本共存。专用栈（CUDA / ROCm / Level Zero /
MPI / RDMA）为 `std::optional`，因为它们要么存在要么不存在，且字段较多，
单独建模比塞进通用向量更便于类型化访问。

## 硬件资源与软件栈分离

sysal 将"硬件资源"与"软件栈"严格分离：

* 硬件资源（`Cpu` / `Memory` / `Accelerators` / `Network` / `Storage`）
  描述机器上**有什么**。
* 软件栈（`SoftwareStack`）描述这些资源**能否被使用**——驱动是否加载、
  运行时是否安装、版本是否匹配。

这样分离的原因：同一台物理机，在不同容器或不同用户环境下，可见的软件栈
可能完全不同；而硬件资源由 `ExecutionContext` 的可见性模型独立约束。

## 子结构体示例

| 结构体 | 说明 | 示例字段 |
|---|---|---|
| `Driver` | 单个驱动 | `id`（DriverId）、`name`、`version`、`loaded`（bool）、`path` |
| `Runtime` | 单个运行时 | `name`、`version`、`path`、`env_var` |
| `Compiler` | 单个编译器 | `name`、`version`、`path`、`target` |
| `Library` | 单个库 | `name`、`version`、`path`、`kind`（BLAS/FFT/...） |
| `Cuda` | CUDA 栈 | `version`、`driver_version`、`nvcc_path`、`home` |
| `Rocm` | ROCm 栈 | `version`、`hip_path`、`rocm_path` |
| `LevelZero` | Level Zero 栈 | `version`、`loader_path` |
| `Mpi` | MPI 栈 | `implementation`（OpenMPI/MPICH/...）、`version`、`path` |
| `RdmaStack` | RDMA 栈 | `rdma_core_version`、`ibverbs_path`、`ucx_version` |

## 设计说明

* `SoftwareStack` 只描述"装了什么"，不描述"当前进程能看到什么"——
  可见性由 `ExecutionContext` 的可见性索引处理。
* 专用栈使用 `std::optional` 而非指针或向量，语义清晰：存在即有完整模型，
  不存在即 `std::nullopt`。
* 字段命名遵循 `snake_case`，类型名遵循 `PascalCase`。
