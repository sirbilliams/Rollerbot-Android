#include "Audio.h"
#include <SDL.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <mutex>
#include <unordered_map>
namespace rb {
struct Audio::Impl {
 struct Voice { std::shared_ptr<const PCM> pcm; double cursor=0; bool looping=false,spatial=false; int db=0,pan=0; float speed=1; float x=0,y=0,z=0; };
 mutable std::mutex mutex; SDL_AudioDeviceID device=0; int rate=48000; bool paused=false,owned=false; uint64_t frames=0; Handle next=1;
 float x=0,y=0,z=0; int master=0; float peak=0; std::unordered_map<Handle,Voice> voices;
};
Audio::Audio():p(new Impl){} Audio::~Audio(){shutdown();}
// 0x45c170: device/primary initialization; SDL owns the output device here.
bool Audio::init(int rate,bool start){ shutdown(); if(rate<8000||rate>192000)return false; p->rate=rate;p->frames=0;p->paused=false;
 if(!start)return true;
 if(SDL_InitSubSystem(SDL_INIT_AUDIO))return false;p->owned=true;
 SDL_AudioSpec want{};want.freq=rate;want.format=AUDIO_F32SYS;want.channels=2;want.samples=1024;want.userdata=this;
 want.callback=[](void* u,Uint8* b,int n){static_cast<Audio*>(u)->mix(reinterpret_cast<float*>(b),size_t(n)/8);};
 p->device=SDL_OpenAudioDevice(nullptr,0,&want,nullptr,0);if(!p->device){shutdown();return false;} SDL_PauseAudioDevice(p->device,0);return true;
}
void Audio::shutdown(){if(p->device)SDL_CloseAudioDevice(p->device);p->device=0;{std::lock_guard<std::mutex> l(p->mutex);p->voices.clear();}if(p->owned)SDL_QuitSubSystem(SDL_INIT_AUDIO);p->owned=false;}
std::shared_ptr<const PCM> Audio::loadWav(const void* bytes,size_t size){
 if(!bytes||size>size_t(std::numeric_limits<int>::max()))return {};
 SDL_AudioSpec spec{};Uint8* data=nullptr;Uint32 len=0;
 if(!SDL_LoadWAV_RW(SDL_RWFromConstMem(bytes,int(size)),1,&spec,&data,&len))return {};
 SDL_AudioCVT cvt{};int r=SDL_BuildAudioCVT(&cvt,spec.format,spec.channels,spec.freq,AUDIO_F32SYS,2,p->rate);
 if(r<0||len>size_t(std::numeric_limits<int>::max())/std::max(1,cvt.len_mult)){SDL_FreeWAV(data);return {};}
 cvt.len=int(len); std::vector<Uint8> buf(size_t(len)*std::max(1,cvt.len_mult));std::memcpy(buf.data(),data,len);SDL_FreeWAV(data);cvt.buf=buf.data();
 if(r && SDL_ConvertAudio(&cvt))return {};auto out=std::make_shared<PCM>();out->rate=p->rate;out->srcRate=spec.freq;out->stereo.resize(size_t(r?cvt.len_cvt:cvt.len)/sizeof(float));std::memcpy(out->stereo.data(),buf.data(),out->stereo.size()*sizeof(float));return out;
}
// Original object start restarts a resident secondary buffer at cursor zero.
Audio::Handle Audio::play(std::shared_ptr<const PCM> pcm,bool loop){std::lock_guard<std::mutex> l(p->mutex);if(!pcm||pcm->rate<=0||pcm->stereo.empty()||pcm->stereo.size()%2)return 0;while(!p->next||p->voices.count(p->next))++p->next;auto h=p->next++;p->voices.emplace(h,Impl::Voice{std::move(pcm),0,loop});return h;}
void Audio::stop(Handle h){std::lock_guard<std::mutex> l(p->mutex);p->voices.erase(h);}
void Audio::loop(Handle h,bool b){std::lock_guard<std::mutex> l(p->mutex);auto i=p->voices.find(h);if(i!=p->voices.end())i->second.looping=b;}
void Audio::volume(Handle h,int v){std::lock_guard<std::mutex> l(p->mutex);auto i=p->voices.find(h);if(i!=p->voices.end())i->second.db=std::clamp(v,-10000,0);}
void Audio::pan(Handle h,int v){std::lock_guard<std::mutex> l(p->mutex);auto i=p->voices.find(h);if(i!=p->voices.end())i->second.pan=std::clamp(v,-10000,10000);}
void Audio::speed(Handle h,float r){if(!std::isfinite(r)||r<=0)return;std::lock_guard<std::mutex> l(p->mutex);auto i=p->voices.find(h);if(i!=p->voices.end())i->second.speed=r;}
void Audio::masterVolume(int v){std::lock_guard<std::mutex> l(p->mutex);p->master=std::clamp(v,-10000,0);}
int Audio::masterVolume()const{std::lock_guard<std::mutex> l(p->mutex);return p->master;}
float Audio::takePeak(){std::lock_guard<std::mutex> l(p->mutex);float r=p->peak;p->peak=0;return r;}
void Audio::position(Handle h,float x,float y,float z){if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z))return;std::lock_guard<std::mutex> l(p->mutex);auto i=p->voices.find(h);if(i!=p->voices.end()){auto& v=i->second;v.spatial=true;v.x=x;v.y=y;v.z=z;}}
void Audio::listener(float x,float y,float z){std::lock_guard<std::mutex> l(p->mutex);p->x=x;p->y=y;p->z=z;}
void Audio::pauseAll(){std::lock_guard<std::mutex> l(p->mutex);p->paused=true;}void Audio::resumeAll(){std::lock_guard<std::mutex> l(p->mutex);p->paused=false;}
double Audio::mediaClock()const{std::lock_guard<std::mutex> l(p->mutex);return double(p->frames)/p->rate;}
void Audio::mix(float* out,size_t frames){std::lock_guard<std::mutex> l(p->mutex);std::fill(out,out+frames*2,0.f);if(p->paused)return;
 for(auto it=p->voices.begin();it!=p->voices.end();){auto& v=it->second;size_t n=v.pcm->stereo.size()/2;float gain=v.db<=-10000?0:std::pow(10.f,v.db/2000.f);if(p->master<=-10000)gain=0;else gain*=std::pow(10.f,p->master/2000.f);float pan=v.pan/10000.f;
 if(v.spatial){float x=v.x-p->x,y=v.y-p->y,z=v.z-p->z;float d=std::sqrt(x*x+y*y+z*z);gain/=std::max(1.f,1.f+.7f*(d-1));pan=std::clamp(x/std::max(1.f,d),-1.f,1.f);}
 float left=gain*(pan>0?std::pow(10.f,-pan*5):1),right=gain*(pan<0?std::pow(10.f,pan*5):1);
 for(size_t f=0;f<frames;++f){if(v.cursor>=n){if(v.looping)v.cursor=std::fmod(v.cursor,double(n));else break;}size_t a=size_t(v.cursor),b=a+1<n?a+1:(v.looping?0:a);float t=float(v.cursor-a);for(int c=0;c<2;++c)out[f*2+c]+=(v.pcm->stereo[a*2+c]*(1-t)+v.pcm->stereo[b*2+c]*t)*(c?right:left);v.cursor+=double(v.pcm->rate)/p->rate*v.speed;}
 if(!v.looping&&v.cursor>=n)it=p->voices.erase(it);else ++it;}
 for(size_t i=0;i<frames*2;++i){p->peak=std::max(p->peak,std::fabs(out[i]));out[i]=std::clamp(out[i],-1.f,1.f);}p->frames+=frames;
}
}
