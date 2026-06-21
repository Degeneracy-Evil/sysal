#pragma once

#include "sysal/enums.hpp"

#include <optional>
#include <string>
#include <vector>

namespace sysal
{

struct ConflictDetail
{
    std::string field;
    std::string value_from_higher;
    std::string value_from_lower;
    RawSource higher_source;
    RawSource lower_source;
};

struct Diagnostic
{
    Severity severity;
    std::string message;
    std::optional<RawSource> source;
    std::optional<ConflictDetail> conflict;
};

struct Diagnostics
{
    std::vector<Diagnostic> records;
};

} // namespace sysal
