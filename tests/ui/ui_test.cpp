#include "ui/Ui.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace rb;
static void check(bool ok) { if(!ok)throw std::runtime_error("check failed"); }
struct Canvas:IRenderer2D {
    int w=640,h=160,lit=0;std::vector<uint8_t> rgb=std::vector<uint8_t>(w*h*3,20);
    void fillRect(int x,int y,int a,int b,uint32_t) override {
        for(int j=std::max(y,0);j<std::min(y+b,h);++j)for(int i=std::max(x,0);i<std::min(x+a,w);++i) {
            auto n=(j*w+i)*3;rgb[n]=250;rgb[n+1]=225;rgb[n+2]=160;++lit;
        }
    }
    void overlay(std::string_view,int,int,int,int) override {}
    void save(const std::string& p) { std::ofstream f(p,std::ios::binary);f<<"P6\n"<<w<<" "<<h<<"\n255\n";f.write(reinterpret_cast<char*>(rgb.data()),rgb.size());check(bool(f)); }
};
struct Vm:IUiRuntime { int starts=0,pauses=0,titles=0,quits=0;
 void startGame()override{++starts;}void setPaused(bool)override{++pauses;}void returnToTitle()override{++titles;}void quit()override{++quits;} };
int main(int argc,char**argv)try {
    auto dir=std::filesystem::temp_directory_path()/"rb-ui-test";
    std::filesystem::remove_all(dir);
    SaveStore a(dir);a.load();a.submitScore(12345);a.submitScore(42);a.globals[220]=3;a.globals[2047]=-5;
    a.settings.iniLines={"0","512","0","","v1.0","0"};a.settings.quality=0x30f;a.save();
    SaveStore b(dir);b.load();check(b.globals==a.globals);check(b.settings.iniLines==a.settings.iniLines);check(b.settings.quality==a.settings.quality);
    check(std::filesystem::file_size(dir/"globvars.dat")==8192);
    { std::ifstream raw(dir/"globvars.dat",std::ios::binary);raw.seekg(400);check(raw.get()==0x39&&raw.get()==0x30&&raw.get()==0&&raw.get()==0); }
    {std::ofstream bad(dir/"quality.dat");bad<<"x";}
    bool threw=false;try{b.load();}catch(const std::runtime_error&){threw=true;}check(threw&&b.highScore()==12345);
    InputMap map;InputState in;in.keys[75]=true;check(map.update(in).hit(Action::Left));check(!map.update(in).hit(Action::Left));
    in={};in.joystickX=1;check(map.update(in).held(Action::Left));in={};in.touch[2]=true;check(map.update(in).hit(Action::Jump));
    Menus menus;Vm vm;map.reset();in={};in.keys[57]=true;menus.update(map.update(in),vm);check(vm.starts==1&&menus.screen()==Screen::Playing);
    in={};map.update(in);in.keys[25]=true;menus.update(map.update(in),vm);check(vm.pauses==1&&menus.screen()==Screen::Paused);
    in={};map.update(in);in.keys[25]=true;menus.update(map.update(in),vm);check(vm.pauses==2&&menus.screen()==Screen::Playing);
    in={};map.update(in);in.keys[1]=true;menus.update(map.update(in),vm);check(vm.titles==1&&menus.screen()==Screen::Title);
    std::vector<uint8_t> synthetic(0xb33+15);synthetic[2]=3;synthetic[809]=15;synthetic[811]=1;synthetic[39+'A']=1;synthetic[0xb33]=128;
    auto font=Font::decode(synthetic);Canvas c;font.draw(c,"A",0,0);check(c.lit==1);check(font.width("AA")==2);
    synthetic.pop_back();threw=false;try{Font::decode(synthetic);}catch(const std::runtime_error&){threw=true;}check(threw);
    if(argc==3) {
        std::ifstream f(argv[1],std::ios::binary);check(bool(f));std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(f),{}};
        auto retail=Font::decode(bytes);check(retail.height()==15);Canvas shot;
        retail.draw(shot,"ROLLERBOT: A Time Journey",12,12,0xffffffff,2);
        retail.draw(shot,"Score 0123456789   Lives 3   Time 120",12,60);
        retail.draw(shot,"ABCDEFGHIJKLMNOPQRSTUVWXYZ",12,90);
        retail.draw(shot,"abcdefghijklmnopqrstuvwxyz !?+-",12,120);
        check(shot.lit>500);shot.save(argv[2]);
    }
    std::filesystem::remove_all(dir);std::cout<<"UI font, input, navigation and persistence checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
