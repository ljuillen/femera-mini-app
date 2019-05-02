#ifndef INCLUDED_SOLV_H
#define INCLUDED_SOLV_H
#if VERB_MAX > 3
#include <iostream>
#endif
class Solv{
public:
  typedef std::valarray<FLOAT_SOLV> vals;
  typedef FLOAT_SOLV* valign;// memory-aligned vals
  const uint valign_byte = 64;
  enum Meth {
    SOLV_GD=0, SOLV_CG=1, SOLV_CR=2
  };
  enum Cond {
    COND_NONE=0, COND_JACO=3, COND_ROW1=1, COND_STRA=4
  };
  INT_MESH iter=0, iter_max=0, udof_n, upad_n;
  FLOAT_SOLV loca_rtol=0.0;
  // Pointers to memory-aligned vanilla C arrays
  valign sys_f;// f=[A]{u}//FIXME Move to Phys*?
  valign sys_u;// solution
  valign sys_r;// Residuals
  //NOTE Additional working vectors needed are defined in each solver subclass
  //FIXME Moved them back here for now
  valign sys_d;//FIXME diagonal preconditioner w/ fixed BC DOFs set to zero
  valign sys_p, sys_g;//FIXME working vectors specific to each method
  // The data is actually stored in corresponding C++ objects.
  Solv::vals dat_f;
  Solv::vals dat_u;
  Solv::vals dat_r;
  Solv::vals dat_d;
  Solv::vals dat_p, dat_g;
  //
  std::string meth_name="";
  //
  virtual int Solve( Elem*, Phys* )=0;//returns # iterations FIXME
  virtual int Setup( Elem*, Phys* )=0;
  //virtual int Setup()=0;
  //virtual int Precond( Elem*, Phys* )=0;
  //virtual int RHS( Mesh* )=0;
  //virtual int BC( Mesh* )=0;
  virtual int Init( Elem*, Phys* )=0;
  virtual int Init()=0;
  virtual int Iter()=0;
  //virtual int Iter (Elem*, Phys*)=0;
  int Precond( Elem*, Phys*);
  //
  //FIXME Make the rest private or protected later?
  FLOAT_SOLV loca_rto2=0.0;// Solution Tolerance Squared
  FLOAT_SOLV loca_res2=0.0;// [r2b, alpha kept local to each iteration]
  INT_MESH halo_loca_0=0;// Associated Elem->halo_remo_n * Phys->ndof_n
  uint udof_flop=0, udof_band=0;
  int solv_cond;
protected:
  inline valign align_resize(RESTRICT Solv::vals&, INT_MESH, const uint);
  Solv( INT_MESH n, INT_MESH i, FLOAT_PHYS r ) :
    iter_max(i), udof_n(n), upad_n(n), loca_rtol(r){
    loca_rto2=loca_rtol*loca_rtol;
    //sys_u.resize(udof_n,0.0);// Initial Solution Guess
    //sys_r.resize(udof_n,0.0);// Residuals
    //sys_d.resize(udof_n,0.0);// Diagonal Preconditioner
    sys_f = align_resize( dat_f, upad_n, valign_byte );// f=Au
    sys_u = align_resize( dat_u, upad_n, valign_byte );
    sys_r = align_resize( dat_r, upad_n, valign_byte );
    sys_d = align_resize( dat_d, upad_n, valign_byte );
#if VERB_MAX > 13
    std::cout << &sys_f[0] <<'\n';
#endif
  };
private:
};
class PCG final: public Solv{
// Preconditioned Conjugate Gradient Kernel ----------------------------
public:
  //PCG() : Solv(){
  //  meth_name="preconditioned cojugate gradient";
  //};
  PCG( INT_MESH n, INT_MESH i, FLOAT_PHYS r ) : Solv(n,i,r){
    meth_name="preconditioned cojugate gradient";
    //sys_p.resize(udof_n,0.0);// CG working vector
    sys_p = align_resize( dat_p, upad_n, valign_byte );
    udof_flop = 12;//*elem_n
    udof_band = 14*sizeof(FLOAT_SOLV);//*udof_n + 2
  };
  //PCG( ):Solv(12,11){};
  int Solve( Elem*, Phys* ) final;
  int Setup( Elem*, Phys* ) final;
  int Init ( Elem*, Phys* ) final;
  int Init () final;
  int Iter () final;
  //int Iter (Elem*, Phys*) final;
  //FIXME Make the rest private or protected later?
  //RESTRICT Solv::vals sys_d;//FIXME diagonal preconditioner w/ fixed BC DOFs set to zero
  //RESTRICT Solv::vals sys_p;// [sys_z no longer needed]
protected:
private:
  //int Precond( Elem*, Phys*);
  int RHS( Mesh* );//FIXME Remove?
  int BC ( Mesh* );//FIXME Remove?
  int RHS( Elem*, Phys* Y );
  int BCS( Elem*, Phys* Y );
  int BC0( Elem*, Phys* Y );
};
class PCR final: public Solv{
// Preconditioned Conjugate Residual Kernel ----------------------------
public:
  //PCR() : Solv(){
  //  meth_name="preconditioned cojugate residual";
  //};
  PCR( INT_MESH n, INT_MESH i, FLOAT_PHYS r ) : Solv(n,i,r){
    meth_name="preconditioned cojugate residual";
    //sys_p.resize(udof_n,0.0);// CR working vector
    //sys_g.resize(udof_n,0.0);// CR working vector
    sys_p = align_resize( dat_p, upad_n, valign_byte );
    sys_g = align_resize( dat_g, upad_n, valign_byte );
    udof_flop = 14;//*elem_n
    udof_band = 17*sizeof(FLOAT_SOLV);//*udof_n +?
  };
  //PCR( ):Solv(14,13){};
  int Solve( Elem*, Phys* ) final;
  int Setup( Elem*, Phys* ) final;
  int Init ( Elem*, Phys* ) final;
  int Init () final;
  int Iter () final;
  //int Iter (Elem*, Phys*) final;
  //FIXME Make the rest private or protected later?
  //RESTRICT Solv::vals sys_d;//FIXME diagonal preconditioner w/ fixed BC DOFs set to zero
  //RESTRICT Solv::vals sys_g;// [f]=[A][r], [g]=[A][p]
protected:
private:
  //int Precond( Elem*, Phys*);
  int RHS( Mesh* );//FIXME Remove?
  int BC ( Mesh* );//FIXME Remove?
  int RHS( Elem*, Phys* Y );
  int BCS( Elem*, Phys* Y );
  int BC0( Elem*, Phys* Y );
};
//============= Inline Function Definitions ===============
//
inline Solv::valign Solv::align_resize(RESTRICT Solv::vals &v,
  INT_MESH n, const uint bytes){
  //FIXME Hack to align sys_f
  v.resize( n + bytes/sizeof(FLOAT_SOLV),0.0);
  intptr_t ptr = reinterpret_cast<intptr_t>(&v[0]);
  const auto offset = bytes - ( ptr % bytes );
  return(&v[( offset % bytes )/sizeof(FLOAT_SOLV)]);
};


#endif
