set_project("sysal")
on_load(function (target)
    local version_header = io.readfile("include/sysal/version.hpp")
    target:set("version", assert(version_header:match('VERSION_STRING%s*=%s*"([^"]+)"'), "missing sysal version"))
end)
set_languages("c++20")
set_rundir(".")
if not get_config("toolchain") then
    set_toolchains("clang")
end
add_rules("mode.debug", "mode.release")
add_rules("plugin.compile_commands.autoupdate", {outputdir = "build"})
add_cxxflags("-Wall", "-Wextra", "-Werror", "-Wpedantic", {force = true})
add_includedirs("include")
add_requires("nlohmann_json", "doctest 2.4.12")

local function cpp_files()
    local files = {}
    for _, directory in ipairs({"include", "src", "tests", "examples"}) do
        for _, extension in ipairs({"h", "hpp", "c", "cpp"}) do
            for _, file in ipairs(os.files(directory .. "/**." .. extension)) do
                table.insert(files, file)
            end
        end
    end
    table.sort(files)
    return files
end

local function translation_units()
    local files = {}
    for _, directory in ipairs({"src", "tests", "examples"}) do
        for _, extension in ipairs({"c", "cpp"}) do
            for _, file in ipairs(os.files(directory .. "/**." .. extension)) do
                table.insert(files, file)
            end
        end
    end
    table.sort(files)
    return files
end

local function enable_assertions(target)
    -- mode.release adds this raw flag after undefines; keep smoke/test assertions active.
    local flags = {}
    for _, flag in ipairs(target:get("cxflags") or {}) do
        if flag ~= "-DNDEBUG" then
            table.insert(flags, flag)
        end
    end
    target:set("cxflags", flags)
end

target("sysal_static")
    set_kind("static")
    set_basename("sysal")
    add_cxxflags("-fPIC", {force = true})
    set_targetdir("$(builddir)/$(plat)/$(arch)/$(mode)/static")
    add_files("src/**.cpp")
    add_includedirs("src")
    add_packages("nlohmann_json")
    add_syslinks("dl")

target("sysal_shared")
    set_kind("shared")
    set_basename("sysal")
    add_files("src/**.cpp")
    add_includedirs("src")
    add_packages("nlohmann_json")
    add_syslinks("dl")

target("unit_tests")
    set_kind("binary")
    set_default(false)
    add_files("tests/test_main.cpp", "tests/unit/**.cpp")
    add_deps("sysal_static")
    add_includedirs("src", "tests")
    add_packages("doctest")
    add_undefines("NDEBUG")
    on_config(enable_assertions)

target("test_replay")
    set_kind("binary")
    set_default(false)
    add_files("tests/test_main.cpp", "tests/integration/test_replay.cpp")
    add_deps("sysal_static")
    add_packages("doctest")
    add_undefines("NDEBUG")
    on_config(enable_assertions)

target("sysal_info")
    set_kind("binary")
    add_files("examples/sysal_info.cpp")
    add_deps("sysal_shared")
    add_undefines("NDEBUG")
    on_config(enable_assertions)

task("test")
    set_category("plugin")
    on_run(function ()
        os.execv("xmake", {"run", "unit_tests"})
        os.execv("xmake", {"run", "test_replay"})
    end)
    set_menu {
        usage = "xmake test",
        description = "Run doctest unit and replay integration tests",
        options = {}
    }

task("format")
    set_category("plugin")
    on_run(function ()
        for _, file in ipairs(cpp_files()) do
            os.execv("clang-format", {"-i", file})
        end
    end)
    set_menu {
        usage = "xmake format",
        description = "Format project C/C++ files in place",
        options = {}
    }

task("check")
    set_category("plugin")
    on_run(function ()
        print("[1/5] clang-format check...")
        for _, file in ipairs(cpp_files()) do
            os.execv("clang-format", {"--dry-run", "--Werror", file})
        end
        print("[2/5] compilation database...")
        -- Include non-default test targets in the database used by clang-tidy.
        os.execv("xmake", {"project", "-k", "compile_commands", "build"})
        print("[3/5] clang-tidy...")
        for _, file in ipairs(translation_units()) do
            os.execv("clang-tidy", {"-p=build", file})
        end
        print("[4/5] rebuild...")
        os.execv("xmake", {"-r"})
        print("[5/5] test...")
        os.execv("xmake", {"test"})
        print("\nAll checks passed.")
    end)
    set_menu {
        usage = "xmake check",
        description = "Validate format, tidy, rebuild, and tests without modifying tracked files",
        options = {}
    }

task("sysal_info")
    set_category("plugin")
    on_run(function ()
        os.execv("xmake", {"run", "sysal_info"})
    end)
    set_menu {
        usage = "xmake sysal_info",
        description = "Build and run sysal_info",
        options = {}
    }
