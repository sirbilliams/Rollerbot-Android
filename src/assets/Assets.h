#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace rb {
using Bytes = std::vector<std::uint8_t>;
// All failures throw std::runtime_error. Returned records own their storage.
class AssetStore {
public:
    void open(const std::filesystem::path& gameDir);
    std::optional<std::filesystem::path> find(const std::string& name) const;
    Bytes read(const std::string& name) const; // removes recognized DWORD wrapper
    const std::filesystem::path& directory() const { return root_; }
    static std::uint32_t hash(const std::string& name); // 0x40d550
    static std::string assetName(const std::string& name); // 0x441700
private:
    std::filesystem::path root_;
};
struct BrainRecord {
    std::uint32_t objectIndex = 0;
    std::vector<std::string> lines; // source preserved, including blank lines
};
struct ObjectRecord {
    std::string name, model, sound;
    std::array<double, 3> position{};
    std::int32_t type = 0; // serialized field 35
    std::vector<std::string> fields; // lossless, unknown fields retained
};
struct ProjectData {
    std::string version;
    std::vector<ObjectRecord> objects;
    std::vector<BrainRecord> brains;
    std::vector<std::string> extraLines;
    std::vector<std::int32_t> collection; // opaque COL slots, not a collision raster
    static ProjectData load(const AssetStore&, const std::string& project);
    static ProjectData parse(const Bytes& project, const Bytes& collection = {});
};
struct MeshPart {
    std::vector<std::array<float, 3>> vertices;
    std::vector<std::vector<std::uint32_t>> faces;
};
struct Mesh {
    std::vector<MeshPart> parts;
    // Geometry only; frame transforms/materials/UVs are not yet interpreted.
    static Mesh decode(const Bytes&);
};
struct Image {
    std::uint32_t width = 0, height = 0;
    Bytes rgb; // top-down RGB8
    static Image decode(const Bytes&);
};
struct Sound {
    std::uint16_t format = 0, channels = 0, bitsPerSample = 0, blockAlign = 0;
    std::uint32_t sampleRate = 0;
    Bytes samples; // interleaved little-endian PCM (8-bit unsigned, 16-bit signed)
    static Sound decode(const Bytes&);
};
// Recognized container name; decoded bytes must be supplied.
std::string assetKind(const Bytes&);
}
