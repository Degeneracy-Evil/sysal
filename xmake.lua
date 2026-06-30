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
        add_includedirs("src")
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

test_target("test_types", "tests/test_types.cpp")
test_target("test_model", "tests/test_model.cpp")
test_target("test_json", "tests/test_json.cpp")
test_target("test_parse_utils", "tests/test_parse_utils.cpp")
test_target("test_raw_store_io", "tests/test_raw_store_io.cpp")
test_target("test_reader", "tests/test_reader.cpp")
test_target("test_parse_platform", "tests/test_parse_platform.cpp")
test_target("test_parse_cpu", "tests/test_parse_cpu.cpp")
test_target("test_parse_memory", "tests/test_parse_memory.cpp")
test_target("test_parse_accelerator", "tests/test_parse_accelerator.cpp")
test_target("test_parse_storage", "tests/test_parse_storage.cpp")
test_target("test_parse_pci", "tests/test_parse_pci.cpp")
test_target("test_parse_network", "tests/test_parse_network.cpp")
test_target("test_parse_software", "tests/test_parse_software.cpp")
test_target("test_parse_execution", "tests/test_parse_execution.cpp")
test_target("test_resolve", "tests/test_resolve.cpp")
test_target("test_collect", "tests/test_collect.cpp")
test_target("test_serialization", "tests/test_serialization.cpp")
test_target("test_replay", "tests/test_replay.cpp")
test_target("testbench", "tests/testbench.cpp")
