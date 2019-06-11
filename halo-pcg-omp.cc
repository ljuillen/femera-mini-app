#include <utility>//std::pair
#include <vector>
#include <set>// This is ordered
#include <algorithm>    // std::copy
#include <cstring>      // std::memcpy
#include <unordered_map>
#include <vector>
#include <tuple>
#include <sstream>
#include <string>
#include <iostream>
#include <chrono>
#include <stdio.h>
#include <omp.h>
#include "femera.h"
std::vector<Mesh::part> HaloPCG::P;
//std::vector<Mesh::part> HaloPCG::Ptoto;


int PCG::BC (Mesh* ){return 0;};
int PCG::RHS(Mesh* ){return 0;};
//
int PCG::RHS(Elem* E, Phys* Y ){
  const uint Dn=uint(Y->node_d);
  INT_MESH n; INT_DOF f; FLOAT_PHYS v;
  for(auto t : E->rhs_vals ){ std::tie(n,f,v)=t;
    this->sys_r[Dn* n+uint(f)]+= v;
  };
  return(0);
};
int PCG::BCS(Elem* E, Phys* Y ){
  uint Dn=uint(Y->node_d);
  INT_MESH n; INT_DOF f; FLOAT_PHYS v;
  for(auto t : E->bcs_vals ){ std::tie(n,f,v)=t;
    //printf("FIX ID %i, DOF %i, val %+9.2e\n",i,E->bcs_vals[i].first,E->bcs_vals[i].second);
    this->sys_u[Dn* n+uint(f)] = v;
    if(std::abs(v) > Y->udof_magn[f]){ Y->udof_magn[f] = std::abs(v); }
  };
  return(0);
};
int PCG::BC0(Elem* E, Phys* Y ){
  uint Dn=uint(Y->node_d);
  INT_MESH n; INT_DOF f; FLOAT_PHYS v;
  for(auto t : E->bcs_vals ){ std::tie(n,f,v)=t;
    this->sys_d[Dn* n+uint(f)]=0.0;
    this->sys_f[Dn* n+uint(f)]=0.0;
  };
  for(auto t : E->bc0_nf   ){ std::tie(n,f)=t;
    this->sys_d[Dn* n+uint(f)]=0.0;
    #if VERB_MAX>10
    printf("BC0: [%i]:0\n",E->bc0_nf[i]);
    #endif
  };
  return(0);
};
int PCG::Setup( Elem* E, Phys* Y ){
  //this->meth_name="preconditioned conjugate gradient";
  this->halo_loca_0 = E->halo_remo_n * Y->node_d;
  this->RHS( E,Y );
  this->BCS( E,Y );// Sync max Y->udof_magn before Precond()
  E->do_halo=true ; Y->ElemLinear( E,this->sys_f,this->sys_u );
  E->do_halo=false; Y->ElemLinear( E,this->sys_f,this->sys_u );
  return(0);
};
int PCG::Init( Elem* E, Phys* Y ){
  this->BC0( E,Y );
  return 0;
};
int PCG::Init(){
  const uint sysn=this->udof_n;// loca_res2 is a member variable.
  const uint sumi0=this->halo_loca_0;
  for(uint i=0; i<sysn; i++){
      this->sys_r[i] -= this->sys_f[i];
  };
  //sys_r  = sys_b - sys_f;// This is done sparsely now.
  //sys_z  = sys_d * sys_r;// This is merged where it's used (2x/iter)
  //sys_p  = sys_z;
  for(INT_MESH i=0; i<sysn; i++){
    sys_p[i]  = sys_d[i] * sys_r[i]; };
  //loca_res2    = inner_product( sys_r,sys_z );
  //loca_res2    = inner_product( sys_r,sys_d * sys_r );
  this->loca_res2=0.0;
//#pragma omp parallel for reduction(+:r)
  for(uint i=sumi0; i<sysn; i++){
    this->loca_res2 += sys_r[i] * sys_r[i] * sys_d[i]; };
  this->loca_rto2 = this->loca_rtol*loca_rtol *loca_res2;//FIXME Move this somewhere.
  return(0);
};
int PCG::Iter(){// 2 FLOP + 12 FLOP/DOF, 14 float/DOF
  //NOTE Compute current sys_f=[k][p] before iterating with this.
  const INT_MESH sysn=udof_n;
  const uint sumi0=this->halo_loca_0;// this->node_d;//FIXME Magic number
  const auto ra=this->loca_res2;// Make a local version of this member variable
  //
  //const FLOAT_SOLV alpha  = ra / inner_product( sys_p,sys_f );// 1 DIV +(2 FLOP/DOF)
  FLOAT_SOLV s=0.0;
  for(uint i=sumi0; i<sysn; i++){
    s += sys_p[i] * sys_f[i];// 2 FLOP/DOF, 2 read =2/DOF
  };
  const FLOAT_SOLV alpha  = ra / s;// 1 DIV FLOP
  //if(!isnan(alpha)){// moved to inside last loop
  //  sys_u += alpha * sys_p; };
  //if( ra < to2 ){ return(SOLV_CNVG_PTOL);};// moved to after last loop
  //sys_r -= alpha * sys_f;//Moved inside next loop
  //sys_z  = sys_d * sys_r;// Inlined sys_z into next two loops...
  //r2b    = inner_product( sys_r,sys_z );
  //sys_p  = sys_z + (r2b/loca_res2)*sys_p;
  FLOAT_SOLV r2b=0.0;
  for(uint i=0; i<sysn; i++){// 4 FLOP/DOF, 4 read + 2 write =6/DOF
    sys_u[i] += alpha * sys_p[i];// works fine here
  };
  for(uint i=0;i<sysn;i++){
      sys_r[i] -= alpha * sys_f[i];
  };
#pragma omp parallel for reduction(+:r2b)
  for(uint i=sumi0; i<sysn; i++){
    r2b += sys_r[i] * sys_r[i] * sys_d[i];// 3 FLOP/DOF, 2 read =2/DOF
  };
  if( ra < loca_rto2 ){ return(SOLV_CNVG_PTOL);};
  //sys_p  = sys_d * sys_r + (r2b/ra)*sys_p;
  const FLOAT_PHYS beta = r2b/ra;//  1 FLOP
  this->loca_res2 = r2b;// Update member residual (squared)
#pragma omp parallel for
  for(uint i=0; i<sysn; i++){// 3 FLOP/DOF, 3 read + 1 write =4/DOF
    sys_p[i] = sys_d[i] * sys_r[i] + beta * sys_p[i];
  };
  //if( r2b < to2 ){ return(SOLV_CNVG_PTOL);};
  return(0);
};
int PCG::Solve( Elem* E, Phys* Y ){//FIXME Redo this
  //this->Setup( E,Y );
  this->Init( E,Y );
  if( !this->Init() ){//============ Solve ================
    printf("SER INIT r2a:%9.2e\n",this->loca_rto2);
    for(this->iter=0; this->iter < this->iter_max; this->iter++){
      const auto sysn = this->udof_n;
      for(uint i=0;i<sysn;i++){ this->sys_f[i]=0.0; };
      //Y->ScatterNode2Elem(E,this->sys_p,Y->elem_inout);
      //Y->ElemLinear( E );
      //Y->GatherElem2Node(E,Y->elem_inout,this->sys_f);
      //Y->ElemLinear( E,this->sys_f,this->sys_p );
      E->do_halo=true ; Y->ElemLinear( E,this->sys_f,this->sys_p );
      E->do_halo=false; Y->ElemLinear( E,this->sys_f,this->sys_p );
      //FIXME HALO SYNC this->sys_f
      if( this->Iter() ){ break ;};// CG Iteration
#if VERB_MAX>2
      if(!((iter+1) % 100) ){
        printf("%6i ||R||%9.2e\n", iter+1, std::sqrt(loca_res2) ); };
          //std::sqrt(loca_res2), std::sqrt(this->loca_rto2) ); };
#endif
    };// End iteration loop.
  };
  return 0;//FIXME
};
int HaloPCG::Init(){// Preconditioned Conjugate Gradient
#ifdef _OPENMP
  const int comp_n = this->comp_n;
#endif
  INT_MESH halo_n=0;
  // Local copies for atomic ops and reduction
  FLOAT_SOLV glob_r2a = this->glob_res2, glob_to2 = this->glob_rto2;
  Phys::vals bcmax={0.0,0.0,0.0,0.0};
#pragma omp parallel num_threads(comp_n)
{// parallel init region
  long int my_scat_count=0, my_prec_count=0,
    my_gat0_count=0,my_gat1_count=0, my_gmap_count=0, my_solv_count=0;
  auto start = std::chrono::high_resolution_clock::now();
  // Make thread-local copies of mesh_part into threadprivate P.
  P.resize(this->mesh_part.size());
  std::copy(this->mesh_part.begin(), this->mesh_part.end(), P.begin());
  int part_0=0; if(std::get<0>( P[0] )==NULL){ part_0=1; };
  const int part_n = int(P.size())-part_0;
  const int part_o = part_n+part_0;
  // Sync max Y->udof_magn
#pragma omp for schedule(OMP_SCHEDULE)
  for(int part_i=part_0; part_i<part_o; part_i++){
    Elem* E; Phys* Y; Solv* S; std::tie(E,Y,S)=P[part_i];
    for(uint i=0;i<Y->udof_magn.size();i++){
      //printf("GLOBAL MAX BC[%u]: %f\n",i,bcmax[i]);
      if(Y->udof_magn[i] > bcmax[i]){//FIXME Atomic read?
//#pragma omp atomic write
        bcmax[i]=Y->udof_magn[i];
      }
    }
  }
#pragma omp single
{
  auto m=bcmax[0];
  for(uint i=1;i<3;i++){ if(bcmax[0] > m){ m=bcmax[i]; } }
  for(uint i=0;i<3;i++){ bcmax[i]=m; }
  for(uint i=0;i<bcmax.size();i++){ if(bcmax[i]<=0.0){ bcmax[i]=1.0; } }
#if VERB_MAX>2
    if(verbosity>2){
      printf("    DOF Scales:");
      for(uint i=0;i<bcmax.size();i++){ printf(" %g",bcmax[i]); }
      printf("\n");
    }
#endif
}
#pragma omp for schedule(OMP_SCHEDULE)
  for(int part_i=part_0; part_i<part_o; part_i++){
    Elem* E; Phys* Y; Solv* S; std::tie(E,Y,S)=P[part_i];
    S->solv_cond=this->solv_cond;
    for(uint i=0;i<Y->udof_magn.size();i++){
//#pragma omp atomic read
      Y->udof_magn[i] = bcmax[i];
      //printf("Sync MAX BC[%u]: %f\n",i,Y->udof_magn[i]);
    }
    S->Precond( E,Y );
  };
  // Sync sys_d [this inits M->halo_map and E->node_haid]//FIXME separate
  time_reset( my_prec_count, start );
#pragma omp for schedule(OMP_SCHEDULE)
    for(int part_i=part_0; part_i<part_o; part_i++){
      Elem* E; Phys* Y; Solv* S; std::tie(E,Y,S)=P[part_i];
      if(E->node_haid.size()==0){ E->node_haid.resize(E->halo_node_n); };
    }
  if(this->solv_cond != Solv::COND_NONE){// Don't sync after sys_d=1;
#pragma omp for schedule(OMP_SCHEDULE)
    for(int part_i=part_0; part_i<part_o; part_i++){
      Elem* E; Phys* Y; Solv* S; std::tie(E,Y,S)=P[part_i];
      //if(E->node_haid.size()==0){ E->node_haid.resize(E->halo_node_n); };
      const INT_MESH d=uint(Y->node_d);
      for(INT_MESH i=0; i<E->halo_node_n; i++){
#pragma omp critical(halomap)
{//FIXME critical section here?
      INT_MESH g=E->node_glid[i];
      if(this->halo_map.count(g)==0){// Initialize halo_map
        this->halo_map[g]=halo_n;
        E->node_haid[i]=halo_n;
        for( uint j=0; j<d; j++){
          this->halo_val[d*E->node_haid[i]+j]  = S->sys_d[d*i +j]; };
        halo_n++;
      }else{// Add in the rest.
        E->node_haid[i]=this->halo_map[g];
        for( uint j=0; j<d; j++){
          this->halo_val[d*E->node_haid[i]+j] += S->sys_d[d*i +j]; };
      };
}
    };
    };// End sys_d gather
    time_reset( my_gmap_count, start );
#pragma omp for schedule(OMP_SCHEDULE)
    for(int part_i=part_0; part_i<part_o; part_i++){
      Elem* E; Phys* Y; Solv* S; std::tie(E,Y,S)=P[part_i];
      const INT_MESH d=uint(Y->node_d);
      for(INT_MESH i=0; i<E->halo_node_n; i++){
        auto f = d* E->node_haid[i];
        for( uint j=0; j<d; j++){
#pragma omp atomic read
          S->sys_d[d*i +j] = this->halo_val[f+j]; };
      };
    };// end sys_d scatter
  }//end skip if no preconditioner
  time_reset( my_scat_count, start );
#pragma omp for schedule(OMP_SCHEDULE)
  for(int part_i=part_0; part_i<part_o; part_i++){
    Elem* E; Phys* Y; Solv* S; std::tie(E,Y,S)=P[part_i];
    const INT_MESH sysn=S->udof_n;
    for(uint i=0;i<sysn;i++){ S->sys_d[i]=FLOAT_SOLV(1.0)/S->sys_d[i]; };
    S->Init( E,Y );// Zeros boundary conditions
  };
  time_reset( my_solv_count, start );
#pragma omp single
{   this->halo_val = 0.0; }// serial halo_vals zero
  time_reset( my_gat0_count, start );
#pragma omp for schedule(OMP_SCHEDULE)
  for(int part_i=part_0; part_i<part_o; part_i++){
    Elem* E; Phys* Y; Solv* S; std::tie(E,Y,S)=P[part_i];
    const INT_MESH d=uint(Y->node_d);
    for(INT_MESH i=0; i<E->halo_node_n; i++){
      auto f = d* E->node_haid[i];
      for( uint j=0; j<d; j++){
#pragma omp atomic update
        this->halo_val[f+j] += S->sys_f[d*i +j]; };
    };
  };// End halo_vals
  time_reset( my_gat1_count, start );
#pragma omp for schedule(OMP_SCHEDULE) reduction(+:glob_r2a)
  for(int part_i=part_0; part_i<part_o; part_i++){
    Elem* E; Phys* Y; Solv* S; std::tie(E,Y,S)=P[part_i];
    const INT_MESH d=uint(Y->node_d);
    for(INT_MESH i=0; i<E->halo_node_n; i++){
      auto f = d* E->node_haid[i];
      for( uint j=0; j<d; j++){
#pragma omp atomic read
        S->sys_f[d*i +j] = this->halo_val[f+j]; };
    };
  time_reset( my_scat_count, start );
#pragma omp critical(init)
{ S->Init(); }//FIXME Why is this serialized?
  glob_r2a += S->loca_res2;
  };
#pragma omp for schedule(OMP_SCHEDULE)
  for(int part_i=part_0; part_i<part_o; part_i++){
    Elem* E; Phys* Y; Solv* S; std::tie(E,Y,S)=P[part_i];
    S->loca_rto2 = S->loca_rtol*S->loca_rtol *glob_r2a;
#pragma omp atomic write
    glob_to2 = S->loca_rto2;// Pass the relative tolerance out.
  };
  time_reset( my_solv_count, start );
#if VERB_MAX>1
#pragma omp critical(time)
{
  this->time_secs[0]+=float(my_prec_count)*1e-9;
  this->time_secs[1]+=float(my_gmap_count)*1e-9;
  this->time_secs[2]+=float(my_gat0_count)*1e-9;
  this->time_secs[3]+=float(my_gat1_count)*1e-9;
  this->time_secs[4]+=float(my_scat_count)*1e-9;
  this->time_secs[5]+=float(my_solv_count)*1e-9;
}
#endif
}// end init parallel region
  this->glob_res2 = glob_r2a;
  this->glob_chk2 = glob_r2a;
  this->glob_rto2 = glob_to2;// / ((FLOAT_SOLV)this->udof_n);
  return 0;
};
int HaloPCG::Iter(){
#ifdef _OPENMP
  const int comp_n = this->comp_n;
#endif
  FLOAT_SOLV glob_sum1=0.0, glob_sum2=0.0;
  FLOAT_SOLV glob_r2a = this->glob_res2;
#pragma omp parallel num_threads(comp_n)
{// iter parallel region
// not needed anymore,since P is threadprivate
//  std::vector<part> P; P.resize(this->mesh_part.size());
//  std::copy(this->mesh_part.begin(), this->mesh_part.end(), P.begin());
  int part_0=0; if(std::get<0>( P[0] )==NULL){ part_0=1; };
  const int part_n = int(P.size())-part_0;
  const int part_o = part_n+part_0;
  Elem* E; Phys* Y; Solv* S;// Seems to be faster to reuse these.
  // Timing variables (used when verbosity > 1)
  long int my_phys_count=0, my_scat_count=0, my_solv_count=0,
    my_gat0_count=0,my_gat1_count=0;
  std::chrono::high_resolution_clock::time_point iter_start,
    solv_start, inte_start, iter_done;
  std::chrono::high_resolution_clock::time_point
    gath_start, scat_start, phys_start;
  time_start( iter_start );
#pragma omp for schedule(OMP_SCHEDULE)
  for(int part_i=part_0; part_i<part_o; part_i++){
    std::tie(E,Y,S)=P[part_i];
    const INT_MESH Dn=uint(Y->node_d);
    time_start( phys_start );
    const auto sysn = S->udof_n;
    for(uint i=0;i<sysn;i++){ S->sys_f[i]=0.0; };
    E->do_halo=true; Y->ElemLinear( E, S->sys_f, S->sys_p );
    time_accum( my_phys_count, phys_start );
    time_start( gath_start );
    const INT_MESH hnn=E->halo_node_n,hrn=E->halo_remo_n;
    for(INT_MESH i=hrn; i<hnn; i++){//NOTE memcpy apparently not critical
      std::memcpy(
        & this->halo_val[Dn* E->node_haid[i]],
        & S->sys_f[Dn* i],
        Dn*sizeof(FLOAT_PHYS) );
    };
    time_accum( my_gat0_count, gath_start );
  };
#pragma omp for schedule(OMP_SCHEDULE)
  for(int part_i=part_0; part_i<part_o; part_i++){
    std::tie(E,Y,S)=P[part_i];
    time_start( gath_start );
    const INT_MESH Dn=uint(Y->node_d);
    const INT_MESH hrn=E->halo_remo_n;
    for(INT_MESH i=0; i<hrn; i++){
      const auto f = Dn* E->node_haid[i];
      for( uint j=0; j<Dn; j++){
#pragma omp atomic update
        this->halo_val[f+j]+= S->sys_f[Dn* i+j]; };
    };
    time_accum( my_gat1_count, gath_start );
  };// End halo_vals sum; now scatter back to elems
#pragma omp for schedule(OMP_SCHEDULE) reduction(+:glob_sum1)
  for(int part_i=part_0; part_i<part_o; part_i++){
    std::tie(E,Y,S)=P[part_i];
    const INT_MESH Dn=uint(Y->node_d);
    time_start( scat_start );
    const INT_MESH hnn=E->halo_node_n,hl0=S->halo_loca_0,sysn=S->udof_n;
    for(INT_MESH i=0; i<hnn; i++){//NOTE appears not to be critical
      std::memcpy(
        & S->sys_f[Dn* i],
        & this->halo_val[Dn* E->node_haid[i]],
        Dn*sizeof(FLOAT_PHYS) );
    };
    time_accum( my_scat_count, scat_start );
    time_start( phys_start );
    E->do_halo=false; Y->ElemLinear( E, S->sys_f, S->sys_p );
    time_accum( my_phys_count, phys_start );
    time_start( solv_start );
#pragma omp simd reduction(+:glob_sum1)
    for(INT_MESH i=hl0; i<sysn; i++){
        glob_sum1 += S->sys_p[i] * S->sys_f[i];
    };
    time_accum( my_solv_count, solv_start );
  };
  time_start( solv_start );
  const FLOAT_SOLV alpha = glob_r2a / glob_sum1;// 1 FLOP
  //printf("ALPHA:%+9.2e\n",alpha);
#pragma omp for schedule(OMP_SCHEDULE) reduction(+:glob_sum2)
  for(int part_i=part_0; part_i<part_o; part_i++){// ? FLOP/DOF
    std::tie(E,Y,S)=P[part_i];
    const INT_MESH hl0=S->halo_loca_0,sysn=S->udof_n;
#pragma omp simd
    for(INT_MESH i=0; i<sysn; i++){
        S->sys_r[i] -= alpha * S->sys_f[i];// Update force residuals
    };
#pragma omp simd reduction(+:glob_sum2)
    for(INT_MESH i=hl0; i<sysn; i++){
      glob_sum2   += S->sys_r[i] * S->sys_r[i] * S->sys_d[i]; };
  };
  const FLOAT_PHYS beta = glob_sum2 / glob_r2a;// 1 FLOP
#pragma omp for schedule(OMP_SCHEDULE)
  for(int part_i=part_0; part_i<part_o; part_i++){
    std::tie(E,Y,S)=P[part_i];
    const INT_MESH sysn=S->udof_n;
    //sys_p  = sys_d * sys_r + (r2b/ra)*sys_p;
    //S->r2a = glob_sum2;// Update member residual (squared)
#pragma omp simd
    for(INT_MESH i=0; i<sysn; i++){// ? FLOP/DOF
      S->sys_u[i] += S->sys_p[i] * alpha;// better data locality here
      S->sys_p[i]  = S->sys_d[i] * S->sys_r[i] + beta*S->sys_p[i]; };
  };
#pragma omp single nowait
{ glob_r2a = glob_sum2; }// Update residual (squared)
#if VERB_MAX>1
  iter_done  = std::chrono::high_resolution_clock::now();
  auto solv_time  = std::chrono::duration_cast<std::chrono::nanoseconds>
    (iter_done - solv_start);
  auto iter_time  = std::chrono::duration_cast<std::chrono::nanoseconds>
    (iter_done - iter_start);// printf("%i ",iter);
#pragma omp critical(time)
{
  this->time_secs[0]+=float(my_phys_count)*1e-9;
  this->time_secs[1]+=float(my_gat0_count)*1e-9;
  this->time_secs[2]+=float(my_gat1_count)*1e-9;
  this->time_secs[3]+=float(my_scat_count)*1e-9;
  this->time_secs[4]+=float(my_solv_count+solv_time.count())*1e-9;
 //this->time_secs[4]+=float(solv_time.count())*1e-9;
  this->time_secs[5]+=float(iter_time.count())*1e-9;
}
#endif
}// end iter parallel region
  this->glob_res2 = glob_r2a;
  this->glob_chk2 = glob_r2a;
  return 0;
};