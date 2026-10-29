#include "Ui.h"
#include <algorithm>
#include <fstream>
#include <limits>
#include <stdexcept>

namespace rb {
Actions InputMap::update(const InputState& s) {
    Actions a;
    for(size_t i=0;i<a.down.size();++i) a.down[i]=s.touch[i] || (keys[i]<256 && s.keys[keys[i]]);
    a.down[0]=a.down[0] || s.joystickX>=.9f;
    a.down[1]=a.down[1] || s.joystickX<=-1.f;
    a.down[2]=a.down[2] || (s.buttons&fireMask);
    a.down[4]=a.down[4] || (s.buttons&fireMask);
    for(size_t i=0;i<a.down.size();++i) a.pressed[i]=a.down[i]&&!previous_.down[i];
    previous_=a; return a;
}
static unsigned u16(const std::vector<uint8_t>& b,size_t p) { return b.at(p)|(unsigned(b.at(p+1))<<8); }
static uint32_t u32(const std::vector<uint8_t>& b,size_t p) {
    return u16(b,p)|(uint32_t(u16(b,p+2))<<16);
}
// 0x44aee0 loads 0x333 header, two 0x400 tables, then stride*height bitmap.
Font Font::decode(const std::vector<uint8_t>& b) {
    if(b.size()<0xb33 || u16(b,2)!=3) throw std::runtime_error("invalid font header");
    Font f;
    f.height_=u16(b,809); f.stride_=u16(b,811);
    f.spacing_=u16(b,815); f.space_=u16(b,817);
    if(f.height_<=0 || f.stride_<=0 || size_t(f.height_)*f.stride_!=b.size()-0xb33)
        throw std::runtime_error("invalid font bitmap size");
    for(size_t i=0;i<256;++i) {
        f.widths_[i]=b[39+i]; f.offsets_[i]=u16(b,295+i*2);
        if(unsigned(f.offsets_[i])+f.widths_[i]>unsigned(f.stride_*8)) throw std::runtime_error("invalid glyph extent");
    }
    f.bitmap_.assign(b.begin()+0xb33,b.end()); return f;
}
int Font::width(std::string_view s) const {
    int x=0,best=0;
    for(unsigned char c:s) {
        if(c=='\n') { best=std::max(best,x); x=0; }
        else x+=(c==' '?space_:widths_[c]+spacing_);
    }
    return std::max(best,x);
}
void Font::draw(IRenderer2D& r,std::string_view s,int x,int y,uint32_t color,int scale) const {
    if(scale<1 || scale>64) throw std::invalid_argument("font scale");
    int origin=x;
    for(unsigned char c:s) {
        if(c=='\n') { x=origin;y+=height_*scale;continue; }
        if(c==' ') { x+=space_*scale;continue; }
        for(int row=0;row<height_;++row) for(int col=0;col<widths_[c];++col) {
            unsigned bit=offsets_[c]+col;
            if(bitmap_.at(row*stride_+bit/8)&(0x80>>(bit%8))) r.fillRect(x+col*scale,y+row*scale,scale,scale,color);
        }
        x+=(widths_[c]+spacing_)*scale;
    }
}
void Hud::draw(IRenderer2D& r,const Font& f,const HudState& s) const {
    for(const auto& o:overlays) r.overlay(o.asset,o.x,o.y,o.w,o.h);
    f.draw(r,std::to_string(s.score),score.x,score.y);
    f.draw(r,std::to_string(s.highScore),highScore.x,highScore.y);
    f.draw(r,std::to_string(s.lives),lives.x,lives.y);
    f.draw(r,std::to_string(s.time),time.x,time.y);
}
void Menus::update(const Actions& a,IUiRuntime& vm) {
    if(screen_==Screen::Title) {
        if(a.hit(Action::Back)) vm.quit();
        else if(a.hit(Action::Accept)) { vm.startGame(); screen_=Screen::Playing; }
    } else if(a.hit(Action::Back)) { vm.returnToTitle();screen_=Screen::Title; }
    else if(a.hit(Action::Pause)) { screen_=screen_==Screen::Paused?Screen::Playing:Screen::Paused;vm.setPaused(screen_==Screen::Paused); }
}
static std::vector<uint8_t> read(const std::filesystem::path& p) {
    std::ifstream f(p,std::ios::binary);
    if(!f) throw std::runtime_error("cannot read "+p.string());
    return {std::istreambuf_iterator<char>(f),{}};
}
static void put32(std::vector<uint8_t>& b,uint32_t n) { for(int i=0;i<4;++i) b.push_back(uint8_t(n>>(8*i))); }
static void write(const std::filesystem::path& p,const std::vector<uint8_t>& b) {
    auto temporary=p; temporary+=".tmp";
    { std::ofstream f(temporary,std::ios::binary|std::ios::trunc);
      if(!f || !f.write(reinterpret_cast<const char*>(b.data()),b.size()) || !f.flush()) throw std::runtime_error("cannot write "+p.string()); }
    std::filesystem::rename(temporary,p);
}
void SaveStore::submitScore(int32_t score,size_t i) { globals.at(i)=std::max(globals.at(i),score); }
void SaveStore::load() {
    auto g=globals;auto s=settings;
    auto p=directory_/"globvars.dat";
    // 0x4422e5 / 0x442873: fixed 8192-byte global bank, high score is global 100.
    if(std::filesystem::exists(p)) {
        auto b=read(p); if(b.size()!=8192) throw std::runtime_error("invalid globals size");
        for(size_t i=0;i<g.size();++i) { auto n=u32(b,i*4); g[i]=n<=0x7fffffff?int32_t(n):int32_t(int64_t(n)-0x100000000LL); }
    }
    p=directory_/"quality.dat";
    if(std::filesystem::exists(p)) { auto b=read(p);if(b.size()!=4) throw std::runtime_error("invalid quality size");s.quality=u32(b,0); }
    p=directory_/"3drad.ini";
    if(std::filesystem::exists(p)) {
        auto b=read(p);s.iniLines.clear();std::string line;
        for(char c:b) { if(c=='\n') { if(!line.empty()&&line.back()=='\r')line.pop_back();s.iniLines.push_back(line);line.clear(); }else line+=c; }
        if(!line.empty())s.iniLines.push_back(line);
    }
    globals=g;settings=std::move(s);
}
void SaveStore::save() const {
    for(const auto& line:settings.iniLines)
        if(line.find_first_of("\r\n")!=std::string::npos)throw std::invalid_argument("INI record contains newline");
    std::filesystem::create_directories(directory_);
    std::vector<uint8_t> b;for(auto n:globals)put32(b,uint32_t(n));write(directory_/"globvars.dat",b);
    b.clear();put32(b,settings.quality);write(directory_/"quality.dat",b);
    b.clear();for(const auto& l:settings.iniLines) {
        b.insert(b.end(),l.begin(),l.end());b.push_back('\r');b.push_back('\n');
    }write(directory_/"3drad.ini",b);
}
}
