#if VERB_MAX > 10
#include <iostream>
#endif
#include <cstring>// std::memcpy
#include "femera.h"
//
int ElastOrtho3D::ElemLinear( Elem* ){ return 1; };//FIXME
int ElastOrtho3D::ElemJacobi( Elem* ){ return 1; };//FIXME
int ElastOrtho3D::ScatStiff( Elem* ){ return 1; };//FIXME
//
int ElastOrtho3D::Setup( Elem* E ){
  JacRot( E );
  const uint jacs_n = E->elip_jacs.size()/E->elem_n/ 10 ;
  const uint intp_n = E->gaus_n;
  this->tens_flop = uint(E->elem_n) * intp_n
    *( uint(E->elem_conn_n)* (36+18+3) + 2*54 + 27 );
  this->tens_band = uint(E->elem_n) *(
     sizeof(FLOAT_PHYS)*(3*uint(E->elem_conn_n)*3+ jacs_n*10)
    +sizeof(INT_MESH)*uint(E->elem_conn_n) );
  this->stif_flop = uint(E->elem_n)
    * 3*uint(E->elem_conn_n) *( 3*uint(E->elem_conn_n) );
  this->stif_band = uint(E->elem_n) *(
    sizeof(FLOAT_PHYS)* 3*uint(E->elem_conn_n) *( 3*uint(E->elem_conn_n) -1+2)
    +sizeof(INT_MESH) *uint(E->elem_conn_n) );
  return 0;
};
int ElastOrtho3D::ElemLinear( Elem* E,
  RESTRICT Phys::vals &sys_f, const RESTRICT Phys::vals &sys_u ){
  //FIXME Cleanup local variables.
  const uint ndof   = 3;//this->ndof_n
  //const int mesh_d = 3;//E->elem_d;
  const uint  Nj =10;//,d2=9;//mesh_d*mesh_d;
  //
  const INT_MESH elem_n = E->elem_n;
  const uint     Nc = E->elem_conn_n;// Number of Nodes/Element
  const uint     Ne = ndof*Nc;
  const uint intp_n = uint(E->gaus_n);
  //
  INT_MESH e0=0, ee=elem_n;
  if(E->do_halo==true){ e0=0; ee=E->halo_elem_n;
  }else{ e0=E->halo_elem_n; ee=elem_n; };
  //
#if VERB_MAX>11
  printf("Dim: %i, Elems:%i, IntPts:%i, Nodes/elem:%i\n",
    (int)mesh_d,(int)elem_n,(int)intp_n,(int)Nc);
#endif
  INT_MESH   conn[Nc];
  FLOAT_PHYS G[Ne], u[Ne],f[Ne];
  //FLOAT_PHYS det, 
  FLOAT_PHYS jac[Nj], A[9], B[9], H[9], S[9];
  //
  FLOAT_PHYS intp_shpg[intp_n*Ne];
  std::copy( &E->intp_shpg[0],// local copy
             &E->intp_shpg[intp_n*Ne], intp_shpg );
  FLOAT_PHYS wgt[intp_n];
  std::copy( &E->gaus_weig[0],
             &E->gaus_weig[intp_n], wgt );
  const FLOAT_PHYS R[9] = {
    mtrl_rotc[0],mtrl_rotc[1],mtrl_rotc[2],
    mtrl_rotc[3],mtrl_rotc[4],mtrl_rotc[5],
    mtrl_rotc[6],mtrl_rotc[7],mtrl_rotc[8]};
#if VERB_MAX>10
  printf( "Material [%u]:", (uint)mtrl_matc.size() );
  for(uint j=0;j<mtrl_matc.size();j++){
    //if(j%mesh_d==0){printf("\n");}
    printf("%+9.2e ",mtrl_matc[j]);
  }; printf("\n");
#endif
  for(INT_MESH ie=e0;ie<ee;ie++){
    //
    std::copy( &E->elem_conn[Nc*ie],
               &E->elem_conn[Nc*ie+Nc], conn );
    std::copy( &E->elip_jacs[Nj*ie],
               &E->elip_jacs[Nj*ie+Nj], jac );// det=jac[9];
    for (uint i=0; i<(Nc); i++){
      std::memcpy( &    u[ndof*i],
                   &sys_u[conn[i]*ndof], sizeof(FLOAT_SOLV)*ndof ); };
    for(uint i=0;i<(Ne);i++){ f[i]=0.0; };
    for(uint ip=0; ip<intp_n; ip++){
      //G = MatMul3x3xN( jac,shg );
      //A = MatMul3xNx3T( G,u );
      for(uint i=0; i< 9 ; i++){ A[i]=0.0;};// H[i]=0.0; B[i]=0.0; };
      //for(uint i=0; i<(Ne) ; i++){ G[i]=0.0; };
#pragma omp simd
      for(uint k=0; k<Nc; k++){
        for(uint i=0; i<3 ; i++){ G[3* k+i ]=0.0;
          for(uint j=0; j<3 ; j++){
            G[(3* k+i) ] += jac[3* i+j ] * intp_shpg[ip*Ne+ 3* k+j ];
          };
          for(uint j=0; j<3 ; j++){
            A[(3* i+j) ] += G[(3* k+i) ] * u[ndof* k+j ];
          };
        };
      };//------------------------------------------------- N*3*6*2 = 36*N FLOP
#if VERB_MAX>10
      printf( "Small Strains (Elem: %i):", ie );
      for(uint j=0;j<HH.size();j++){
        if(j%mesh_d==0){printf("\n");}
        printf("%+9.2e ",H[j]);
      }; printf("\n");
#endif
      // [H] Small deformation tensor
      // [H][RT] : matmul3x3x3T
#pragma omp simd
      for(uint i=0; i<3; i++){
        for(uint k=0; k<3; k++){ H[3* i+k ]=0.0;
          for(uint j=0; j<3; j++){
            H[(3* i+k) ] += A[(3* i+j)] * R[3* k+j ];
      };};};//---------------------------------------------- 27*2 =      54 FLOP
      //det=jac[9 +Nj*l]; FLOAT_PHYS w = det * wgt[ip];
      FLOAT_PHYS w = jac[9] * wgt[ip];
      //
      S[0]=(mtrl_matc[0]* H[0] + mtrl_matc[3]* H[4] + mtrl_matc[5]* H[8])*w;//Sxx
      S[4]=(mtrl_matc[3]* H[0] + mtrl_matc[1]* H[4] + mtrl_matc[4]* H[8])*w;//Syy
      S[8]=(mtrl_matc[5]* H[0] + mtrl_matc[4]* H[4] + mtrl_matc[2]* H[8])*w;//Szz
      //
      S[1]=( H[1] + H[3] )*mtrl_matc[6]*w;// S[3]= S[1];//Sxy Syx
      S[5]=( H[5] + H[7] )*mtrl_matc[7]*w;// S[7]= S[5];//Syz Szy
      S[2]=( H[2] + H[6] )*mtrl_matc[8]*w;// S[6]= S[2];//Sxz Szx
      S[3]=S[1]; S[7]=S[5]; S[6]=S[2];
#if VERB_MAX>10
      printf( "Stress (Natural Coords):");
      for(uint j=0;j<9;j++){
        if(j%3==0){printf("\n");}
        printf("%+9.2e ",S[j]);
      }; printf("\n");
#endif
      //--------------------------------------------------------- 18+9= 27 FLOP
      // [S][R] : matmul3x3x3, R is transposed
      //for(int i=0; i<9; i++){ B[i]=0.0; };
#pragma omp simd
      for(uint i=0; i<3; i++){
        //for(int k=0; k<3; k++){ B[3* i+k ]=0.0;
        for(uint k=0; k<3; k++){ B[3* k+i ]=0.0;
          for(uint j=0; j<3; j++){
            //B[3* i+k ] += S[3* i+j ] * R[3* j+k ];
            B[(3* k+i) ] += S[(3* i+j) ] * R[3* j+k ];// B is transposed
      };};};//-------------------------------------------------- 27*2 = 54 FLOP
      //NOTE [B] is not symmetric Cauchy stress.
      //NOTE Cauchy stress is ( B + BT ) /2
#if VERB_MAX>10
      printf( "Rotated Stress (Global Coords):");
      for(uint j=0;j<9;j++){
        if(j%3==0){printf("\n");}
        printf("%+9.2e ",S[j]);
      }; printf("\n");
#endif
#pragma omp simd
      for(uint i=0; i<Nc; i++){
        for(uint k=0; k<3; k++){
          for(uint j=0; j<3; j++){
            f[(3* i+k) ] += G[(3* i+j) ] * B[(3* k+j) ];
      };};};//---------------------------------------------- N*3*6 = 18*N FLOP
      // This is way slower:
      //for(uint i=0; i<Nc; i++){
      //  for(uint k=0; k<3 ; k++){
      //    for(uint j=0; j<3 ; j++){
      //    for(uint l=0; l<3 ; l++){
      //      f[3* i+l ] += G[3* i+j ] * S[3* j+k ] * R[3* k+l ];
      //};};};};
#if VERB_MAX>10
      printf( "ff:");
      for(uint j=0;j<Ne;j++){
        if(j%mesh_d==0){printf("\n");}
        printf("%+9.2e ",f[j]);
      }; printf("\n");
#endif
    };//end intp loop
    for (uint i=0; i<Nc; i++){
      for(uint j=0; j<3; j++){
        sys_f[3*conn[i]+j] += f[(3*i+j)];
    }; };//--------------------------------------------------- N*3 =  3*N FLOP
  };//end elem loop
  return 0;
};
int ElastOrtho3D::BlocLinear( Elem* E,
  RESTRICT Phys::vals &sys_f, const RESTRICT Phys::vals &sys_u ){
  //FIXME Cleanup local variables.
  const uint ndof   = 3;//this->ndof_n
  //const int mesh_d = 3;//E->elem_d;
  const uint  Nj =10;//,d2=9;//mesh_d*mesh_d;
  //
  const INT_MESH elem_n = E->elem_n;
  const uint     Nc = E->elem_conn_n;// Number of Nodes/Element
  const uint     Ne = ndof*Nc;
  const uint intp_n = uint(E->gaus_n);
  uint           Nv = E->simd_n;// Vector block size
  //
  INT_MESH e0=0, ee=elem_n;
  if(E->do_halo==true){ e0=0; ee=E->halo_elem_n;
  }else{ e0=E->halo_elem_n; ee=elem_n; };
  //
#if VERB_MAX>11
  printf("Dim: %i, Elems:%i, IntPts:%i, Nodes/elem:%i\n",
    (int)mesh_d,(int)elem_n,(int)intp_n,(int)Nc);
#endif
  INT_MESH   conn[Nc*Nv];
  FLOAT_PHYS G[Ne*Nv], u[Ne*Nv],f[Ne*Nv];
  //FLOAT_PHYS det, 
  FLOAT_PHYS jac[Nj*Nv], A[9*Nv], B[9*Nv], H[9*Nv], S[9*Nv];
  //
  FLOAT_PHYS intp_shpg[intp_n*Ne];
  std::copy( &E->intp_shpg[0],// local copy
             &E->intp_shpg[intp_n*Ne], intp_shpg );
  FLOAT_PHYS wgt[intp_n];
  std::copy( &E->gaus_weig[0],
             &E->gaus_weig[intp_n], wgt );
  const FLOAT_PHYS R[9] = {
    mtrl_rotc[0],mtrl_rotc[1],mtrl_rotc[2],
    mtrl_rotc[3],mtrl_rotc[4],mtrl_rotc[5],
    mtrl_rotc[6],mtrl_rotc[7],mtrl_rotc[8]};
#if VERB_MAX>10
  printf( "Material [%u]:", (uint)mtrl_matc.size() );
  for(uint j=0;j<mtrl_matc.size();j++){
    //if(j%mesh_d==0){printf("\n");}
    printf("%+9.2e ",mtrl_matc[j]);
  }; printf("\n");
#endif
  for(INT_MESH ie=e0;ie<ee;ie+=Nv){
    if( (ie+Nv)>=ee ){ Nv=ee-ie; };
    //
    std::copy( &E->elem_conn[Nc*ie],
               &E->elem_conn[Nc*ie+Nc*Nv], conn );
    std::copy( &E->elip_jacs[Nj*ie],
               &E->elip_jacs[Nj*ie+Nj*Nv], jac );// det=jac[9];
    for (uint i=0; i<(Nc*Nv); i++){
      std::memcpy( &    u[ndof*i],
                   &sys_u[conn[i]*ndof], sizeof(FLOAT_SOLV)*ndof ); };
    for(uint i=0;i<(Ne*Nv);i++){ f[i]=0.0; };
    for(uint ip=0; ip<intp_n; ip++){
      //G = MatMul3x3xN( jac,shg );
      //A = MatMul3xNx3T( G,u );
      for(uint i=0; i< 9*Nv ; i++){ H[i]=0.0; A[i]=0.0; B[i]=0.0; };
      for(uint i=0; i<(Ne*Nv) ; i++){ G[i]=0.0; };
      for(uint k=0; k<Nc; k++){
#pragma omp simd
            for(uint l=0;l<Nv;l++){
        for(uint i=0; i<3 ; i++){// G[3* k+i ]=0.0;
          for(uint j=0; j<3 ; j++){
            //FIXME can this vectorize?
            G[(3* k+i)*Nv+l ] += jac[3* i+j +l*Nj ] * intp_shpg[ip*Ne+ 3* k+j ];
            };
          };
        };
      };
      for(uint k=0; k<Nc; k++){
        for(uint i=0; i<3 ; i++){
          for(uint j=0; j<3 ; j++){
#pragma omp simd
            for(uint l=0;l<Nv;l++){
            A[(3* i+j)*Nv+l ] += G[(3* k+i)*Nv+l ] * u[ndof* k+j +l*Ne ];
            };
          };
        };
      };//------------------------------------------------- N*3*6*2 = 36*N FLOP
#if VERB_MAX>10
      printf( "Small Strains (Elem: %i):", ie );
      for(uint j=0;j<HH.size();j++){
        if(j%mesh_d==0){printf("\n");}
        printf("%+9.2e ",H[j]);
      }; printf("\n");
#endif
      // [H] Small deformation tensor
      // [H][RT] : matmul3x3x3T
      for(uint i=0; i<3; i++){
        for(uint k=0; k<3; k++){// H[3* i+k ]=0.0;
          for(uint j=0; j<3; j++){
#pragma omp simd
            for(uint l=0;l<Nv;l++){
            H[(3* i+k)*Nv+l ] += A[(3* i+j)*Nv +l] * R[3* k+j ]; };
      };};};//---------------------------------------------- 27*2 =      54 FLOP
#pragma omp simd
      for(uint l=0;l<Nv;l++){
      //det=jac[9 +Nj*l]; FLOAT_PHYS w = det * wgt[ip];
      FLOAT_PHYS w = jac[9 +Nj*l] * wgt[ip];
      //
      S[0*Nv+l]=(mtrl_matc[0]* H[0*Nv+l] + mtrl_matc[3]* H[4*Nv+l] + mtrl_matc[5]* H[8*Nv+l])*w;//Sxx
      S[4*Nv+l]=(mtrl_matc[3]* H[0*Nv+l] + mtrl_matc[1]* H[4*Nv+l] + mtrl_matc[4]* H[8*Nv+l])*w;//Syy
      S[8*Nv+l]=(mtrl_matc[5]* H[0*Nv+l] + mtrl_matc[4]* H[4*Nv+l] + mtrl_matc[2]* H[8*Nv+l])*w;//Szz
      //
      S[1*Nv+l]=( H[1*Nv+l] + H[3*Nv+l] )*mtrl_matc[6]*w;// S[3]= S[1];//Sxy Syx
      S[5*Nv+l]=( H[5*Nv+l] + H[7*Nv+l] )*mtrl_matc[7]*w;// S[7]= S[5];//Syz Szy
      S[2*Nv+l]=( H[2*Nv+l] + H[6*Nv+l] )*mtrl_matc[8]*w;// S[6]= S[2];//Sxz Szx
      S[3*Nv+l]=S[1*Nv+l]; S[7*Nv+l]=S[5*Nv+l]; S[6*Nv+l]=S[2*Nv+l];
      };
#if VERB_MAX>10
      printf( "Stress (Natural Coords):");
      for(uint j=0;j<9;j++){
        if(j%3==0){printf("\n");}
        printf("%+9.2e ",S[j]);
      }; printf("\n");
#endif
      //--------------------------------------------------------- 18+9= 27 FLOP
      // [S][R] : matmul3x3x3, R is transposed
      //for(int i=0; i<9; i++){ B[i]=0.0; };
      for(uint i=0; i<3; i++){
        //for(int k=0; k<3; k++){ B[3* i+k ]=0.0;
        for(uint k=0; k<3; k++){// B[3* k+i ]=0.0;
          for(uint j=0; j<3; j++){
#pragma omp simd
            for(uint l=0;l<Nv;l++){
            //B[3* i+k ] += S[3* i+j ] * R[3* j+k ];
            B[(3* k+i)*Nv+l ] += S[(3* i+j)*Nv+l ] * R[3* j+k ]; };// B is transposed
      };};};//-------------------------------------------------- 27*2 = 54 FLOP
      //NOTE [B] is not symmetric Cauchy stress.
      //NOTE Cauchy stress is ( B + BT ) /2
#if VERB_MAX>10
      printf( "Rotated Stress (Global Coords):");
      for(uint j=0;j<9;j++){
        if(j%3==0){printf("\n");}
        printf("%+9.2e ",S[j]);
      }; printf("\n");
#endif
      for(uint i=0; i<Nc; i++){
        for(uint k=0; k<3; k++){
          for(uint j=0; j<3; j++){
#pragma omp simd
            for(uint l=0;l<Nv;l++){
            f[(3* i+k)*Nv+l ] += G[(3* i+j)*Nv+l ] * B[(3* k+j)*Nv+l ]; };
      };};};//---------------------------------------------- N*3*6 = 18*N FLOP
      // This is way slower:
      //for(uint i=0; i<Nc; i++){
      //  for(uint k=0; k<3 ; k++){
      //    for(uint j=0; j<3 ; j++){
      //    for(uint l=0; l<3 ; l++){
      //      f[3* i+l ] += G[3* i+j ] * S[3* j+k ] * R[3* k+l ];
      //};};};};
#if VERB_MAX>10
      printf( "ff:");
      for(uint j=0;j<Ne;j++){
        if(j%mesh_d==0){printf("\n");}
        printf("%+9.2e ",f[j]);
      }; printf("\n");
#endif
    };//end intp loop
    for (uint i=0; i<Nc; i++){
      for(uint j=0; j<3; j++){
#pragma omp simd
        for(uint l=0;l<Nv;l++){//Cv
        sys_f[3*conn[i+Nc*l]+j] += f[(3*i+j)*Nv+l];
        };
    }; };//--------------------------------------------------- N*3 =  3*N FLOP
  };//end elem loop
  return 0;
};
int ElastOrtho3D::ElemJacobi(Elem* E, RESTRICT Phys::vals &sys_d ){//FIXME Doesn't do rotation yet
  const uint ndof   = 3;//this->ndof_n
  //const int mesh_d = E->elem_d;
  const uint elem_n = E->elem_n;
  //const int intp_n = E->elip_dets.size()/elem_n;
  const uint  Nc = E->elem_conn_n;
  const uint  Nj = 10,d2=9;
  const uint  Ne = ndof*Nc;// printf("GAUSS PTS: %i\n",(int)E->gaus_n);
  const uint intp_n = uint(E->gaus_n);//E->elip_jacs.size()/elem_n/Nj;
  //
  FLOAT_PHYS det;
  RESTRICT Phys::vals elem_diag(Ne);//,  shg(Ne), G(Ne),jac(d2);
  //RESTRICT Phys::vals B(Ne*6);// 6 rows, Ne cols
  FLOAT_PHYS B[Ne*6];
  FLOAT_PHYS G[Ne],jac[Nj];//,elem_diag[Ne];// 6 rows, Ne cols
  for(uint j=0; j<(Ne*6); j++){ B[j]=0.0; };
  const FLOAT_PHYS D[]={
    mtrl_matc[0],mtrl_matc[3],mtrl_matc[5],0.0,0.0,0.0,
    mtrl_matc[3],mtrl_matc[1],mtrl_matc[4],0.0,0.0,0.0,
    mtrl_matc[5],mtrl_matc[4],mtrl_matc[2],0.0,0.0,0.0,
    0.0,0.0,0.0,mtrl_matc[6],0.0,0.0,
    0.0,0.0,0.0,0.0,mtrl_matc[7],0.0,
    0.0,0.0,0.0,0.0,0.0,mtrl_matc[8]};
    // Does the following wrong one actually converge faster?
    //mtrl_matc[0],mtrl_matc[1],mtrl_matc[1],0.0,0.0,0.0,
    //mtrl_matc[1],mtrl_matc[0],mtrl_matc[1],0.0,0.0,0.0,
    //mtrl_matc[1],mtrl_matc[1],mtrl_matc[0],0.0,0.0,0.0,
    //0.0,0.0,0.0,mtrl_matc[2],0.0,0.0,
    //0.0,0.0,0.0,0.0,mtrl_matc[2],0.0,
    //0.0,0.0,0.0,0.0,0.0,mtrl_matc[2]};
  //elem_inout=0.0;
  for(uint ie=0;ie<elem_n;ie++){
    uint ij=Nj*ie;
    std::copy( &E->elip_jacs[ij],
               &E->elip_jacs[ij+Nj], jac ); det=jac[d2];
    for(uint ip=0;ip<intp_n;ip++){
      //uint ij=Nj*ie*intp_n +Nj*ip;
      //std::copy( &E->elip_jacs[ij],
      //           &E->elip_jacs[ij+Nj], jac ); det=jac[d2];
      //jac = E->elip_jacs[std::slice(ie*intp_n*Nj+ip*Nj,d2,1)];
      //det = E->elip_jacs[ie*intp_n*Nj+ip*Nj +d2];
      //shg = E->intp_shpg[std::slice(ip*Ne,Ne,1)];
      //G   = MatMul3x3xN(jac,shg);
      uint ig=ip*Ne;
      for(uint i=0;i<Ne;i++){ G[i]=0.0; };
      for(uint k=0;k<Nc;k++){
      for(uint i=0;i<3;i++){
      for(uint j=0;j<3;j++){
        //G[3* i+k] += jac[3* i+j] * E->intp_shpg[ig+Nc* j+k]; }; }; };
        G[3* i+k] += jac[3* i+j ] * E->intp_shpg[ig+3* k+j ]; }; }; };
      #if VERB_MAX>10
      printf( "Jacobian Inverse & Determinant:");
      for(uint j=0;j<d2;j++){
        if(j%3==0){printf("\n");}
        printf("%+9.2e",jac[j]);
      }; printf(" det:%+9.2e\n",det);
      #endif
      // xx yy zz
      //B[std::slice(Ne*0 + 0,Nc,ndof)] = G[std::slice(Nc*0,Nc,1)];
      //B[std::slice(Ne*1 + 1,Nc,ndof)] = G[std::slice(Nc*1,Nc,1)];
      //B[std::slice(Ne*2 + 2,Nc,ndof)] = G[std::slice(Nc*2,Nc,1)];
      for(uint j=0; j<Nc; j++){
        B[Ne*0 + 0+j*ndof] = G[Nc*0+j];
        B[Ne*1 + 1+j*ndof] = G[Nc*1+j];
        B[Ne*2 + 2+j*ndof] = G[Nc*2+j];
      // xy yx
      //B[std::slice(Ne*3 + 0,Nc,ndof)] = G[std::slice(Nc*1,Nc,1)];
      //B[std::slice(Ne*3 + 1,Nc,ndof)] = G[std::slice(Nc*0,Nc,1)];
        B[Ne*3 + 0+j*ndof] = G[Nc*1+j];
        B[Ne*3 + 1+j*ndof] = G[Nc*0+j];
      // yz zy
      //B[std::slice(Ne*4 + 1,Nc,ndof)] = G[std::slice(Nc*2,Nc,1)];
      //B[std::slice(Ne*4 + 2,Nc,ndof)] = G[std::slice(Nc*1,Nc,1)];
        B[Ne*4 + 1+j*ndof] = G[Nc*2+j];
        B[Ne*4 + 2+j*ndof] = G[Nc*1+j];
      // xz zx
      //B[std::slice(Ne*5 + 0,Nc,ndof)] = G[std::slice(Nc*2,Nc,1)];
      //B[std::slice(Ne*5 + 2,Nc,ndof)] = G[std::slice(Nc*0,Nc,1)];
        B[Ne*5 + 0+j*ndof] = G[Nc*2+j];
        B[Ne*5 + 2+j*ndof] = G[Nc*0+j];
      };
      #if VERB_MAX>10
      printf( "[B]:");
      for(uint j=0;j<B.size();j++){
        if(j%Ne==0){printf("\n");}
        printf("%+9.2e ",B[j]);
      }; printf("\n");
      #endif
      FLOAT_PHYS w = det * E->gaus_weig[ip];
      for(uint i=0; i<Ne; i++){
      for(uint k=0; k<6 ; k++){
      //for(uint l=0; l<6 ; l++){
        //K[Ne*i+l]+= B[Ne*j + ij] * D[6*j + k] * B[Ne*k + l];
        //uint l=i;
        //elem_d[ie*Ne+i]+= B[Ne*j + i] * D[6*j + k] * B[Ne*k + i];
        elem_diag[i]+=(B[Ne*0 + i] * D[6*0 + k] * B[Ne*k + i])*w;
        elem_diag[i]+=(B[Ne*1 + i] * D[6*1 + k] * B[Ne*k + i])*w;
        elem_diag[i]+=(B[Ne*2 + i] * D[6*2 + k] * B[Ne*k + i])*w;
        elem_diag[i]+=(B[Ne*3 + i] * D[6*3 + k] * B[Ne*k + i])*w;
        elem_diag[i]+=(B[Ne*4 + i] * D[6*4 + k] * B[Ne*k + i])*w;
        elem_diag[i]+=(B[Ne*5 + i] * D[6*5 + k] * B[Ne*k + i])*w;
      };};//};//};
    };//end intp loop
    for (uint i=0; i<Nc; i++){
      //int c=E->elem_conn[Nc*ie+i]*3;
      for(uint j=0; j<3; j++){
        sys_d[E->elem_conn[Nc*ie+i]*3+j] += elem_diag[3*i+j];
      }; };
    elem_diag=0.0;
  };
  return 0;
};
