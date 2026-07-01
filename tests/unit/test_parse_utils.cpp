#include "parser/parse_utils.hpp"

#include <cassert>
#include <string>

using namespace sysal::detail;

int main()
{
    // trim
    assert(trim("  hello  ") == "hello");
    assert(trim("\t\nworld\r\n") == "world");
    assert(trim("") == "");
    assert(trim("   ") == "");

    // split
    auto parts = split("a,b,c", ',');
    assert(parts.size() == 3);
    assert(parts[0] == "a");
    assert(parts[1] == "b");
    assert(parts[2] == "c");

    auto single = split("hello", ',');
    assert(single.size() == 1);
    assert(single[0] == "hello");

    auto empty = split("", ',');
    assert(empty.size() == 1);

    // parse_kv
    auto [k1, v1] = parse_kv("key : value");
    assert(k1 == "key");
    assert(v1 == "value");

    auto [k2, v2] = parse_kv("noseparator");
    assert(k2 == "noseparator");
    assert(v2.empty());

    auto [k3, v3] = parse_kv("model name : Intel(R) Xeon(R)");
    assert(k3 == "model name");
    assert(v3 == "Intel(R) Xeon(R)");

    // parse_uint
    assert(parse_uint("42").has_value() && *parse_uint("42") == 42);
    assert(parse_uint("  123  ").has_value() && *parse_uint("  123  ") == 123);
    assert(!parse_uint("abc").has_value());
    assert(!parse_uint("").has_value());

    // parse_uint 拒绝部分消费
    assert(!parse_uint("123abc").has_value());
    assert(!parse_uint("12 34").has_value());
    assert(!parse_uint("0x10").has_value());

    // parse_hex
    assert(parse_hex("0a").has_value() && *parse_hex("0a") == 10);
    assert(parse_hex("ff").has_value() && *parse_hex("ff") == 255);
    assert(parse_hex("41").has_value() && *parse_hex("41") == 65);
    assert(!parse_hex("xyz").has_value());

    // parse_hex 拒绝部分消费
    assert(!parse_hex("ffxyz").has_value());
    assert(!parse_hex("fg").has_value());
    assert(!parse_hex("10ab cd").has_value());

    // parse_pci_address (十六进制，修复 B-1 bug)
    auto addr1 = parse_pci_address("0000:41:00.0");
    assert(addr1.has_value());
    assert(addr1->domain == 0);
    assert(addr1->bus == 0x41);
    assert(addr1->device == 0);
    assert(addr1->function == 0);

    auto addr2 = parse_pci_address("0000:0a:1f.2");
    assert(addr2.has_value());
    assert(addr2->domain == 0);
    assert(addr2->bus == 0x0a);
    assert(addr2->device == 0x1f);
    assert(addr2->function == 2);

    assert(!parse_pci_address("invalid").has_value());
    assert(!parse_pci_address("").has_value());

    // parse_kb_to_bytes
    auto kb1 = parse_kb_to_bytes("1024");
    assert(kb1.has_value());
    assert(kb1->value == 1024ULL * 1024ULL);

    assert(!parse_kb_to_bytes("abc").has_value());

    return 0;
}
