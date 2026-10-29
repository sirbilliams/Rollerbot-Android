#pragma once
#include <cstdint>
#include <cstddef>
#include <memory>
#include <vector>
#include <string>
namespace rb {
struct PCM { int rate=48000; std::vector<float> stereo; };
class Audio {
public:
 using Handle=uint32_t;
 Audio(); ~Audio(); Audio(const Audio&)=delete; Audio& operator=(const Audio&)=delete;
 bool init(int rate=48000, bool startDevice=true); void shutdown();
 std::shared_ptr<const PCM> loadWav(const void* bytes,size_t size);
 Handle play(std::shared_ptr<const PCM>,bool loop=false); void stop(Handle); void loop(Handle,bool);
 // DirectSound units: volume [-10000,0], pan [-10000,10000]. Position in meters.
 void volume(Handle,int hundredthsDB); void pan(Handle,int hundredthsDB);
 void position(Handle,float x,float y,float z); void listener(float x,float y,float z);
 void pauseAll(); void resumeAll(); double mediaClock() const;
 // Same mixer as device callback. For offline use initialize with startDevice=false.
 void mix(float* interleavedStereo,size_t frames);
private: struct Impl; std::unique_ptr<Impl> p;
};
struct VideoFrame { int width=0,height=0; double pts=0; std::vector<uint8_t> rgba; };
class Movie {
public:
 Movie(); ~Movie(); Movie(const Movie&)=delete; Movie& operator=(const Movie&)=delete;
 // Supports BI_RGB 24/32-bit AVI + PCM audio; rejects other codecs explicitly.
 bool open(const std::string& file); bool decode(VideoFrame&);
 std::shared_ptr<const PCM> audio() const; void pause(bool); bool paused() const;
 double mediaClock() const; void teardown(); const std::string& error() const;
private: struct Impl; std::unique_ptr<Impl> p;
};
}
