-- sysal 构建配置
-- C++23 / clang / libc++ / lld / compiler-rt

set_languages("c++23")
set_toolchains("clang")
set_rundir(".")

-- 全局编译/链接选项，所有 target 共享
add_cxxflags("-Wall", "-Wextra", "-Werror", "-stdlib=libc++", {force = true})
add_ldflags("-stdlib=libc++", "-fuse-ld=lld", "-rtlib=compiler-rt", "-unwindlib=libunwind",
            {force = true})
add_includedirs("include")

-- 辅助函数：创建测试 binary target
local function test_target(name, source)
    target(name)
        set_kind("binary")
        add_files(source)
        add_deps("sysal")
end

---------------------------------------- 主库

target("sysal")
    set_kind("static")
    add_files("src/**.cpp")
    add_includedirs("src")

    on_load(function (target)
        -- 首次构建时自动配置 git hooks
        if os.isdir(".githooks") and os.isdir(".git") then
            local configured = try { function() os.runv("git", {"config", "core.hooksPath"}); return true end }
            if not configured then
                os.runv("git", {"config", "core.hooksPath", ".githooks"})
            end
        end

        -- 通过 pkg-config 检测 hwloc，存在则启用 SYSAL_HAVE_HWLOC 条件编译
        local has_hwloc = try { function() os.runv("pkg-config", {"--exists", "hwloc"}); return true end }
        if has_hwloc then
            target:add("defines", "SYSAL_HAVE_HWLOC")
            target:add("links", "hwloc")
        end
    end)

    after_build(function (target)
        -- 自动生成 compile_commands.json 供 clang-tidy / clangd 使用
        local cc_file = path.join(os.projectdir(), "build", "compile_commands.json")
        if not os.isfile(cc_file) then
            local project_dir = os.projectdir()
            local entries = {}
            for _, sourcefile in ipairs(target:sourcefiles()) do
                local abs_source = path.absolute(sourcefile, project_dir)
                local cmd = "clang++ -std=c++23 -Wall -Wextra -Werror -stdlib=libc++ -Iinclude -Isrc -c " .. abs_source
                table.insert(entries, string.format(
                    '{"directory":"%s","command":"%s","file":"%s"}',
                    project_dir, cmd, abs_source
                ))
            end
            local json = "[" .. table.concat(entries, ",") .. "]"
            io.writefile(cc_file, json)
        end
    end)

---------------------------------------- 测试目标

test_target("test_collect", "tests/test_collect.cpp")
test_target("test_replay", "tests/test_replay.cpp")
test_target("testbench", "tests/testbench.cpp")
