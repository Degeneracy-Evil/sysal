#include "parser/parse_utils.hpp"

#include "test_macros.hpp"
#include <string>

using namespace sysal::detail;

int main()
{
    // trim
    CHECK(trim("  hello  ") == "hello");
    CHECK(trim("\t\nworld\r\n") == "world");
    CHECK(trim("") == "");
    CHECK(trim("   ") == "");

    // split
    auto parts = split("a,b,c", ',');
    CHECK(parts.size() == 3);
    CHECK(parts[0] == "a");
    CHECK(parts[1] == "b");
    CHECK(parts[2] == "c");

    auto single = split("hello", ',');
    CHECK(single.size() == 1);
    CHECK(single[0] == "hello");

    auto empty = split("", ',');
    CHECK(empty.size() == 1);

    // parse_kv
    auto [k1, v1] = parse_kv("key : value");
    CHECK(k1 == "key");
    CHECK(v1 == "value");

    auto [k2, v2] = parse_kv("noseparator");
    CHECK(k2 == "noseparator");
    CHECK(v2.empty());

    auto [k3, v3] = parse_kv("model name : Intel(R) Xeon(R)");
    CHECK(k3 == "model name");
    CHECK(v3 == "Intel(R) Xeon(R)");

    // parse_uint
    CHECK(parse_uint("42").has_value() && *parse_uint("42") == 42);
    CHECK(parse_uint("  123  ").has_value() && *parse_uint("  123  ") == 123);
    CHECK(!parse_uint("abc").has_value());
    CHECK(!parse_uint("").has_value());

    // parse_uint 拒绝部分消费
    CHECK(!parse_uint("123abc").has_value());
    CHECK(!parse_uint("12 34").has_value());
    CHECK(!parse_uint("0x10").has_value());

    // parse_hex
    CHECK(parse_hex("0a").has_value() && *parse_hex("0a") == 10);
    CHECK(parse_hex("ff").has_value() && *parse_hex("ff") == 255);
    CHECK(parse_hex("41").has_value() && *parse_hex("41") == 65);
    CHECK(!parse_hex("xyz").has_value());

    // parse_hex 拒绝部分消费
    CHECK(!parse_hex("ffxyz").has_value());
    CHECK(!parse_hex("fg").has_value());
    CHECK(!parse_hex("10ab cd").has_value());

    // parse_pci_address (十六进制，修复 B-1 bug)
    auto addr1 = parse_pci_address("0000:41:00.0");
    CHECK(addr1.has_value());
    CHECK(addr1->domain == 0);
    CHECK(addr1->bus == 0x41);
    CHECK(addr1->device == 0);
    CHECK(addr1->function == 0);

    auto addr2 = parse_pci_address("0000:0a:1f.2");
    CHECK(addr2.has_value());
    CHECK(addr2->domain == 0);
    CHECK(addr2->bus == 0x0a);
    CHECK(addr2->device == 0x1f);
    CHECK(addr2->function == 2);

    CHECK(!parse_pci_address("invalid").has_value());
    CHECK(!parse_pci_address("").has_value());

    // parse_kb_to_bytes
    auto kb1 = parse_kb_to_bytes("1024");
    CHECK(kb1.has_value());
    CHECK(kb1->value == 1024ULL * 1024ULL);

    CHECK(!parse_kb_to_bytes("abc").has_value());

    TEST_SUMMARY();
}
