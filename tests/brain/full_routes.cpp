// Input-only retail completion routes. Planner uses decoded ground and fruit positions.
// Player variable slots below follow the declarations in the retail player brains.
// 0x410450 runs those brains in object order; 0x442220 carries globals between projects.
#include "brain/Brain.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <queue>
#include <sstream>
#include <filesystem>
#include <tuple>
#include <stdexcept>
using namespace rb;
namespace fs=std::filesystem;
constexpr int playerHitSlot=12, playerMainSlot=15, playerJumpSlot=2, playerDragSlot=7;
static double mt(int x){return double(x)/kMt;}
struct Nav {
 static constexpr double step=2;
 int nx=0,nz=0; double x0=0,z0=0; std::vector<unsigned char> floor;
 int cell(double x,double z) const {int a=std::lround((x-x0)/step),b=std::lround((z-z0)/step);return a>=0&&b>=0&&a<nx&&b<nz?b*nx+a:-1;}
 std::pair<double,double> point(int c) const {return {x0+c%nx*step,z0+c/nx*step};}
 bool safe(int c)const{return c>=0&&c<int(floor.size())&&floor[c];}
 Nav() = default;
 Nav(const AssetStore& store,const ProjectData& pd) {
  Mesh mesh;
  for(auto&o:pd.objects) if(o.type==12)mesh=Mesh::decode(store.read(o.model));
  double xmax=-1e9,zmax=-1e9;x0=z0=1e9;
  for(auto&p:mesh.parts)for(auto&v:p.vertices){x0=std::min(x0,double(v[0])/1000);z0=std::min(z0,double(v[2])/1000);xmax=std::max(xmax,double(v[0])/1000);zmax=std::max(zmax,double(v[2])/1000);}
  if(mesh.parts.empty())return;
  x0=std::floor(x0/step)*step;z0=std::floor(z0/step)*step;nx=int((xmax-x0)/step)+2;nz=int((zmax-z0)/step)+2;floor.resize(nx*nz);
  for(auto&p:mesh.parts)for(auto&f:p.faces)for(size_t k=2;k<f.size();++k){auto a=p.vertices[f[0]],b=p.vertices[f[k-1]],c=p.vertices[f[k]];
   for(auto*v:{&a,&b,&c})for(auto&n:*v)n/=1000;
   double det=(b[2]-c[2])*(a[0]-c[0])+(c[0]-b[0])*(a[2]-c[2]);if(std::abs(det)<1e-8)continue;
   int loX=std::max(0,int((std::min({a[0],b[0],c[0]})-x0)/step)),hiX=std::min(nx-1,int((std::max({a[0],b[0],c[0]})-x0)/step)+1);
   int loZ=std::max(0,int((std::min({a[2],b[2],c[2]})-z0)/step)),hiZ=std::min(nz-1,int((std::max({a[2],b[2],c[2]})-z0)/step)+1);
   for(int z=loZ;z<=hiZ;z++)for(int x=loX;x<=hiX;x++){double px=x0+x*step,pz=z0+z*step,u=((b[2]-c[2])*(px-c[0])+(c[0]-b[0])*(pz-c[2]))/det,v=((c[2]-a[2])*(px-c[0])+(a[0]-c[0])*(pz-c[2]))/det;if(u>=-1e-7&&v>=-1e-7&&u+v<=1.0000001&&u*a[1]+v*b[1]+(1-u-v)*c[1]>0)floor[z*nx+x]=1;}
  }
 }
 std::vector<int> path(const World&w,int player,int banned=-1,int forced=-1)const {
  auto&p=w.elements[player];int start=cell(mt(p.pos.x),mt(p.pos.z));if(start<0)return {};
  std::vector<double>d(floor.size(),1e30);std::vector<int>prev(floor.size(),-1);std::vector<int>targets(floor.size(),-1);
  for(auto&e:w.elements)if((forced>=0?int(e.index)==forced:bool((e.id&64)&&(e.flags&1)&&int(e.index)!=banned))){int c=cell(mt(e.pos.x),mt(e.pos.z));if(c>=0&&(forced>=0||safe(c)))targets[c]=e.index;}
  using Q=std::pair<double,int>;std::priority_queue<Q,std::vector<Q>,std::greater<Q>>q;d[start]=0;q.push({0,start});int goal=-1;
  while(!q.empty()){auto [cost,c]=q.top();q.pop();if(cost!=d[c])continue;if(targets[c]>=0){goal=c;break;}
   int x=c%nx,z=c/nx;
   for(int dz=-1;dz<=1;dz++)for(int dx=-1;dx<=1;dx++){if(!dx&&!dz)continue;int xx=x+dx,zz=z+dz,n=zz*nx+xx;if(xx<0||zz<0||xx>=nx||zz>=nz)continue;
    double extra=(dx&&dz?1.414:1)+(safe(n)?0:25); // Penalize edges to keep the turning circle on the track.
    for(int az=-5;az<=5;az++)for(int ax=-5;ax<=5;ax++)if(xx+ax<0||zz+az<0||xx+ax>=nx||zz+az>=nz||!safe((zz+az)*nx+xx+ax))extra+=.15;
    if(cost+extra<d[n]){d[n]=cost+extra;prev[n]=c;q.push({d[n],n});}
   }
  }
  std::vector<int>out;for(int c=goal;c>=0;c=prev[c]){out.push_back(c);if(c==start)break;}std::reverse(out.begin(),out.end());return out;
 }
};
static std::map<int,std::set<int>> readKeys(const fs::path&p){std::ifstream f(p);if(!f)throw std::runtime_error("missing route "+p.string());std::string s((std::istreambuf_iterator<char>(f)),{}),part;std::stringstream ss(s);std::map<int,std::set<int>>out;while(std::getline(ss,part,';')){auto c=part.find(':');if(c==part.npos)continue;int t=std::stoi(part.substr(0,c));out[t]={};std::stringstream ks(part.substr(c+1));std::string k;while(std::getline(ks,k,','))if(k.find_first_not_of(" \r\n\t")!=k.npos)out[t].insert(std::stoi(k));}return out;}
int runFullRoutes(int argc,char**argv){try{
 if(argc<4)return 2;std::string game=argv[1];fs::path dir=argv[2];bool generate=std::string(argv[3])=="--generate";int from=std::getenv("RB_ROUTE_FROM")?std::atoi(std::getenv("RB_ROUTE_FROM")):1;int only=argc>4?std::stoi(argv[4]):0;
 AssetStore store;store.open(game);BrainVM vm;MockInput input;MockScene scene;MockAudio audio;vm.setServices(&scene,&audio,&input);
 int level=only?only:1,finished=0;bool generationStarted=from==1;
 for(int visit=0;visit<20;visit++){
  if(level==from)generationStarted=true;bool gen=generate&&generationStarted;char name[32];std::snprintf(name,sizeof(name),"level%03d",level);auto pd=ProjectData::load(store,std::string(name)+".prj");vm.load(pd,game);if(!vm.compileErrors().empty())throw std::runtime_error("compile errors");
  int player=-1;for(auto&e:vm.world().elements)if(e.id&8){player=e.index;break;}
  auto schedule=gen?std::map<int,std::set<int>>{}:readKeys(dir/(std::string(name)+".keys"));std::set<int>last={-1};std::ofstream out;if(gen)out.open(dir/(std::string(name)+".keys"));
  BrainVM forecast;MockInput futureInput;MockScene futureScene;MockAudio futureAudio;forecast.setServices(&futureScene,&futureAudio,&futureInput);if(gen){forecast.load(pd,game);forecast.tick();forecast.clearEvents();futureAudio.log.clear();}
  Nav nav;if(gen)nav=Nav(store,pd);std::vector<int>path;std::vector<std::set<int>> todo,escape;size_t todoAt=0;int lastFruit=-1,stale=0,banned=-1;int ticks=0,maxFruit=0,fruitGoal=0;for(auto&e:vm.world().elements)fruitGoal+=bool((e.id&64)&&(e.flags&1));
  for(;ticks<18000&&!vm.finished();ticks++){
   if(gen){if(ticks%6==0||level==1){input.keys.clear();if(level==1)input.keys={57};else if(level!=14&&player>=0){
    auto&w=vm.world();auto&p=w.elements[player];double x=mt(p.pos.x),z=mt(p.pos.z);
    if(w.globals[201]!=lastFruit){stale=0;banned=-1;}else stale++;if(stale>140&&!path.empty()){auto [a,b]=nav.point(path.back());double best=1e30;for(auto&e:w.elements)if((e.id&64)&&(e.flags&1)){double dd=std::hypot(mt(e.pos.x)-a,mt(e.pos.z)-b);if(dd<best){best=dd;banned=e.index;}}stale=0;}
    int forced=-1;if(level==13){int remaining=100;for(auto&e:w.elements)if(e.name.find("Time Limit")!=std::string::npos)remaining=e.credits;for(auto&e:w.elements)if(e.name=="TIME-RING-1"&&(e.flags&1)&&e.vars.size()>5&&e.vars[5]==0&&remaining<=15&&std::hypot(mt(e.pos.x)-x,mt(e.pos.z)-z)<100)forced=e.index;}
    if(ticks%10==0||w.globals[201]!=lastFruit||forced>=0){path=nav.path(w,player,banned,forced);lastFruit=w.globals[201];}
    double best=1e30;int near=0;for(int j=0;j<int(path.size());j++){auto[a,b]=nav.point(path[j]);double dd=std::hypot(a-x,b-z);if(dd<best){best=dd;near=j;}}
    if(!path.empty()){
     bool waypoint=int(path.size())-near>45;int end=waypoint?std::min(int(path.size())-1,near+30):int(path.size())-1;while(waypoint&&end>near&&!nav.safe(path[end]))--end;
     int goalFruit=forced;{auto[a,b]=nav.point(path.back());double best=1e30;for(auto&e:w.elements)if((e.id&64)&&(e.flags&1)){double dd=std::hypot(mt(e.pos.x)-a,mt(e.pos.z)-b);if(dd<best&&forced<0){best=dd;goalFruit=e.index;}}}
     auto potential=[&](const World& state){const auto&q=state.elements[player];double px=mt(q.pos.x),pz=mt(q.pos.z),v=1e9;for(int j=near;j<=end;j++){auto[a,b]=nav.point(path[j]);v=std::min(v,std::hypot(a-px,b-pz)+double(end-j)*2.1);}if(!waypoint&&goalFruit>=0){auto&g=state.elements[goalFruit];auto&old=w.elements[goalFruit];if(std::hypot(mt(g.pos.x-old.pos.x),mt(g.pos.z-old.pos.z))>2)v=std::hypot(mt(g.pos.x)-px,mt(g.pos.z)-pz);}
      double dy=waypoint?10.093-mt(q.pos.y):goalFruit<0?0:mt(state.elements[goalFruit].pos.y-q.pos.y);return std::hypot(v,dy);};
     if(todoAt>=todo.size()&&w.globals[206]==0&&p.vars[playerMainSlot]==0){
      struct Node{World state;int parent,action,steps,got;};
      struct Entry{double rank;int index;bool operator<(const Entry& b)const{return rank>b.rank;}};
      std::vector<Node>nodes;nodes.push_back({w,-1,0,0,-1});std::priority_queue<Entry>q;q.push({potential(w),0});std::map<std::tuple<int,int,int,int,int,int,int>,double>seen;
      int found=-1,expanded=0;double closest=1e9;int fallback=0;
      while(!q.empty()&&expanded<10000){auto entry=q.top();q.pop();auto n=std::move(nodes[entry.index].state);int parent=nodes[entry.index].parent;nodes[entry.index].state=World{};Node node{std::move(n),parent,nodes[entry.index].action,nodes[entry.index].steps,nodes[entry.index].got};auto& nref=node;expanded++;const auto& np=nref.state.elements[player];
       if(nref.got>=0&&nref.steps-nref.got>=(level==6||level==10||level==15?0:6)&&nav.safe(nav.cell(mt(np.pos.x),mt(np.pos.z)))){found=entry.index;break;}
       if(nref.steps>100)continue;
       bool allowJump=level!=13||forced>=0;for(int j=near;j<=end;j++)if(!nav.safe(path[j]))allowJump=true;
       for(int action=0;action<(allowJump?6:3);action++){
        futureInput.keys.clear();if(action%3==1)futureInput.keys.insert(75);if(action%3==2)futureInput.keys.insert(77);if(action>=3)futureInput.keys.insert(29);futureInput.any=!futureInput.keys.empty();// Speculation stays in this separate VM; the replay VM receives keys only.
        forecast.world()=nref.state;bool dead=false;int got=nref.got;
        for(int k=0;k<6&&!forecast.finished();k++){forecast.tick();forecast.clearEvents();futureAudio.log.clear();const auto& fw=forecast.world();const auto& fp=fw.elements[player];if(fw.globals[220]<w.globals[220]||(fp.vars[playerHitSlot]&&!p.vars[playerHitSlot])){dead=true;break;}if(((level==13&&!waypoint?goalFruit>=0&&(forced>=0?fw.elements[goalFruit].vars[5]!=0:!(fw.elements[goalFruit].flags&1)):fw.globals[201]>w.globals[201])||(waypoint&&potential(fw)<6&&nav.safe(nav.cell(mt(fp.pos.x),mt(fp.pos.z)))))&&got<0)got=nref.steps;}
        if(dead)continue;auto state=forecast.world();auto&fp=state.elements[player];double h=potential(state);int steps=nref.steps+1;
        auto key=std::make_tuple(int(std::floor(mt(fp.pos.x)/3)),int(std::floor(mt(fp.pos.z)/3)),int(std::floor(std::fmod(double(fp.rot.y)/kDg+720,360)/12)),int(std::floor(mt(fp.pos.y)/3)),int(fp.vars[playerJumpSlot]!=0),int(mt(fp.vars[playerDragSlot])*2),got>=0?1:0);
        double cost=steps*1.8;if(seen.count(key)&&seen[key]<=cost)continue;seen[key]=cost;
        int index=nodes.size();nodes.push_back({std::move(state),entry.index,action,steps,got});double rank=cost+h-(got>=0?100:0);q.push({rank,index});if(h<closest){closest=h;fallback=index;}
       }
      }
      bool succeeded=found>=0;todo.clear();todoAt=0;if(found<0)found=fallback;
      for(int i=found;i>0;i=nodes[i].parent){std::set<int>keys;int a=nodes[i].action;if(a%3==1)keys.insert(75);if(a%3==2)keys.insert(77);if(a>=3)keys.insert(29);todo.push_back(keys);}std::reverse(todo.begin(),todo.end());
      // Keep the safe post-pickup continuation as a fallback, without spending it by default.
      if(succeeded){size_t cut=std::min(todo.size(),size_t(nodes[found].got+1));escape.assign(todo.begin()+cut,todo.end());todo.resize(cut);}else if(!escape.empty()){todo=std::move(escape);escape.clear();}
      std::cerr<<name<<" search t "<<ticks<<" expanded "<<expanded<<" steps "<<todo.size()<<" found "<<(nodes[found].got>=0)<<"\n";
     }
     if(todoAt<todo.size())input.keys=todo[todoAt++];
    }
   }
   }
   if(input.keys!=last){if(ticks)out<<';';out<<ticks<<':';bool comma=false;for(int k:input.keys){if(comma)out<<',';out<<k;comma=true;}last=input.keys;}
   }else{auto it=schedule.find(ticks);if(it!=schedule.end())input.keys=it->second;}
   int oldLives=vm.world().globals[220];auto oldP=vm.world().elements[player].pos;input.any=!input.keys.empty();vm.tick();if(vm.world().globals[220]<0)throw std::runtime_error(std::string("all lives lost in ")+name);maxFruit=std::max(maxFruit,vm.world().globals[201]);if(gen&&vm.world().globals[220]<oldLives)std::cerr<<"DIED t "<<ticks<<" at "<<mt(oldP.x)<<","<<mt(oldP.z)<<" y "<<mt(oldP.y)<<" fruit "<<vm.world().globals[201]<<" collided "<<vm.world().elements[player].collided<<"\n";vm.clearEvents();audio.log.clear();
   if(gen&&ticks%300==0&&player>=0){auto&p=vm.world().elements[player];std::cerr<<name<<" t "<<ticks<<" fruit "<<vm.world().globals[201]<<" lives "<<vm.world().globals[220]<<" pos "<<mt(p.pos.x)<<","<<mt(p.pos.z)<<" path "<<path.size()<<'\n';}
  }
  if(gen)out<<'\n';auto&w=vm.world();std::cout<<name<<" exit="<<w.exitCode<<" ticks="<<ticks<<" score="<<w.globals[0]<<" lives="<<w.globals[220]<<" stars="<<w.globals[221]<<" fruit="<<maxFruit<<"/"<<fruitGoal<<std::endl;
  if(!vm.unknownCommands().empty())throw std::runtime_error("unknown commands");
  // 0x441cb0 maps a positive exit directly to levelNNN; multilev rows select loading art.
  int expected=level==1?3:level==14?1:level==15?13:level==12?15:level+1;
  if(w.exitCode!=expected||w.globals[220]<0||(level!=1&&level!=14&&maxFruit<fruitGoal)){std::cerr<<"BLOCKED "<<name<<" expected "<<expected<<'\n';return 1;}
  if(level!=1)finished++;
  if(level==14){std::cout<<"FINISHED "<<finished<<"/13 score="<<w.globals[0]<<'\n';return finished==13?0:1;}
  if(only)return 0;level=w.exitCode;
 }
 return 1;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}

#ifndef RB_ROUTES_LIBRARY
int main(int argc, char** argv) { return runFullRoutes(argc, argv); }
#endif
