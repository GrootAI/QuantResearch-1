#ifndef _SPARSE_H_
#define _SPARSE_H_
#include <stdint.h>
#include <cmath>
#include <vector>
#include <iostream>

#include "mcdramvec.h"





template <typename real_t>
mcdramvec::vector<real_t> operator+(const mcdramvec::vector<real_t>& a,const mcdramvec::vector<real_t>& b){
  mcdramvec::vector<real_t> out(a);
#pragma omp parallel for simd schedule(static)
  for(int64_t i=0;i<a.size();i++){
    out[i]=a[i]+b[i];
  }
  return out;
}

template <typename real_t>
mcdramvec::vector<real_t> operator*(const mcdramvec::vector<real_t>& a,const real_t& b){
  mcdramvec::vector<real_t> out(a);
#pragma omp parallel for simd schedule(static)
  for(int64_t i=0;i<a.size();i++){
    real_t _b=b;
    out[i]=a[i]*_b;
  }
  return out;
}

template <typename real_t>
mcdramvec::vector<real_t> operator*(const real_t& b,const mcdramvec::vector<real_t>& a){
  mcdramvec::vector<real_t> out(a);
#pragma omp parallel for simd schedule(static)
  for(int64_t i=0;i<a.size();i++){
    real_t _b=b;
    out[i]=a[i]*_b;
  }
  return out;
}

template <typename real_t>
real_t dot(const mcdramvec::vector<real_t>& a,const mcdramvec::vector<real_t>& b){
  real_t out=0.0;
#pragma omp parallel for simd schedule(static) reduction(+:out)
  for(int64_t i=0;i<a.size();i++){
    out+=a[i]*b[i];
  }
  return out;
}

template <typename real_t>
real_t norm(const mcdramvec::vector<real_t>& a){return sqrt(dot(a,a));}





template <typename real_t>
class sparse_t{
  public :
    int64_t _nrows;
    int64_t _ncols;
    mcdramvec::vector<real_t>    nz;
    mcdramvec::vector<int64_t>  rptrs;
    mcdramvec::vector<int64_t>   cids;




    /*Assemble simple finite difference operator.*/
    sparse_t(int64_t nrows,int64_t ncols) : rptrs(nrows+1,0),cids(),nz(){
      _nrows=nrows;
      _ncols=ncols;
      int64_t ndofs=nrows;
      int64_t nnz=0;
      rptrs[0]=nnz;
      for(int64_t i=0;i<ndofs;i++){

        if(i==0){
          nz.push_back(-2.0);cids.push_back(0);   nnz++;
          nz.push_back( 1.0);cids.push_back(1);   nnz++;
        }
        else if(i==ndofs-1){
          nz.push_back( 1.0);cids.push_back(i-1); nnz++;
          nz.push_back(-2.0);cids.push_back(i  ); nnz++;
        }
        else{
          nz.push_back( 1.0);cids.push_back(i-1); nnz++;
          nz.push_back(-2.0);cids.push_back(i  ); nnz++;
          nz.push_back( 1.0);cids.push_back(i+1); nnz++;
        }
        rptrs[i+1]=nnz;
      }
    }

friend mcdramvec::vector<real_t> operator*(const sparse_t<real_t>& A,const mcdramvec::vector<real_t>& in){
      mcdramvec::vector<real_t> out(in);
#pragma omp parallel for schedule(static)
      for(int64_t r=0;r<out.size();r++){
        int64_t rid0=A.rptrs[r];
        int64_t rid1=A.rptrs[r+1];
        out[r]=0.0;
#pragma omp simd
        for(int64_t c=rid0;c<rid1;c++){
          out[r]+=A.nz[c]*in[A.cids[c]];
        }
      }

      return out;
}






};




template <typename real_t>
mcdramvec::vector<real_t> cg(const sparse_t<real_t>& A,const mcdramvec::vector<real_t>& b){
  real_t tol=1e-5;
  /*Zero initial guess.*/
  auto x=0.0*b;
  auto Ax=A*x;
  auto r=b+(-1.0*Ax);
  int64_t it=0;
  auto p=r;
  auto rp=r;
  auto Ap=r;
  auto relres=norm(r)/norm(b);
  while(relres > tol){
    if(it++%100==0) std::cout<<"relres("<<it<<")="<<relres<<std::endl;


    auto Ap      =   A*p;
#pragma omp parallel for schedule(static)
      for(int64_t row=0;row<Ap.size();row++){
        int64_t rid0=A.rptrs[row];
        int64_t rid1=A.rptrs[row+1];
        Ap[row]=0.0;
        real_t tmp=0.0;
#pragma omp simd reduction(+:tmp)
        for(int64_t c=rid0;c<rid1;c++){
          tmp+=A.nz[c]*p[A.cids[c]];
        }
        Ap[row]=tmp;
      }



    real_t alpha =   0.0;
    real_t pdotAp=   0.0;
#pragma omp parallel for simd reduction(+:alpha,pdotAp) schedule(static)
    for(int64_t i=0;i<r.size();i++){
      alpha +=r[i]*r[i];
      pdotAp+=p[i]*Ap[i];
    }
    alpha=alpha/pdotAp;

#pragma omp parallel for simd schedule(static)
    for(int64_t i=0;i<r.size();i++){
      x[i] =x[i]+alpha*p[i];
      rp[i]=r[i]-alpha*Ap[i];
    }


    real_t beta=0.0;
    real_t rdotr=0.0;
#pragma omp parallel for simd reduction(+:beta,rdotr) schedule(static)
    for(int64_t i=0;i<r.size();i++){
      beta+=rp[i]*rp[i];
      rdotr+=r[i]*r[i];
    }
    beta=beta/rdotr;


#pragma omp parallel for simd schedule(static)
    for(int64_t i=0;i<r.size();i++){
      p[i]=rp[i]+beta*p[i];
      r[i]=rp[i];
    }


    rdotr=0.0;
    real_t normb=0.0;
#pragma omp parallel for simd reduction(+:rdotr,normb) schedule(static)
    for(int64_t i=0;i<r.size();i++){
      rdotr+=r[i]*r[i];
      normb+=b[i]*b[i];
    }

    relres  =   sqrt(rdotr)/sqrt(normb);
  }
  return x;
}






#endif 
