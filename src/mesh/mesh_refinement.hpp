#ifndef MESH_MESH_REFINEMENT_HPP_
#define MESH_MESH_REFINEMENT_HPP_
//========================================================================================
// AthenaXXX astrophysical plasma code
// Copyright(C) 2020 James M. Stone <jmstone@ias.edu> and the Athena code team
// Licensed under the 3-clause BSD License (the "LICENSE")
//========================================================================================
//! \file mesh_refinement.hpp
//! \brief defines MeshRefinement class containing data and functions controlling SMR/AMR

//----------------------------------------------------------------------------------------
//! \fn int CreateAMR_MPI_Tag(int lid, int ox1, int ox2, int ox3)
//! \brief calculate an MPI tag for AMR communications.  Note maximum size of
//! lid that can be encoded is set by (NUM_BITS_LID) macro.
//! The convention in Athena++ is lid is for the *receiving* process.
//! The MPI standard requires signed int tag, with MPI_TAG_UB>=2^15-1 = 32,767 (inclusive)
static int CreateAMR_MPI_Tag(int lid, int ox1, int ox2, int ox3) {
  return (ox1<<(NUM_BITS_LID+2)) | (ox2<<(NUM_BITS_LID+1))| (ox3<<(NUM_BITS_LID)) | lid;
}

//----------------------------------------------------------------------------------------
//! \struct AMRBuffer
//! \brief container for index ranges, storage, and flags for AMR buffers used with load
//! balancing.

#if MPI_PARALLEL_ENABLED
struct AMRBuffer {
  int bis, bie, bjs, bje, bks, bke;
  int ncells_cc, ncells_fc;
  int data_size;
  int lid;
  bool refine=false, derefine=false;

  DvceArray1D<Real> vars;               // View that stores buffer data on device
  MPI_Request req;

  AMRBuffer() : vars("amr_vars",1), req(MPI_REQUEST_NULL) {}
};
#endif

//----------------------------------------------------------------------------------------
//! \class MeshRefinement
//! \brief data/functions associated with SMR/AMR

class MeshRefinement {
  // mesh classes (Mesh, MeshBlock, MeshBlockPack, MeshBlockTree) like to play together
//  friend class Mesh;
//  friend class MeshBlockPack;

 public:
  MeshRefinement(Mesh *pm, ParameterInput *pin);
  ~MeshRefinement();

  // data
  int nmb_created_thisrank;   // # of MeshBlocks created via AMR on this rank
  int nmb_deleted_thisrank;   // # of MeshBlocks deleted via AMR on this rank
  int nmb_sent_thisrank;      // # of MeshBlocks sent during load balancing on this rank
  int ncyc_check_amr;         // # of cycles between checking mesh for ref/derefinement
  int refinement_interval;    // # of cycles between allowing successive ref/derefinement

  // following 2x Views are dimensioned [nmb_total]
  DualArray1D<int> refine_flag;    // refinement flag for each MeshBlock
  HostArray1D<int> ncyc_since_ref; // # of cycles since MB last refined/derefined

  // following 4x arrays allocated with length [nranks] only with AMR
  int *nref_eachrank;     // number of MBs refined per rank
  int *nderef_eachrank;   // number of MBs de-refined per rank
  int *nref_rsum;         // running sum of number of MBs refined per rank
  int *nderef_rsum;       // running sum of number of MBs de-refined per rank
  // following 2x arrays allocated with length [nmb_new] and [nmb_old]] only with AMR
  int *newtoold;          // mapping of new gid (index n) to old gid
  int *oldtonew;          // mapping of old gid (index n) to new gid

  // arrays in Mesh class created for new MB hieararchy with AMR
  // following 3x arrays allocated with length [new_nmb_total]
  float *new_cost_eachmb;            // cost of each MeshBlock
  int *new_rank_eachmb;              // rank of each MeshBlock
  LogicalLocation *new_lloc_eachmb;  // LogicalLocations for each MeshBlock
  // following 2x arrays allocated with length [nranks]
  int *new_gids_eachrank;      // starting global ID of MeshBlocks in each rank
  int *new_nmb_eachrank;       // number of MeshBlocks on each rank

#if MPI_PARALLEL_ENABLED
  int nmb_send, nmb_recv;
  MPI_Comm amr_comm;                  // unique communicator for AMR
  AMRBuffer *send_buf, *recv_buf;     // send/recv buffers (dimensioned nsend/nrecv)
#endif

  // functions
  void CheckForRefinement(MeshBlockPack* pmbp);
  void AdaptiveMeshRefinement(Driver *pdrive, ParameterInput *pin);
  void UpdateMeshBlockTree(int &nnew, int &ndel);
  void RedistAndRefineMeshBlocks(ParameterInput *pin, int nnew, int ndel);

  void DerefineCCSameRank(DvceArray5D<Real> &a, DvceArray5D<Real> &ca, int nleaf);
  void DerefineFCSameRank(DvceFaceFld4D<Real> &b, DvceFaceFld4D<Real> &cb, int nleaf);

  void MoveLeftCC(DvceArray5D<Real> &a);
  void MoveLeftFC(DvceFaceFld4D<Real> &b);
  void MoveRightCC(DvceArray5D<Real> &a);
  void MoveRightFC(DvceFaceFld4D<Real> &b);

  void MoveForRefinementCC(DvceArray5D<Real> &a, DvceArray5D<Real> &ca, int nleaf);
  void MoveForRefinementFC(DvceFaceFld4D<Real> &b, DvceFaceFld4D<Real> &cb, int nleaf);

  void RefineCC(DualArray1D<int> &n2o, DvceArray5D<Real> &a, DvceArray5D<Real> &ca);
  void RefineFC(DualArray1D<int> &n2o, DvceFaceFld4D<Real> &b, DvceFaceFld4D<Real> &cb);

  void RestrictCC(DvceArray5D<Real> &a, DvceArray5D<Real> &ca);
  void RestrictFC(DvceFaceFld4D<Real> &b, DvceFaceFld4D<Real> &cb);

  // functions for load balancing (in file load_balance.cpp)
  void InitRecvAMR(int nleaf);
  void PackAndSendAMR(int nleaf);
  void PackAMRBuffersCC(DvceArray5D<Real> &a, DvceArray5D<Real> &ca, int offset);
  void RecvAndUnpackAMR();
  void UnpackAMRBuffersCC(DvceArray5D<Real> &a, DvceArray5D<Real> &ca, int offset);
  void ClearSendAMR();
 private:
  // data
  Mesh *pmy_mesh;
  Real d_threshold_, dd_threshold_, dp_threshold_, dv_threshold_;
  bool check_cons_;
};

#endif // MESH_MESH_REFINEMENT_HPP_
