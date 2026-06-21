#include "sysal/test/replay.hpp"

#include "detail/json.hpp"
#include "detail/pipeline.hpp"

#include <chrono>
#include <fstream>
#include <sstream>
#include <utility>

namespace sysal::test
{

Expected<void, SysalError> save_raw_store(const RawStore& raw, const std::string& path)
{
    std::ofstream out(path, std::ios::binary);
    if(!out.is_open())
    {
        return make_unexpected(
            SysalError(ErrorKind::IoError, "cannot open file for writing: " + path));
    }
    const std::string json = detail::raw_store_to_json(raw, true);
    out.write(json.data(), static_cast<std::streamsize>(json.size()));
    if(!out)
    {
        return make_unexpected(SysalError(ErrorKind::IoError, "write failed: " + path));
    }
    return {};
}

Expected<RawStore, SysalError> load_raw_store(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    if(!in.is_open())
    {
        return make_unexpected(SysalError(ErrorKind::FileNotFound, "cannot open file: " + path));
    }
    std::stringstream ss;
    ss << in.rdbuf();
    const std::string content = ss.str();

    auto parsed = detail::parse_json(content);
    if(!parsed)
    {
        return make_unexpected(parsed.error());
    }
    return detail::raw_store_from_json(*parsed);
}

Expected<SystemSnapshot, SysalError> collect_from_raw(const RawStore& raw, const CollectSpec& spec)
{
    const auto start_time = std::chrono::system_clock::now();
    return detail::run_pipeline(raw, spec, start_time);
}

} // namespace sysal::test
