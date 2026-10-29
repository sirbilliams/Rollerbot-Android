#include "scene/Scene.h"
#define GL_GLEXT_PROTOTYPES 1
#include <GLES2/gl2.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <unordered_map>

namespace rb {

// ---------------------------------------------------------------- math
Vec3 operator+(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vec3 operator-(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
Vec3 operator*(Vec3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
Vec3 cross(Vec3 a, Vec3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
Vec3 normalize(Vec3 a) {
    float l = std::sqrt(dot(a, a));
    return l > 1e-12f ? a * (1.0f / l) : Vec3{0, 0, 0};
}
Quat quatFromAxisAngle(Vec3 axis, float r) {
    axis = normalize(axis);
    float s = std::sin(r * 0.5f);
    return {std::cos(r * 0.5f), axis.x * s, axis.y * s, axis.z * s};
}
Quat operator*(Quat a, Quat b) {
    return {a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z, a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
            a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x, a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w};
}
static Quat qnorm(Quat q) {
    float l = std::sqrt(q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z);
    if (l < 1e-12f) return {};
    return {q.w / l, q.x / l, q.y / l, q.z / l};
}
Mat4 matFromQuat(Quat q) {
    q = qnorm(q);
    float xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z, xy = q.x * q.y, xz = q.x * q.z, yz = q.y * q.z,
          wx = q.w * q.x, wy = q.w * q.y, wz = q.w * q.z;
    return {1 - 2 * (yy + zz), 2 * (xy + wz), 2 * (xz - wy), 0,
            2 * (xy - wz), 1 - 2 * (xx + zz), 2 * (yz + wx), 0,
            2 * (xz + wy), 2 * (yz - wx), 1 - 2 * (xx + yy), 0,
            0, 0, 0, 1};
}
Mat4 operator*(const Mat4& a, const Mat4& b) {
    Mat4 r{};
    for (int c = 0; c < 4; c++)
        for (int rr = 0; rr < 4; rr++) {
            float s = 0;
            for (int k = 0; k < 4; k++) s += a[k * 4 + rr] * b[c * 4 + k];
            r[c * 4 + rr] = s;
        }
    return r;
}
static Mat4 identity() { return {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}; }
static Vec3 xformPoint(const Mat4& m, Vec3 p) {
    return {m[0] * p.x + m[4] * p.y + m[8] * p.z + m[12], m[1] * p.x + m[5] * p.y + m[9] * p.z + m[13],
            m[2] * p.x + m[6] * p.y + m[10] * p.z + m[14]};
}
static Vec3 xformDir(const Mat4& m, Vec3 p) {
    return {m[0] * p.x + m[4] * p.y + m[8] * p.z, m[1] * p.x + m[5] * p.y + m[9] * p.z,
            m[2] * p.x + m[6] * p.y + m[10] * p.z};
}
// General affine inverse (3x3 inverse + translation).
static Mat4 affineInverse(const Mat4& m) {
    float a = m[0], b = m[4], c = m[8], d = m[1], e = m[5], f = m[9], g = m[2], h = m[6], i = m[10];
    float A = e * i - f * h, B = -(d * i - f * g), C = d * h - e * g;
    float det = a * A + b * B + c * C;
    float id = std::fabs(det) > 1e-20f ? 1.0f / det : 0.0f;
    Mat4 r = identity();
    r[0] = A * id; r[4] = -(b * i - c * h) * id; r[8] = (b * f - c * e) * id;
    r[1] = B * id; r[5] = (a * i - c * g) * id;  r[9] = -(a * f - c * d) * id;
    r[2] = C * id; r[6] = -(a * h - b * g) * id; r[10] = (a * e - b * d) * id;
    r[12] = -(r[0] * m[12] + r[4] * m[13] + r[8] * m[14]);
    r[13] = -(r[1] * m[12] + r[5] * m[13] + r[9] * m[14]);
    r[14] = -(r[2] * m[12] + r[6] * m[13] + r[10] * m[14]);
    return r;
}
static Quat quatFromMat3(Vec3 x, Vec3 y, Vec3 z) { // columns of a rotation matrix
    float m00 = x.x, m10 = x.y, m20 = x.z, m01 = y.x, m11 = y.y, m21 = y.z, m02 = z.x, m12 = z.y, m22 = z.z;
    float t = m00 + m11 + m22;
    Quat q;
    if (t > 0) {
        float s = std::sqrt(t + 1) * 2;
        q = {0.25f * s, (m21 - m12) / s, (m02 - m20) / s, (m10 - m01) / s};
    } else if (m00 > m11 && m00 > m22) {
        float s = std::sqrt(1 + m00 - m11 - m22) * 2;
        q = {(m21 - m12) / s, 0.25f * s, (m01 + m10) / s, (m02 + m20) / s};
    } else if (m11 > m22) {
        float s = std::sqrt(1 + m11 - m00 - m22) * 2;
        q = {(m02 - m20) / s, (m01 + m10) / s, 0.25f * s, (m12 + m21) / s};
    } else {
        float s = std::sqrt(1 + m22 - m00 - m11) * 2;
        q = {(m10 - m01) / s, (m02 + m20) / s, (m12 + m21) / s, 0.25f * s};
    }
    return qnorm(q);
}
Quat quatFromBasis(Vec3 dir, Vec3 up) {
    Vec3 z = normalize(dir);
    if (dot(z, z) == 0) return {};
    Vec3 x = cross(up, z);
    if (dot(x, x) < 1e-10f) x = cross(std::fabs(z.y) < 0.99f ? Vec3{0, 1, 0} : Vec3{1, 0, 0}, z);
    x = normalize(x);
    Vec3 y = cross(z, x);
    return quatFromMat3(x, y, z);
}

// ---------------------------------------------------------------- mesh data helpers
void MeshData::generateNormals(float crease) {
    if (materials.empty()) materials.push_back({});
    std::vector<Vec3> fn(faces.size());
    for (size_t i = 0; i < faces.size(); i++) {
        auto& f = faces[i];
        Vec3 a = positions[f.v[0]], b = positions[f.v[1]], c = positions[f.v[2]];
        fn[i] = normalize(cross(b - a, c - a)); // outward for clockwise-front (LH)
    }
    // corner lists per original position
    std::unordered_map<uint64_t, std::vector<std::pair<uint32_t, uint8_t>>> byPos;
    auto key = [&](Vec3 p) {
        auto q = [](float v) { return (uint64_t)(uint32_t)(int32_t)std::lround(v * 1024.0f) & 0x1FFFFF; };
        return q(p.x) | (q(p.y) << 21) | (q(p.z) << 42);
    };
    for (uint32_t i = 0; i < faces.size(); i++)
        for (uint8_t k = 0; k < 3; k++) byPos[key(positions[faces[i].v[k]])].push_back({i, k});
    float cosC = std::cos(std::min(crease, 3.14159265f));
    std::vector<Vec3> np;
    std::vector<Vec3> nn;
    std::vector<std::array<float, 2>> nu;
    bool hasUv = uvs.size() == positions.size();
    for (uint32_t i = 0; i < faces.size(); i++)
        for (uint8_t k = 0; k < 3; k++) {
            uint32_t v = faces[i].v[k];
            Vec3 n{};
            if (crease <= 0) n = fn[i];
            else
                for (auto& [fj, kj] : byPos[key(positions[v])])
                    if (crease >= 3.14159f || dot(fn[i], fn[fj]) >= cosC) n = n + fn[fj];
            np.push_back(positions[v]);
            nn.push_back(normalize(n));
            if (hasUv) nu.push_back(uvs[v]);
        }
    for (uint32_t i = 0; i < faces.size(); i++)
        for (int k = 0; k < 3; k++) faces[i].v[k] = i * 3 + k;
    positions = std::move(np);
    normals = std::move(nn);
    uvs = std::move(nu);
}
void MeshData::applyFlatWrap(Vec3 origin, Vec3 dir, Vec3 up, float ou, float ov, float su, float sv) {
    Vec3 z = normalize(dir), x = normalize(cross(up, z)), y = cross(z, x);
    uvs.resize(positions.size());
    for (size_t i = 0; i < positions.size(); i++) {
        Vec3 p = positions[i] - origin;
        uvs[i] = {dot(p, x) * su + ou, 1.0f - (dot(p, y) * sv + ov)};
    }
}

// ---------------------------------------------------------------- scene impl
namespace {
struct Frame {
    FrameId parent = 0;
    std::vector<FrameId> children;
    Vec3 pos, scale{1, 1, 1};
    Quat rot;
    bool visible = true, zbuf = true, pickable = true;
    SortMode sort = SortMode::FromParent;
    std::vector<MeshId> visuals;
    std::vector<LightId> lights;
};
struct Batch { uint32_t first, count; Material mat; };
struct Mesh {
    MeshData cpu;
    GLuint vbo = 0, ibo = 0;
    std::vector<Batch> batches;
    Vec3 center;
};
struct Tex { GLuint id = 0; };
struct Light { LightType type; Color color; float range = 0; };
struct Draw { const Mesh* mesh; const Batch* b; Mat4 world; float dist; bool noZ; };

const char* kVS = R"(
attribute vec3 aPos; attribute vec3 aNrm; attribute vec2 aUv;
uniform mat4 uMVP; uniform mat4 uModel; uniform mat4 uView;
uniform vec4 uDiffuse; uniform vec3 uEmissive;
uniform vec4 uLC[4]; uniform vec4 uLD[4]; // color.rgb + kind, dir/pos + range
uniform float uFogOn; uniform vec2 uFogRange;
varying vec4 vColor; varying vec2 vUv; varying float vFog;
void main(){
  gl_Position = uMVP*vec4(aPos,1.0);
  vec3 wp = (uModel*vec4(aPos,1.0)).xyz;
  vec3 n = normalize((uModel*vec4(aNrm,0.0)).xyz);
  vec3 lit = vec3(0.0);
  for(int i=0;i<4;i++){
    float k = uLC[i].w;
    if(k>0.5 && k<1.5) lit += uLC[i].rgb;
    else if(k>1.5 && k<2.5) lit += uLC[i].rgb*max(dot(n,-uLD[i].xyz),0.0);
    else if(k>2.5){
      vec3 d = uLD[i].xyz-wp; float l = length(d);
      float att = uLD[i].w>0.0 ? clamp(1.0-l/uLD[i].w,0.0,1.0) : 1.0;
      lit += uLC[i].rgb*max(dot(n,d/max(l,1e-5)),0.0)*att;
    }
  }
  vColor = vec4(uEmissive + uDiffuse.rgb*min(lit,vec3(1.0)), uDiffuse.a);
  vUv = aUv;
  float z = (uView*vec4(wp,1.0)).z;
  vFog = uFogOn>0.5 ? clamp((uFogRange.y-z)/max(uFogRange.y-uFogRange.x,1e-5),0.0,1.0) : 1.0;
}
)";
const char* kFS = R"(
precision mediump float;
varying vec4 vColor; varying vec2 vUv; varying float vFog;
uniform sampler2D uTex; uniform float uUseTex; uniform vec3 uFogColor;
void main(){
  vec4 c = vColor;
  if(uUseTex>0.5){ vec4 t = texture2D(uTex,vUv); if(t.a<0.5) discard; c *= t; }
  c.rgb = mix(uFogColor, c.rgb, vFog);
  gl_FragColor = c;
}
)";
} // namespace

struct Scene::Impl {
    std::map<FrameId, Frame> frames;
    std::map<MeshId, Mesh> meshes;
    std::map<TextureId, Tex> texs;
    std::map<LightId, Light> lights;
    FrameId nextF = 2, nextM = 1, nextT = 1, nextL = 1, rootId = 1, cam = 0;
    float field = 0.9375f, front = 0.01f, back = 100.f;
    int vx = 0, vy = 0, vw = 640, vh = 480;
    Color bg{0, 0, 0, 1};
    bool fogOn = false;
    Color fogColor;
    float fogA = 0, fogB = 1;
    GLuint prog = 0;
    GLint uMVP, uModel, uView, uDiffuse, uEmissive, uLC, uLD, uFogOn, uFogRange, uFogColor, uTex, uUseTex;
    GLint aPos, aNrm, aUv;

    Mat4 local(const Frame& f) const {
        Mat4 m = matFromQuat(f.rot);
        for (int i = 0; i < 3; i++) { m[i] *= f.scale.x; m[4 + i] *= f.scale.y; m[8 + i] *= f.scale.z; }
        m[12] = f.pos.x; m[13] = f.pos.y; m[14] = f.pos.z;
        return m;
    }
    Mat4 world(FrameId id) const {
        Mat4 m = identity();
        std::vector<FrameId> chain;
        for (FrameId i = id; i; i = frames.at(i).parent) chain.push_back(i);
        for (auto it = chain.rbegin(); it != chain.rend(); ++it) m = m * local(frames.at(*it));
        return m;
    }
    bool effZ(FrameId id) const {
        for (FrameId i = id; i; i = frames.at(i).parent) if (!frames.at(i).zbuf) return false;
        return true;
    }
    SortMode effSort(FrameId id) const {
        for (FrameId i = id; i; i = frames.at(i).parent)
            if (frames.at(i).sort != SortMode::FromParent) return frames.at(i).sort;
        return SortMode::None;
    }
    bool effVisible(FrameId id) const {
        for (FrameId i = id; i; i = frames.at(i).parent) if (!frames.at(i).visible) return false;
        return true;
    }
    Mat4 projection() const {
        float n = std::max(front, 0.01f), f = std::max(back, n + 0.01f);
        float aspect = vh > 0 ? float(vw) / vh : 1.f;
        Mat4 p{};
        p[0] = 1.f / field; p[5] = aspect / field;
        p[10] = (f + n) / (f - n); p[11] = 1.f; p[14] = -2 * f * n / (f - n);
        return p;
    }
    void collect(FrameId id, std::vector<Frame*>&) {}
};

Scene::Scene() : d(new Impl) {}
Scene::~Scene() = default;

static GLuint compile(GLenum t, const char* src, std::string* err) {
    GLuint s = glCreateShader(t);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(s, sizeof log, nullptr, log);
        if (err) *err = log;
        return 0;
    }
    return s;
}

bool Scene::init(std::string* error) {
    GLuint vs = compile(GL_VERTEX_SHADER, kVS, error), fs = compile(GL_FRAGMENT_SHADER, kFS, error);
    if (!vs || !fs) return false;
    d->prog = glCreateProgram();
    glAttachShader(d->prog, vs); glAttachShader(d->prog, fs);
    glBindAttribLocation(d->prog, 0, "aPos"); glBindAttribLocation(d->prog, 1, "aNrm");
    glBindAttribLocation(d->prog, 2, "aUv");
    glLinkProgram(d->prog);
    GLint ok = 0;
    glGetProgramiv(d->prog, GL_LINK_STATUS, &ok);
    if (!ok) { if (error) *error = "link failed"; return false; }
    auto U = [&](const char* n) { return glGetUniformLocation(d->prog, n); };
    d->uMVP = U("uMVP"); d->uModel = U("uModel"); d->uView = U("uView"); d->uDiffuse = U("uDiffuse");
    d->uEmissive = U("uEmissive"); d->uLC = U("uLC"); d->uLD = U("uLD"); d->uFogOn = U("uFogOn");
    d->uFogRange = U("uFogRange"); d->uFogColor = U("uFogColor"); d->uTex = U("uTex"); d->uUseTex = U("uUseTex");
    GLint vp[4];
    glGetIntegerv(GL_VIEWPORT, vp);
    d->vx = vp[0]; d->vy = vp[1]; d->vw = vp[2]; d->vh = vp[3];
    d->frames[d->rootId] = Frame{};
    return true;
}

FrameId Scene::root() const { return d->rootId; }
FrameId Scene::createFrame(FrameId parent) {
    if (!parent || !d->frames.count(parent)) parent = d->rootId;
    FrameId id = d->nextF++;
    d->frames[id].parent = parent;
    d->frames[parent].children.push_back(id);
    return id;
}
void Scene::destroyFrame(FrameId id) {
    if (id == d->rootId || !d->frames.count(id)) return;
    auto kids = d->frames[id].children;
    for (auto k : kids) destroyFrame(k);
    auto& pc = d->frames[d->frames[id].parent].children;
    pc.erase(std::remove(pc.begin(), pc.end(), id), pc.end());
    d->frames.erase(id);
}
void Scene::setParent(FrameId id, FrameId p) {
    if (id == d->rootId || !d->frames.count(id)) return;
    if (!p || !d->frames.count(p)) p = d->rootId;
    for (FrameId i = p; i; i = d->frames[i].parent) if (i == id) return; // cycle
    auto& pc = d->frames[d->frames[id].parent].children;
    pc.erase(std::remove(pc.begin(), pc.end(), id), pc.end());
    d->frames[id].parent = p;
    d->frames[p].children.push_back(id);
}
FrameId Scene::parent(FrameId id) const { return d->frames.at(id).parent; }
void Scene::setVisible(FrameId id, bool v) { d->frames.at(id).visible = v; }
void Scene::setPosition(FrameId id, Vec3 p) { d->frames.at(id).pos = p; }
void Scene::setScale(FrameId id, Vec3 s) { d->frames.at(id).scale = s; }
void Scene::setRotation(FrameId id, Quat q) { d->frames.at(id).rot = qnorm(q); }
void Scene::setOrientation(FrameId id, Vec3 dir, Vec3 up) { d->frames.at(id).rot = quatFromBasis(dir, up); }
void Scene::addRotation(FrameId id, Combine c, Vec3 axis, float r) {
    auto& f = d->frames.at(id);
    Quat q = quatFromAxisAngle(axis, r);
    f.rot = qnorm(c == Combine::Replace ? q : c == Combine::Before ? f.rot * q : q * f.rot);
}
void Scene::addTranslation(FrameId id, Combine c, Vec3 t) {
    auto& f = d->frames.at(id);
    if (c == Combine::Replace) f.pos = t;
    else if (c == Combine::After) f.pos = f.pos + t;
    else {
        Mat4 r = matFromQuat(f.rot);
        f.pos = f.pos + xformDir(r, {t.x * f.scale.x, t.y * f.scale.y, t.z * f.scale.z});
    }
}
void Scene::addScale(FrameId id, Combine c, Vec3 s) {
    auto& f = d->frames.at(id);
    if (c == Combine::Replace) f.scale = s;
    else f.scale = {f.scale.x * s.x, f.scale.y * s.y, f.scale.z * s.z};
}
void Scene::lookAt(FrameId id, Vec3 target, Vec3 up) {
    auto& f = d->frames.at(id);
    Mat4 pw = f.parent ? d->world(f.parent) : identity();
    Vec3 lt = xformPoint(affineInverse(pw), target);
    f.rot = quatFromBasis(lt - f.pos, xformDir(affineInverse(pw), up));
}
Vec3 Scene::position(FrameId id) const { return d->frames.at(id).pos; }
Quat Scene::rotation(FrameId id) const { return d->frames.at(id).rot; }
Mat4 Scene::localMatrix(FrameId id) const { return d->local(d->frames.at(id)); }
Mat4 Scene::worldMatrix(FrameId id) const { return d->world(id); }
Vec3 Scene::worldPosition(FrameId id) const { auto m = d->world(id); return {m[12], m[13], m[14]}; }
void Scene::setSortMode(FrameId id, SortMode m) { d->frames.at(id).sort = m; }
void Scene::setZBuffer(FrameId id, bool e) { d->frames.at(id).zbuf = e; }
void Scene::setPickable(FrameId id, bool p) { d->frames.at(id).pickable = p; }

TextureId Scene::createTextureRGBA(int w, int h, const std::uint8_t* rgba, bool linear) {
    Tex t;
    glGenTextures(1, &t.id);
    glBindTexture(GL_TEXTURE_2D, t.id);
    bool pot = (w & (w - 1)) == 0 && (h & (h - 1)) == 0;
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    GLint filt = linear ? GL_LINEAR : GL_NEAREST;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filt);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filt);
    GLint wrap = pot ? GL_REPEAT : GL_CLAMP_TO_EDGE; // ES2: NPOT cannot repeat
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);
    TextureId id = d->nextT++;
    d->texs[id] = t;
    return id;
}
TextureId Scene::createTextureRGB(int w, int h, const std::uint8_t* rgb, bool key,
                                  std::array<std::uint8_t, 3> k, bool linear) {
    std::vector<std::uint8_t> px((size_t)w * h * 4);
    for (size_t i = 0; i < (size_t)w * h; i++) {
        px[i * 4] = rgb[i * 3]; px[i * 4 + 1] = rgb[i * 3 + 1]; px[i * 4 + 2] = rgb[i * 3 + 2];
        bool isKey = key && rgb[i * 3] == k[0] && rgb[i * 3 + 1] == k[1] && rgb[i * 3 + 2] == k[2];
        px[i * 4 + 3] = isKey ? 0 : 255;
    }
    return createTextureRGBA(w, h, px.data(), linear);
}
void Scene::destroyTexture(TextureId id) {
    auto it = d->texs.find(id);
    if (it == d->texs.end()) return;
    glDeleteTextures(1, &it->second.id);
    d->texs.erase(it);
}

MeshId Scene::createMesh(const MeshData& in) {
    Mesh m;
    m.cpu = in;
    if (m.cpu.materials.empty()) m.cpu.materials.push_back({});
    if (m.cpu.normals.size() != m.cpu.positions.size()) m.cpu.generateNormals(0);
    if (m.cpu.uvs.size() != m.cpu.positions.size()) m.cpu.uvs.assign(m.cpu.positions.size(), {0, 0});
    if (m.cpu.positions.size() > 65535) return 0; // ES2 16-bit indices
    std::vector<float> vb;
    Vec3 mn{1e30f, 1e30f, 1e30f}, mx{-1e30f, -1e30f, -1e30f};
    for (size_t i = 0; i < m.cpu.positions.size(); i++) {
        auto p = m.cpu.positions[i], n = m.cpu.normals[i];
        vb.insert(vb.end(), {p.x, p.y, p.z, n.x, n.y, n.z, m.cpu.uvs[i][0], m.cpu.uvs[i][1]});
        mn = {std::min(mn.x, p.x), std::min(mn.y, p.y), std::min(mn.z, p.z)};
        mx = {std::max(mx.x, p.x), std::max(mx.y, p.y), std::max(mx.z, p.z)};
    }
    m.center = (mn + mx) * 0.5f;
    std::vector<uint16_t> ib;
    for (uint32_t mi = 0; mi < m.cpu.materials.size(); mi++) {
        uint32_t first = (uint32_t)ib.size();
        for (auto& f : m.cpu.faces)
            if (f.material == mi) for (int k = 0; k < 3; k++) ib.push_back((uint16_t)f.v[k]);
        if (ib.size() > first) m.batches.push_back({first, (uint32_t)ib.size() - first, m.cpu.materials[mi]});
    }
    glGenBuffers(1, &m.vbo); glGenBuffers(1, &m.ibo);
    glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
    glBufferData(GL_ARRAY_BUFFER, vb.size() * 4, vb.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, ib.size() * 2, ib.data(), GL_STATIC_DRAW);
    MeshId id = d->nextM++;
    d->meshes[id] = std::move(m);
    return id;
}
void Scene::destroyMesh(MeshId id) {
    auto it = d->meshes.find(id);
    if (it == d->meshes.end()) return;
    glDeleteBuffers(1, &it->second.vbo); glDeleteBuffers(1, &it->second.ibo);
    d->meshes.erase(it);
    for (auto& [fid, f] : d->frames) f.visuals.erase(std::remove(f.visuals.begin(), f.visuals.end(), id), f.visuals.end());
}
void Scene::addVisual(FrameId f, MeshId m) { if (d->meshes.count(m)) d->frames.at(f).visuals.push_back(m); }
void Scene::removeVisual(FrameId f, MeshId m) {
    auto& v = d->frames.at(f).visuals;
    v.erase(std::remove(v.begin(), v.end(), m), v.end());
}

void Scene::setCamera(FrameId c, float field, float front, float back) {
    d->cam = c; d->field = field; d->front = front; d->back = back;
}
void Scene::setViewport(int x, int y, int w, int h) { d->vx = x; d->vy = y; d->vw = w; d->vh = h; }
void Scene::setBackground(Color c) { d->bg = c; }
LightId Scene::createLight(LightType t, Color c) { LightId id = d->nextL++; d->lights[id] = {t, c, 0}; return id; }
void Scene::addLight(FrameId f, LightId l) { if (d->lights.count(l)) d->frames.at(f).lights.push_back(l); }
void Scene::setLightRange(LightId l, float r) { if (d->lights.count(l)) d->lights[l].range = r; }
void Scene::destroyLight(LightId l) {
    d->lights.erase(l);
    for (auto& [id, f] : d->frames) f.lights.erase(std::remove(f.lights.begin(), f.lights.end(), l), f.lights.end());
}
void Scene::setFog(bool on, Color c, float a, float b) { d->fogOn = on; d->fogColor = c; d->fogA = a; d->fogB = b; }

void Scene::render() {
    glViewport(d->vx, d->vy, d->vw, d->vh);
    glEnable(GL_SCISSOR_TEST);
    glScissor(d->vx, d->vy, d->vw, d->vh);
    glClearColor(d->bg.r, d->bg.g, d->bg.b, 1);
    glDepthMask(GL_TRUE);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_SCISSOR_TEST);
    if (!d->cam || !d->frames.count(d->cam)) return;
    Mat4 camW = d->world(d->cam);
    Mat4 view = affineInverse(camW);
    Mat4 vp = d->projection() * view;
    Vec3 camPos{camW[12], camW[13], camW[14]};

    // lights: all lights in visible frames (up to 4)
    float lc[16] = {}, ld[16] = {};
    int nl = 0;
    for (auto& [fid, f] : d->frames) {
        if (!d->effVisible(fid)) continue;
        for (LightId l : f.lights) {
            if (nl >= 4) break;
            auto& L = d->lights[l];
            Mat4 w = d->world(fid);
            lc[nl * 4] = L.color.r; lc[nl * 4 + 1] = L.color.g; lc[nl * 4 + 2] = L.color.b;
            lc[nl * 4 + 3] = L.type == LightType::Ambient ? 1.f : L.type == LightType::Directional ? 2.f : 3.f;
            Vec3 v = L.type == LightType::Directional ? normalize(xformDir(w, {0, 0, 1}))
                                                      : Vec3{w[12], w[13], w[14]};
            ld[nl * 4] = v.x; ld[nl * 4 + 1] = v.y; ld[nl * 4 + 2] = v.z; ld[nl * 4 + 3] = L.range;
            nl++;
        }
    }
    if (nl == 0) { lc[0] = lc[1] = lc[2] = 1; lc[3] = 1; } // no lights: fully lit
    std::vector<Draw> opaque, noZ, blend;
    for (auto& [fid, f] : d->frames) {
        if (f.visuals.empty() || !d->effVisible(fid)) continue;
        Mat4 w = d->world(fid);
        bool z = d->effZ(fid);
        for (MeshId mid : f.visuals) {
            const Mesh& m = d->meshes.at(mid);
            Vec3 c = xformPoint(w, m.center);
            Vec3 dd = c - camPos;
            float dist = std::sqrt(dot(dd, dd));
            for (auto& b : m.batches) {
                Draw dr{&m, &b, w, dist, !z};
                if (!z) noZ.push_back(dr);
                else if (b.mat.diffuse.a < 0.999f) blend.push_back(dr);
                else opaque.push_back(dr);
            }
        }
    }
    std::stable_sort(blend.begin(), blend.end(), [](const Draw& a, const Draw& b) { return a.dist > b.dist; });
    std::stable_sort(noZ.begin(), noZ.end(), [](const Draw& a, const Draw& b) { return a.dist > b.dist; });

    glUseProgram(d->prog);
    glEnable(GL_CULL_FACE); glCullFace(GL_BACK); glFrontFace(GL_CW); // D3DRM front = clockwise
    glUniform4fv(d->uLC, 4, lc); glUniform4fv(d->uLD, 4, ld);
    glUniformMatrix4fv(d->uView, 1, GL_FALSE, view.data());
    glUniform1f(d->uFogOn, d->fogOn ? 1.f : 0.f);
    glUniform2f(d->uFogRange, d->fogA, d->fogB);
    glUniform3f(d->uFogColor, d->fogColor.r, d->fogColor.g, d->fogColor.b);
    glUniform1i(d->uTex, 0);
    glEnableVertexAttribArray(0); glEnableVertexAttribArray(1); glEnableVertexAttribArray(2);
    auto draw = [&](const std::vector<Draw>& list, bool depth, bool blendOn) {
        if (depth) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
        glDepthMask(depth && !blendOn ? GL_TRUE : GL_FALSE);
        if (blendOn) { glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); }
        else glDisable(GL_BLEND);
        for (auto& dr : list) {
            Mat4 mvp = vp * dr.world;
            glUniformMatrix4fv(d->uMVP, 1, GL_FALSE, mvp.data());
            glUniformMatrix4fv(d->uModel, 1, GL_FALSE, dr.world.data());
            auto& mt = dr.b->mat;
            glUniform4f(d->uDiffuse, mt.diffuse.r, mt.diffuse.g, mt.diffuse.b, mt.diffuse.a);
            glUniform3f(d->uEmissive, mt.emissive.r, mt.emissive.g, mt.emissive.b);
            auto ti = d->texs.find(mt.texture);
            glUniform1f(d->uUseTex, ti != d->texs.end() ? 1.f : 0.f);
            if (ti != d->texs.end()) { glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, ti->second.id); }
            glBindBuffer(GL_ARRAY_BUFFER, dr.mesh->vbo);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, dr.mesh->ibo);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 32, (void*)0);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 32, (void*)12);
            glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 32, (void*)24);
            glDrawElements(GL_TRIANGLES, dr.b->count, GL_UNSIGNED_SHORT, (void*)(uintptr_t)(dr.b->first * 2));
        }
    };
    draw(noZ, false, false);   // sky etc. first, no depth
    draw(opaque, true, false);
    draw(blend, true, true);   // blended transparency, back to front
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glDisableVertexAttribArray(0); glDisableVertexAttribArray(1); glDisableVertexAttribArray(2);
}

bool Scene::readPixels(std::vector<std::uint8_t>& rgba, int& w, int& h) const {
    w = d->vw; h = d->vh;
    rgba.assign((size_t)w * h * 4, 0);
    std::vector<std::uint8_t> tmp(rgba.size());
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(d->vx, d->vy, w, h, GL_RGBA, GL_UNSIGNED_BYTE, tmp.data());
    for (int y = 0; y < h; y++) std::memcpy(&rgba[(size_t)y * w * 4], &tmp[(size_t)(h - 1 - y) * w * 4], (size_t)w * 4);
    return true;
}

// ---------------------------------------------------------------- picking
std::vector<PickHit> Scene::rayPick(Vec3 o, Vec3 dir, float maxDist) const {
    std::vector<PickHit> hits;
    dir = normalize(dir);
    for (auto& [fid, f] : d->frames) {
        if (f.visuals.empty() || !f.pickable || !d->effVisible(fid)) continue;
        Mat4 w = d->world(fid), inv = affineInverse(w);
        Vec3 lo = xformPoint(inv, o), ld = xformDir(inv, dir); // not renormalised: t is in world units
        for (MeshId mid : f.visuals) {
            const MeshData& m = d->meshes.at(mid).cpu;
            for (uint32_t i = 0; i < m.faces.size(); i++) {
                Vec3 a = m.positions[m.faces[i].v[0]], b = m.positions[m.faces[i].v[1]], c = m.positions[m.faces[i].v[2]];
                Vec3 e1 = b - a, e2 = c - a, p = cross(ld, e2);
                float det = dot(e1, p);
                if (std::fabs(det) < 1e-12f) continue;
                float id = 1.f / det;
                Vec3 t = lo - a;
                float u = dot(t, p) * id;
                if (u < 0 || u > 1) continue;
                Vec3 q = cross(t, e1);
                float v = dot(ld, q) * id;
                if (v < 0 || u + v > 1) continue;
                float dist = dot(e2, q) * id;
                if (dist < 0 || dist > maxDist) continue;
                PickHit h;
                h.frame = fid; h.mesh = mid; h.face = i; h.distance = dist;
                h.point = o + dir * dist;
                h.normal = normalize(xformDir(w, cross(b - a, c - a)));
                hits.push_back(h);
            }
        }
    }
    std::sort(hits.begin(), hits.end(), [](const PickHit& a, const PickHit& b) { return a.distance < b.distance; });
    return hits;
}
bool Scene::rayPickFirst(Vec3 o, Vec3 dir, PickHit& out, float maxDist) const {
    auto h = rayPick(o, dir, maxDist);
    if (h.empty()) return false;
    out = h[0];
    return true;
}
bool Scene::pickPixel(int x, int y, PickHit& out) const {
    if (!d->cam || d->vw <= 0 || d->vh <= 0) return false;
    float nx = (2.f * (x - d->vx) / d->vw - 1.f) * d->field;
    float ny = (1.f - 2.f * (y - d->vy) / d->vh) * d->field * d->vh / d->vw;
    Mat4 cw = d->world(d->cam);
    Vec3 dir = normalize(xformDir(cw, {nx, ny, 1.f}));
    return rayPickFirst({cw[12], cw[13], cw[14]}, dir, out, d->back);
}

} // namespace rb
