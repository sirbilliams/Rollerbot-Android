#include "Assets.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <functional>
#include <iomanip>
#include <regex>
#include <sstream>
#include <stdexcept>
namespace rb {
namespace {
void require(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
struct Reader {
 const Bytes& b;
 void check(size_t p,size_t n) const { require(p<=b.size() && n<=b.size()-p,"truncated asset"); }
 uint32_t u(size_t p,size_t n=4) const { check(p,n);uint32_t v=0;for(size_t i=0;i<n;i++)v|=uint32_t(b[p+i])<<(8*i);return v; }
 float f(size_t p) const {auto v=u(p);float x;std::memcpy(&x,&v,4);require(std::isfinite(x),"nonfinite vertex");return x;}
};
bool starts(const Bytes& b,const std::string& s) {return b.size()>=s.size() && std::equal(s.begin(),s.end(),b.begin());}
std::string lower(std::string s){for(auto& c:s)if(c>='A'&&c<='Z')c+=32;return s;}
std::vector<std::string> split(const std::string& s,char c){std::vector<std::string> a;size_t p=0,q;do{q=s.find(c,p);a.push_back(s.substr(p,q==s.npos?q:q-p));p=q+1;}while(q!=s.npos);return a;}
Bytes disk(const std::filesystem::path& p){std::ifstream f(p,std::ios::binary);require(bool(f),"cannot open asset");return Bytes(std::istreambuf_iterator<char>(f),{});}
}
std::string assetKind(const Bytes& b){
 if(starts(b,"BM"))return "bmp";if(b.size()>=4&&b[0]==10&&b[2]==1&&b[3]==8)return "pcx";
 if(starts(b,"RIFF")&&b.size()>=12&&std::string(b.begin()+8,b.begin()+12)=="WAVE")return "wav";
 for(auto s:{"xof ","MM","d3m.","d30.",";v1.","1|./"})if(starts(b,s))return s==std::string("xof ")?"x":s==std::string("MM")?"3ds":s==std::string("d3m.")?"d3m":s==std::string("d30.")?"d30":s==std::string(";v1.")?"prj":"descriptor";
 return "unknown";
}
void AssetStore::open(const std::filesystem::path& p){require(std::filesystem::is_directory(p),"invalid game directory");root_=std::filesystem::canonical(p);}
uint32_t AssetStore::hash(const std::string& s){uint32_t h=0;for(unsigned char c:s){if(!c)break;h=h*31+static_cast<int8_t>(c);}return h;}
std::string AssetStore::assetName(const std::string& s){ // 0x441700: basename is only used for the null sound comparison.
 auto p=s.find_last_of("/\\:");auto base=s.substr(p==s.npos?0:p+1);if(base=="null_.wav")return base;
 std::ostringstream o;o<<std::uppercase<<std::hex<<std::setfill('0')<<std::setw(8)<<hash(s)<<".dat";return o.str();
}
std::optional<std::filesystem::path> AssetStore::find(const std::string& name) const {
 require(!root_.empty(),"asset store not open");
 std::string n=name;std::replace(n.begin(),n.end(),'\\','/');
 auto lookup=[&](const std::string& s)->std::optional<std::filesystem::path>{
  auto rel=std::filesystem::path(s).lexically_normal();if(rel.is_absolute())return {};for(const auto& c:rel)if(c=="..")return {};
  auto p=root_/rel;if(std::filesystem::is_regular_file(p))return p;
  if(rel.has_parent_path()&&rel.parent_path()!=".")return {};
  for(const auto& e:std::filesystem::directory_iterator(root_))if(e.is_regular_file()&&lower(e.path().filename().string())==lower(rel.filename().string()))return e.path();return {};
 };
 if(auto p=lookup(n))return p;return lookup(assetName(name));
}
Bytes AssetStore::read(const std::string& n) const {auto p=find(n);require(bool(p),"asset not found");auto b=disk(*p);if(assetKind(b)!="unknown")return b;
 auto d=b;Reader r{b};for(size_t i=0;i+4<=b.size();i+=4){auto v=r.u(i)^0xe195fd83u;v=(v>>17)|(v<<15);for(int j=0;j<4;j++)d[i+j]=v>>(j*8);}return assetKind(d)!="unknown"?d:b;}
ProjectData ProjectData::load(const AssetStore& s,const std::string& p){auto c=std::filesystem::path(p);c.replace_extension(".col");return parse(s.read(p),s.read(c.string()));}
ProjectData ProjectData::parse(const Bytes& b,const Bytes& col){
 ProjectData p;auto lines=split(std::string(b.begin(),b.end()),'\n');for(auto& l:lines)if(!l.empty()&&l.back()=='\r')l.pop_back();require(lines.size()>=2&&starts(b,";v1."),"invalid project");p.version=lines[0];size_t n=std::stoul(lines[1]);require(n<=lines.size()-2,"invalid object count");
 for(size_t i=0;i<n;i++){ObjectRecord o;o.fields=split(lines[i+2],'|');require(o.fields.size()>35,"short object record");o.name=o.fields[0];o.model=o.fields[1];o.sound=o.fields[2];for(int k=0;k<3;k++)o.position[k]=std::stod(o.fields[k+4]);o.type=std::stoi(o.fields[35]);p.objects.push_back(std::move(o));}
 for(size_t i=n+2;i<lines.size();i++){auto& l=lines[i];if(!l.empty()&&l[0]=='@'&&l.size()>1&&std::all_of(l.begin()+1,l.end(),[](char c){return c>='0'&&c<='9';})){BrainRecord br;br.objectIndex=std::stoul(l.substr(1));p.brains.push_back(std::move(br));}else if(!p.brains.empty())p.brains.back().lines.push_back(l);else p.extraLines.push_back(l);}
 require(col.size()%4==0,"invalid collection");Reader r{col};for(size_t i=0;i<col.size();i+=4)p.collection.push_back(static_cast<int32_t>(r.u(i)));return p;
}
Sound Sound::decode(const Bytes& b){require(assetKind(b)=="wav","not WAV");Reader r{b};size_t declared=size_t(r.u(4))+8;require(declared>=12,"invalid RIFF size");size_t end=std::min(declared,b.size()); // Retail RIFF sizes can overstate EOF; each chunk still must fit.
Sound s;bool fmt=false,data=false;
 for(size_t p=12;p+8<=end;){size_t n=r.u(p+4),q=p+8;require(n<=end-q,"bad WAV chunk");std::string tag(b.begin()+p,b.begin()+p+4);if(tag=="fmt "){require(n>=16,"short WAV format");s.format=r.u(q,2);s.channels=r.u(q+2,2);s.sampleRate=r.u(q+4);s.blockAlign=r.u(q+12,2);s.bitsPerSample=r.u(q+14,2);fmt=true;}if(tag=="data"){s.samples.insert(s.samples.end(),b.begin()+q,b.begin()+q+n);data=true;}p=q+n+(n&1);}
 require(fmt&&data&&s.format==1&&s.channels&&s.sampleRate&&(s.bitsPerSample==8||s.bitsPerSample==16)&&s.blockAlign==s.channels*(s.bitsPerSample/8)&&s.samples.size()%s.blockAlign==0,"unsupported WAV");return s;}
Image Image::decode(const Bytes& b){Reader r{b};Image im;auto kind=assetKind(b);Bytes raw;size_t stride=0,planes=0,pal=0;
 if(kind=="pcx") {r.check(0,128);im.width=r.u(8,2)-r.u(4,2)+1;im.height=r.u(10,2)-r.u(6,2)+1;planes=r.u(65,1);stride=r.u(66,2);require((planes==1||planes==3)&&stride>=im.width&&stride<=16384&&im.height<=16384,"unsupported PCX");size_t need=stride*planes*im.height;size_t p=128;while(raw.size()<need){auto v=r.u(p++,1);size_t count=1;if(v>=192){count=v&63;v=r.u(p++,1);}require(count&&count<=need-raw.size(),"bad PCX run");raw.insert(raw.end(),count,v);}if(planes==1){require(b.size()>=769&&b[b.size()-769]==12,"missing PCX palette");pal=b.size()-768;} }
 else {require(kind=="bmp","not image");r.check(0,54);auto w=static_cast<int32_t>(r.u(18)),h=static_cast<int32_t>(r.u(22));require(w>0&&w<=16384&&h!=0&&h>=-16384&&h<=16384,"bad BMP dimensions");im.width=w;im.height=std::abs(h);auto bits=r.u(28,2),comp=r.u(30);require((bits==8||bits==24||bits==32)&&(comp==0||(comp==1&&bits==8)),"unsupported BMP");pal=14+r.u(14);size_t off=r.u(10);stride=((im.width*bits+31)/32)*4;raw.resize(size_t(im.width)*im.height*3);
 auto pixel=[&](size_t x,size_t y,size_t pos){require(x<im.width&&y<im.height,"BMP pixel bounds");size_t dest=((h>0?im.height-1-y:y)*im.width+x)*3;r.check(pos,3);for(int c=0;c<3;c++)raw[dest+c]=b[pos+2-c];};
 if(comp==0){r.check(off,stride*im.height);for(size_t y=0;y<im.height;y++)for(size_t x=0;x<im.width;x++){size_t pos=off+y*stride+x*(bits/8);pixel(x,y,bits==8?pal+4*r.u(pos,1):pos);}}
 else {Bytes idx(size_t(im.width)*im.height);size_t x=0,y=0,p=off;bool ended=false;while(p<b.size()){auto count=r.u(p++,1),v=r.u(p++,1);if(!count&&v==1){ended=true;break;}if(!count&&v==0){x=0;y++;continue;}if(!count&&v==2){x+=r.u(p++,1);y+=r.u(p++,1);require(x<=im.width&&y<=im.height,"BMP delta bounds");continue;}size_t n=count?count:v;require(y<im.height&&x+n<=im.width,"BMP run bounds");for(size_t i=0;i<n;i++)idx[y*im.width+x++]=count?v:r.u(p++,1);if(!count&&(n&1))p++;}require(ended,"missing BMP end");for(size_t yy=0;yy<im.height;yy++)for(size_t xx=0;xx<im.width;xx++)pixel(xx,yy,pal+4*idx[yy*im.width+xx]);}im.rgb=std::move(raw);return im;}
 require(im.width&&im.height,"empty image");im.rgb.resize(size_t(im.width)*im.height*3);for(size_t y=0;y<im.height;y++)for(size_t x=0;x<im.width;x++)for(size_t c=0;c<3;c++)im.rgb[(y*im.width+x)*3+c]=planes==1?b[pal+3*raw[y*stride+x]+c]:raw[y*stride*3+c*stride+x];return im;}
Mesh Mesh::decode(const Bytes& b){Reader r{b};Mesh m;auto kind=assetKind(b);
 if(kind=="d3m"){auto start=r.u(4),end=r.u(8);require(end>start&&(end-start)%36==0,"invalid d3m span");r.check(64,end-start);MeshPart part;for(size_t p=64;p<64+size_t(end-start);p+=12)part.vertices.push_back({float(static_cast<int32_t>(r.u(p))),float(static_cast<int32_t>(r.u(p+4))),float(static_cast<int32_t>(r.u(p+8)))});for(uint32_t i=0;i<part.vertices.size();i+=3)part.faces.push_back({i,i+1,i+2});m.parts.push_back(std::move(part));}
 else if(kind=="3ds"){
 std::function<void(size_t,size_t,int)> walk=[&](size_t p,size_t end,int current){while(p<end){require(end-p>=6,"short 3DS chunk");auto tag=r.u(p,2);size_t len=r.u(p+2);require(len>=6&&len<=end-p,"bad 3DS chunk");size_t q=p+6,stop=p+len;
 if(tag==0x4000){while(q<stop&&b[q])q++;require(q<stop,"unterminated 3DS name");m.parts.emplace_back();walk(q+1,stop,int(m.parts.size()-1));}
 else if(tag==0x4d4d||tag==0x3d3d||tag==0x4100)walk(q,stop,current);
 else if((tag==0x4110||tag==0x4120)&&current>=0){auto n=r.u(q,2);q+=2;size_t size=tag==0x4110?12:8;require(size*n<=stop-q,"short 3DS array");auto& part=m.parts[current];for(size_t i=0;i<n;i++,q+=size)if(tag==0x4110)part.vertices.push_back({r.f(q),r.f(q+4),r.f(q+8)});else part.faces.push_back({r.u(q,2),r.u(q+2,2),r.u(q+4,2)});}
 p=stop;}};walk(0,b.size(),-1);}
 else {require(kind=="x","not geometry");r.check(0,16);std::vector<std::string> t;
 if(std::string(b.begin()+8,b.begin()+12)=="txt "){std::string s(b.begin()+16,b.end());std::regex re("[A-Za-z_][A-Za-z_0-9.-]*|[-+]?[0-9]*\\.?[0-9]+(?:[eE][-+]?[0-9]+)?|[{}]");for(std::sregex_iterator it(s.begin(),s.end(),re),end;it!=end;++it)t.push_back(it->str());}
 else {require(std::string(b.begin()+8,b.begin()+12)=="bin ","compressed X unsupported");for(size_t p=16;p<b.size();){auto code=r.u(p,2);p+=2;if(code==1||code==2){auto n=r.u(p);p+=4;r.check(p,n);t.emplace_back(b.begin()+p,b.begin()+p+n);while(!t.back().empty()&&t.back().back()==0)t.back().pop_back();p+=n+(code==2?2:0);}
 else if(code==3){t.push_back(std::to_string(r.u(p)));p+=4;}else if(code==6||code==7){auto n=r.u(p);p+=4;size_t size=code==7&&std::string(b.begin()+12,b.begin()+16)=="0064"?8:4;r.check(p,size_t(n)*size);for(size_t i=0;i<n;i++,p+=size){if(code==6)t.push_back(std::to_string(r.u(p)));else if(size==4)t.push_back(std::to_string(r.f(p)));else {double x;r.check(p,8);uint64_t v=uint64_t(r.u(p))|(uint64_t(r.u(p+4))<<32);std::memcpy(&x,&v,8);require(std::isfinite(x),"nonfinite X float");t.push_back(std::to_string(x));}}}
 else if(code==5){r.check(p,16);p+=16;}else if(code==10||code==11)t.push_back(code==10?"{":"}");else require((code>=12&&code<=20)||code==31||(code>=40&&code<=52),"unknown X token");}}
 for(size_t i=0;i<t.size();i++)if(t[i]=="Mesh"){size_t j=i+1;require(j<t.size(),"short X mesh");if(t[j]!="{")j++;if(j>=t.size()||t[j]!="{")continue;j++;auto next=[&](){require(j<t.size(),"short X array");return t[j++];};auto count=[&](){auto n=std::stoul(next());require(n<=b.size(),"bad X count");return n;};MeshPart p;auto n=count();for(size_t k=0;k<n;k++)p.vertices.push_back({std::stof(next()),std::stof(next()),std::stof(next())});n=count();for(size_t k=0;k<n;k++){std::vector<uint32_t> f;auto nf=count();for(size_t a=0;a<nf;a++)f.push_back(count());p.faces.push_back(std::move(f));}m.parts.push_back(std::move(p));}}
 require(!m.parts.empty(),"no mesh");for(auto& p:m.parts){for(auto& v:p.vertices)for(auto c:v)require(std::isfinite(c),"nonfinite mesh");for(auto& f:p.faces){require(f.size()>=3,"short face");for(auto i:f)require(i<p.vertices.size(),"invalid mesh index");}}return m;}
}
