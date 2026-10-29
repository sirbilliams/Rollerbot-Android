#include "audio/Audio.h"
#include <SDL.h>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
static void check(bool b){if(!b)throw std::runtime_error("audio check failed");}
int main(int argc,char**argv){try{rb::Audio a;check(a.init(48000,false));auto pcm=std::make_shared<rb::PCM>();for(int i=0;i<4800;++i){float v=.25f*std::sin(i*440*6.2831853/48000);pcm->stereo.push_back(v);pcm->stereo.push_back(v);}auto h=a.play(pcm,true);float b[960]{};a.pauseAll();a.mix(b,480);check(a.mediaClock()==0);a.resumeAll();a.pan(h,10000);a.mix(b,480);check(std::abs(b[3])>std::abs(b[2])*100);a.volume(h,-10000);a.mix(b,480);for(float v:b)check(v==0);a.stop(h);a.play(pcm,true);std::vector<float> out(48000*2);a.mix(out.data(),48000);check(a.mediaClock()>1);a.shutdown();
 std::ofstream f(argc>1?argv[1]:"mix.wav",std::ios::binary);auto u16=[&](unsigned v){f.put(v);f.put(v>>8);};auto u32=[&](unsigned v){u16(v);u16(v>>16);};f.write("RIFF",4);u32(36+out.size()*2);f.write("WAVEfmt ",8);u32(16);u16(1);u16(2);u32(48000);u32(192000);u16(4);u16(16);f.write("data",4);u32(out.size()*2);for(float v:out)u16(uint16_t(int16_t(v*32767)));f.close();
 std::ifstream wav(argc>1?argv[1]:"mix.wav",std::ios::binary);std::vector<char> bytes((std::istreambuf_iterator<char>(wav)),{});check(bool(a.loadWav(bytes.data(),bytes.size())));check(!a.loadWav("bad",3));check(a.init());SDL_Delay(30);a.shutdown();
 if(argc>2){rb::Movie movie;check(movie.open(argv[2]));check(bool(movie.audio()));check(movie.audio()->rate==8000&&movie.audio()->stereo.size()==8);check(std::abs(movie.audio()->stereo[2]-1000.f/32768)<.00001f);rb::VideoFrame frame;movie.pause(true);check(!movie.decode(frame));movie.pause(false);check(movie.decode(frame));check(frame.rgba[0]==255&&frame.rgba[1]==0&&frame.rgba[3]==255);if(argc>3){std::ofstream raw(argv[3],std::ios::binary);raw.write((char*)frame.rgba.data(),frame.rgba.size());}check(movie.mediaClock()>0);check(!movie.decode(frame));movie.teardown();check(!movie.open("/nonexistent.avi"));}
 std::cout<<"audio/movie checks passed\n";return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
