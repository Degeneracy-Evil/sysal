-- sysal 构建配置
-- C++20 / 自适应工具链（clang 或 gcc）
--
-- compile_commands.json 生成：utils/check.sh 自动调用
--   xmake project -k compile_commands build
-- 或手动执行上述命令。

set_languages("c++20")
set_rundir(".")
set_version("0.0.3")

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
    set_targetdir("$(builddir)/$(plat)/$(arch)/$(mode)/static")
    add_files(SYSAL_SOURCES)
    add_includedirs("src")

    on_load(function (target)
        if os.isdir(".githooks") and os.isdir(".git") then
            local configured = try { function() os.runv("git", {"config", "core.hooksPath"}); return true end }
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
