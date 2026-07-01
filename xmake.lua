-- sysal 构建配置
-- C++23 / clang / libc++ / lld / compiler-rt
--
-- compile_commands.json 生成：utils/check.sh 自动调用
--   xmake project -k compile_commands build
-- 或手动执行上述命令。

set_languages("c++23")
set_toolchains("clang")
set_rundir(".")
set_version("0.0.1")

-- 全局编译/链接选项，所有 target 共享
add_cxxflags("-Wall", "-Wextra", "-Werror", "-stdlib=libc++", {force = true})
add_ldflags("-stdlib=libc++", "-fuse-ld=lld", "-rtlib=compiler-rt", "-unwindlib=libunwind",
            {force = true})
add_includedirs("include")

-- 源文件列表（共享给静态库和动态库）
local SYSAL_SOURCES = {
    "src/**.cpp"
}

-- ========== 库 ==========

target("sysal_static")
    set_kind("static")
    set_basename("sysal")
    -- 静态库放到 static/ 子目录，避免链接器在同目录下优先选择 .so
    set_targetdir("$(builddir)/$(plat)/$(arch)/$(mode)/static")
    add_files(SYSAL_SOURCES)
    add_includedirs("src")

    on_load(function (target)
        -- 项目级一次性设置：首次构建时自动配置 git hooks
        -- 挂在 sysal_static 上，因为 xmake 默认构建所有 target，此 target 必被加载
        if os.isdir(".githooks") and os.isdir(".git") then
            local configured = try { function() os.runv("git", {"config", "core.hooksPath"}); return true end }
            if not configured then
                os.runv("git", {"config", "core.hooksPath", ".githooks"})
            end
        end
    end)

target("sysal_shared")
    set_kind("shared")
    set_basename("sysal")
    add_files(SYSAL_SOURCES)
    add_includedirs("src")

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
        add_cxxflags("-UNDEBUG", {force = true})
end

-- 单元测试（链接静态库，白盒可访问内部头）
local unit_tests = {
    "test_types",
    "test_model",
    "test_json",
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
    test_target(name, "tests/" .. name .. ".cpp")
end

-- testbench 链接动态库，仅访问公共头
test_target("testbench", "tests/testbench.cpp", true)

-- ========== task ==========

task("testbench")
    set_category("plugin")
    on_run(function ()
        os.execv("xmake", {"run", "testbench"})
    end)
    set_menu {
        usage = "xmake testbench",
        description = "Build and run testbench with terminal output",
        options = {}
    }
