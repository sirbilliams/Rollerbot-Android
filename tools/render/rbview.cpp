// rbview: object viewer for rb::Scene. Loads game meshes/textures through AssetStore.
//   rbview GAMEDIR (--desc D.dat | --mesh M.dat [--tex T.dat]) [--yaw deg] [--pitch deg]
//          [--dist k] [--wrap x|y|z] [--flip] [--size WxH] [--shot out.png] [--bg r,g,b]
// Without --shot opens an interactive window (drag = orbit, wheel = zoom, Esc quits).
// With --shot the camera is fully determined by the arguments (deterministic screenshots).
#include "assets/Assets.h"
#include "scene/Scene.h"
#include <SDL.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using namespace rb;

static std::uint32_t crcTable[256];
static std::uint32_t crc(const std::uint8_t* p, size_t n, std::uint32_t c = 0xffffffffu) {
    if (!crcTable[1])
        for (std::uint32_t i = 0; i < 256; i++) {
            std::uint32_t v = i;
            for (int k = 0; k < 8; k++) v = v & 1 ? 0xedb88320u ^ (v >> 1) : v >> 1;
            crcTable[i] = v;
        }
    for (size_t i = 0; i < n; i++) c = crcTable[(c ^ p[i]) & 0xff] ^ (c >> 8);
    return c;
}
static void be32(std::vector<std::uint8_t>& o, std::uint32_t v) {
    for (int s = 24; s >= 0; s -= 8) o.push_back((v >> s) & 255);
}
static void chunk(std::vector<std::uint8_t>& o, const char* t, const std::vector<std::uint8_t>& d) {
    be32(o, (std::uint32_t)d.size());
    std::vector<std::uint8_t> b(t, t + 4);
    b.insert(b.end(), d.begin(), d.end());
    o.insert(o.end(), b.begin(), b.end());
    be32(o, ~crc(b.data(), b.size()));
}
static bool writePng(const std::string& path, const std::vector<std::uint8_t>& rgba, int w, int h) {
    std::vector<std::uint8_t> raw;
    for (int y = 0; y < h; y++) {
        raw.push_back(0);
        raw.insert(raw.end(), rgba.begin() + (size_t)y * w * 4, rgba.begin() + (size_t)(y + 1) * w * 4);
    }
    std::vector<std::uint8_t> z = {0x78, 0x01};
    size_t pos = 0;
    std::uint32_t a = 1, b = 0;
    for (auto c : raw) { a = (a + c) % 65521; b = (b + a) % 65521; }
    while (pos < raw.size() || raw.empty()) {
        size_t n = std::min<size_t>(65535, raw.size() - pos);
        z.push_back(pos + n >= raw.size());
        z.push_back(n & 255); z.push_back(n >> 8); z.push_back(~n & 255); z.push_back((~n >> 8) & 255);
        z.insert(z.end(), raw.begin() + pos, raw.begin() + pos + n);
        pos += n;
        if (pos >= raw.size()) break;
    }
    be32(z, (b << 16) | a);
    std::vector<std::uint8_t> out = {0x89, 'P', 'N', 'G', 13, 10, 26, 10}, ihdr;
    be32(ihdr, w); be32(ihdr, h);
    ihdr.insert(ihdr.end(), {8, 6, 0, 0, 0});
    chunk(out, "IHDR", ihdr); chunk(out, "IDAT", z); chunk(out, "IEND", {});
    std::ofstream f(path, std::ios::binary);
    f.write((const char*)out.data(), out.size());
    return bool(f);
}

int main(int argc, char** argv) {
    if (argc < 2) { std::cerr << "usage: rbview GAMEDIR --desc D.dat|--mesh M.dat [--tex T.dat] [options]\n"; return 2; }
    std::string game = argv[1], desc, meshName, texName, shot, wrap = "z";
    float yaw = 30, pitch = 20, distK = 2.6f;
    bool flip = true, ckey = true; // 3DS is right-handed CCW; D3DRM wants clockwise fronts
    int W = 640, H = 480;
    Color bg{0.25f, 0.30f, 0.38f, 1};
    for (int i = 2; i < argc; i++) {
        std::string a = argv[i];
        auto next = [&]() { return i + 1 < argc ? std::string(argv[++i]) : std::string(); };
        if (a == "--desc") desc = next();
        else if (a == "--mesh") meshName = next();
        else if (a == "--tex") texName = next();
        else if (a == "--yaw") yaw = std::stof(next());
        else if (a == "--pitch") pitch = std::stof(next());
        else if (a == "--dist") distK = std::stof(next());
        else if (a == "--wrap") wrap = next();
        else if (a == "--noflip") flip = false;
        else if (a == "--nokey") ckey = false;
        else if (a == "--shot") shot = next();
        else if (a == "--size") std::sscanf(next().c_str(), "%dx%d", &W, &H);
        else if (a == "--bg") std::sscanf(next().c_str(), "%f,%f,%f", &bg.r, &bg.g, &bg.b);
    }
    try {
        AssetStore store;
        store.open(game);
        if (!desc.empty()) { // "1|./TEX|./MESH|..." descriptor
            auto b = store.read(desc);
            std::string t(b.begin(), b.end());
            std::vector<std::string> f;
            size_t s = 0;
            for (size_t p; (p = t.find('|', s)) != std::string::npos; s = p + 1) f.push_back(t.substr(s, p - s));
            if (f.size() < 3) throw std::runtime_error("bad descriptor");
            texName = f[1].substr(2);
            meshName = f[2].substr(2);
        }
        if (meshName.empty()) throw std::runtime_error("no mesh given");
        Mesh gm = Mesh::decode(store.read(meshName));
        MeshData md;
        md.materials.push_back({});
        for (auto& part : gm.parts) {
            uint32_t base = (uint32_t)md.positions.size();
            for (auto& v : part.vertices) md.positions.push_back({v[0], v[1], v[2]});
            for (auto& f : part.faces)
                for (size_t k = 2; k < f.size(); k++) {
                    MeshData::Face fc{{base + f[0], base + f[k - 1], base + f[k]}, 0};
                    if (flip) std::swap(fc.v[1], fc.v[2]);
                    md.faces.push_back(fc);
                }
        }
        if (md.positions.empty()) throw std::runtime_error("empty mesh");
        Vec3 mn = md.positions[0], mx = mn;
        for (auto& p : md.positions) {
            mn = {std::min(mn.x, p.x), std::min(mn.y, p.y), std::min(mn.z, p.z)};
            mx = {std::max(mx.x, p.x), std::max(mx.y, p.y), std::max(mx.z, p.z)};
        }
        Vec3 c = (mn + mx) * 0.5f, ext = mx - mn;
        float radius = 0.5f * std::sqrt(dot(ext, ext));
        if (radius < 1e-6f) radius = 1;
        // centre and normalise to a unit sphere so any mesh scale views the same way
        for (auto& p : md.positions) p = (p - c) * (1.0f / radius);
        md.generateNormals(1.0f); // ~57 degree crease
        Vec3 axis = wrap == "x" ? Vec3{1, 0, 0} : wrap == "y" ? Vec3{0, 1, 0} : Vec3{0, 0, 1};
        Vec3 up = wrap == "y" ? Vec3{0, 0, 1} : Vec3{0, 1, 0};
        md.applyFlatWrap({-0.7f, -0.7f, -0.7f}, axis, up, 0, 0, 0.7f, 0.7f);

        if (SDL_Init(SDL_INIT_VIDEO) != 0) throw std::runtime_error(SDL_GetError());
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
        SDL_Window* win = SDL_CreateWindow("rbview", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, W, H,
                                           SDL_WINDOW_OPENGL | (shot.empty() ? SDL_WINDOW_RESIZABLE : 0));
        if (!win) throw std::runtime_error(SDL_GetError());
        SDL_GLContext ctx = SDL_GL_CreateContext(win);
        if (!ctx) throw std::runtime_error(SDL_GetError());

        Scene sc;
        std::string err;
        if (!sc.init(&err)) throw std::runtime_error("scene init: " + err);
        if (!texName.empty()) {
            Image im = Image::decode(store.read(texName));
            md.materials[0].texture = sc.createTextureRGB((int)im.width, (int)im.height, im.rgb.data(), ckey);
            std::cout << "texture " << texName << " " << im.width << "x" << im.height << "\n";
        }
        MeshId mesh = sc.createMesh(md);
        if (!mesh) throw std::runtime_error("mesh upload failed");
        FrameId obj = sc.createFrame();
        sc.addVisual(obj, mesh);
        LightId amb = sc.createLight(LightType::Ambient, {0.45f, 0.45f, 0.45f, 1});
        LightId sun = sc.createLight(LightType::Directional, {0.75f, 0.75f, 0.75f, 1});
        FrameId lf = sc.createFrame();
        sc.setOrientation(lf, {0.4f, -0.6f, 0.7f}, {0, 1, 0}); // light travels along its +Z
        sc.addLight(sc.root(), amb);
        sc.addLight(lf, sun);
        FrameId cam = sc.createFrame();
        sc.setBackground(bg);
        sc.setViewport(0, 0, W, H);
        auto place = [&] {
            float y = yaw * 3.14159265f / 180, p = pitch * 3.14159265f / 180, d = distK;
            // orbit position on a sphere around the origin; +Y up, camera looks at the origin
            Vec3 pos{d * std::cos(p) * std::sin(y), d * std::sin(p), -d * std::cos(p) * std::cos(y)};
            sc.setPosition(cam, pos);
            sc.setOrientation(cam, pos * -1.0f, {0, 1, 0});
            sc.setCamera(cam, 0.9375f, 0.1f, 50.f);
        };
        auto frame = [&] {
            place();
            sc.render();
        };
        if (!shot.empty()) {
            frame();
            std::vector<std::uint8_t> px;
            int w, h;
            sc.readPixels(px, w, h);
            if (!writePng(shot, px, w, h)) throw std::runtime_error("cannot write " + shot);
            std::cout << "wrote " << shot << "\n";
            return 0;
        }
        bool run = true, drag = false;
        while (run) {
            SDL_Event e;
            while (SDL_PollEvent(&e)) {
                if (e.type == SDL_QUIT || (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)) run = false;
                if (e.type == SDL_MOUSEBUTTONDOWN) drag = true;
                if (e.type == SDL_MOUSEBUTTONUP) drag = false;
                if (e.type == SDL_MOUSEMOTION && drag) {
                    yaw += e.motion.xrel * 0.4f;
                    pitch = std::max(-89.f, std::min(89.f, pitch + e.motion.yrel * 0.4f));
                }
                if (e.type == SDL_MOUSEWHEEL) distK = std::max(1.2f, distK - e.wheel.y * 0.2f);
                if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                    W = e.window.data1; H = e.window.data2; sc.setViewport(0, 0, W, H);
                }
            }
            frame();
            SDL_GL_SwapWindow(win);
            SDL_Delay(16);
        }
    } catch (const std::exception& e) {
        std::cerr << "rbview: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
