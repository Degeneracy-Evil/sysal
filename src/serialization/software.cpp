#include "serialization/software.hpp"
#include "serialization/json_values.hpp"

#include <limits>
#include <utility>

namespace sysal::detail
{
    using json = nlohmann::json;

    namespace
    {

        [[nodiscard]] json driver_to_json(const Driver &d)
        {
            return json{
                {"id", d.id.value()}, {"name", d.name}, {"version", d.version}, {"loaded", d.loaded}, {"path", d.path},
            };
        }

        [[nodiscard]] Driver driver_from_json(const json &j)
        {
            Driver d;
            d.id = DriverId(uint32_from_json(j.at("id"), "id"));
            j.at("name").get_to(d.name);
            j.at("version").get_to(d.version);
            d.loaded = j.at("loaded").get<bool>();
            j.at("path").get_to(d.path);
            return d;
        }

        [[nodiscard]] json runtime_to_json(const Runtime &r)
        {
            return json{
                {"name", r.name},
                {"version", r.version},
                {"path", r.path},
                {"env_var", r.env_var},
            };
        }

        [[nodiscard]] Runtime runtime_from_json(const json &j)
        {
            Runtime r;
            j.at("name").get_to(r.name);
            j.at("version").get_to(r.version);
            j.at("path").get_to(r.path);
            j.at("env_var").get_to(r.env_var);
            return r;
        }

        [[nodiscard]] json compiler_to_json(const Compiler &c)
        {
            return json{
                {"name", c.name},
                {"version", c.version},
                {"path", c.path},
                {"target", c.target},
            };
        }

        [[nodiscard]] Compiler compiler_from_json(const json &j)
        {
            Compiler c;
            j.at("name").get_to(c.name);
            j.at("version").get_to(c.version);
            j.at("path").get_to(c.path);
            j.at("target").get_to(c.target);
            return c;
        }

        [[nodiscard]] json library_to_json(const Library &l)
        {
            return json{
                {"name", l.name},
                {"version", l.version},
                {"path", l.path},
                {"kind", l.kind},
            };
        }

        [[nodiscard]] Library library_from_json(const json &j)
        {
            Library l;
            j.at("name").get_to(l.name);
            j.at("version").get_to(l.version);
            j.at("path").get_to(l.path);
            j.at("kind").get_to(l.kind);
            return l;
        }

        [[nodiscard]] json cuda_to_json(const Cuda &c)
        {
            return json{
                {"version", c.version},
                {"driver_version", c.driver_version},
                {"nvcc_path", c.nvcc_path},
                {"home", c.home},
            };
        }

        [[nodiscard]] Cuda cuda_from_json(const json &j)
        {
            Cuda c;
            j.at("version").get_to(c.version);
            j.at("driver_version").get_to(c.driver_version);
            j.at("nvcc_path").get_to(c.nvcc_path);
            j.at("home").get_to(c.home);
            return c;
        }

        [[nodiscard]] json rocm_to_json(const Rocm &r)
        {
            return json{
                {"version", r.version},
                {"hip_path", r.hip_path},
                {"rocm_path", r.rocm_path},
            };
        }

        [[nodiscard]] Rocm rocm_from_json(const json &j)
        {
            Rocm r;
            j.at("version").get_to(r.version);
            j.at("hip_path").get_to(r.hip_path);
            j.at("rocm_path").get_to(r.rocm_path);
            return r;
        }

        [[nodiscard]] json level_zero_to_json(const LevelZero &lz)
        {
            return json{
                {"version", lz.version},
                {"loader_path", lz.loader_path},
            };
        }

        [[nodiscard]] LevelZero level_zero_from_json(const json &j)
        {
            LevelZero lz;
            j.at("version").get_to(lz.version);
            j.at("loader_path").get_to(lz.loader_path);
            return lz;
        }

        [[nodiscard]] json mpi_to_json(const Mpi &m)
        {
            return json{
                {"implementation", m.implementation},
                {"version", m.version},
                {"path", m.path},
            };
        }

        [[nodiscard]] Mpi mpi_from_json(const json &j)
        {
            Mpi m;
            j.at("implementation").get_to(m.implementation);
            j.at("version").get_to(m.version);
            j.at("path").get_to(m.path);
            return m;
        }

        [[nodiscard]] json rdma_to_json(const RdmaStack &r)
        {
            return json{
                {"rdma_core_version", r.rdma_core_version},
                {"ibverbs_path", r.ibverbs_path},
                {"ucx_version", r.ucx_version},
            };
        }

        [[nodiscard]] RdmaStack rdma_from_json(const json &j)
        {
            RdmaStack r;
            j.at("rdma_core_version").get_to(r.rdma_core_version);
            j.at("ibverbs_path").get_to(r.ibverbs_path);
            j.at("ucx_version").get_to(r.ucx_version);
            return r;
        }

    } // namespace

    [[nodiscard]] json software_to_json(const SoftwareStack &sw)
    {
        json j;

        json drivers = json::array();
        for(const auto &d : sw.drivers)
        {
            drivers.push_back(driver_to_json(d));
        }
        j["drivers"] = std::move(drivers);

        json runtimes = json::array();
        for(const auto &r : sw.runtimes)
        {
            runtimes.push_back(runtime_to_json(r));
        }
        j["runtimes"] = std::move(runtimes);

        json compilers = json::array();
        for(const auto &c : sw.compilers)
        {
            compilers.push_back(compiler_to_json(c));
        }
        j["compilers"] = std::move(compilers);

        json libraries = json::array();
        for(const auto &l : sw.libraries)
        {
            libraries.push_back(library_to_json(l));
        }
        j["libraries"] = std::move(libraries);

        if(sw.cuda)
        {
            j["cuda"] = cuda_to_json(*sw.cuda);
        }
        if(sw.rocm)
        {
            j["rocm"] = rocm_to_json(*sw.rocm);
        }
        if(sw.level_zero)
        {
            j["level_zero"] = level_zero_to_json(*sw.level_zero);
        }
        if(sw.mpi)
        {
            j["mpi"] = mpi_to_json(*sw.mpi);
        }
        if(sw.rdma)
        {
            j["rdma"] = rdma_to_json(*sw.rdma);
        }

        return j;
    }

    [[nodiscard]] SoftwareStack software_from_json(const json &j)
    {
        SoftwareStack sw;

        for(const auto &elem : j.at("drivers"))
        {
            sw.drivers.push_back(driver_from_json(elem));
        }
        for(const auto &elem : j.at("runtimes"))
        {
            sw.runtimes.push_back(runtime_from_json(elem));
        }
        for(const auto &elem : j.at("compilers"))
        {
            sw.compilers.push_back(compiler_from_json(elem));
        }
        for(const auto &elem : j.at("libraries"))
        {
            sw.libraries.push_back(library_from_json(elem));
        }

        if(j.contains("cuda"))
        {
            sw.cuda = cuda_from_json(j.at("cuda"));
        }
        if(j.contains("rocm"))
        {
            sw.rocm = rocm_from_json(j.at("rocm"));
        }
        if(j.contains("level_zero"))
        {
            sw.level_zero = level_zero_from_json(j.at("level_zero"));
        }
        if(j.contains("mpi"))
        {
            sw.mpi = mpi_from_json(j.at("mpi"));
        }
        if(j.contains("rdma"))
        {
            sw.rdma = rdma_from_json(j.at("rdma"));
        }

        return sw;
    }

} // namespace sysal::detail
