#pragma once

#include "sysal/value_types.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace sysal
{

struct DriverInfo
{
    std::string name;
    std::string version;
    bool loaded{};
};

struct RuntimeInfo
{
    std::string name;
    std::string version;
    std::string path;
};

struct CompilerInfo
{
    std::string name;
    std::string version;
    std::string path;
};

struct LibraryInfo
{
    std::string name;
    std::string version;
    std::string path;
};

struct CudaInfo
{
    std::string driver_version;
    std::string runtime_version;
    std::uint32_t device_count{};
};

struct RocmInfo
{
    std::string version;
};

struct LevelZeroInfo
{
    std::string version;
};

struct MpiInfo
{
    std::string implementation;
    std::string version;
};

struct RdmaStackInfo
{
    bool ibverbs_available{};
    std::vector<std::string> rdma_devices;
};

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

} // namespace sysal
