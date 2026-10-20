#include "libslic3r/LanPrinterConfig.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <nlohmann/json.hpp>

#include <stdexcept>
#include <string>

using namespace Slic3r;

namespace {
nlohmann::json valid_config()
{
    return {
        {"version", 1},
        {"printers", {{{"name", "Workshop P1S"}, {"model", "P1S"}, {"ip", "192.168.1.50"},
                       {"serial", "01P00EXAMPLE0001"}, {"access_code", "AbCd1234"}}}}
    };
}
}

TEST_CASE("LAN printer configurations preserve connection details without account credentials", "[LanPrinterConfig]")
{
    auto document = valid_config();
    document["printers"].push_back(document["printers"][0]);
    document["printers"][1]["name"] = "Office P1S";
    document["printers"][1]["ip"] = "10.1.0.20";
    document["printers"][1]["serial"] = "01P00EXAMPLE0002";

    const auto printers = parse_lan_printer_config(document.dump());
    REQUIRE(printers.size() == 2);
    CHECK(printers[0].name == "Workshop P1S");
    CHECK(printers[0].ip == "192.168.1.50");
    CHECK(printers[0].serial == "01P00EXAMPLE0001");
    CHECK(printers[0].access_code == "AbCd1234");
    CHECK(printers[1].serial == "01P00EXAMPLE0002");
}

TEST_CASE("LAN printer configurations accept private IPv4 networks", "[LanPrinterConfig]")
{
    const auto ip = GENERATE("10.0.0.1", "172.16.0.1", "172.31.255.254", "192.168.0.1", "169.254.1.1");
    auto document = valid_config();
    document["printers"][0]["ip"] = ip;
    CHECK(parse_lan_printer_config(document.dump()).front().ip == ip);
}

TEST_CASE("LAN printer configurations reject nonlocal and ambiguous addresses", "[LanPrinterConfig]")
{
    const auto ip = GENERATE("8.8.8.8", "127.0.0.1", "0.0.0.0", "224.0.0.1", "172.15.0.1",
                             "172.32.0.1", "192.168.0.256", "192.168.01.2", "192.168..2",
                             "192.168.1.1.", ".192.168.1.1", "192.168.1", "printer.local",
                             "https://192.168.1.1", "192.168.1.1:8883", "::1", "192.168.1. 1");
    auto document = valid_config();
    document["printers"][0]["ip"] = ip;
    CHECK_THROWS_AS(parse_lan_printer_config(document.dump()), std::runtime_error);
}

TEST_CASE("LAN printer configurations require every connection field to be a string", "[LanPrinterConfig]")
{
    const auto field = GENERATE("name", "ip", "serial", "model", "access_code");
    auto document = valid_config();
    SECTION("Missing fields fail") {
        document["printers"][0].erase(field);
    }
    SECTION("Numbers are not silently converted") {
        document["printers"][0][field] = 12345678;
    }
    SECTION("Empty strings fail") {
        document["printers"][0][field] = "";
    }
    CHECK_THROWS_AS(parse_lan_printer_config(document.dump()), std::runtime_error);
}

TEST_CASE("LAN printer configurations reject unsupported models and malformed credentials", "[LanPrinterConfig]")
{
    auto document = valid_config();
    SECTION("Only P1S is supported") {
        document["printers"][0]["model"] = "X1C";
    }
    SECTION("Serials do not contain MQTT topic separators") {
        document["printers"][0]["serial"] = "01P00/EXAMPLE001";
    }
    SECTION("Serials cannot be truncated") {
        document["printers"][0]["serial"] = "01P";
    }
    SECTION("Codes have exactly eight characters") {
        document["printers"][0]["access_code"] = GENERATE("1234567", "123456789", "12 45678", "1234567\n");
    }
    SECTION("Names do not contain control characters") {
        document["printers"][0]["name"] = "Workshop\nP1S";
    }
    SECTION("Names are bounded") {
        document["printers"][0]["name"] = std::string(129, 'a');
    }
    SECTION("Names contain visible text") {
        document["printers"][0]["name"] = "   ";
    }
    CHECK_THROWS_AS(parse_lan_printer_config(document.dump()), std::runtime_error);
}

TEST_CASE("LAN printer configurations reject duplicate identities before returning any printers", "[LanPrinterConfig]")
{
    auto document = valid_config();
    document["printers"].push_back(document["printers"][0]);
    document["printers"][1]["name"] = "Second P1S";
    SECTION("Two serials cannot use the same address") {
        document["printers"][1]["serial"] = "01P00EXAMPLE0002";
    }
    SECTION("A serial cannot refer to two addresses") {
        document["printers"][1]["ip"] = "192.168.1.51";
    }
    SECTION("Names cannot ambiguously identify two printers") {
        document["printers"][1]["ip"] = "192.168.1.51";
        document["printers"][1]["serial"] = "01P00EXAMPLE0002";
        document["printers"][1]["name"] = document["printers"][0]["name"];
    }
    CHECK_THROWS_AS(parse_lan_printer_config(document.dump()), std::runtime_error);
}

TEST_CASE("LAN printer configurations reject invalid schema versions and unknown fields", "[LanPrinterConfig]")
{
    auto document = valid_config();
    SECTION("Unknown versions fail") { document["version"] = 2; }
    SECTION("String versions fail") { document["version"] = "1"; }
    SECTION("Fractional versions fail") { document["version"] = 1.0; }
    SECTION("Missing versions fail") { document.erase("version"); }
    SECTION("Unknown root fields fail") { document["account"] = "not-used"; }
    SECTION("Misspelled printer fields fail") { document["printers"][0]["accesscode"] = "12345678"; }
    SECTION("An empty printer list fails") { document["printers"] = nlohmann::json::array(); }
    SECTION("Missing printer lists fail") { document.erase("printers"); }
    SECTION("A printer list must be an array") { document["printers"] = nlohmann::json::object(); }
    SECTION("Printer entries must be objects") { document["printers"][0] = "printer"; }
    SECTION("The root must be an object") { document = nlohmann::json::array(); }
    CHECK_THROWS_AS(parse_lan_printer_config(document.dump()), std::runtime_error);
}

TEST_CASE("LAN printer configuration errors never echo secrets from malformed JSON", "[LanPrinterConfig]")
{
    const std::string contents = R"({"version":1,"printers":[{"access_code":"SECRET42" broken}])";
    try {
        parse_lan_printer_config(contents);
        FAIL("Malformed JSON was accepted");
    } catch (const std::runtime_error& error) {
        CHECK(std::string(error.what()).find("SECRET42") == std::string::npos);
    }
}

TEST_CASE("LAN printer configurations reject duplicate JSON keys and oversized inputs", "[LanPrinterConfig]")
{
    SECTION("Duplicate root keys fail") {
        CHECK_THROWS_AS(parse_lan_printer_config(R"({"version":1,"version":1,"printers":[]})"), std::runtime_error);
    }
    SECTION("Duplicate credential keys fail") {
        CHECK_THROWS_AS(parse_lan_printer_config(
            R"({"version":1,"printers":[{"access_code":"12345678","access_code":"ABCDEFGH"}]})"), std::runtime_error);
    }
    SECTION("Oversized files fail") {
        CHECK_THROWS_AS(parse_lan_printer_config(std::string(MAX_LAN_PRINTER_CONFIG_BYTES + 1, ' ')), std::runtime_error);
    }
    SECTION("Excessive nesting fails") {
        CHECK_THROWS_AS(parse_lan_printer_config(std::string(100, '[') + std::string(100, ']')), std::runtime_error);
    }
    SECTION("More than 128 printers fails") {
        auto document = valid_config();
        const auto printer = document["printers"][0];
        for (int i = 1; i < 129; ++i)
            document["printers"].push_back(printer);
        CHECK_THROWS_AS(parse_lan_printer_config(document.dump()), std::runtime_error);
    }
}
