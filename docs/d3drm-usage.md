# D3DRM / DirectDraw surface used by Rollerbot.exe (render lane)

Source: static scan of the decompiled call sites (`(**(code **)(**(int **)G + OFF))(...)`) in
`decomp/rb/raw/_free.c`, ~700 vtable calls grouped by global object and offset. Addresses are the
`0x40xxxx` jump-thunk aliases first, with the implementation in parentheses where resolved.
Counts are call sites in the binary, not runtime frequencies. Confidence: **V** verified by argument
shape/values, **I** inferred from the interface vtable order, **?** unresolved.

The exe is 3D Rad 3.0: one engine, driven by project data. Game objects are records tagged by a
4-byte type id ("d3l.", "d30.", "d3b.", "d3a.", "d3v.", "d3f." ...); the type id at
0x4340f0 selects the frame behaviour. IID at 0x46e1a8 is IID_IDirect3DRM3 (4516ec83-...), so
all objects are the v3 flavour (Frame3, MeshBuilder3, Texture3, Face2, Light, Viewport (v1 methods), Wrap).
Immediate-mode D3D (device 0x5b4ce8, `0x5c` = SetRenderState x17 at 0x40100f/0x416660) and DDraw
(0x5b72b4/bc/c0: DirectDraw, primary, back surfaces) exist as plumbing only; the Retained Mode
device (0x4e0508) is created over them. There is no DirectDraw drawing the scene needs besides
presenting; DDraw Lock/Unlock/Blt (0x5b72bc/c0 +0x60/6c/64/80...) are the 2D HUD / blit path
(owned by the platform lane).

## Globals
| Global | Object | Notes |
|---|---|---|
| 0x47c97c | IDirect3DRM3 | created 0x40221b (0x4021b..) `Direct3DRMCreate` + QI(IID 0x46e1a8) |
| 0x4bf5a8 | Frame3 "scene" root | CreateFrame(NULL) |
| 0x4c4150 / 0x4c4154 | Frame3 | children of root; 0x4c4150 = camera-attached layer (HUD/sky), 0x4c4154 = background/sky anchor |
| 0x56d748 | Frame3 main camera | viewport camera frame |
| 0x4db908[i] | Frame3 per object | one per project object, child of root (or of 0x4c4150 when `0x57f14c`) |
| 0x4be5c4 | MeshBuilder3 scratch | procedural geometry (see below) |
| 0x4e4514 | Face2 scratch | AddVertex x3/4 + colour, added to 0x4be5c4 |
| 0x4b8980 | Wrap | CreateWrap, ApplyWrap target mesh (0x30 = Apply) |
| 0x56d540/0x56d544 | MeshBuilder3 | loaded meshes ("Load" via 0x2c with file name), cube-face/ground meshes |
| 0x56c930/0x56c934 | Frame3 | attach those meshes; on 0x4c4154 |
| 0x568930[i] | Texture3 | LoadTexture(name) results (per-file); 0x47c178 = temp |
| 0x47c980 + i*0x3c | D3DRMIMAGE | 60-byte structs, CreateTexture(image) for runtime-generated textures |
| 0x4e0508 | Device (v2/3) | CreateDeviceFromD3D(...) over the DDraw/D3D context |
| 0x4be5cc | Viewport | CreateViewport(dev, 0x56d748, 0,0,640,260) (0x280 x 0x104) |
| 0x4cbf34[i] | Viewport | one per camera object ("d30.") |
| 0x56d13c / 0x4c2f50 | Animation / ?? | CreateAnimation x2 (0x4340f0 per-object tick) |

## IDirect3DRM3 (0x47c97c)
| Off | Method | Sites | Args seen |
|---|---|---|---|
| 0x10 | CreateFrame(parent, &frame) | 24 | parent = NULL (root), 0x4bf5a8, 0x4c4150, 0x4c4154; per object into 0x4db908[i] |
| 0x18 | CreateMeshBuilder(&mb) | 8 | scratch 0x4be5c4; 0x56d540/4 |
| 0x1c | CreateFace(&face) | 8 | one per triangle of procedural meshes |
| 0x20 | CreateAnimation(&a) | 2 | 0x56d13c; only touched in 0x4340f0 (see Animation) |
| 0x28 | CreateTexture(D3DRMIMAGE*, &tex) | 2 | image i*0x3c+0x47c980 |
| 0x30 | CreateLightRGB(type, r,g,b, &light) | 2 | type 0 = AMBIENT, colour (1,1,1) (scaled later) |
| 0x34 | CreateMaterial(power, &mat) | 2 | power = global 0x57f1b0, 0.0 or 5.0 |
| 0x50 | CreateViewport(dev, camera, x,y,w,h, &vp) | 4 | main (0,0,640,260); per camera (fields from the d30 record) |
| 0x54 | CreateWrap(type, ref, ox,oy,oz, dx,dy,dz, ux,uy,uz, ou,ov, su,sv, &wrap) | 4 | 6 box-face loop (flat wrap, type 0) |
| 0x5c | LoadTexture(name, &tex) | 14 | file names from project data |
| 0x70 | SetDefaultTextureColors(0x40) | 1 | 0x45b850 |
| 0x74 | SetDefaultTextureShades(0x20) | 1 | 0x45b850 |
| 0x08 | Release | 1 | |
No CreateDevice*/Load/Tick/CreateShadow/CreateUserVisual in the scan: the device creation is
via IDirect3DRM3 slot not in this table (0x45b850 region) and **Tick() is not called**; the
engine calls Device::Update and Viewport::Render itself.

## Frame3 (0x4bf5a8, 0x4c4150, 0x4c4154, 0x56c93x, 0x56d748, 0x4db908[i])
| Off | Method | Args seen |
|---|---|---|
| 0x48 | AddVisual(meshbuilder) | (0x56d540/4 into 0x56c930/4) V |
| 0x30 | AddLight (root frame, ambient light 0x4bf5a4) | I |
| 0xbc, 0xd0 | scene fog colour/params (colour global 0x57f2b4) | I - fog **is used** (root, 0x4c4150, 0x4c4154) |
| 0xcc, 0xd4 | scene fog enable / mode (1, 0) | I |
| 0xec | SetOrientation(ref, dx,dy,dz, ux,uy,uz) | (0x4c4154, 0,0,1, 0,1,0) V |
| 0xf0 | SetPosition(ref, x,y,z) | (camera/root, 0,0,0) V |
| 0xf8 | SetSortMode(2|3) | 2 = front-to-back, 3 = back-to-front V |
| 0x104 | SetZbufferMode(2) | 2 = disable z write/test for sky meshes V |
| 0x13c | RayPick(ref, ray, flags, &pickedArray) | root (0x4bf5a8) and local frames; 4 sites V |
| 0x14c | SetTraversalOptions(0) | 4 sites, 0x4340f0: object frames of model-type objects V |
Transforms for object frames come out of fixed-point sin/cos (16.16, table 0x57efe4) and are
applied through SetOrientation/SetPosition on the 0x4db908[i] frames (0x4340f0 / 0x4339c0); the
exe imports `D3DRMQuaternionFromRotation` and `D3DRMQuaternionMultiply` (two calls) for object rotation.
World units = project integer coords / constant at 0x46d028 (float).

## MeshBuilder3 (0x4be5c4, 0x56d540/4)
2c Load(name,...) (x2, 5 args, flags 0 = default), 34 Scale(f,f,f), 40 GetBox (8), 44 GenerateNormals(0 crease, flags 3),
58 AddFace(face), 60 ReserveSpace(verts, normals, faces), 64 SetColorRGB (1,1,1), 6c SetTexture(tex[i]),
70 SetMaterial(mat), 78 SetQuality(0x40, 0x80 variants), 7c (1). Procedural meshes only add triangles.
Face2 (0x4e4514): 2c AddVertex(x,y,z) x3 per triangle (15 sites), 34 SetColorRGB, 38 SetColor(ARGB 0x80ffffff or
random 0x80rrggbb), 3c SetTexture, 40 SetTextureCoordinates(i,u,v) (20 sites, 3 per triangle), 48 SetTextureTopology(0,0).
Wrap (0x4b8980): 30 Apply(meshbuilder), applied per cube face to generate UVs.

## Texture3 (0x568930[i], 0x47c178, 0x4d2d08[i])
44 SetDecalSize(1,1) V, 48 SetDecalOrigin(w/2,h/2) V, 4c SetDecalScale(1) V, 50 SetDecalTransparency(bool) V (the
colour-key flag), 40 SetShades(0x20) V, 88 (tex,0,0x20) ? (v3 slot after SetCacheOptions family; value 0x20 matches shades),
60/ 6c Get/Init ? . No SetDecalTransparentColor seen: transparent colour is the default (black, palette index 0).
LoadTexture reads PCX/BMP via d3drm; here AssetStore decodes them.

## Light (global 0x4bf5a4 ambient)
CreateLightRGB(AMBIENT) via 0x30 above; Release only. No point/directional lights found in the scan,
so scene lighting is ambient only plus per-face colours. (Point/directional still implemented in Scene::Light.)

## Device (0x4e0508)
38 Update V (once per frame, after Viewport.Render), 44 SetBufferCount(2) V, 4c SetDither(0/1) V,
50 SetShades(0x20) V, 54 SetQuality(0x89) = GOURAUD|LIGHT_ON|SOLID? (I), 58 SetTextureQuality(0/1) V (0 nearest, 1 linear),
90 SetRenderMode(1 | 0x11) V (BLENDEDTRANSPARENCY, +VIEWDEPENDENTSPECULAR).

## Viewport (0x4be5cc, 0x4cbf34[i])
30 Clear(7 = ALL) V, 34 Render(rootFrame) V, 38 SetFront(0 / x) V, 3c SetBack(16.0 / x) V, 40 SetField(0.9375 / f) V.
Per camera viewports use SetField(k*scale/-zoom), SetFront(fVar7), SetBack(far).
Pick/hit-test: 4cbf34[i]+0x54 (Pick(x,y,&picked)), +0x64 ?, 0x13c RayPick on frames.

## Not used (so not rebuilt)
Shadows, CreateUserVisual, Mesh (non-builder), AnimationSet, progressive meshes, Load of .x via
D3DRM Load (meshes are loaded by name through MeshBuilder 0x2c; assets lane provides decoded data),
texture-wrap U/V topology, spot lights, MIP-maps.

## Rendering requirements derived
- Left-handed, Y-up, +Z into the screen; D3DRM positive rotation is clockwise looking down the axis.
  Triangles wind clockwise when facing the camera (front faces CW). Row-vector matrices (v*M).
- Viewport 640x260 main + per-camera sub viewports; Clear all (colour+depth).
- Colour-key texture transparency (palette index 0 / black) via SetDecalTransparency.
- Ambient light, per-face colours with alpha (0x80ffffff) and blended transparency render mode.
- Z-buffer on/off per frame (sky), sort modes front-to-back/back-to-front.
- Fog via scene frame fog params (linear).
- RayPick on frames for object picking/ground collision.
