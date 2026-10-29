#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace rb {
enum class Action { Left, Right, Jump, Pause, Accept, Back, Count };
struct InputState {
    // Original PC scan codes, not SDL scancodes. Platform performs translation.
    std::array<bool,256> keys{};
    float joystickX=0; // normalized [-1,1]; original project reverses X
    uint32_t buttons=0;
    std::array<bool,static_cast<size_t>(Action::Count)> touch{};
};
struct Actions {
    std::array<bool,static_cast<size_t>(Action::Count)> down{}, pressed{};
    bool held(Action a) const { return down.at(static_cast<size_t>(a)); }
    bool hit(Action a) const { return pressed.at(static_cast<size_t>(a)); }
};
class InputMap {
public:
    std::array<unsigned,6> keys{75,77,29,25,57,1};
    uint32_t fireMask=16; // joy_b(16), shared by jump and title accept
    Actions update(const InputState&);
    void reset() { previous_={}; }
private: Actions previous_;
};
struct IRenderer2D {
    virtual ~IRenderer2D()=default;
    // RGBA pixels; implementation clips to target and handles display scaling.
    virtual void fillRect(int x,int y,int w,int h,uint32_t rgba)=0;
    virtual void overlay(std::string_view asset,int x,int y,int w,int h)=0;
};
class Font {
public:
    static Font decode(const std::vector<uint8_t>&);
    int height() const { return height_; }
    int width(std::string_view) const;
    void draw(IRenderer2D&,std::string_view,int x,int y,uint32_t rgba=0xffffffff,int scale=1) const;
private:
    std::array<uint8_t,256> widths_{};
    std::array<uint16_t,256> offsets_{};
    int height_=0,stride_=0,spacing_=2,space_=9;
    std::vector<uint8_t> bitmap_;
};
struct HudState { int32_t score=0,highScore=0,lives=3,time=0; };
struct Overlay { std::string asset; int x=0,y=0,w=0,h=0; };
class Hud {
public:
    // Project-provided 2D overlays are drawn before counter text.
    std::vector<Overlay> overlays;
    struct Point { int x,y; };
    Point score{84,8},highScore{281,67},lives{86,66},time{304,22};
    void draw(IRenderer2D&,const Font&,const HudState&) const;
};
struct IUiRuntime {
    virtual ~IUiRuntime()=default;
    virtual void startGame()=0;
    virtual void setPaused(bool)=0;
    virtual void returnToTitle()=0;
    virtual void quit()=0;
};
enum class Screen { Title, Playing, Paused };
class Menus {
public:
    Screen screen() const { return screen_; }
    void update(const Actions&,IUiRuntime&);
    void showTitle() { screen_=Screen::Title; }
private: Screen screen_=Screen::Title;
};
struct Settings {
    // Ordered line records, not a Windows INI key/value file. Unknown fields retained.
    std::vector<std::string> iniLines;
    uint32_t quality=0x30f;
};
class SaveStore {
public:
    explicit SaveStore(std::filesystem::path directory):directory_(std::move(directory)){}
    std::array<int32_t,2048> globals{};
    Settings settings;
    int32_t highScore(size_t index=100) const { return globals.at(index); }
    void submitScore(int32_t score,size_t index=100);
    // Missing files preserve defaults; malformed files throw without partial state changes.
    void load();
    void save() const;
private: std::filesystem::path directory_;
};
}
