#include "serialization/platform.hpp"
#include "serialization/json_values.hpp"

#include <limits>
#include <utility>

namespace sysal::detail
{
    using json = nlohmann::json;

    namespace
    {

        [[nodiscard]] json host_to_json(const Host &h)
        {
            json j{
                {"hostname", h.hostname},   {"machine_id", h.machine_id}, {"product_name", h.product_name},
                {"vendor", h.vendor.value}, {"serial", h.serial},
            };
            if(!h.product_family.empty())
            {
                j["product_family"] = h.product_family;
            }
            if(!h.product_version.empty())
            {
                j["product_version"] = h.product_version;
            }
            if(!h.product_sku.empty())
            {
                j["product_sku"] = h.product_sku;
            }
            if(!h.product_uuid.empty())
            {
                j["product_uuid"] = h.product_uuid;
            }
            return j;
        }

        [[nodiscard]] Host host_from_json(const json &j)
        {
            Host h;
            j.at("hostname").get_to(h.hostname);
            j.at("machine_id").get_to(h.machine_id);
            j.at("product_name").get_to(h.product_name);
            j.at("vendor").get_to(h.vendor.value);
            j.at("serial").get_to(h.serial);
            h.product_family = j.value("product_family", std::string{});
            h.product_version = j.value("product_version", std::string{});
            h.product_sku = j.value("product_sku", std::string{});
            h.product_uuid = j.value("product_uuid", std::string{});
            return h;
        }

        [[nodiscard]] json os_to_json(const Os &o)
        {
            return json{
                {"name", o.name},
                {"version", o.version},
                {"distribution", o.distribution},
                {"distribution_version", o.distribution_version},
                {"codename", o.codename},
            };
        }

        [[nodiscard]] Os os_from_json(const json &j)
        {
            Os o;
            j.at("name").get_to(o.name);
            j.at("version").get_to(o.version);
            j.at("distribution").get_to(o.distribution);
            j.at("distribution_version").get_to(o.distribution_version);
            j.at("codename").get_to(o.codename);
            return o;
        }

        [[nodiscard]] json kernel_to_json(const Kernel &k)
        {
            return json{
                {"release", k.release},
                {"version", k.version},
                {"compiled_at", k.compiled_at},
                {"architecture", k.architecture},
            };
        }

        [[nodiscard]] Kernel kernel_from_json(const json &j)
        {
            Kernel k;
            j.at("release").get_to(k.release);
            j.at("version").get_to(k.version);
            j.at("compiled_at").get_to(k.compiled_at);
            j.at("architecture").get_to(k.architecture);
            return k;
        }

        [[nodiscard]] json arch_to_json(const Architecture &a)
        {
            return json{
                {"name", a.name},
                {"bits", a.bits},
                {"byte_order", a.byte_order},
            };
        }

        [[nodiscard]] Architecture arch_from_json(const json &j)
        {
            Architecture a;
            j.at("name").get_to(a.name);
            a.bits = uint32_from_json(j.at("bits"), "bits");
            j.at("byte_order").get_to(a.byte_order);
            return a;
        }

        [[nodiscard]] json firmware_to_json(const Firmware &f)
        {
            json j{
                {"bios_vendor", f.bios_vendor.value},
                {"bios_version", f.bios_version},
                {"bios_date", f.bios_date},
                {"uefi", f.uefi},
            };
            if(!f.bios_release.empty())
            {
                j["bios_release"] = f.bios_release;
            }
            if(!f.ec_firmware_release.empty())
            {
                j["ec_firmware_release"] = f.ec_firmware_release;
            }
            return j;
        }

        [[nodiscard]] Firmware firmware_from_json(const json &j)
        {
            Firmware f;
            j.at("bios_vendor").get_to(f.bios_vendor.value);
            j.at("bios_version").get_to(f.bios_version);
            j.at("bios_date").get_to(f.bios_date);
            f.uefi = j.at("uefi").get<bool>();
            f.bios_release = j.value("bios_release", std::string{});
            f.ec_firmware_release = j.value("ec_firmware_release", std::string{});
            return f;
        }

        [[nodiscard]] json virt_to_json(const Virtualization &v)
        {
            return json{
                {"kind", static_cast<std::uint32_t>(v.kind)},
                {"hypervisor", v.hypervisor},
            };
        }

        [[nodiscard]] Virtualization virt_from_json(const json &j)
        {
            Virtualization v;
            v.kind = validate_enum(uint32_from_json(j.at("kind"), "kind"), VirtualizationKind::Other, "kind");
            j.at("hypervisor").get_to(v.hypervisor);
            return v;
        }

        [[nodiscard]] json baseboard_to_json(const Baseboard &b)
        {
            return json{{"vendor", b.vendor.value},
                        {"name", b.name},
                        {"version", b.version},
                        {"serial", b.serial},
                        {"asset_tag", b.asset_tag}};
        }

        [[nodiscard]] Baseboard baseboard_from_json(const json &j)
        {
            Baseboard b;
            b.vendor = Vendor{j.value("vendor", std::string{})};
            b.name = j.value("name", std::string{});
            b.version = j.value("version", std::string{});
            b.serial = j.value("serial", std::string{});
            b.asset_tag = j.value("asset_tag", std::string{});
            return b;
        }

        [[nodiscard]] json chassis_to_json(const Chassis &c)
        {
            json j{
                {"vendor", c.vendor.value}, {"version", c.version}, {"serial", c.serial}, {"asset_tag", c.asset_tag}};
            if(c.type)
            {
                j["type"] = *c.type;
            }
            return j;
        }

        [[nodiscard]] Chassis chassis_from_json(const json &j)
        {
            Chassis c;
            c.vendor = Vendor{j.value("vendor", std::string{})};
            c.version = j.value("version", std::string{});
            c.serial = j.value("serial", std::string{});
            c.asset_tag = j.value("asset_tag", std::string{});
            if(j.contains("type"))
            {
                c.type = uint32_from_json(j.at("type"), "type");
            }
            return c;
        }

    } // namespace

    [[nodiscard]] json platform_to_json(const Platform &p)
    {
        json j = {
            {"host", host_to_json(p.host)},
            {"os", os_to_json(p.os)},
            {"kernel", kernel_to_json(p.kernel)},
            {"architecture", arch_to_json(p.architecture)},
        };
        if(p.firmware)
        {
            j["firmware"] = firmware_to_json(*p.firmware);
        }
        if(p.virtualization)
        {
            j["virtualization"] = virt_to_json(*p.virtualization);
        }
        if(p.baseboard)
        {
            j["baseboard"] = baseboard_to_json(*p.baseboard);
        }
        if(p.chassis)
        {
            j["chassis"] = chassis_to_json(*p.chassis);
        }
        return j;
    }

    [[nodiscard]] Platform platform_from_json(const json &j)
    {
        Platform p;
        p.host = host_from_json(j.at("host"));
        p.os = os_from_json(j.at("os"));
        p.kernel = kernel_from_json(j.at("kernel"));
        p.architecture = arch_from_json(j.at("architecture"));
        if(j.contains("firmware"))
        {
            p.firmware = firmware_from_json(j.at("firmware"));
        }
        if(j.contains("virtualization"))
        {
            p.virtualization = virt_from_json(j.at("virtualization"));
        }
        if(j.contains("baseboard"))
        {
            p.baseboard = baseboard_from_json(j.at("baseboard"));
        }
        if(j.contains("chassis"))
        {
            p.chassis = chassis_from_json(j.at("chassis"));
        }
        return p;
    }

} // namespace sysal::detail
