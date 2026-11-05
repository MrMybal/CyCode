#include "../src/PluginPackageData.h"
#include "cycode/Paths.h"
#include "cycode/PluginManifest.h"
#include "cycode/PluginPackages.h"
#include <array>
#include <fstream>
#include <monocypher-ed25519.h>
#include <stdexcept>
namespace {
using Json = nlohmann::json;
namespace fs = std::filesystem;
void require(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
template <class F> void rejects(F f) {
    bool failed = false;
    try {
        f();
    } catch (const std::exception &) {
        failed = true;
    }
    require(failed, "Invalid plugin package operation accepted");
}
std::string hex(const uint8_t *p, size_t count) {
    std::string result;
    constexpr auto digits = "0123456789abcdef";
    for (size_t i = 0; i < count; ++i) {
        result += digits[p[i] >> 4];
        result += digits[p[i] & 15];
    }
    return result;
}
void write(const fs::path &p, const std::string &bytes) {
    std::ofstream output(p, std::ios::binary);
    output.write(bytes.data(), bytes.size());
    if (!output)
        throw std::runtime_error("Cannot write package fixture");
}
} // namespace
void pluginPackageContracts(const std::filesystem::path &fixture, const std::filesystem::path &library) {
    using namespace cycode;
    using namespace cycode::package_data;
    const auto root = fixture / "package-contracts";
    fs::create_directories(root / "installation");
    // Deterministic, public test-only seed; never used for distributed packages.
    std::array<uint8_t, 32> seed{}, publicKey{};
    std::array<uint8_t, 64> secret{};
    crypto_ed25519_key_pair(secret.data(), publicKey.data(), seed.data());
    const auto key = ::hex(publicKey.data(), publicKey.size());
#ifdef _WIN32
    const auto name = "fixture.dll";
#elif defined(__APPLE__)
    const auto name = "fixture.dylib";
#else
    const auto name = "fixture.so";
#endif
    const auto payload = read(library, maxPayloadFile);
    auto make = [&](const std::string &version, const Json &dependencies) {
        const auto folder = root / utf8Path("package-" + version);
        fs::create_directory(folder);
        write(folder / name, payload);
        Json document = {{"schemaVersion", 1},
                         {"platform", pluginHostInfo().at("platform")},
                         {"plugin",
                          {{"id", "fixture.managed"},
                           {"name", "Managed fixture"},
                           {"version", version},
                           {"abi", 2},
                           {"library", "fixture"},
                           {"permissions", Json::array()},
                           {"dependencies", dependencies}}},
                         {"files", Json::array({{{"path", name},
                                                 {"size", uint64_t(payload.size())},
                                                 {"sha512", sha512(payload)}}})}};
        const auto bytes = document.dump(2) + "\n", message = "CyCode plugin package v1\n" + bytes;
        std::array<uint8_t, 64> signature{};
        crypto_ed25519_sign(signature.data(), secret.data(),
                            reinterpret_cast<const uint8_t *>(message.data()), message.size());
        write(folder / "package.json", bytes);
        write(folder / "signature.json", Json{{"algorithm", "Ed25519"},
                                              {"publicKey", key},
                                              {"signature", ::hex(signature.data(), signature.size())}}
                                             .dump());
        return folder;
    };
    PluginPackages manager(root / "installation");
    auto first = make("1.0.0", Json::array()), second = make("2.0.0", Json::array()),
         third = make("3.0.0", Json::array());
    auto a = manager.inspect(first);
    require(a.at("signatureValid") && a.at("payloadVerified") && !a.at("trusted"),
            "Unsigned trust was invented");
    rejects([&] { manager.install(first, a.at("revision"), a.at("trustRevision"), a.at("digest")); });
    auto catalog = manager.trust(key, "Test publisher", true, a.at("trustRevision"));
    rejects([&] { manager.trust(key, "Stale", false, a.at("trustRevision")); });
    a = manager.inspect(first);
    catalog = manager.install(first, a.at("revision"), a.at("trustRevision"), a.at("digest"));
    require(catalog.at("entries").size() == 1, "Managed package not selected");
    Json errors = Json::array();
    auto liveA = manager.load(errors), liveB = manager.load(errors);
    require(liveA.size() == 1 && liveB.size() == 1 && errors.empty(),
            "Two instances cannot hold the same signed package");
    auto b = manager.inspect(second);
    catalog = manager.install(second, b.at("revision"), b.at("trustRevision"), b.at("digest"));
    rejects([&] { manager.remove("fixture.managed", a.at("revision")); });
    catalog = manager.rollback("fixture.managed", a.at("digest"), catalog.at("revision"),
                               catalog.at("trustRevision"));
    require(catalog.at("entries")[0].at("version") == "1.0.0", "Rollback did not select retained version");
    catalog = manager.remove("fixture.managed", catalog.at("revision"));
    const auto held = manager.clean("fixture.managed", a.at("digest"), catalog.at("revision"));
    require(held.at("action") == "inUse", "Cleanup removed an open instance's binary");
    liveA.clear();
    liveB.clear();
    catalog = manager.clean("fixture.managed", a.at("digest"), catalog.at("revision"));
    require(catalog.at("action") == "cleaned", "Inactive package cleanup failed");
    auto c = manager.inspect(third);
    write(third / name, payload + "tampered");
    rejects([&] { manager.install(third, c.at("revision"), c.at("trustRevision"), c.at("digest")); });
    require(manager.catalog().at("entries").empty(), "Failed copy changed selected packages");
    for (const auto &bad :
         {"../outside", "a/../b", "a\\b", "con.dll", "a:stream", "trailing. ", "/absolute", "double//slash"})
        require(!portablePath(bad), "Nonportable package path accepted");
    rejects([&] { parse("{\"version\":1,\"version\":2}"); });
    require(portablePath(std::string(128, 'a')), "Digest-length path component was rejected");
    const auto altered = make("5.0.0", Json::array());
    write(altered / "package.json", read(altered / "package.json", 256 * 1024) + " ");
    rejects([&] { manager.inspect(altered); });
    const auto link = root / "outside-link";
    std::error_code linkError;
    fs::create_symlink(library, link, linkError);
    if (!linkError)
        rejects([&] { join(root, "outside-link"); });
    auto dependency = make("4.0.0", Json::array({{{"id", "missing.plugin"}}}));
    auto d = manager.inspect(dependency);
    rejects([&] { manager.install(dependency, d.at("revision"), d.at("trustRevision"), d.at("digest")); });
    require(manager.catalog().at("entries").empty(), "Dependency failure changed selection");
    catalog = manager.trust(key, "Test publisher", false, manager.catalog().at("trustRevision"));
    auto old = manager.inspect(second);
    rejects([&] { manager.install(second, old.at("revision"), old.at("trustRevision"), old.at("digest")); });
}
