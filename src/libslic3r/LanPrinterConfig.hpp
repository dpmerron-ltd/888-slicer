#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace Slic3r {

// Portable connection details, independent of account credentials and slicing presets.
struct LanPrinterConfig
{
    std::string name;
    std::string ip;
    std::string serial;
    std::string access_code;
};

constexpr std::size_t MAX_LAN_PRINTER_CONFIG_BYTES = 1024 * 1024;

// Validates the entire document before returning any printers. Errors never contain
// input values: JSON parser diagnostics can otherwise disclose access codes.
std::vector<LanPrinterConfig> parse_lan_printer_config(const std::string& contents);

} // namespace Slic3r
