# Warnings（警告信息）

记录采集过程中出现的问题。这些问题不一定导致整次采集失败，
因此以 `SystemSnapshot` 中的 `std::vector<std::string> warnings` 形式保留。

```cpp
struct SystemSnapshot
{
    // ...其余字段省略...
    std::vector<std::string> warnings;  // 采集过程中的警告信息
};
```

## 设计说明

在系统信息采集过程中，部分采集器失败或部分数据缺失是正常现象：
不同后端的可用性因环境而异，某一项无法获取不应导致整次采集终止。
因此 sysal 不使用复杂的诊断结构体，而是用简单的字符串列表记录原因，
既便于实现，也便于上层直接展示或记录日志。

当采集本身彻底失败（而非部分失败）时，`System` 的构造会抛出 `SysalError`，
不会进入 `warnings`。

## 示例

```txt
/proc/cpuinfo reports 192 logical CPUs, but cpuset exposes only 32.
NVML is unavailable, GPU collection skipped.
RDMA device found but no associated netdev was resolved.
```
