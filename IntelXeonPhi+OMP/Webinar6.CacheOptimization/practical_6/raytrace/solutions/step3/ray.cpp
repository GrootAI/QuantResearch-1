#include <iostream>
#include <array>
#include <random>
#include <chrono>

#include <sdlt/sdlt.h>


using namespace std;


constexpr int dim=3;
typedef float real_t;

class ray_t{
  public:
    real_t   x, y, z;
    real_t  px,py,pz;

    ray_t operator+(ray_t r)const{
      ray_t out;
      out.x=r.x+this->x;
      out.y=r.y+this->y;
      out.z=r.z+this->z;
      out.px=r.px+this->px;
      out.py=r.py+this->py;
      out.pz=r.pz+this->pz;
      return out;
    }
    ray_t operator*(real_t c)const{
      ray_t out;
      out.x=c*this->x;
      out.y=c*this->y;
      out.z=c*this->z;
      out.px=c*this->px;
      out.py=c*this->py;
      out.pz=c*this->pz;
      return out;
    }
};

SDLT_PRIMITIVE(
    ray_t,
    x,y,z,px,py,pz);


typedef sdlt::soa1d_container<ray_t> Container;

real_t vel(real_t x,real_t y,real_t z);
array<real_t,dim> dvel(real_t x,real_t y,real_t z);
ray_t dray_dt(ray_t r);
ray_t rk4(ray_t r0,real_t dt);



real_t vel(real_t x,real_t y,real_t z){return x*x+y*y+z*z;}

array<real_t,dim> dvel(real_t x,real_t y,real_t z){
  array<real_t,dim> out;
  out[0]=2*x;
  out[1]=2*y;
  out[2]=2*z;
  return out;
}

ray_t dray_dt(ray_t r){
  ray_t out;
  real_t v=vel (r.x,r.y,r.z);
  auto  dv=dvel(r.x,r.y,r.z);
  out.x=v*v*r.px;
  out.y=v*v*r.py;
  out.z=v*v*r.pz;
  out.px=-dv[0];
  out.py=-dv[1];
  out.pz=-dv[2];
  return out;
}
//Do one step of RK4 to advance a single ray in time.
ray_t rk4(ray_t r0,real_t dt){
  ray_t k1=dray_dt(r0);
  ray_t k2=dray_dt(r0+k1*0.5*dt);
  ray_t k3=dray_dt(r0+k2*0.5*dt);
  ray_t k4=dray_dt(r0+k3*dt);
  return r0 + (k1+k2*2.0+k3*2.0+k4)*(dt/6.0);
}



int main(int argc,char** argv){

  random_device rd;
  mt19937 gen(rd());
  uniform_real_distribution<> dis(-1,1);

  if(argc<=1){cout<<"give number of rays\n";return 1;}
  int n=stoi(string(argv[1]),nullptr,10);
  if(argc<=2){cout<<"Give number of timesteps\n";return 1;}
  int nsteps=stoi(string(argv[2]),nullptr,10);

//  vector<ray_t> rays(n);
  Container rays(n);

#pragma forceinline recursive
  {
  const int nrays=rays.get_size_d1();
  for(int i=0;i<nrays;i++){
    rays[i].x()=0;
    rays[i].y()=0;
    rays[i].z()=0;

    rays[i].px()=dis(gen);
    rays[i].py()=dis(gen);
    rays[i].pz()=dis(gen);
  }

#define BLOCK 512
#define MIN(x,y) (((x)<(y))?(x):(y))
  auto start = std::chrono::steady_clock::now();
  real_t dt =0.1;
  //Loop over all rays and step nsteps in time 

#pragma omp parallel for
  for(int jj=0;jj<nrays;jj+=BLOCK){
    for(int i=0;i<nsteps;i++){
      int jend=MIN(nrays,jj+BLOCK);
#pragma omp simd
      for(int j=jj;j<jend;j++){
        rays[j]=rk4(rays[j],dt);
      }
    }
  } 
  auto finish = std::chrono::steady_clock::now();
  std::cout << std::chrono::duration_cast<std::chrono::duration<double> >(finish-start).count() << "s\n";
  }







}
