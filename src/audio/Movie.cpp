#include "Audio.h"
#include <fstream>
#include <cstring>
#include <stdexcept>
#include <functional>
namespace rb {
struct Movie::Impl {std::vector<uint8_t> bytes;std::vector<std::pair<size_t,size_t>> frames;std::shared_ptr<PCM> pcm;size_t next=0;int w=0,h=0,bits=0;double step=0;bool paused=false;std::string error;};
Movie::Movie():p(new Impl){}Movie::~Movie()=default;
void Movie::teardown(){p.reset(new Impl);}const std::string& Movie::error()const{return p->error;}
// 0x404bd0: replacement for MCIWnd-managed AVI playback.
bool Movie::open(const std::string& file){teardown();try{
 std::ifstream in(file,std::ios::binary|std::ios::ate);if(!in)throw std::runtime_error("Cannot open AVI");auto len=in.tellg();if(len<12||len>512*1024*1024)throw std::runtime_error("Invalid AVI size");p->bytes.resize(size_t(len));in.seekg(0);if(!in.read((char*)p->bytes.data(),len))throw std::runtime_error("AVI read failed");auto& b=p->bytes;
 auto u16=[&](size_t o)->uint16_t{if(o+2>b.size())throw std::runtime_error("Truncated AVI");return b[o]|uint16_t(b[o+1])<<8;};auto u32=[&](size_t o)->uint32_t{return u16(o)|uint32_t(u16(o+2))<<16;};auto is=[&](size_t o,const char* s){return o+4<=b.size()&&!memcmp(b.data()+o,s,4);};
 if(!is(0,"RIFF")||!is(8,"AVI ")||uint64_t(u32(4))+8>b.size())throw std::runtime_error("Invalid RIFF AVI");
 int stream=-1,video=-1,audio=-1,channels=0,rate=0,bitsAudio=0;std::vector<uint8_t> rawAudio;
 std::function<void(size_t,size_t,int)> walk=[&](size_t start,size_t end,int current){for(size_t o=start;o<end;){if(end-o<8)throw std::runtime_error("Truncated chunk");size_t n=u32(o+4),d=o+8;if(n>end-d)throw std::runtime_error("Chunk exceeds container");
 if(is(o,"LIST")){if(n<4)throw std::runtime_error("Short LIST");int child=current;if(is(d,"strl"))child=++stream;walk(d+4,d+n,child);}
 else if(is(o,"avih")){if(n<40)throw std::runtime_error("Short avih");p->step=u32(d)/1000000.;}
 else if(is(o,"strh")){if(n<48)throw std::runtime_error("Short strh");if(is(d,"vids")){if(video!=-1)throw std::runtime_error("Multiple video streams");video=current;auto scale=u32(d+20),rateV=u32(d+24);if(rateV)p->step=double(scale)/rateV;}if(is(d,"auds"))audio=current;}
 else if(is(o,"strf")&&current==video){if(n<40||u32(d)<40)throw std::runtime_error("Short bitmap format");p->w=int32_t(u32(d+4));p->h=int32_t(u32(d+8));p->bits=u16(d+14);if(u32(d+16)!=0||(p->bits!=24&&p->bits!=32))throw std::runtime_error("Unsupported AVI codec (requires BI_RGB 24/32)");}
 else if(is(o,"strf")&&current==audio){if(n<16||u16(d)!=1)throw std::runtime_error("Unsupported AVI audio (requires PCM)");channels=u16(d+2);rate=u32(d+4);bitsAudio=u16(d+14);if((channels!=1&&channels!=2)||(bitsAudio!=8&&bitsAudio!=16)||rate<1||rate>192000)throw std::runtime_error("Invalid PCM format");}
 else if(b[o]>='0'&&b[o]<='9'&&b[o+1]>='0'&&b[o+1]<='9'){int id=(b[o]-'0')*10+b[o+1]-'0';if(id==video&&(b[o+2]=='d'&&(b[o+3]=='b'||b[o+3]=='c')))p->frames.emplace_back(d,n);if(id==audio&&b[o+2]=='w'&&b[o+3]=='b')rawAudio.insert(rawAudio.end(),b.begin()+d,b.begin()+d+n);}
 size_t padded=n+(n&1);if(padded>end-d)throw std::runtime_error("Missing chunk padding");o=d+padded;}};
 walk(12,size_t(u32(4))+8,-1);if(video<0||p->w<=0||p->w>8192||p->h==0||p->h < -8192||p->h>8192||p->step<=0||p->frames.empty())throw std::runtime_error("Invalid video metadata");
 size_t stride=(size_t(p->w)*(p->bits/8)+3)&~size_t(3);for(auto f:p->frames)if(f.second<stride*size_t(std::abs(p->h)))throw std::runtime_error("Short video frame");
 if(audio>=0){if(!channels||!rate||!bitsAudio)throw std::runtime_error("Missing PCM format");size_t align=channels*(bitsAudio/8);if(rawAudio.size()%align)throw std::runtime_error("Partial PCM frame");p->pcm=std::make_shared<PCM>();p->pcm->rate=rate;for(size_t o=0;o<rawAudio.size();o+=align)for(int c=0;c<2;++c){size_t a=o+(channels==1?0:c)*(bitsAudio/8);float v=bitsAudio==8?(int(rawAudio[a])-128)/128.f:int16_t(rawAudio[a]|uint16_t(rawAudio[a+1])<<8)/32768.f;p->pcm->stereo.push_back(v);}}
 return true;
 }catch(const std::exception& e){std::string error=e.what();teardown();p->error=error;return false;}}
bool Movie::decode(VideoFrame& out){if(p->paused||p->next>=p->frames.size())return false;out.width=p->w;out.height=std::abs(p->h);out.pts=p->next*p->step;out.rgba.resize(size_t(out.width)*out.height*4);size_t stride=(size_t(p->w)*(p->bits/8)+3)&~size_t(3);auto base=p->frames[p->next++].first;for(int y=0;y<out.height;++y)for(int x=0;x<out.width;++x){size_t s=base+(p->h>0?out.height-1-y:y)*stride+x*(p->bits/8),d=(size_t(y)*out.width+x)*4;out.rgba[d]=p->bytes[s+2];out.rgba[d+1]=p->bytes[s+1];out.rgba[d+2]=p->bytes[s];out.rgba[d+3]=255;}return true;}
std::shared_ptr<const PCM> Movie::audio()const{return p->pcm;}void Movie::pause(bool b){p->paused=b;}bool Movie::paused()const{return p->paused;}double Movie::mediaClock()const{return p->next*p->step;}
}
