// Headless Scene fixtures (needs a GL ES2 context: run under Xvfb, e.g. xvfb-run -a rb_scene_test).
#include "scene/Scene.h"
#include <SDL.h>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace rb;
static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::cerr << "FAIL " #c " line " << __LINE__ << "\n"; fails++; } } while (0)
static bool near(float a, float b, float e = 1e-4f) { return std::fabs(a - b) < e; }
static bool near(Vec3 a, Vec3 b, float e = 1e-4f) { return near(a.x, b.x, e) && near(a.y, b.y, e) && near(a.z, b.z, e); }

static MeshData quad(bool clockwise, Color c, TextureId tex = 0) { // unit quad at z=0, facing -Z camera
    MeshData m;
    m.positions = {{-1, -1, 0}, {-1, 1, 0}, {1, 1, 0}, {1, -1, 0}};
    m.uvs = {{0, 1}, {0, 0}, {1, 0}, {1, 1}};
    // viewed from -Z looking +Z (x right, y up): (-1,-1)->(-1,1)->(1,1) is clockwise
    if (clockwise) m.faces = {{{0, 1, 2}, 0}, {{0, 2, 3}, 0}};
    else m.faces = {{{0, 2, 1}, 0}, {{0, 3, 2}, 0}};
    m.materials.push_back({c, {0, 0, 0, 1}, tex});
    return m;
}
static void px(Scene& s, int x, int y, unsigned char out[4]) {
    std::vector<std::uint8_t> p; int w, h;
    s.readPixels(p, w, h);
    for (int i = 0; i < 4; i++) out[i] = p[((size_t)y * w + x) * 4 + i];
}

int main() {
    // ---- pure math
    Quat q = quatFromAxisAngle({0, 0, 1}, 1.5707963f);
    Mat4 m = matFromQuat(q);
    CHECK(near(Vec3{m[0], m[1], m[2]}, Vec3{0, 1, 0})); // +90 about Z takes +X to +Y
    Quat o = quatFromBasis({1, 0, 0}, {0, 1, 0});     // look along +X
    Mat4 mo = matFromQuat(o);
    CHECK(near(Vec3{mo[8], mo[9], mo[10]}, Vec3{1, 0, 0})); // local +Z -> dir
    CHECK(near(Vec3{mo[4], mo[5], mo[6]}, Vec3{0, 1, 0}));  // local +Y -> up
    CHECK(near(Vec3{mo[0], mo[1], mo[2]}, Vec3{0, 0, -1})); // x = y cross z (LH basis)
    Quat idq = quatFromBasis({0, 0, 1}, {0, 1, 0});
    CHECK(near(idq.w, 1.f) );

    if (SDL_Init(SDL_INIT_VIDEO) != 0) { std::cerr << "SDL: " << SDL_GetError() << "\n"; return 77; }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_Window* w = SDL_CreateWindow("t", 0, 0, 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    SDL_GLContext ctx = w ? SDL_GL_CreateContext(w) : nullptr;
    if (!ctx) { std::cerr << "no GLES2 context: " << SDL_GetError() << "\n"; return 77; }

    Scene s;
    std::string err;
    CHECK(s.init(&err));
    if (!err.empty()) std::cerr << err << "\n";
    s.setViewport(0, 0, 64, 64);
    s.setBackground({0, 0, 0, 1});
    FrameId cam = s.createFrame();
    s.setPosition(cam, {0, 0, -5});
    s.setCamera(cam, 1.0f, 0.1f, 50.f);
    LightId amb = s.createLight(LightType::Ambient, {1, 1, 1, 1});
    s.addLight(s.root(), amb);

    // hierarchy / transforms
    FrameId a = s.createFrame(), b = s.createFrame(a);
    s.setPosition(a, {1, 0, 0}); s.setRotation(a, quatFromAxisAngle({0, 1, 0}, 1.5707963f));
    s.setPosition(b, {0, 0, 2});
    CHECK(near(s.worldPosition(b), Vec3{3, 0, 0}, 1e-3f)); // +90 about Y takes +Z to +X
    s.destroyFrame(a);

    // winding: clockwise front face is drawn, counter-clockwise is culled
    FrameId f = s.createFrame();
    MeshId cw = s.createMesh(quad(true, {1, 0, 0, 1}));
    s.addVisual(f, cw);
    s.render();
    unsigned char p[4];
    px(s, 32, 32, p);
    CHECK(p[0] > 200 && p[1] < 10);
    s.removeVisual(f, cw);
    MeshId ccw = s.createMesh(quad(false, {1, 0, 0, 1}));
    s.addVisual(f, ccw);
    s.render();
    px(s, 32, 32, p);
    CHECK(p[0] < 10);
    s.removeVisual(f, ccw);

    // colour key: key pixels (0,0,0) are transparent, others show
    std::uint8_t tex[4 * 3] = {0, 0, 0, 0, 255, 0, 0, 255, 0, 0, 0, 0}; // 2x2: key,green / green,key
    TextureId t = s.createTextureRGB(2, 2, tex, true);
    MeshId tm = s.createMesh(quad(true, {1, 1, 1, 1}, t));
    s.addVisual(f, tm);
    s.render();
    unsigned char lo[4], hi[4];
    px(s, 28, 36, lo); // lower-left of quad: u<.5,v>.5 -> texel (0,1) green
    px(s, 28, 28, hi); // upper-left: texel (0,0) key -> background
    CHECK(lo[1] > 200);
    CHECK(hi[0] < 10 && hi[1] < 10);

    // picking
    PickHit h;
    CHECK(s.rayPickFirst({0, 0, -5}, {0, 0, 1}, h));
    CHECK(h.frame == f && near(h.distance, 5.f, 1e-3f));
    CHECK(near(h.normal, Vec3{0, 0, -1}));
    CHECK(!s.rayPickFirst({3, 0, -5}, {0, 0, 1}, h));
    CHECK(s.pickPixel(32, 32, h));

    // blending: half-transparent white over black gives grey
    s.removeVisual(f, tm);
    MeshId bl = s.createMesh(quad(true, {1, 1, 1, 0.5f}));
    s.addVisual(f, bl);
    s.render();
    px(s, 32, 32, p);
    CHECK(p[0] > 110 && p[0] < 145);

    std::cout << (fails ? "FAIL" : "PASS") << " rb_scene\n";
    return fails ? 1 : 0;
}
