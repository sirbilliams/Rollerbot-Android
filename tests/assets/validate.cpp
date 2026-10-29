#include "assets/Assets.h"
#include <fstream>
#include <iostream>
#include <regex>
#include <set>
#include <stdexcept>
using namespace rb;
void check(bool b,const char* s){if(!b)throw std::runtime_error(s);}
int main(int argc,char** argv){try{
 check(argc==2,"usage: rb_assets_validate GAME");AssetStore s;s.open(argv[1]);size_t dat=0,projects=0,images=0,meshes=0,sounds=0,refs=0,brains=0;std::set<std::string> names;
 auto collect=[&](const Bytes& b){std::string text(b.begin(),b.end());std::regex re("[A-Za-z0-9_./\\\\: -]+\\.(?:dat|prj|col|wav|pcx|bmp|3ds|x)",std::regex::icase);for(std::sregex_iterator i(text.begin(),text.end(),re),end;i!=end;++i)names.insert(i->str());};
 for(const auto& e:std::filesystem::directory_iterator(s.directory())){auto name=e.path().filename().string(),ext=e.path().extension().string();if(!e.is_regular_file())continue;auto b=s.read(name);auto k=assetKind(b);
 try{if(ext==".dat"){dat++;check(k!="unknown"||name=="quality.dat"||name=="globvars.dat","unknown DAT");if(k=="d30")check(b.size()==176,"bad d30");if(name=="quality.dat")check(b.size()==4,"bad quality");if(name=="globvars.dat")check(b.size()==8192,"bad globals");}
 if(k=="pcx"||k=="bmp"){auto im=Image::decode(b);check(im.rgb.size()==size_t(im.width)*im.height*3,"image size");images++;}
 if(k=="x"||k=="3ds"||k=="d3m"){Mesh::decode(b);meshes++;}if(k=="wav"){Sound::decode(b);sounds++;}
 if(k=="prj"){auto p=ProjectData::load(s,name);check(p.collection.size()==16384,"COL size");projects++;brains+=p.brains.size();collect(b);for(auto& o:p.objects)for(auto& n:{o.model,o.sound}){if(n=="./0000002D.dat")continue;check(bool(s.find(n)),("project reference missing: "+n).c_str());refs++;}}
 if(k=="descriptor"){std::string t(b.begin(),b.end());std::regex re("\\./[^|\\r\\n]+");for(std::sregex_iterator i(t.begin(),t.end(),re),end;i!=end;++i){check(bool(s.find(i->str())),"descriptor reference missing");refs++;}}
 if(ext==".exe")collect(b);
 }catch(const std::exception& x){throw std::runtime_error(name+": "+x.what());}}
 size_t roundtrips=0;for(auto& n:names){uint32_t h=0;for(unsigned char c:n){if(!c)break;int v=c<128?c:int(c)-256;h=h*31+v;}check(h==AssetStore::hash(n),"hash mismatch");auto a=AssetStore::assetName(n);auto base=n.substr(n.find_last_of("/\\:")==n.npos?0:n.find_last_of("/\\:")+1);if(base!="null_.wav")check(std::stoul(a.substr(0,8),nullptr,16)==h,"hash formatting");roundtrips++;}
 check(AssetStore::assetName("./null_.wav")=="null_.wav","null rule");check(AssetStore::hash(std::string(1,char(255)))==0xffffffffu,"signed hash");check(AssetStore::hash("a/b")!=AssetStore::hash("a\\b"),"path normalization");
 size_t rejected=0;for(auto k:{"bmp","pcx","wav","mesh","project"}){try{if(std::string(k)=="bmp")Image::decode(Bytes{'B','M'});else if(std::string(k)=="pcx")Image::decode(Bytes{10,5,1,8});else if(std::string(k)=="wav")Sound::decode(Bytes{'R','I','F','F'});else if(std::string(k)=="mesh")Mesh::decode(Bytes{'M','M'});else ProjectData::parse({});}catch(const std::exception&){rejected++;}}check(rejected==5,"malformed accepted");
 check(dat==344&&projects==14,"incomplete retail inventory");std::cout<<"PASS projects/collections="<<projects<<" DAT="<<dat<<" images="<<images<<" meshes="<<meshes<<" sounds="<<sounds<<" references="<<refs<<" brains="<<brains<<" hash_roundtrips="<<roundtrips<<" malformed="<<rejected<<"\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}}
