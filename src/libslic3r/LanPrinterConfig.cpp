#include "LanPrinterConfig.hpp"
#include "I18N.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <initializer_list>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

namespace Slic3r {
namespace {

bool is_ascii_alphanumeric(const std::string& value)
{
    return std::all_of(value.begin(), value.end(), [](unsigned char c) {
        return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
    });
}

bool is_lan_ipv4(const std::string& value)
{
    std::array<unsigned int, 4> octets{};
    std::size_t start = 0;
    for (std::size_t i = 0; i < octets.size(); ++i) {
        const auto end = value.find('.', start);
        if ((i == 3) != (end == std::string::npos))
            return false;
        const auto length = (end == std::string::npos ? value.size() : end) - start;
        if (length == 0 || length > 3 || (length > 1 && value[start] == '0'))
            return false;
        for (std::size_t j = start; j < start + length; ++j) {
            if (value[j] < '0' || value[j] > '9')
                return false;
            octets[i] = octets[i] * 10 + static_cast<unsigned int>(value[j] - '0');
        }
        if (octets[i] > 255)
            return false;
        start += length + 1;
    }
    return octets[0] == 10 ||
           (octets[0] == 172 && octets[1] >= 16 && octets[1] <= 31) ||
           (octets[0] == 192 && octets[1] == 168) ||
           (octets[0] == 169 && octets[1] == 254);
}

bool has_only_keys(const nlohmann::json& object, std::initializer_list<const char*> keys)
{
    if (!object.is_object())
        return false;
    for (auto it = object.begin(); it != object.end(); ++it) {
        if (std::none_of(keys.begin(), keys.end(), [&](const char* key) { return it.key() == key; }))
            return false;
    }
    return true;
}

std::string required_string(const nlohmann::json& object, const char* key)
{
    const auto it = object.find(key);
    if (it == object.end() || !it->is_string() || it->get_ref<const std::string&>().empty())
        throw std::runtime_error(_u8L("Every LAN printer needs non-empty name, ip, serial, model and access_code strings."));
    return it->get<std::string>();
}

} // namespace

std::vector<LanPrinterConfig> parse_lan_printer_config(const std::string& contents)
{
    if (contents.size() > MAX_LAN_PRINTER_CONFIG_BYTES)
        throw std::runtime_error(_u8L("The LAN printer configuration exceeds the 1 MiB size limit."));

    nlohmann::json document;
    try {
        // Bound nesting before constructing a deeply nested document, and reject
        // duplicate object keys instead of silently choosing the last credential.
        std::vector<std::set<std::string>> object_keys;
        document = nlohmann::json::parse(contents, [&](int depth, nlohmann::json::parse_event_t event, nlohmann::json& value) {
            if (depth > 8)
                throw std::runtime_error("invalid JSON");
            if (event == nlohmann::json::parse_event_t::object_start)
                object_keys.emplace_back();
            else if (event == nlohmann::json::parse_event_t::object_end)
                object_keys.pop_back();
            else if (event == nlohmann::json::parse_event_t::key &&
                     !object_keys.back().insert(value.get<std::string>()).second)
                throw std::runtime_error("invalid JSON");
            return true;
        });
    } catch (const nlohmann::json::exception&) {
        throw std::runtime_error(_u8L("The LAN printer configuration is not valid JSON."));
    } catch (const std::runtime_error&) {
        throw std::runtime_error(_u8L("The LAN printer configuration has duplicate keys or excessive nesting."));
    }

    if (!has_only_keys(document, {"version", "printers"}))
        throw std::runtime_error(_u8L("The LAN printer configuration must be an object containing only version and printers."));
    const auto version = document.find("version");
    if (version == document.end() || !version->is_number_integer() || *version != 1)
        throw std::runtime_error(_u8L("The LAN printer configuration must use version 1."));
    const auto printers = document.find("printers");
    if (printers == document.end() || !printers->is_array() || printers->empty() || printers->size() > 128)
        throw std::runtime_error(_u8L("The LAN printer configuration must contain between 1 and 128 printers."));

    std::vector<LanPrinterConfig> result;
    std::set<std::string> serials;
    std::set<std::string> addresses;
    std::set<std::string> names;
    for (const auto& item : *printers) {
        if (!has_only_keys(item, {"name", "ip", "serial", "model", "access_code"}))
            throw std::runtime_error(_u8L("A LAN printer contains unknown fields. Use name, ip, serial, model and access_code."));
        LanPrinterConfig printer;
        printer.name        = required_string(item, "name");
        printer.ip          = required_string(item, "ip");
        printer.serial      = required_string(item, "serial");
        printer.access_code = required_string(item, "access_code");
        if (required_string(item, "model") != "P1S")
            throw std::runtime_error(_u8L("This LAN printer importer supports model P1S only."));
        if (printer.name.size() > 128 ||
            std::all_of(printer.name.begin(), printer.name.end(), [](char c) { return c == ' '; }) ||
            std::any_of(printer.name.begin(), printer.name.end(), [](unsigned char c) { return c < 32 || c == 127; }))
            throw std::runtime_error(_u8L("Printer names must contain visible text, no control characters, and at most 128 UTF-8 bytes."));
        if (!is_lan_ipv4(printer.ip))
            throw std::runtime_error(_u8L("Each printer needs a private IPv4 address in 10.x.x.x, 172.16-31.x.x, 192.168.x.x or 169.254.x.x."));
        if (printer.serial.size() < 10 || printer.serial.size() > 32 || !is_ascii_alphanumeric(printer.serial))
            throw std::runtime_error(_u8L("Printer serial numbers must contain 10 to 32 letters and digits."));
        if (printer.access_code.size() != 8 || !is_ascii_alphanumeric(printer.access_code))
            throw std::runtime_error(_u8L("LAN access codes must contain exactly 8 letters and digits."));
        // Distinct initial names make a fleet easier to identify in the device UI.
        if (!serials.insert(printer.serial).second || !addresses.insert(printer.ip).second ||
            !names.insert(printer.name).second)
            throw std::runtime_error(_u8L("Each printer must have a unique name, serial number and IP address in the configuration."));
        result.emplace_back(std::move(printer));
    }
    return result;
}

} // namespace Slic3r
