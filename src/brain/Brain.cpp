// Brain compiler (to a small tree-walking program) and VM. See docs/brain.md.
#include "brain/Brain.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iterator>
#include <sstream>
#include <iomanip>
#include <limits>
#include <stdexcept>
#include <unordered_map>

namespace rb {
namespace {

std::int32_t wrap(std::int64_t v) { return static_cast<std::int32_t>(static_cast<std::uint32_t>(static_cast<std::uint64_t>(v))); }

// 0x44c360: angle normalisation. Note the exe's modulus is 0x2cfff, not 0x2d000.
std::int32_t normAngle(std::int32_t a) {
    if (a < 0) { std::int32_t m = a % 0x2cfff; return m + 0x2cfff; }
    if (a > 0x2cfff) return a % 0x2cfff;
    return a;
}

// 0x45ea10: normalize to an 8-bit index into round(16*sqrt(i)), then restore the exponent.
// Generate the mathematical table value instead of embedding executable bytes.
std::int32_t coarseSqrt(std::uint64_t square) {
    if (square > 0xffffffffu) return 0x7fffffff;
    if (!square) return 0;
    int exponent=0;
    while (square>255) { square >>= 2; ++exponent; }
    while (square<64) { square <<= 2; --exponent; }
    auto mantissa=std::uint32_t(std::lround(16*std::sqrt(double(square))));
    return exponent>=4 ? std::int32_t(mantissa << (exponent-4)) : std::int32_t(mantissa >> (4-exponent));
}

std::string lower(std::string s) { for (auto& c : s) c = char(std::tolower((unsigned char)c)); return s; }

// ---------------------------------------------------------------- lexer
enum class T { Num, Id, Op, End };
struct Tok { T t; std::string s; std::int32_t num = 0; };

bool isDecl(const std::string& k) { return k == "var" || k.rfind("user", 0) == 0; }

std::vector<Tok> lex(const std::string& line, std::string* err) {
    std::vector<Tok> out;
    size_t i = 0, n = line.size();
    while (i < n) {
        char c = line[i];
        if (c == ';') break;
        if (std::isspace((unsigned char)c)) { ++i; continue; }
        if (std::isdigit((unsigned char)c) || (c == '.' && i + 1 < n && std::isdigit((unsigned char)line[i + 1]))) {
            size_t j = i;
            while (j < n && (std::isdigit((unsigned char)line[j]) || line[j] == '.')) ++j;
            size_t k = j;
            while (k < n && std::isalpha((unsigned char)line[k])) ++k;
            std::string num = line.substr(i, j - i), unit = lower(line.substr(j, k - j));
            if (unit != "mt" && unit != "dg" && unit != "ms" && unit != "rm") {
                unit.clear();
                // identifiers may start with a digit (e.g. 3dbg_color_set, 0x4720..): word = digits + letters/_/digits
                size_t w = i;
                while (w < n && (std::isalnum((unsigned char)line[w]) || line[w] == '_')) ++w;
                bool allDigits = true, hasAlpha = false;
                for (size_t q = i; q < w; ++q) { if (!std::isdigit((unsigned char)line[q])) allDigits = false; if (std::isalpha((unsigned char)line[q]) || line[q] == '_') hasAlpha = true; }
                if (!allDigits && hasAlpha && j == i + (num.size()) && num.find('.') == std::string::npos) {
                    out.push_back({T::Id, line.substr(i, w - i)}); i = w; continue;
                }
            } else j = k;
            // thousandths, as the exe's number reader (0x401ded) hands to the unit converters
            double v = std::atof(num.c_str());
            std::int64_t th = std::int64_t(std::floor(v * 1000.0 + 1e-6));
            std::int64_t r;
            if (unit.empty()) r = (num.find('.') == std::string::npos) ? std::atoll(num.c_str()) : std::int64_t(v);
            else if (unit == "mt") r = th * 512;            // 0x426ac0: thousandths << 9
            else if (unit == "dg") r = th * 0x200 / 1000;   // 0x426b10
            else if (unit == "ms") r = th * 0x42ab / 1000;  // 0x426b80
            else r = th * 0x400 / 10000;                    // rm: 0x426bf0
            out.push_back({T::Num, num, wrap(r)});
            i = j;
            continue;
        }
        if (std::isalpha((unsigned char)c) || c == '_') {
            size_t j = i;
            while (j < n && (std::isalnum((unsigned char)line[j]) || line[j] == '_')) ++j;
            out.push_back({T::Id, line.substr(i, j - i)});
            i = j;
            continue;
        }
        static const char* two[] = {"=<", "=>", "<>", "<=", ">=", "!="};
        bool done = false;
        for (auto t : two) if (line.compare(i, 2, t) == 0) { out.push_back({T::Op, t}); i += 2; done = true; break; }
        if (done) continue;
        out.push_back({T::Op, std::string(1, c)});
        ++i;
    }
    return out;
}

// ---------------------------------------------------------------- AST
struct Expr {
    enum K { Num, Var, Call, Bin, Neg, Elem } k = Num;
    std::int32_t num = 0;
    int slot = -1;
    std::string fn, op;
    std::vector<std::unique_ptr<Expr>> a;
    std::int32_t elemOff = 0;
};
using ExprP = std::unique_ptr<Expr>;

struct Stmt {
    enum K { Assign, Call, If, Else, Endif } k = Assign;
    int slot = -1;
    ExprP e;
    int jump = -1; // If: index of matching Else/Endif ; Else: index of Endif
    int line = 0;
};

struct Parser {
    std::vector<Tok> t;
    size_t p = 0;
    std::map<std::string, int>* vars;
    std::string err;
    const Tok& cur() const { static Tok end{T::End, ""}; return p < t.size() ? t[p] : end; }
    bool isOp(const char* s) const { return cur().t == T::Op && cur().s == s; }
    bool isId(const char* s) const { return cur().t == T::Id && lower(cur().s) == s; }

    ExprP parseOr() { return parsePriority(0); }
    ExprP parsePriority(int level) {
        // 0x414a.. / 0x415590: / then * then - then +, followed by comparisons and bit operators.
        static const std::vector<std::vector<std::string>> groups = {
            {"xor"}, {"or"}, {"and"}, {">", ">=", "=>"}, {"<", "<=", "=<"},
            {"<>", "!="}, {"="}, {"+"}, {"-"}, {"*"}, {"/"}
        };
        if (level == int(groups.size())) return parseUnary();
        auto l=parsePriority(level+1);
        while (err.empty()) {
            std::string op=lower(cur().s);
            const auto& g=groups[level];
            if (std::find(g.begin(),g.end(),op)==g.end()) break;
            ++p; auto r=parsePriority(level+1); l=bin(op,std::move(l),std::move(r));
        }
        // 0x414e30 / 0x415ac0 replace the first call with @0: trailing 0 gives @00 (slot zero).
        // Preserve that retail spelling; other suffixes can alias arbitrary compiler temporaries.
        if (level==0 && cur().t==T::Num && cur().s=="0") ++p;
        return l;
    }
    ExprP parseUnary() {
        if (isOp("-")) { ++p; auto e = std::make_unique<Expr>(); e->k = Expr::Neg; e->a.push_back(parseUnary()); return e; }
        if (isOp("+")) { ++p; return parseUnary(); }
        return parsePrimary();
    }
    static ExprP bin(const std::string& op, ExprP l, ExprP r) {
        auto e = std::make_unique<Expr>(); e->k = Expr::Bin; e->op = op;
        e->a.push_back(std::move(l)); e->a.push_back(std::move(r)); return e;
    }
    ExprP parsePrimary() {
        auto e = std::make_unique<Expr>();
        const Tok tk = cur();
        if (tk.t == T::Num) { ++p; e->k = Expr::Num; e->num = tk.num; return e; }
        if (tk.t == T::Op && (tk.s == "[" || tk.s == "(")) {
            const char* close = tk.s == "[" ? "]" : ")";
            ++p; auto in = parseOr();
            if (!isOp(close)) { err = std::string("expected ") + close; return in; }
            ++p; return in;
        }
        if (tk.t == T::Id) {
            ++p;
            std::string name = lower(tk.s);
            if (name == "element") {
                e->k = Expr::Elem;
                if (isOp("#")) {
                    ++p; bool neg = false;
                    if (isOp("-")) { neg = true; ++p; }
                    if (cur().t != T::Num) { err = "bad element#"; return e; }
                    e->elemOff = (neg ? -1 : 1) * std::atoi(cur().s.c_str()); ++p;
                }
                return e;
            }
            if (isOp("(")) {
                ++p; e->k = Expr::Call; e->fn = name;
                if (!isOp(")")) {
                    for (;;) {
                        e->a.push_back(parseOr());
                        if (!err.empty()) return e;
                        if (isOp(",")) { ++p; continue; }
                        break;
                    }
                }
                if (!isOp(")")) { err = "expected )"; return e; }
                ++p; return e;
            }
            auto it = vars->find(name);
            if (it == vars->end()) { err = "Unknown symbol '" + tk.s + "'"; return e; }
            e->k = Expr::Var; e->slot = it->second; return e;
        }
        err = "Variable (or value, or function) expected";
        return e;
    }
};

} // namespace

class Brain {
public:
    std::vector<Stmt> code;
    std::vector<std::string> varNames;
    std::vector<std::int32_t> init;
    std::vector<std::string> errors;
};

namespace {
struct Ctx { BrainVM* vm; int self; };
std::int32_t evalExpr(const Expr& e, Ctx& c, std::vector<std::int32_t>& vars);

std::int32_t refOf(int idx) { return wrap(std::int64_t(kElementRefBase) + std::int64_t(idx) * kElementStride); }

std::int32_t evalExpr(const Expr& e, Ctx& c, std::vector<std::int32_t>& vars) {
    switch (e.k) {
    case Expr::Num: return e.num;
    case Expr::Var: return vars[e.slot];
    case Expr::Elem: return refOf(c.self + e.elemOff);
    case Expr::Neg: return wrap(-std::int64_t(evalExpr(*e.a[0], c, vars)));
    case Expr::Call: {
        std::vector<std::int32_t> a;
        for (auto& x : e.a) a.push_back(evalExpr(*x, c, vars));
        return c.vm->callCommand(e.fn, a, c.self);
    }
    case Expr::Bin: {
        std::int64_t l = evalExpr(*e.a[0], c, vars), r = evalExpr(*e.a[1], c, vars);
        const std::string& o = e.op;
        if (o == "+") return wrap(l + r);
        if (o == "-") return wrap(l - r);
        if (o == "*") return wrap(l * r);
        if (o == "/") return r == 0 ? 0 : wrap(l / r);
        if (o == "=") return l == r ? -1 : 0;
        if (o == "<>" || o == "!=") return l != r ? -1 : 0;
        if (o == "<") return l < r ? -1 : 0;
        if (o == ">") return l > r ? -1 : 0;
        if (o == "=<" || o == "<=") return l <= r ? -1 : 0;
        if (o == "=>" || o == ">=") return l >= r ? -1 : 0;
        if (o == "and") return wrap(l & r);
        if (o == "or") return wrap(l | r);
        if (o == "xor") return wrap(l ^ r);
        return 0;
    }
    }
    return 0;
}
} // namespace

std::int32_t parseUnitNumber(const std::string& tok, bool* ok) {
    std::string err; auto t = lex(tok, &err);
    if (ok) *ok = t.size() == 1 && t[0].t == T::Num;
    return t.empty() ? 0 : t[0].num;
}

std::shared_ptr<Brain> compileBrain(const std::vector<std::string>& lines, std::vector<std::string>* errors) {
    auto b = std::make_shared<Brain>();
    std::map<std::string, int> vars;
    auto report = [&](int ln, const std::string& m) {
        std::string s = "line " + std::to_string(ln) + ": " + m;
        b->errors.push_back(s); if (errors) errors->push_back(s);
    };
    // pass 1: declare variables (var x=expr; initialiser is a constant expression)
    struct Pend { std::string name; std::vector<Tok> toks; int ln; };
    std::vector<Pend> decls;
    for (size_t i = 0; i < lines.size(); ++i) {
        std::string err; auto t = lex(lines[i], &err);
        if (t.size() >= 2 && t[0].t == T::Id && isDecl(lower(t[0].s)) && t[1].t == T::Id) {
            std::string nm = lower(t[1].s);
            if (!vars.count(nm)) { int s = int(vars.size()); vars[nm] = s; b->varNames.push_back(nm); b->init.push_back(0); }
            decls.push_back({nm, std::vector<Tok>(t.begin() + 2, t.end()), int(i + 1)});
        }
    }
    {
        BrainVM* none = nullptr; (void)none;
        for (auto& d : decls) {
            if (d.toks.size() >= 1 && d.toks[0].t == T::Op && d.toks[0].s == "=") {
                Parser ps; ps.t.assign(d.toks.begin() + 1, d.toks.end()); ps.vars = &vars;
                auto ex = ps.parseOr();
                if (!ps.err.empty()) { report(d.ln, ps.err); continue; }
                // constant initialisers only reference numbers/vars of earlier decls
                Ctx c{nullptr, 0};
                std::vector<std::int32_t> tmp = b->init;
                try { b->init[vars[d.name]] = evalExpr(*ex, c, tmp); } catch (...) {}
            }
        }
    }
    // pass 2: statements
    std::vector<int> stack;
    for (size_t i = 0; i < lines.size(); ++i) {
        std::string err; auto t = lex(lines[i], &err);
        if (t.empty()) continue;
        int ln = int(i + 1);
        Parser ps; ps.t = t; ps.vars = &vars;
        std::string kw = t[0].t == T::Id ? lower(t[0].s) : "";
        Stmt s; s.line = ln;
        if (isDecl(kw) && t.size() >= 2 && t[1].t == T::Id) continue;
        if (kw == "if") {
            ps.p = 1; s.k = Stmt::If; s.e = ps.parseOr();
            if (!ps.err.empty() || ps.cur().t != T::End) { report(ln, ps.err.empty() ? "junk after condition" : ps.err); continue; }
            stack.push_back(int(b->code.size()));
            b->code.push_back(std::move(s));
        } else if (kw == "else") {
            if (stack.empty()) { report(ln, "If...endif nesting error!"); continue; }
            s.k = Stmt::Else; b->code[stack.back()].jump = int(b->code.size());
            stack.back() = int(b->code.size()); b->code.push_back(std::move(s));
        } else if (kw == "endif") {
            if (stack.empty()) { report(ln, "If...endif nesting error!"); continue; }
            s.k = Stmt::Endif; b->code[stack.back()].jump = int(b->code.size()); stack.pop_back();
            b->code.push_back(std::move(s));
        } else if (t[0].t == T::Id && t.size() >= 2 && t[1].t == T::Op && t[1].s == "=") {
            auto it = vars.find(kw);
            if (it == vars.end()) { report(ln, "Unknown symbol '" + t[0].s + "'"); continue; }
            ps.p = 2; s.k = Stmt::Assign; s.slot = it->second; s.e = ps.parseOr();
            if (!ps.err.empty() || ps.cur().t != T::End) { report(ln, ps.err.empty() ? "junk after expression" : ps.err); continue; }
            b->code.push_back(std::move(s));
        } else if (t[0].t == T::Id && vars.count(kw) && t.size() >= 2 &&
                   t[1].t == T::Op && (t[1].s == "=>" || t[1].s == "=<")) {
            // Retail level004 has bare comparisons where assignments were likely intended.
            // Compatibility policy: evaluate and discard, preserving comparison semantics.
            // 0x4125f0 / 0x414880 handle statements/expressions; exact malformed-source
            // behaviour in the original compiler remains unverified. Never infer a clamp.
            s.k = Stmt::Call; s.e = ps.parseOr();
            if (!ps.err.empty() || ps.cur().t != T::End) { report(ln, ps.err.empty() ? "junk after comparison" : ps.err); continue; }
            b->code.push_back(std::move(s));
        } else if (t[0].t == T::Id && t.size() >= 2 && t[1].t == T::Op && t[1].s == "(") {
            ps.p = 0; s.k = Stmt::Call; s.e = ps.parseOr();
            if (!ps.err.empty() || ps.cur().t != T::End) { report(ln, ps.err.empty() ? "junk after call" : ps.err); continue; }
            b->code.push_back(std::move(s));
        } else {
            report(ln, "Unknown function/instruction '" + (t[0].s) + "'");
        }
    }
    if (!stack.empty()) report(int(lines.size()), "If...endif nesting error!");
    return b;
}

// ---------------------------------------------------------------- VM
struct BrainVM::Impl {
    std::vector<std::int32_t> atan;
    std::map<std::string, Mesh> meshes;
    std::map<std::string, std::int32_t> radii;
    std::map<std::string, std::array<std::int32_t,6>> bounds;
};
BrainVM::BrainVM() : impl_(new Impl), scene_(nullptr), audio_(nullptr), input_(nullptr) {}
BrainVM::~BrainVM() = default;

void BrainVM::setServices(IScene* s, IAudio* a, IInput* i) { scene_ = s; audio_ = a; input_ = i; }

namespace {
std::vector<std::uint8_t> slurp(const std::filesystem::path& p) {
    std::ifstream f(p, std::ios::binary);
    return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(f), {});
}
std::filesystem::path findCI(const std::filesystem::path& dir, const std::string& name) {
    std::error_code ec;
    for (auto& e : std::filesystem::directory_iterator(dir, ec))
        if (lower(e.path().filename().string()) == lower(name)) return e.path();
    return {};
}
std::int32_t fieldNum(const ObjectRecord& o, size_t i) {
    if (i >= o.fields.size()) return 0;
    return std::int32_t(std::atof(o.fields[i].c_str()));
}
} // namespace

void BrainVM::load(const ProjectData& pd, const std::filesystem::path& gameDir) {
    static MockScene ms; static MockAudio ma; static MockInput mi;
    if (!scene_) scene_ = &ms;
    if (!audio_) audio_ = &ma;
    if (!input_) input_ = &mi;
    // tables the interpreter indexes (0x57efe0 sinus.bin, 0x57efe4 sinusbig.bin, random.bin)
    auto sb = slurp(findCI(gameDir, "sinus.bin"));
    sinus_.assign(sb.begin(), sb.end());
    auto big = slurp(findCI(gameDir, "sinusbig.bin"));
    sinusBig_.resize(big.size() / 4);
    if (!big.empty()) std::memcpy(sinusBig_.data(), big.data(), sinusBig_.size() * 4);
    randomBytes_ = slurp(findCI(gameDir, "random.bin"));
    auto atanBytes=slurp(findCI(gameDir,"atan.bin"));
    impl_->atan.resize(atanBytes.size()/4);
    if(!atanBytes.empty()) std::memcpy(impl_->atan.data(),atanBytes.data(),impl_->atan.size()*4);
    const auto carriedGlobals = world_.globals;
    const auto carriedRandom = world_.randomPtr;
    const auto carriedUpdates = world_.updateCount;
    impl_->meshes.clear(); impl_->radii.clear(); impl_->bounds.clear();
    // globals start from globvars.dat (restored at load by 0x442220); the file is wrapped
    world_ = World{};
    try {
        AssetStore st; st.open(gameDir);
        auto gv = st.read("globvars.dat");
        for (size_t i = 0; i + 4 <= gv.size() && i / 4 < kMaxGlobals; i += 4) std::memcpy(&world_.globals[i / 4], &gv[i], 4);
    } catch (const std::exception&) {}
    if (loaded_) { world_.globals = carriedGlobals; world_.randomPtr = carriedRandom; world_.updateCount = carriedUpdates; }
    loaded_ = true;
    programs_.clear(); compileErrors_.clear(); events_.clear(); unknown_.clear(); unknownCounts_.clear(); collisionMap_.clear(); hitColors_.clear();
    std::map<std::uint32_t, const BrainRecord*> bm;
    for (auto& b : pd.brains) bm[b.objectIndex] = &b;
    for (size_t i = 0; i < pd.objects.size(); ++i) {
        const auto& o = pd.objects[i];
        Element e;
        e.index = std::uint32_t(i);
        e.name = o.name; e.model = o.model; e.sound = o.sound; e.type = o.type;
        // 2D overlay elements (types 5 sprite, 8 counter) live in screen pixels: brains write raw pixel values
        // (position_y_set(element,-50), position_y(element)=>199), so no metre scaling for them.
        const double posScale = (o.type == 5 || o.type == 8) ? 1.0 : double(kMt);
        e.pos = {wrap(std::llround(o.position[0] * posScale)), wrap(std::llround(o.position[1] * posScale)), wrap(std::llround(o.position[2] * posScale))};
        auto deg = [&](size_t f) { return o.fields.size() > f ? wrap(std::llround(std::atof(o.fields[f].c_str()) * kDg)) : 0; };
        e.rot = {deg(10), deg(11), deg(12)}; // fields 10..12 are rotation in degrees (guess from value ranges)
        e.id = std::uint32_t(fieldNum(o, 30));
        std::uint32_t f32 = std::uint32_t(fieldNum(o, 32)), f33 = std::uint32_t(fieldNum(o, 33));
        e.flags = (f32 & 1) | (f33 ? kFlagPaused : 0) | kFlagVisible | kFlagCollide; // 0x442... loader enables visibility and collisions
        e.credits = 0;
        auto sc = [&](size_t f) { auto v = fieldNum(o, f); return v <= 1 ? 1000 : v; };
        e.scale[0] = sc(41); e.scale[1] = sc(42); e.scale[2] = sc(43);
        // 0x44cd00: radius is mean half-extent, unless project field 31 supplies one.
        if (!impl_->meshes.count(e.model)) {
            Mesh mesh; std::int32_t radius = 0;
            try {
                AssetStore st; st.open(gameDir); auto bytes = st.read(e.model);
                if (assetKind(bytes) == "descriptor") {
                    std::stringstream ss(std::string(bytes.begin(), bytes.end()));
                    std::vector<std::string> f; std::string v;
                    while (std::getline(ss, v, '|')) f.push_back(v);

                    if (f.size() > 1) bytes = st.read(f[1]);
                }
                mesh = Mesh::decode(bytes);
                // d3m vertices are integer millimetres, unlike visual meshes (metres).
                if (assetKind(bytes) == "d3m")
                    for (auto& part : mesh.parts) for (auto& v : part.vertices) for (auto& c : v) c /= 1000;
                if (!radius) {
                    float lo[3] = {0,0,0}, hi[3] = {0,0,0};
                    for (auto& part : mesh.parts) for (auto& v : part.vertices) for (int k=0;k<3;++k) {
                        lo[k] = std::min(lo[k],v[k]); hi[k] = std::max(hi[k],v[k]);
                    }
                    radius = std::int32_t(((hi[0]-lo[0])+(hi[1]-lo[1])+(hi[2]-lo[2]))*1000/6);
                }
            } catch (const std::exception&) {}
            std::array<std::int32_t,6> bounds{};
            for (auto& part : mesh.parts) for (auto& v : part.vertices) for(int k=0;k<3;++k) {
                auto mm=std::int32_t(v[k]*1000); bounds[k]=std::min(bounds[k],mm); bounds[k+3]=std::max(bounds[k+3],mm);
            }
            impl_->bounds[e.model]=bounds;
            impl_->meshes[e.model] = std::move(mesh); impl_->radii[e.model] = radius;
        }
        e.radius = impl_->radii[e.model];
        if (!e.radius) e.radius = scene_->modelRadius(e.model);
        if (std::atof(o.fields[31].c_str()) != 0) e.radius = std::int32_t(std::abs(std::atof(o.fields[31].c_str()))*1000);
        e.credits = fieldNum(o,28);
        e.vel = {wrap(std::llround(std::atof(o.fields[16].c_str())*kMt)), wrap(std::llround(std::atof(o.fields[17].c_str())*kMt)), wrap(std::llround(std::atof(o.fields[18].c_str())*kMt))};
        e.spin = {deg(22),deg(23),deg(24)};
        auto bounds=impl_->bounds[e.model];
        e.boxRadius=std::int32_t(std::sqrt(double(std::max(std::abs(bounds[0]),std::abs(bounds[3])))*std::max(std::abs(bounds[0]),std::abs(bounds[3])) + double(std::max(std::abs(bounds[2]),std::abs(bounds[5])))*std::max(std::abs(bounds[2]),std::abs(bounds[5]))));
        if (!e.boxRadius) e.boxRadius=e.radius;
        // 0x442... project loader selects the last d3m ground, including paused scenery.
        if (e.type == 12) world_.groundElement = int(i);
        auto it = bm.find(std::uint32_t(i));
        if (it != bm.end()) {
            std::vector<std::string> errs;
            auto prog = compileBrain(it->second->lines, &errs);
            for (auto& m : errs) compileErrors_.push_back("e" + std::to_string(i) + " " + o.name + " " + m);
            e.brain = int(programs_.size());
            e.vars = prog->init;
            // Field 34 is the editor's variable-memory snapshot (0x4125f0): "count/type/value/slot/..." in declaration
            // order; type 7 = var, 2 = userInt (raw), 3 = userMet (metres). Only the user values set in the editor
            // override the source defaults (e.g. the Score Counter's points per collision colour); plain vars do not.
            if (o.fields.size() > 34) {
                std::vector<std::string> tok; std::string cur;
                for (char ch : o.fields[34]) { if (ch == '/') { tok.push_back(cur); cur.clear(); } else cur += ch; }
                tok.push_back(cur);
                size_t n = tok.empty() ? 0 : size_t(std::atoi(tok[0].c_str()));
                for (size_t k = 0; k < n && 1 + k * 3 + 2 < tok.size() && k < e.vars.size(); ++k) {
                    int type = std::atoi(tok[1 + k * 3].c_str());
                    if (type == 7) continue;
                    std::int64_t val = std::atoll(tok[2 + k * 3].c_str());
                    e.vars[k] = wrap(type == 3 ? val * kMt : val);
                }
            }
            programs_.push_back(prog);
        }
        world_.elements.push_back(std::move(e));
    }
    // 0x44cd00: byte at row b, column a represents pair (a,b).
    if (pd.collection.size() >= 0x10000/4) {
        auto byte = [&](size_t off) { return (std::uint32_t(pd.collection[off/4]) >> (8*(off%4))) & 255; };
        for (size_t b=0;b<world_.elements.size();++b) for (size_t a=b+1;a<world_.elements.size();++a) {
            unsigned color=byte(b*256+a);
            if (color) collisionMap_[{int(b),int(a)}] = int(color);
        }
    }
    accum_ = 0;
}

std::string BrainVM::projectForExit(int code) {
    if (code <= 0 || code > 999) return {};
    std::ostringstream s; s << "level" << std::setfill('0') << std::setw(3) << code << ".prj";
    return s.str(); // 0x4417e0; multilev.ini rows select loading images, not project names.
}
LevelSelection BrainVM::selectLevel(const std::filesystem::path& gameDir,int code) {
    LevelSelection level; level.project=projectForExit(code);
    if(level.project.empty()) return level;
    std::ifstream file(findCI(gameDir,"multilev.ini")); std::string row;
    for(int i=0;i<code && std::getline(file,row);++i) {
        if(i==code-1) { std::stringstream ss(row); std::getline(ss,level.loadingScreen,'|'); std::getline(ss,level.background,'|'); }
    }
    return level;
}
bool BrainVM::transition(const std::filesystem::path& gameDir) {
    auto name = selectLevel(gameDir,world_.exitCode).project;
    if (name.empty()) return false;
    AssetStore st; st.open(gameDir);
    load(ProjectData::load(st,name),gameDir);
    return true;
}

std::int32_t BrainVM::groundHeight(int index) {
    if (world_.groundElement < 0) return scene_ ? scene_->groundLevel(world_,index) : 0;
    const auto& g = world_.elements[world_.groundElement];
    const auto& e = world_.elements[index];
    auto it = impl_->meshes.find(g.model);
    if (it == impl_->meshes.end() || it->second.parts.empty()) return scene_ ? scene_->groundLevel(world_,index) : 0;
    // 0x40189d: XZ triangle inclusion and plane interpolation. d3m coordinates are world millimetres.
    const double x = double(e.pos.x)/kMt, z = double(e.pos.z)/kMt;
    double best = 0, single = 0, distance = std::numeric_limits<double>::infinity();
    int hits=0; const double top=double(e.pos.y)+double(e.radius)*512;
    for (const auto& part : it->second.parts) for (const auto& f : part.faces) for (size_t k=2;k<f.size();++k) {
        const auto &a=part.vertices[f[0]], &b=part.vertices[f[k-1]], &c=part.vertices[f[k]];
        double det=(b[2]-c[2])*(a[0]-c[0])+(c[0]-b[0])*(a[2]-c[2]);
        if (std::abs(det)<1e-12) continue;
        double u=((b[2]-c[2])*(x-c[0])+(c[0]-b[0])*(z-c[2]))/det;
        double v=((c[2]-a[2])*(x-c[0])+(a[0]-c[0])*(z-c[2]))/det;
        if (u < -1e-8 || v < -1e-8 || u+v > 1+1e-8) continue;
        double y=(u*a[1]+v*b[1]+(1-u-v)*c[1])*kMt;
        single=y; ++hits;
        if(y>=top) continue;
        double d=y>e.pos.y ? y-e.pos.y : top-y;
        if (d<distance) { distance=d; best=y; }
    }
    return wrap(std::int64_t((hits==1 ? single : best)/512)*512);
}

int BrainVM::advance(double dt) {
    accum_ += dt;
    int n = 0;
    const double step = 1.0 / kTicksPerSecond;
    while (accum_ >= step) { accum_ -= step; tick(); ++n; }
    return n;
}

std::int32_t BrainVM::sinBig(std::int32_t idx) const {
    return (idx >= 0 && size_t(idx) < sinusBig_.size()) ? sinusBig_[idx] : 0;
}
std::int32_t BrainVM::sinB(std::int32_t a) const { return std::int32_t(std::lround(std::sin(double(normAngle(a)) / 29335.0) * 512)); }
std::int32_t BrainVM::cosB(std::int32_t a) const { return std::int32_t(std::lround(std::cos(double(normAngle(a)) / 29335.0) * 512)); }

std::uint32_t BrainVM::nextRandom() {
    // 0x45f540: pointer into random.bin advances one byte per call, wraps after 1997 bytes.
    // 0x57f000 counts shared updates, incremented before brains by 0x45f7b0.
    if (randomBytes_.size() < 2000) return world_.tickCount * 2654435761u;
    if (world_.randomPtr >= 1997) world_.randomPtr = 0;
    ++world_.randomPtr;
    std::uint32_t d; std::memcpy(&d, &randomBytes_[world_.randomPtr], 4);
    std::uint32_t c = world_.updateCount;
    return (c + (c << 16) + d) >> 1;
}

void BrainVM::runCollisions() {
    // 0x44cad0: reset hit state of every registered pair, then test each registered pair.
    auto& els = world_.elements;
    for (auto& kv : collisionMap_) {
        if (kv.first.first < int(els.size())) els[kv.first.first].collided = 0;
        if (kv.first.second < int(els.size())) els[kv.first.second].collided = 0;
    }
    hitColors_.clear();
    for (auto& kv : collisionMap_) {
        int ai = kv.first.first, bi = kv.first.second;
        if (ai >= int(els.size()) || bi >= int(els.size())) continue;
        Element& a = els[ai]; Element& b = els[bi];
        if ((a.flags & 0x81) != 0x81 || (b.flags & 0x81) != 0x81) continue;
        bool boxes=(a.flags & b.flags & kFlagSphere2)!=0;
        std::int64_t rr = boxes ? std::int64_t(a.boxRadius)+b.boxRadius : std::int64_t(a.radius) + b.radius;
        std::int64_t dx = std::int64_t(wrap(std::int64_t(a.pos.x) - b.pos.x)) >> 9, dy = std::int64_t(wrap(std::int64_t(a.pos.y) - b.pos.y)) >> 9, dz = std::int64_t(wrap(std::int64_t(a.pos.z) - b.pos.z)) >> 9;
        if (std::llabs(dx) >= rr || std::llabs(dy) >= rr || std::llabs(dz) >= rr) continue;
        if (coarseSqrt(std::uint64_t(dx*dx)+std::uint64_t(dy*dy)+std::uint64_t(dz*dz)) > rr) continue;
        if (boxes) {
            // 0x436c30: vertical intervals, then yaw-rotated XZ rectangles (pitch/roll ignored).
            auto bounds=[&](const Element& e) {
                auto b=impl_->bounds[e.model];
                for(int k=0;k<6;++k) b[k]=wrap(std::int64_t(b[k])*e.scale[k%3]/1000);
                return b;
            };
            auto ab=bounds(a), bb=bounds(b);
            if (std::int64_t(a.pos.y)+ab[4]*512LL < std::int64_t(b.pos.y)+bb[1]*512LL ||
                std::int64_t(b.pos.y)+bb[4]*512LL < std::int64_t(a.pos.y)+ab[1]*512LL) continue;
            using Point=std::array<double,2>;
            auto corners=[&](const Element& e,const std::array<std::int32_t,6>& bound) {
                std::array<Point,4> p; int angle=normAngle(e.rot.y)>>6;
                auto sn=sinBig(angle),cs=sinBig(angle+720);
                for(int i=0;i<4;++i) {
                    std::int64_t x=bound[(i==1||i==2)?3:0],z=bound[i>=2?5:2];
                    p[i]=angle ? Point{double((cs*x>>15)-(sn*z>>15)+(e.pos.x>>9)),double((sn*x>>15)+(cs*z>>15)+(e.pos.z>>9))} : Point{double(x+(e.pos.x>>9)),double(z+(e.pos.z>>9))};
                } return p;
            };
            auto ap=corners(a,ab),bp=corners(b,bb);
            bool separated=false;
            for(const auto& poly : {ap,bp}) for(int i=0;i<2;++i) {
                double nx=poly[i+1][1]-poly[i][1],nz=poly[i][0]-poly[i+1][0];
                double amin=1e100,amax=-1e100,bmin=1e100,bmax=-1e100;
                for(int j=0;j<4;++j) { double av=ap[j][0]*nx+ap[j][1]*nz,bv=bp[j][0]*nx+bp[j][1]*nz;
                    amin=std::min(amin,av);amax=std::max(amax,av);bmin=std::min(bmin,bv);bmax=std::max(bmax,bv); }
                if(amax<bmin || bmax<amin) separated=true;
            }
            if(separated) continue;
        }
        a.collided |= b.id; b.collided |= a.id;
        events_.push_back({world_.tickCount,ai,"collision " + std::to_string(ai) + " " + std::to_string(bi) + " color " + std::to_string(kv.second)});
        // 0x44d0f0 -> 0x450f80: colour code is converted to the engine palette index.
        static constexpr int colors[] = {218,188,128,62,32,27,17,12,7,42,112,183};
        hitColors_.insert(colors[kv.second <= 11 ? kv.second : 0]);
    }
}

void BrainVM::runElement(int index) {
    Element& e = world_.elements[index];
    if (e.brain < 0) return;
    Brain& b = *programs_[e.brain];
    Ctx c{this, index};
    size_t pc = 0;
    int guard = 0;
    while (pc < b.code.size()) {
        if (++guard > 200000) break;
        Stmt& s = b.code[pc];
        switch (s.k) {
        case Stmt::Assign: world_.elements[index].vars[s.slot] = evalExpr(*s.e, c, world_.elements[index].vars); ++pc; break;
        case Stmt::Call: evalExpr(*s.e, c, world_.elements[index].vars); ++pc; break;
        case Stmt::If: {
            std::int32_t v = evalExpr(*s.e, c, world_.elements[index].vars);
            if (v != 0) ++pc;
            else pc = size_t(s.jump) + (b.code[s.jump].k == Stmt::Else ? 1 : 1); // past else/endif marker
            break;
        }
        case Stmt::Else: pc = size_t(s.jump) + 1; break; // reached from the true branch: skip to after endif
        case Stmt::Endif: ++pc; break;
        }
    }
}

void BrainVM::tick() {
    if (finished()) return;
    // Order reconstructed from the transition loop at 0x442220 (0x442795..0x4427a9) and 0x410450:
    // 0x42a110 animation, 0x44cad0 collisions, 0x403f50/0x404060 input, 0x410450 brains in element order.
    ++world_.updateCount;
    for (auto& e : world_.elements) e.flags &= ~std::uint32_t(kFlagChild);
    runCollisions();
    for (size_t i = 0; i < world_.elements.size(); ++i) {
        Element& e = world_.elements[i];
        if ((e.flags & kFlagOn) && !(e.flags & kFlagPaused)) {
            runElement(int(i));
            world_.elements[i].runCount++;
        }
    }
    world_.tickCount++;
}

// ---------------------------------------------------------------- commands
namespace {
int idxOf(const World& w, std::int32_t ref) {
    std::int64_t d = std::int64_t(ref) - kElementRefBase;
    if (d < 0 || d % kElementStride) return -1;
    d /= kElementStride;
    return d < std::int64_t(w.elements.size()) ? int(d) : -1;
}
std::int32_t rotIdx(std::int32_t angle) { return normAngle(angle) >> 6; }

struct V3 { std::int64_t x, y, z; };
// Child/speed rotation chain: rx on (y,z), rz on (x,y), ry on (x,z); 15-bit fixed point, each product shifted separately
// (0x409af0, 0x40d090).
V3 rotateChain(const BrainVM& vm, V3 v, const Vec3& r) {
    auto stage = [&](std::int32_t ang, std::int64_t& A, std::int64_t& B) {
        std::int32_t i = rotIdx(ang);
        if (i == 0) return;
        std::int64_t s = vm.sinBig(i), c = vm.sinBig(i + 720);
        std::int32_t na = wrap((c * A >> 15)) - wrap((s * B >> 15));
        std::int32_t nb = wrap((s * A >> 15)) + wrap((c * B >> 15));
        A = na; B = nb;
    };
    stage(r.x, v.y, v.z);
    stage(r.z, v.x, v.y);
    stage(r.y, v.x, v.z);
    return v;
}

int atanIndex(const std::vector<std::int32_t>& table, std::int64_t ratio) {
    if(table.size()<720) return int(std::atan(double(ratio)/1024)*1440/M_PI);
    int lo=0,hi=719;
    for(;;) { int mid=(lo+hi)/2; if(mid==lo || table[mid]==ratio) return mid;
        if(ratio>table[mid]) lo=mid; else hi=mid; }
}
std::int32_t yawTo(const Element& a, const Element& b, const std::vector<std::int32_t>& table) {
    // 0x409e80: atan lookup, quadrant correction, then subtract 90 degrees.
    if(a.index==b.index) return 0;
    auto dx=wrap(std::int64_t(b.pos.x)-a.pos.x)>>9;
    auto dz=wrap(std::int64_t(b.pos.z)-a.pos.z);
    std::int64_t ratio=dx ? std::llabs(std::int64_t(wrap(std::int64_t(dz)*2))/dx) : 512000;
    int angle=ratio<512000 ? atanIndex(table,ratio) : 720;
    if(dx<0 && dz<0) angle+=1440;
    else if(dz<0) angle=2880-angle;
    else if(dx<0) angle=1440-angle;
    return (angle-720)*64;
}
std::int32_t pitchTo(const Element& a, const Element& b, const std::vector<std::int32_t>& table) {
    // 0x409df0: targets above produce negative pitch, including the vertical case.
    auto dx=wrap(std::int64_t(b.pos.x)-a.pos.x)>>9, dz=wrap(std::int64_t(b.pos.z)-a.pos.z)>>9;
    auto dy=wrap(std::int64_t(b.pos.y)-a.pos.y)>>9;
    auto h=std::int64_t(std::sqrt(double(dx)*dx+double(dz)*dz));
    if(!h) return dy>0 ? -0xb400 : 0xb400;
    int angle=atanIndex(table,(std::llabs(std::int64_t(dy))*1024)/h)*64;
    return dy>0 ? -angle : angle;
}
std::int32_t frictionOne(std::int32_t v, std::int32_t f) {
    std::int32_t p = wrap(std::int64_t(v) * f);
    return p < 0 ? -((-p) >> 9) : (p >> 9);
}
} // namespace

std::int32_t BrainVM::callCommand(const std::string& name, const std::vector<std::int32_t>& a, int self) {
    World& w = world_;
    auto arg = [&](size_t i) -> std::int32_t { return i < a.size() ? a[i] : 0; };
    auto el = [&](size_t i) -> Element* { int x = idxOf(w, arg(i)); return x < 0 ? nullptr : &w.elements[x]; };
    auto ev = [&](const std::string& s, int idx) { events_.push_back({w.tickCount, idx, s}); };
    Element* e0 = el(0);
    const std::string& n = name;

    // --- position / rotation / velocity / spin accessors (0x40c510..0x40d2e0)
    if (n == "position_x") return e0 ? e0->pos.x : 0;
    if (n == "position_y") return e0 ? e0->pos.y : 0;
    if (n == "position_z") return e0 ? e0->pos.z : 0;
    if (n == "position_x_set") { if (e0) e0->pos.x = arg(1); return 0; }
    if (n == "position_y_set") { if (e0) e0->pos.y = arg(1); return 0; }
    if (n == "position_z_set") { if (e0) e0->pos.z = arg(1); return 0; }
    if (n == "position_y_add") { if (e0) e0->pos.y = wrap(std::int64_t(e0->pos.y) + arg(1)); return 0; }
    if (n == "position_copy") { Element* b = el(1); if (e0 && b) b->pos = e0->pos; return 0; }
    if (n == "rotation_x") return e0 ? normAngle(e0->rot.x) : 0;
    if (n == "rotation_y") return e0 ? normAngle(e0->rot.y) : 0;
    if (n == "rotation_z") return e0 ? normAngle(e0->rot.z) : 0;
    if (n == "rotation_x_set") { if (e0) e0->rot.x = arg(1); return 0; }
    if (n == "rotation_y_set") { if (e0) e0->rot.y = arg(1); return 0; }
    if (n == "rotation_z_set") { if (e0) e0->rot.z = arg(1); return 0; }
    if (n == "rotation_x_add") { if (e0) e0->rot.x = wrap(std::int64_t(e0->rot.x) + arg(1)); return 0; }
    if (n == "rotation_y_add") { if (e0) e0->rot.y = wrap(std::int64_t(e0->rot.y) + arg(1)); return 0; }
    if (n == "rotation_z_add") { if (e0) e0->rot.z = wrap(std::int64_t(e0->rot.z) + arg(1)); return 0; }
    if (n == "velocity_x_set") { if (e0) e0->vel.x = arg(1); return 0; }
    if (n == "velocity_y_set") { if (e0) e0->vel.y = arg(1); return 0; }
    if (n == "velocity_z_set") { if (e0) e0->vel.z = arg(1); return 0; }
    if (n == "velocity_y_add") { if (e0) e0->vel.y = wrap(std::int64_t(e0->vel.y) + arg(1)); return 0; }
    if (n == "spin_y_set") { if (e0) e0->spin.y = arg(1); return 0; }
    if (n == "velocity_update") { if (e0) { e0->pos.x = wrap(std::int64_t(e0->pos.x) + e0->vel.x); e0->pos.y = wrap(std::int64_t(e0->pos.y) + e0->vel.y); e0->pos.z = wrap(std::int64_t(e0->pos.z) + e0->vel.z);} return 0; }
    if (n == "spin_update") { if (e0) { e0->rot.x = wrap(std::int64_t(e0->rot.x) + e0->spin.x); e0->rot.y = wrap(std::int64_t(e0->rot.y) + e0->spin.y); e0->rot.z = wrap(std::int64_t(e0->rot.z) + e0->spin.z);} return 0; }
    // --- heading based speed (0x40d040, 0x40d090, 0x40b570)
    if (n == "speed_xz_set" || n == "thrust_xz") {
        if (!e0) return 0;
        size_t row = size_t(normAngle(e0->rot.y) >> 9);
        auto sb = [&](size_t off) -> std::int32_t { size_t i = row * 256 + off; return i < sinus_.size() ? sinus_[i] : 0; };
        std::int64_t s = sb(1), c = sb(0x5aff), sp = arg(1);
        if (n == "speed_xz_set") { e0->vel.x = wrap((s * sp) >> 7); e0->vel.z = wrap((c * sp) >> 7); }
        else { e0->vel.x = wrap(std::int64_t(e0->vel.x) + ((s * sp) >> 7)); e0->vel.z = wrap(std::int64_t(e0->vel.z) + ((c * sp) >> 7)); }
        return 0;
    }
    if (n == "speed_set") {
        if (!e0) return 0;
        V3 v = rotateChain(*this, {0, 0, arg(1)}, e0->rot);
        e0->vel = {wrap(v.x), wrap(v.y), wrap(v.z)};
        return 0;
    }
    if (n == "friction_y") { if (e0) e0->vel.y = frictionOne(e0->vel.y, arg(1)); return 0; }
    if (n == "friction_xz") { if (e0) { e0->vel.x = frictionOne(e0->vel.x, arg(1)); e0->vel.z = frictionOne(e0->vel.z, arg(1)); } return 0; }
    if (n == "attract_xz") {
        Element* b = el(1);
        if (!e0 || !b) return 0;
        std::int64_t dx = std::int64_t(e0->pos.x) - b->pos.x, dz = std::int64_t(e0->pos.z) - b->pos.z;
        std::int64_t m = std::max(std::llabs(dx), std::llabs(dz));
        if (m == 0) return 0;
        e0->vel.x = wrap(e0->vel.x + (std::int64_t(wrap(-dx)) * arg(2)) / m);
        e0->vel.z = wrap(e0->vel.z + (std::int64_t(wrap(-dz)) * arg(2)) / m);
        return 0;
    }
    // --- parenting / following (0x40b280, 0x40b2f0, 0x409af0, 0x409cb0, 0x40b630)
    if (n == "join") { Element* b = el(1); if (e0 && b) { b->pos = e0->pos; b->rot = e0->rot; b->vel = e0->vel; b->spin = e0->spin; } return 0; }
    if (n == "join_pos") { Element* b = el(1); if (e0 && b) { b->pos = e0->pos; b->vel = e0->vel; } return 0; }
    if (n == "child_pos") {
        Element* p = el(1);
        if (!e0 || !p) return 0;
        V3 v = rotateChain(*this, {arg(2), arg(3), arg(4)}, p->rot);
        e0->pos = {wrap(p->pos.x + v.x), wrap(p->pos.y + v.y), wrap(p->pos.z + v.z)};
        return 0;
    }
    if (n == "child_rot") {
        Element* p = el(1);
        if (!e0 || !p) return 0;
        e0->flags |= kFlagChild; e0->rot = p->rot; e0->childRot = {arg(2), arg(3), arg(4)};
        return 0;
    }
    if (n == "follow") {
        Element* t = el(1);
        if (!e0 || !t) return 0;
        std::int64_t x = 0, y = arg(2), z = arg(3), k = arg(4);
        std::int32_t i = rotIdx(t->rot.y);
        if (i) {
            std::int64_t s = sinBig(i), c = sinBig(i + 720);
            std::int32_t nx = wrap((c * x >> 15)) - wrap((s * z >> 15));
            std::int32_t nz = wrap((s * x >> 15)) + wrap((c * z >> 15));
            x = nx; z = nz;
        }
        std::int64_t tx = t->pos.x + x, ty = t->pos.y + y, tz = t->pos.z + z;
        e0->pos.x = wrap(e0->pos.x + (((wrap(tx - e0->pos.x) >> 9) * k) << 2));
        e0->pos.y = wrap(e0->pos.y + (((wrap(ty - e0->pos.y) >> 9) * k) << 2));
        e0->pos.z = wrap(e0->pos.z + (((wrap(tz - e0->pos.z) >> 9) * k) << 2));
        return 0;
    }
    if (n == "track") { Element* b = el(1); if (e0 && b) { e0->rot.x = pitchTo(*e0, *b,impl_->atan); e0->rot.y = yawTo(*e0, *b,impl_->atan); } return 0; }
    if (n == "track_yr") { Element* b = el(1); if (e0 && b) e0->rot.y = yawTo(*e0, *b,impl_->atan); return 0; }
    // --- flags
    if (n == "on") { if (e0) { e0->flags |= kFlagOn; ev("on", e0->index); if (scene_) scene_->elementEvent(e0->index, "on"); if (audio_) audio_->on(int(e0->index)); } return 0; }
    if (n == "off") { if (e0) { if (e0->flags & kFlagOn) { ev("off", e0->index); if (audio_) audio_->off(int(e0->index)); } e0->flags &= ~std::uint32_t(kFlagOn); if (scene_) scene_->elementEvent(e0->index, "off"); } return 0; }
    if (n == "show") { if (e0) { e0->flags |= kFlagVisible; if (scene_) scene_->elementEvent(e0->index, "show"); } return 0; }
    if (n == "hide") { if (e0) { e0->flags &= ~std::uint32_t(kFlagVisible); if (scene_) scene_->elementEvent(e0->index, "hide"); } return 0; }
    if (n == "box_collisions_on") { if(e0) e0->flags |= kFlagSphere2; return 0; }
    if (n == "box_collisions_off") { if(e0) e0->flags &= ~std::uint32_t(kFlagSphere2); return 0; }
    if (n == "collisions_on") { if (e0) e0->flags |= kFlagCollide; return 0; }
    if (n == "collisions_off") { if (e0) e0->flags &= ~std::uint32_t(kFlagCollide); return 0; }
    if (n == "translucent") { if (e0) { e0->flags = (e0->flags & ~std::uint32_t(kFlagTranslucent)) | ((arg(1) & 1) << 5); if (scene_) scene_->elementEvent(e0->index, "translucent"); } return 0; }
    if (n == "state") { return (arg(0) != 0 && idxOf(w, arg(0)) >= 0) ? std::int32_t(e0->flags & std::uint32_t(arg(1))) : 0; }
    // --- scale
    // 0x406810 / 0x424b70 update visual/box bounds only; sphere radius is fixed at load.
    if (n == "scale_set") { if (e0) { e0->scale[0]=arg(1); e0->scale[1]=arg(2); e0->scale[2]=arg(3); } return 0; }
    if (n == "flat_scale_set") { if (e0) { e0->flatScale[0] = arg(1); e0->flatScale[1] = arg(2); } return 0; }
    // --- globals / credits(score)
    if (n == "global") return w.globals[arg(0) & 0x7ff];
    if (n == "global_set") { w.globals[arg(0) & 0x7ff] = arg(1); ev("global_set " + std::to_string(arg(0) & 0x7ff) + "=" + std::to_string(arg(1)), self); return 0; }
    if (n == "global_add") { w.globals[arg(0) & 0x7ff] = wrap(std::int64_t(w.globals[arg(0) & 0x7ff]) + arg(1)); ev("global_add " + std::to_string(arg(0) & 0x7ff) + "+=" + std::to_string(arg(1)), self); return 0; }
    if (n == "credits" || n == "score") return e0 ? e0->credits : 0;
    if (n == "credits_set" || n == "score_set") { if (e0) e0->credits = arg(1); return 0; }
    if (n == "credits_add" || n == "score_add") { if (e0) e0->credits = wrap(std::int64_t(e0->credits) + arg(1)); return 0; }
    // --- collision & queries
    if (n == "collision") return e0 ? std::int32_t(e0->collided & std::uint32_t(arg(1))) : 0;
    if (n == "collision_color") return hitColors_.count(arg(0)) ? -1 : 0;
    if (n == "collision_map_set") {
        Element* b = el(1); int c = arg(2);
        if (!e0 || !b || c < 0 || c > 0xb) return 0;
        auto key = std::make_pair(std::min(e0->index, b->index), std::max(e0->index, b->index));
        std::pair<int,int> k2(int(key.first), int(key.second));
        if (c == 0) collisionMap_.erase(k2); else collisionMap_[k2] = c;
        return 0;
    }
    if (n == "ground_set") { w.groundElement = e0 ? int(e0->index) : -1; return 0; }
    if (n == "ground_level") return e0 ? groundHeight(int(e0->index)) : 0;
    if (n == "agl" || n == "aglevel" || n == "above_ground_level") return e0 ? wrap(std::int64_t(e0->pos.y)-groundHeight(int(e0->index))) : 0;
    if (n == "element_index") { return wrap(std::int64_t(arg(0)) + (std::int64_t(arg(1)) << 10)); }
    if (n == "element_id") return e0 ? std::int32_t(e0->id) : 0;
    if (n == "first_element") return refOf(0);
    if (n == "nearest_element") {
        if (!e0) return 0;
        std::int32_t best = 0; std::int64_t bd = 0x7fffffff; // exe seeds with a large constant
        for (auto& o : w.elements) {
            if (o.index == e0->index || !(o.flags & kFlagOn) || !(o.id & std::uint32_t(arg(1)))) continue;
            double dx = double(e0->pos.x - o.pos.x) / 512, dy = double(e0->pos.y - o.pos.y) / 512, dz = double(e0->pos.z - o.pos.z) / 512;
            std::int64_t d = std::int64_t(std::sqrt(dx*dx + dy*dy + dz*dz)) << 9;
            if (d < bd) { bd = d; best = refOf(int(o.index)); }
        }
        return best;
    }
    if (n == "distance") {
        Element* b = el(1); if (!e0 || !b) return 0;
        double dx = double(e0->pos.x - b->pos.x) / 512, dy = double(e0->pos.y - b->pos.y) / 512, dz = double(e0->pos.z - b->pos.z) / 512;
        return wrap(std::int64_t(std::sqrt(dx*dx + dy*dy + dz*dz)) << 9);
    }
    if (n == "execute") { if (e0 && int(e0->index) != self && e0->brain >= 0) { static thread_local int depth = 0; if (depth < 8) { ++depth; runElement(int(e0->index)); --depth; } } return 0; }
    // --- math
    if (n == "sin_512") return sinB(arg(0));
    if (n == "cos_512") return cosB(arg(0));
    if (n == "random") {
        std::int32_t lo = arg(0), hi = arg(1); if (lo >= hi) return lo;
        std::int32_t v = std::int32_t(nextRandom());
        if (lo >= hi) return hi; // 0x4608b0 (edx>=ecx)
        if (v >= lo && v <= hi) return v;
        std::int32_t top = hi + 1;
        if (v < lo) return std::int32_t((std::int64_t(v) - top) % (top - lo) + top);
        return std::int32_t((std::int64_t(v) - top) % (top - lo) + lo);
    }
    if (n == "every_tics") { std::uint32_t t = w.tickCount; return (std::uint32_t(arg(1)) > t && ((t & std::uint32_t(arg(0) - 1)) == 0)) ? -1 : 0; }
    // --- input
    if (n == "keyboard") return input_ && input_->keyboard(arg(0)) ? -1 : 0;
    if (n == "any_input") return input_ && input_->anyInput() ? -1 : 0;
    if (n == "joy_b") return std::int32_t(((input_ ? input_->joyButtons() : 0) << 4 | (input_ ? input_->joyButtons() : 0) >> 28) & std::uint32_t(arg(0)));
    if (n == "joy1_x") {
        // 0x40a140: linear map of the raw axis onto [arg0,arg1]; axis range here is -1000..1000 (guess)
        std::int64_t lo = arg(0), hi = arg(1), x = input_ ? input_->joyAxisX() : 0;
        std::int64_t span = hi - lo;
        std::int64_t r = (1000 - x) * span / 2000;
        return wrap(lo > hi ? r - hi : r + lo);
    }
    // --- audio / scene
    if (n == "play_sound") { if (e0 && audio_) { audio_->play(int(e0->index), e0->sound); ev("play_sound " + e0->sound, e0->index); } return 0; }
    if (n == "stop_sound") { if (e0 && audio_) audio_->stop(int(e0->index)); return 0; }
    if (n == "sound_rate_set") { if (e0 && audio_) { e0->soundRate = arg(1); audio_->setRate(int(e0->index), arg(1)); } return 0; }
    if (n == "listener") { w.listener = e0 ? int(e0->index) : -1; return 0; }
    if (n == "light_color_set") { if (e0 && scene_) scene_->lightColor(int(e0->index), arg(1), arg(2), arg(3)); return 0; }
    if (n == "light_intensity_set") { if (e0 && scene_) scene_->lightIntensity(int(e0->index), arg(1)); return 0; }
    if (n == "3dbg_color_set") { if (scene_) scene_->backgroundColor(arg(0), arg(1), arg(2)); return 0; }
    if (n == "pause") return 0; // 0x40ff20 blocks for a key in the original; GUESS: no-op headless
    // --- flow
    if (n == "exit") { if (w.exitCode == -1) { w.exitCode = arg(0); ev("exit " + std::to_string(arg(0)), self); } return 0; }
    ++unknownCounts_[name];
    if (unknown_.insert(name).second) ev("UNIMPLEMENTED " + name, self);
    return 0;
}

} // namespace rb
