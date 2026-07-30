set_languages("c++20")
set_rundir(".")
add_rules("plugin.compile_commands.autoupdate", {outputdir = "build"})

add_cxxflags("-Wall", "-Wextra", "-Werror", "-O2", {force = true})
add_includedirs("include")
add_requires("nlohmann_json")
add_packages("nlohmann_json")

-- clang 专属选项：检测到 clang 编译器时添加 libc++/lld/compiler-rt
local function clang_flags(target)
    local comp = target:tool("cxx")
    if comp and comp:find("clang", 1, true) then
        target:add("cxxflags", "-stdlib=libc++", {force = true})
        target:add("ldflags", "-stdlib=libc++", "-fuse-ld=lld", "-rtlib=compiler-rt",
                   "-unwindlib=libunwind", {force = true})
    end
end

-- 源文件列表（共享给静态库和动态库）
local SYSAL_SOURCES = {
    "src/**.cpp"
}

-- ========== 库 ==========

target("sysal_static")
    set_kind("static")
    set_basename("sysal")
    add_cxxflags("-fPIC", {force = true})
    set_targetdir("$(builddir)/$(plat)/$(arch)/$(mode)/static")
    add_files(SYSAL_SOURCES)
    add_includedirs("src")

    on_load(function (target)
        if os.isdir(".githooks") and (os.isdir(".git") or os.isfile(".git")) then
            local configured = try { function() os.execv("git", {"config", "core.hooksPath"}); return true end }
            if not configured then
                os.runv("git", {"config", "core.hooksPath", ".githooks"})
            end
        end
    end)
    on_config(function (target)
        clang_flags(target)
    end)

target("sysal_shared")
    set_kind("shared")
    set_basename("sysal")
    add_files(SYSAL_SOURCES)
    add_includedirs("src")
    on_config(function (target)
        clang_flags(target)
    end)

-- ========== 测试 ==========

-- 辅助函数：创建测试 binary target
-- link_shared=true 链接动态库（不暴露内部头），否则链接静态库（可访问 src/ 内部头）
local function test_target(name, source, link_shared)
    target(name)
        set_kind("binary")
        add_files(source)
        add_deps(link_shared and "sysal_shared" or "sysal_static")
        if not link_shared then
            add_includedirs("src")
        end
        add_includedirs("tests")
        add_cxxflags("-UNDEBUG", {force = true})
        on_config(function (target)
            clang_flags(target)
        end)
end

-- 单元测试（链接静态库，白盒可访问内部头）
local unit_tests = {
    "test_types",
    "test_model",
    "test_parse_utils",
    "test_raw_store_io",
    "test_reader",
    "test_parse_platform",
    "test_parse_cpu",
    "test_parse_memory",
    "test_parse_accelerator",
    "test_parse_storage",
    "test_parse_pci",
    "test_parse_network",
    "test_parse_software",
    "test_parse_execution",
    "test_resolve",
    "test_collect",
    "test_serialization",
    "test_replay",
}

for _, name in ipairs(unit_tests) do
    if name == "test_replay" then
        test_target(name, "tests/integration/" .. name .. ".cpp")
    else
        test_target(name, "tests/unit/" .. name .. ".cpp")
    end
end

-- sysal_info 链接动态库，仅访问公共头（demo，非测试）
target("sysal_info")
    set_kind("binary")
    add_files("examples/sysal_info.cpp")
    add_deps("sysal_shared")
    add_cxxflags("-UNDEBUG", {force = true})
    on_config(function (target)
        clang_flags(target)
    end)

-- ========== task ==========

task("test")
    set_category("plugin")
    on_run(function ()
        local targets = {
            "test_types", "test_model", "test_parse_utils", "test_raw_store_io",
            "test_reader", "test_parse_platform", "test_parse_cpu", "test_parse_memory",
            "test_parse_accelerator", "test_parse_storage", "test_parse_pci",
            "test_parse_network", "test_parse_software", "test_parse_execution",
            "test_resolve", "test_collect", "test_serialization", "test_replay",
        }
        local failed = 0
        for _, name in ipairs(targets) do
            if not os.execv("xmake", {"run", name}) then
                failed = failed + 1
            end
        end
        if failed > 0 then
            raise(failed .. " test(s) failed")
        end
    end)
    set_menu {
        usage = "xmake test",
        description = "Run all unit and integration tests",
        options = {}
    }

task("check")
    set_category("plugin")
    on_run(function ()
        local fmt_cmd = "find include src tests -type f \\( -name '*.h' -o -name '*.hpp' -o -name '*.c' -o -name '*.cpp' \\) -print0 2>/dev/null | xargs -0 clang-format -i"
        local tidy_cmd = "find src tests -type f \\( -name '*.c' -o -name '*.cpp' \\) -print0 2>/dev/null | xargs -0 clang-tidy -p=build"

        print("[1/4] clang-format...")
        os.execv("bash", {"-c", fmt_cmd})

        print("[2/4] clang-tidy...")
        os.execv("xmake", {"project", "-k", "compile_commands", "build"})
        os.execv("bash", {"-c", tidy_cmd})

        print("[3/4] rebuild...")
        os.execv("xmake", {"-r"})

        print("[4/4] test...")
        os.execv("xmake", {"test"})

        print("\nAll checks passed.")
    end)
    set_menu {
        usage = "xmake check",
        description = "Full quality check: format + tidy + rebuild + test",
        options = {}
    }

task("sysal_info")
    set_category("plugin")
    on_run(function ()
        os.execv("xmake", {"run", "sysal_info"})
    end)
    set_menu {
        usage = "xmake sysal_info",
        description = "Build and run sysal_info demo with terminal output",
        options = {}
    }
