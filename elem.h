#ifndef INCLUDED_ELEM_H
#define INCLUDED_ELEM_H
#include <vector>
#include <set>
//#include <map>
#include <unordered_map>
class Elem {
public:
  enum Tess {// ELEM_TIE=11,
    //ELEM_BAR=12, ELEM_TRI=23, ELEM_QUA=24,
    //ELEM_TET=34, ELEM_BRI=38, ELEM_PRI=36, ELEM_PYR=35 };
    ELEM_SCA=0, ELEM_VEC=1, ELEM_BAR=2, ELEM_TRI=3, ELEM_QUA=4,
    ELEM_TET=5, ELEM_PYR=6, ELEM_PRI=7, ELEM_BRI=8 };
  inline FLOAT_MESH Jac1Det( const FLOAT_MESH  );
  inline FLOAT_MESH Jac2Det( const FLOAT_MESH* );
  inline FLOAT_MESH Jac3Det( const FLOAT_MESH* );
  inline FLOAT_MESH Jac1Det( const RESTRICT Mesh::vals&);
  inline FLOAT_MESH Jac2Det( const RESTRICT Mesh::vals&);
  inline FLOAT_MESH Jac3Det( const RESTRICT Mesh::vals&);
  inline int Jac1Inv( FLOAT_MESH  , const FLOAT_MESH);
  inline int Jac2Inv( FLOAT_MESH*, const FLOAT_MESH);
  inline int Jac3Inv( FLOAT_MESH*, const FLOAT_MESH);
  inline int Jac1Inv(RESTRICT Mesh::vals&, const FLOAT_MESH);
  inline int Jac2Inv(RESTRICT Mesh::vals&, const FLOAT_MESH);
  inline int Jac3Inv(RESTRICT Mesh::vals&, const FLOAT_MESH);
  inline int Jac2Tnv(RESTRICT Mesh::vals&, const FLOAT_MESH);
  inline int Jac3Tnv(RESTRICT Mesh::vals&, const FLOAT_MESH);
  //inline RESTRICT Mesh::vals JacInElem(RESTRICT const Mesh::vals&);
  //
  int Jac1Dets( );
  int Jac2Dets( );
  int Jac3Dets( );
  virtual int JacsDets( )=0;
  //
  //int ScatterVert2Elem( );
  //int ScatterVert2Elem( Mesh* );//FIXME No longer needed?
  //
  int Setup( );
  int Setup( Mesh* );//FIXME No longer needed?
  //
  // Shape Function Bulk Eval -----------------------------
  const RESTRICT Mesh::vals ShapeFunction(const INT_ORDER, RESTRICT Mesh::vals);
  const RESTRICT Mesh::vals ShapeGradient(const INT_ORDER,
                                          const RESTRICT Mesh::vals);
  //FIXME need to handle higher-order elements
  // Shape Function Kernels -------------------------------
  //FIXME virtual functions are difficult to inline...how to fix?
  virtual const RESTRICT Mesh::vals ShapeFunction(const INT_ORDER,
                                                  const FLOAT_MESH[])=0;
  virtual const RESTRICT Mesh::vals ShapeGradient(const INT_ORDER,
                                                  const FLOAT_MESH[])=0;
  // Integration Kernel -----------------------------------
  // integration points & weights
  virtual const RESTRICT Mesh::vals GaussLegendre(const INT_ORDER )=0;
  const RESTRICT Mesh::vals GaussLegendre( ){
    return GaussLegendre( gaus_p ); };
  //
  int SavePartCSV( const char* bname );
  int ReadPartCSV( const char* bname );//FIXME should be in Mesh* and return Elem*?
  //
  int   SavePartFMR( const char* bname, bool is_bin );//FIXME ASCII/Binary file format
  Phys* ReadPartFMR( const char* bname, bool is_bin );
  //
#ifdef HAS_PATCH
  virtual Mesh* Mesh1Natu( )=0;//FIXME These are defined in patch.cc
  virtual Mesh* MeshPatch( )=0;//FIXME Only first order
#endif
  INT_ORDER mesh_d=3;// Mesh dimension
  INT_ORDER elem_d=3;// Element dimension
  INT_ORDER elem_p=1;
  INT_ORDER gaus_p=1; INT_ORDER gaus_n=1;
  INT_ORDER jacs_f=mesh_d*elem_d + 1;// size of jac matrix + det
  INT_ORDER     elem_jacs_n=1;//1 for tri & tet, 3? for qua, 4? for bri
  INT_ELEM_NODE elem_vert_n;// Size of one linear element connectivity
  INT_ELEM_NODE elem_edge_n;// Edges of one element
  INT_ELEM_NODE elem_face_n;// Faces of one element containing a node
  INT_ELEM_NODE elem_conn_n;
  uint simd_n=1;
  //INT_MESH  jacs_n=0;//=elem_n * elem_jacs_n;
  //FIXME Use this instead of gauss_n*elem_n*(d^2+1)
  // = elem_vert_n + (elem_p-1)*elem_edge_n + elem_face_n
  INT_MESH elem_n=0, vert_n=0, node_n=0, jacs_n=0,
    halo_node_n=0, halo_loca_n=0, halo_remo_n=0, halo_elem_n=0;
  // Local halo and system sizes.
  // Halo nodes are [0...halo_n-1]; interior nodes are [halo_n...node_n-1].
  RESTRICT Mesh::vals intp_shpg={};
  RESTRICT Mesh::ints elem_conn={};// Grouped so halo nodes come first.
  //FIXME Should be conn_node or elem_node?
  //RESTRICT Mesh::vals elem_vert={};// Fill w/ Elem::ScatterVert2Elem(Mesh*)
  //(mesh_d*elem_vert_n*elem_n) used for jac calc
  RESTRICT Mesh::vals elip_jacs={};
  //FIXME Change to elem_jacs, size: jacs_f * jacs_n
  //FIXME Broke 2D physics
  RESTRICT Mesh::vals gaus_weig={};
  //
  RESTRICT Mesh::vals vert_coor={};// Local element vertices.
  //FIXME contains all nodal coordinates, not just vertices
  // Nodes are grouped:
  // 0           .. (halo_remo_n-1) [Ghost nodes]
  // halo_remo_n .. (halo_node_n-1) [There are halo_loca_n of these.]
  // halo_node_n .. (     node_n-1) [Interior nodes]
  //
  RESTRICT Mesh::ints elem_glid ={};// xref from local to global element number
  //FIXME Should be <int> to match Gmsh tags
  RESTRICT Mesh::ints node_glid ={};// xref from local to global node number
  // global_id=node_glid[local_id];
  std::unordered_map<int,INT_MESH> elem_loid;
  std::unordered_map<int,INT_MESH> node_loid;
  std::vector<int> halo_glid ={};// = loca_glid U remo_glid
  std::vector<int> loca_glid ={};
  std::vector<int> remo_glid ={};
  //NOTE Global node numbers are 1-indexed.
  //NOTE Local (partition) node numbers are 0-indexed.
  bool do_halo;//=true;// false: do_interior
  //
  //typedef std::set<INT_MESH> elemset;//FIXED REMOVE
  //std::vector<elemset> elem_sets;//FIXED REMOVE [used for coloring]
  //std::vector<Mesh::ints> conn_color;
  // Local boundary conditions [were in Mesh]
  Mesh::nfvals rhs_vals={};// Nodal forces applied (nonzeros)
  Mesh::nfvals bcs_vals={};// Dirichlet bundary conditions (nonzeros)
  Mesh::nflist bc0_nf  ={};// Essential (u=0) Dirichlet BCs (node_id,dof_id)
  //
  //Mesh* in_mesh;//FIXED OLD: contains the nodes
protected:
  Elem( INT_ORDER d, INT_MESH vn, INT_MESH en, INT_ORDER p, INT_MESH e ) :
    elem_d(d), elem_p(p), elem_vert_n(vn), elem_edge_n(en),
    elem_conn_n(vn+(p-1)*en), elem_n(e){// Elem Constructor
      gaus_p=p;
      elem_conn.resize( elem_n* elem_conn_n );
      //elem_vert.resize( elem_n*elem_d* elem_vert_n );
      //elip_jacs.resize( elem_n* (elem_d*elem_d+1) );
      elem_glid.resize( elem_n );
    };//FIXME Remove this one?
  Elem( INT_ORDER d, INT_MESH vn, INT_MESH en, INT_MESH ef,
        INT_ORDER p, INT_ORDER gp, INT_MESH e ) :// Elem Constructor
    elem_d(d), elem_p(p), gaus_p(gp), elem_vert_n(vn),
    elem_edge_n(en), elem_face_n(ef), elem_conn_n(vn+(p-1)*en+ef), elem_n(e){
      elem_conn.resize( elem_n* elem_conn_n );
      //elem_vert.resize( elem_n*elem_d* elem_vert_n );
      //GaussLegendre( gp );// sets this->guass_n
      //elip_jacs.resize( elem_n* (elem_d*elem_d+1) * gaus_n );
      elem_glid.resize( elem_n );
    };
private:
};
class Bar final: public Elem{
public: Bar(INT_ORDER p) : Elem(1,2,1,p,1){}// Set properties in base Elem class
  //Elem(elem_d, elem_n, elem_vert_n, elem_edge_n , elem_p)
  const RESTRICT Mesh::vals ShapeFunction(const INT_ORDER,
                                          const FLOAT_MESH p[1])final;
  const RESTRICT Mesh::vals ShapeGradient(const INT_ORDER,
                                          const FLOAT_MESH  [1])final;
  const RESTRICT Mesh::vals GaussLegendre(const INT_ORDER p )final;
  int JacsDets()final;
#ifdef HAS_PATCH
  Mesh* Mesh1Natu()final;
  Mesh* MeshPatch()final;
#endif
protected:
  const RESTRICT Mesh::ints vert_conn={ 0,1 };
  const RESTRICT Mesh::ints vert_edge={ 0,1 };// Bar2 connectivity of 3 sides
  const RESTRICT Mesh::vals vert_coor={-1.0,1.0};
private: 
};
class Tri final: public Elem{
public: Tri(INT_ORDER p) : Elem(2,3,3,p,1){}// Set properties in base Elem class
  const RESTRICT Mesh::vals ShapeFunction(const INT_ORDER,
                                          const FLOAT_MESH p[2])final;
  const RESTRICT Mesh::vals ShapeGradient(const INT_ORDER,
                                          const FLOAT_MESH  [2])final;
  const RESTRICT Mesh::vals GaussLegendre(const INT_ORDER p )final;
  int JacsDets()final;
#ifdef HAS_PATCH
  Mesh* Mesh1Natu()final;
  Mesh* MeshPatch()final;
#endif
protected:
  const RESTRICT Mesh::ints vert_conn={ 0,1,2 };
  const RESTRICT Mesh::ints vert_edge={ 0,1, 1,2, 2,0 };// Bar2 connectivity of 3 sides
  const RESTRICT Mesh::vals vert_coor={
    0.0, 0.0,
    1.0, 0.0,
    0.0, 1.0};
private: 
};
class Qua final: public Elem{
public: Qua(INT_ORDER p) : Elem(2,4,4,p,1){}
  const RESTRICT Mesh::vals ShapeFunction(const INT_ORDER,
                                          const FLOAT_MESH p[2])final;
  const RESTRICT Mesh::vals ShapeGradient(const INT_ORDER,
                                          const FLOAT_MESH p[2])final;
  const RESTRICT Mesh::vals GaussLegendre(const INT_ORDER p )final;
  int JacsDets()final;
#ifdef HAS_PATCH
  Mesh* Mesh1Natu()final;
  Mesh* MeshPatch()final;
#endif
protected:
  const RESTRICT Mesh::ints vert_conn={ 0,1,2,3 };
  const RESTRICT Mesh::ints vert_edge={ 0,1, 1,2, 2,3, 3,0 };// Bar2 connectivity of 3 sides
  const RESTRICT Mesh::vals vert_coor={
    -1.0,-1.0,
     1.0,-1.0,
     1.0, 1.0,
    -1.0, 1.0};
   //-1.0, 1.0, 1.0,-1.0,
   //-1.0,-1.0, 1.0, 1.0};
private:
};
class Tet final: public Elem{
public:
  //Tet(INT_ORDER p) : Elem(3,4,6,p,1){ this->gaus_p=p; };//FIXME Remove?
  Tet(INT_ORDER p) : Elem(3,4,6,((p<3)?0:4),p,p,1){  };//FIXME Remove?
  Tet(INT_ORDER p, INT_MESH e) : Elem(3,4,6,((p<3)?0:4),p,p,e){};
  const RESTRICT Mesh::vals ShapeFunction(const INT_ORDER,
                                          const FLOAT_MESH p[3])final;
  const RESTRICT Mesh::vals ShapeGradient(const INT_ORDER,
                                          const FLOAT_MESH  [3])final;
  const RESTRICT Mesh::vals GaussLegendre(const INT_ORDER p )final;
  int JacsDets()final;
#ifdef HAS_PATCH
  Mesh* Mesh1Natu()final;
  Mesh* MeshPatch()final;
#endif
protected:
  const RESTRICT Mesh::ints vert_conn={ 0,1,2,3 };
  //const RESTRICT Mesh::ints vert_edge={ 0,1, 1,2, 2,0, 0,3, 1,3, 2,3 };
  //const RESTRICT Mesh::ints vert_face={ 0,1,2, 0,1,3, 1,2,3, 2,0,3 }
  //NOTE The following match gmsh convention for quadratic tets
  const RESTRICT Mesh::ints vert_edge={ 0,1, 1,2, 2,0, 0,3, 2,3, 1,3 };
  const RESTRICT Mesh::ints vert_face={ 0,1,2, 0,1,3, 0,3,2, 1,2,3 };
  const RESTRICT Mesh::vals vert_coor={
    0.0, 0.0, 0.0,
    1.0, 0.0, 0.0,
    0.0, 1.0, 0.0,
    0.0, 0.0, 1.0};
    //0.0, 1.0, 0.0, 0.0,
    //0.0, 0.0, 1.0, 0.0,
    //0.0, 0.0, 0.0, 1.0};
private:
};
class Bri final: public Elem{
public: Bri(INT_ORDER p) : Elem(3,8,12,p,1){}
  //Elem(elem_d, elem_n, elem_vert_n, elem_edge_n , elem_p)
  const RESTRICT Mesh::vals ShapeFunction(const INT_ORDER,
                                          const FLOAT_MESH p[3])final;
  const RESTRICT Mesh::vals ShapeGradient(const INT_ORDER,
                                          const FLOAT_MESH  [3])final;
  const RESTRICT Mesh::vals GaussLegendre(const INT_ORDER p )final;
  int JacsDets()final;
#ifdef HAS_PATCH
  Mesh* Mesh1Natu()final;
  Mesh* MeshPatch()final;
#endif
protected:
  const RESTRICT Mesh::ints vert_conn={ 0,1,2,3, 4,5,6,7 };
  const RESTRICT Mesh::ints vert_edge// Bar2 connectivity
    ={ 0,1, 1,2, 2,3,3,0, 4,5,5,6,6,7,7,4, 0,4,1,5,2,6,3,7 };
  const RESTRICT Mesh::ints vert_face
    ={ 0,1,2,3, 7,6,5,4, 0,4,5,1, 2,3,7,6, 0,3,7,4, 1,5,6,2 };
  const RESTRICT Mesh::vals vert_coor={
                   //       7---------6
     0.0, 0.0, 0.0,//      /|        /|
     1.0, 0.0, 0.0,//     / |       / |
     1.0, 1.0, 0.0,//    /  |      /  |
     0.0, 1.0, 0.0,//   4---------5   |
     0.0, 0.0, 1.0,//   |   |     |   |
     1.0, 0.0, 1.0,//   |   3-----|---2
     1.0, 1.0, 1.0,//   |  /      |  /
     0.0, 1.0, 1.0 //   | /       | /
                   //   |/        |/
  };               //   0---------1
private:
};
//============= Inline Function Definitions ===============
// JD is jacobian dimension (1-3)
#define JD 1
inline FLOAT_MESH Elem::Jac1Det(const FLOAT_MESH m){
  return( m );
};
inline FLOAT_MESH Elem::Jac1Det(RESTRICT const Mesh::vals& m){
  return( m[0] );
};
inline int Elem::Jac1Inv(RESTRICT Mesh::vals& m, const FLOAT_MESH jacdet){
  if(jacdet==0){return 1;}
  m= static_cast<FLOAT_MESH>(1.0)/jacdet;
  if(jacdet < 0){return -1;} else{return 0;}
};
#undef JD
#define JD 2
inline FLOAT_MESH Elem::Jac2Det(const FLOAT_MESH* m){
  return( m[JD*0+ 0]*m[JD*1+ 1] - m[JD*0+ 1]*m[JD*1+ 0]);
};
inline FLOAT_MESH Elem::Jac2Det(const RESTRICT Mesh::vals& m){
  return( m[JD*0+ 0]*m[JD*1+ 1] - m[JD*0+ 1]*m[JD*1+ 0]);
};
inline int Elem::Jac2Inv( FLOAT_MESH* m, const FLOAT_MESH jacdet){
  if(jacdet==0){return 1;}
  //RESTRICT Mesh::vals minv(4);
  FLOAT_MESH minv[4]; FLOAT_MESH dinv=static_cast<FLOAT_MESH>(1.0)/jacdet;
  for(int i=0;i<4;i++){ minv[i]=dinv; };
  minv[JD*0+ 0]*= m[JD*1+ 1] ;
  minv[JD*0+ 1]*=-m[JD*0+ 1] ;
  minv[JD*1+ 0]*=-m[JD*1+ 0] ;
  minv[JD*1+ 1]*= m[JD*0+ 0] ;
  //m=minv*(static_cast<FLOAT_MESH>(1.0)/jacdet);
  for(int i=0;i<4;i++){ m[i]=minv[i]; };
  if(jacdet < 0){return -1;} else{return 0;}
};
inline int Elem::Jac2Inv(RESTRICT Mesh::vals& m, const FLOAT_MESH jacdet){
  if(jacdet==0){return 1;}
  RESTRICT Mesh::vals minv(4);
  minv[JD*0+ 0] = m[JD*1+ 1] ;
  minv[JD*0+ 1] =-m[JD*0+ 1] ;
  minv[JD*1+ 0] =-m[JD*1+ 0] ;
  minv[JD*1+ 1] = m[JD*0+ 0] ;
  m=minv*(static_cast<FLOAT_MESH>(1.0)/jacdet);
  if(jacdet < 0){return -1;} else{return 0;}
};
inline int Elem::Jac2Tnv(RESTRICT Mesh::vals& m, const FLOAT_MESH jacdet){
  if(jacdet==0){return 1;}
  RESTRICT Mesh::vals minv(4);
  minv[JD*0+ 0] = m[JD*1+ 1] ;
  minv[JD*0+ 1] =-m[JD*1+ 0] ;
  minv[JD*1+ 0] =-m[JD*0+ 1] ;
  minv[JD*1+ 1] = m[JD*0+ 0] ;
  m=minv*(static_cast<FLOAT_MESH>(1.0)/jacdet);
  if(jacdet < 0){return -1;} else{return 0;}
};
#undef JD
#define JD 3
inline FLOAT_MESH Elem::Jac3Det(const FLOAT_MESH* m ){
  return(
      m[JD*0+ 0]*(m[JD*1+ 1] * m[JD*2+ 2] - m[JD*2+ 1] * m[JD*1+ 2])
    + m[JD*0+ 1]*(m[JD*1+ 2] * m[JD*2+ 0] - m[JD*2+ 2] * m[JD*1+ 0])
    + m[JD*0+ 2]*(m[JD*1+ 0] * m[JD*2+ 1] - m[JD*2+ 0] * m[JD*1+ 1]) );
};
inline FLOAT_MESH Elem::Jac3Det(RESTRICT const Mesh::vals& m){
  return(
      m[JD*0+ 0]*(m[JD*1+ 1] * m[JD*2+ 2] - m[JD*2+ 1] * m[JD*1+ 2])
    + m[JD*0+ 1]*(m[JD*1+ 2] * m[JD*2+ 0] - m[JD*2+ 2] * m[JD*1+ 0])
    + m[JD*0+ 2]*(m[JD*1+ 0] * m[JD*2+ 1] - m[JD*2+ 0] * m[JD*1+ 1]) );
};
inline int Elem::Jac3Inv( FLOAT_MESH* m, const FLOAT_MESH jacdet){
  if(jacdet==0){return 1;}
  //returns inverse in m
  //RESTRICT Mesh::vals minv(static_cast<FLOAT_MESH>(1.0)/jacdet,9);
  FLOAT_MESH minv[9]; FLOAT_MESH dinv=static_cast<FLOAT_MESH>(1.0)/jacdet;
  for(int i=0;i<9;i++){ minv[i]=dinv; };
  //
  minv[JD*0+ 0]*= (m[JD*1+ 1] * m[JD*2+ 2] - m[JD*2+ 1] * m[JD*1+ 2]) ;
  minv[JD*0+ 1]*= (m[JD*0+ 2] * m[JD*2+ 1] - m[JD*0+ 1] * m[JD*2+ 2]) ;
  minv[JD*0+ 2]*= (m[JD*0+ 1] * m[JD*1+ 2] - m[JD*0+ 2] * m[JD*1+ 1]) ;
  //
  minv[JD*1+ 0]*= (m[JD*1+ 2] * m[JD*2+ 0] - m[JD*1+ 0] * m[JD*2+ 2]) ;
  minv[JD*1+ 1]*= (m[JD*0+ 0] * m[JD*2+ 2] - m[JD*0+ 2] * m[JD*2+ 0]) ;
  minv[JD*1+ 2]*= (m[JD*1+ 0] * m[JD*0+ 2] - m[JD*0+ 0] * m[JD*1+ 2]) ;
  //
  minv[JD*2+ 0]*= (m[JD*1+ 0] * m[JD*2+ 1] - m[JD*2+ 0] * m[JD*1+ 1]) ;
  minv[JD*2+ 1]*= (m[JD*2+ 0] * m[JD*0+ 1] - m[JD*0+ 0] * m[JD*2+ 1]) ;
  minv[JD*2+ 2]*= (m[JD*0+ 0] * m[JD*1+ 1] - m[JD*1+ 0] * m[JD*0+ 1]) ;
  //m=minv;//*(static_cast<FLOAT_MESH>(1.0)/jacdet);
  for(int i=0;i<9;i++){ m[i]=minv[i]; };
  if(jacdet < 0){return -1;} else{return 0;}
};
inline int Elem::Jac3Inv(RESTRICT Mesh::vals& m, const FLOAT_MESH jacdet){
  if(jacdet==0){return 1;}
  //returns inverse in m
  RESTRICT Mesh::vals minv(static_cast<FLOAT_MESH>(1.0)/jacdet,9);
  //
  minv[JD*0+ 0]*= (m[JD*1+ 1] * m[JD*2+ 2] - m[JD*2+ 1] * m[JD*1+ 2]) ;
  minv[JD*0+ 1]*= (m[JD*0+ 2] * m[JD*2+ 1] - m[JD*0+ 1] * m[JD*2+ 2]) ;
  minv[JD*0+ 2]*= (m[JD*0+ 1] * m[JD*1+ 2] - m[JD*0+ 2] * m[JD*1+ 1]) ;
  //
  minv[JD*1+ 0]*= (m[JD*1+ 2] * m[JD*2+ 0] - m[JD*1+ 0] * m[JD*2+ 2]) ;
  minv[JD*1+ 1]*= (m[JD*0+ 0] * m[JD*2+ 2] - m[JD*0+ 2] * m[JD*2+ 0]) ;
  minv[JD*1+ 2]*= (m[JD*1+ 0] * m[JD*0+ 2] - m[JD*0+ 0] * m[JD*1+ 2]) ;
  //
  minv[JD*2+ 0]*= (m[JD*1+ 0] * m[JD*2+ 1] - m[JD*2+ 0] * m[JD*1+ 1]) ;
  minv[JD*2+ 1]*= (m[JD*2+ 0] * m[JD*0+ 1] - m[JD*0+ 0] * m[JD*2+ 1]) ;
  minv[JD*2+ 2]*= (m[JD*0+ 0] * m[JD*1+ 1] - m[JD*1+ 0] * m[JD*0+ 1]) ;
  m=minv;//*(static_cast<FLOAT_MESH>(1.0)/jacdet);
  if(jacdet < 0){return -1;} else{return 0;}
};
inline int Elem::Jac3Tnv(RESTRICT Mesh::vals& m, const FLOAT_MESH jacdet){
  if(jacdet==0){return 1;}
  //returns transposed inverse in m
  RESTRICT Mesh::vals minv(static_cast<FLOAT_MESH>(1.0)/jacdet,9);// minv= 1 / jacdet; // inverse of matrix m
  //
  minv[JD*0+ 0]*= (m[JD*1+ 1] * m[JD*2+ 2] - m[JD*2+ 1] * m[JD*1+ 2]) ;
  minv[JD*0+ 1]*= (m[JD*1+ 2] * m[JD*2+ 0] - m[JD*1+ 0] * m[JD*2+ 2]) ;
  minv[JD*0+ 2]*= (m[JD*1+ 0] * m[JD*2+ 1] - m[JD*2+ 0] * m[JD*1+ 1]) ;
  //
  minv[JD*1+ 0]*= (m[JD*0+ 2] * m[JD*2+ 1] - m[JD*0+ 1] * m[JD*2+ 2]) ;
  minv[JD*1+ 1]*= (m[JD*0+ 0] * m[JD*2+ 2] - m[JD*0+ 2] * m[JD*2+ 0]) ;
  minv[JD*1+ 2]*= (m[JD*2+ 0] * m[JD*0+ 1] - m[JD*0+ 0] * m[JD*2+ 1]) ;
  //
  minv[JD*2+ 0]*= (m[JD*0+ 1] * m[JD*1+ 2] - m[JD*0+ 2] * m[JD*1+ 1]) ;
  minv[JD*2+ 1]*= (m[JD*1+ 0] * m[JD*0+ 2] - m[JD*0+ 0] * m[JD*1+ 2]) ;
  minv[JD*2+ 2]*= (m[JD*0+ 0] * m[JD*1+ 1] - m[JD*1+ 0] * m[JD*0+ 1]) ;
  m=minv;//*(static_cast<FLOAT_MESH>(1.0)/jacdet);
  if(jacdet < 0){return -1;} else{return 0;}
};
#undef JD







#endif
