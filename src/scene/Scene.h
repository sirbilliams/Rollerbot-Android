#pragma once
// rb::Scene: retained scene graph on OpenGL ES 2 (shaders + VBOs, no fixed function).
// Replaces the Direct3D Retained Mode surface Rollerbot.exe uses (see docs/d3drm-usage.md).
//
// Conventions (D3DRM): left-handed, +X right, +Y up, +Z into the screen. Front faces are
// clockwise as seen by the camera. Rotation angles/axes use the standard Rodrigues formula
// applied to those coordinates (what D3DRMQuaternionFromRotation computes). A frame's local
// transform is T * R * S (column-vector convention internally; matrices exposed to callers
// are column-major float[16], v' = M * v).
//
// Threading: all calls on the thread owning the GL context. Ids are plain handles, 0 = none.
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace rb {

struct Vec3 { float x = 0, y = 0, z = 0; };
struct Quat { float w = 1, x = 0, y = 0, z = 0; };
struct Color { float r = 1, g = 1, b = 1, a = 1; };
using Mat4 = std::array<float, 16>; // column-major

Vec3 operator+(Vec3, Vec3); Vec3 operator-(Vec3, Vec3); Vec3 operator*(Vec3, float);
Vec3 cross(Vec3, Vec3); float dot(Vec3, Vec3); Vec3 normalize(Vec3);
Quat quatFromAxisAngle(Vec3 axis, float radians); // D3DRMQuaternionFromRotation
Quat operator*(Quat a, Quat b);                   // D3DRMQuaternionMultiply: a*b applies b first
Quat quatFromBasis(Vec3 dir, Vec3 up);            // SetOrientation(dir, up): +Z=dir, +Y~up
Mat4 matFromQuat(Quat);
Mat4 operator*(const Mat4&, const Mat4&);

using FrameId = std::uint32_t;
using MeshId = std::uint32_t;
using TextureId = std::uint32_t;
using LightId = std::uint32_t;

enum class Combine { Replace, Before, After }; // D3DRMCOMBINE_REPLACE / BEFORE / AFTER
enum class SortMode { FromParent, None, FrontToBack, BackToFront }; // D3DRMSORT_*
enum class LightType { Ambient, Point, Directional }; // spot -> Point (not used by the game)

struct Material {
    Color diffuse{1, 1, 1, 1}; // alpha < 1 => blended
    Color emissive{0, 0, 0, 1};
    TextureId texture = 0;
};

// Triangle list mesh as D3DRM MeshBuilder + Face: one material per face.
struct MeshData {
    std::vector<Vec3> positions;
    std::vector<Vec3> normals;               // empty or size == positions
    std::vector<std::array<float, 2>> uvs;   // empty or size == positions
    struct Face { std::uint32_t v[3]; std::uint32_t material = 0; };
    std::vector<Face> faces;                 // clockwise = front (D3DRM)
    std::vector<Material> materials;         // at least one (default white added if empty)
    // MeshBuilder::GenerateNormals(creaseAngle): splits vertices so that edges sharper than
    // the angle stay hard. creaseAngle<=0 means flat shading, >=pi means fully smooth.
    void generateNormals(float creaseAngleRadians);
    // Wrap (CreateWrap type flat): uv = ((p-origin).(x axis)*su+ou, (p-origin).(y axis)*sv+ov)
    // z = direction, up = v axis, as D3DRM planar wrap; x = up cross dir.
    void applyFlatWrap(Vec3 origin, Vec3 dir, Vec3 up, float ou, float ov, float su, float sv);
};

struct PickHit {
    FrameId frame = 0;
    MeshId mesh = 0;
    std::uint32_t face = 0;
    float distance = 0;
    Vec3 point, normal; // world space
};

class Scene {
public:
    Scene();
    ~Scene();
    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;

    // Needs a current GL ES 2 context (or desktop GL 2.1+, shaders are portable).
    bool init(std::string* error = nullptr);

    // ---- frames (hierarchy). The root frame exists after init: root().
    FrameId root() const;
    FrameId createFrame(FrameId parent = 0); // 0 -> root
    void destroyFrame(FrameId);              // destroys children too
    void setParent(FrameId, FrameId parent); // keeps local transform
    FrameId parent(FrameId) const;
    void setVisible(FrameId, bool);
    void setPosition(FrameId, Vec3);
    void setScale(FrameId, Vec3);
    void setRotation(FrameId, Quat);
    void setOrientation(FrameId, Vec3 dir, Vec3 up);          // Frame::SetOrientation (local)
    void addRotation(FrameId, Combine, Vec3 axis, float rad); // Frame::AddRotation
    void addTranslation(FrameId, Combine, Vec3);
    void addScale(FrameId, Combine, Vec3);
    void lookAt(FrameId, Vec3 worldTarget, Vec3 up = {0, 1, 0});
    Vec3 position(FrameId) const;
    Quat rotation(FrameId) const;
    Mat4 localMatrix(FrameId) const;
    Mat4 worldMatrix(FrameId) const;
    Vec3 worldPosition(FrameId) const;
    void setSortMode(FrameId, SortMode);   // inherited by children when FromParent
    void setZBuffer(FrameId, bool enabled); // Frame::SetZbufferMode (false = sky)
    void setPickable(FrameId, bool);        // SetTraversalOptions: pick flag (default true)

    // ---- textures. RGBA8, top-down rows. colorKey: pixels equal to key become alpha 0
    // (D3DRM SetDecalTransparency + transparent colour, default black).
    TextureId createTextureRGBA(int w, int h, const std::uint8_t* rgba, bool linear = false);
    TextureId createTextureRGB(int w, int h, const std::uint8_t* rgb, bool colorKey,
                               std::array<std::uint8_t, 3> key = {0, 0, 0}, bool linear = false);
    void destroyTexture(TextureId);

    // ---- meshes (shared geometry; instantiate with addVisual)
    MeshId createMesh(const MeshData&);
    void destroyMesh(MeshId);
    void addVisual(FrameId, MeshId);   // Frame::AddVisual
    void removeVisual(FrameId, MeshId);

    // ---- camera/viewport
    // fieldOfView = D3DRM "field" (half-extent at distance 1: tan(fov/2) for square view)
    void setCamera(FrameId cameraFrame, float field, float front, float back);
    void setViewport(int x, int y, int w, int h); // pixels, origin top-left of the window
    void setBackground(Color);

    // ---- lights
    LightId createLight(LightType, Color color);
    void addLight(FrameId, LightId);   // directional: along frame +Z; point: at frame origin
    void setLightRange(LightId, float range);
    void destroyLight(LightId);

    // ---- fog (Frame scene fog; linear)
    void setFog(bool enable, Color color, float start, float end);

    // ---- rendering
    void render(); // Viewport::Clear + Viewport::Render. Caller swaps buffers (Device::Update).
    // Read back the viewport as RGBA8, top-down.
    bool readPixels(std::vector<std::uint8_t>& rgba, int& w, int& h) const;

    // ---- queries (CPU, world space; frames flagged unpickable or invisible are skipped)
    std::vector<PickHit> rayPick(Vec3 origin, Vec3 dir, float maxDist = 1e30f) const; // sorted
    bool rayPickFirst(Vec3 origin, Vec3 dir, PickHit& out, float maxDist = 1e30f) const;
    // Ray through a viewport pixel using the current camera (Viewport::Pick).
    bool pickPixel(int x, int y, PickHit& out) const;

private:
    struct Impl;
    std::unique_ptr<Impl> d;
};

} // namespace rb
