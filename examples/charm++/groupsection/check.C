
#include "check.decl.h"
#include "ckmulticast.h"

/* readonly */ CProxy_Main mainProxy;
/* readonly */ CProxy_Check checkGroup;
///* readonly */ CkGroupID mCastGrpId;


#define BRANCHING_FACTOR 3


struct sectionBcastMsg : public CkMcastBaseMsg, public CMessage_sectionBcastMsg {

   int k;
   int from_pe;
   CkGroupID gid;
   sectionBcastMsg(int _k, int pe, CkGroupID id) : k(_k),from_pe(pe),gid(id) {}
   void pup(PUP::er &p){
	  CMessage_sectionBcastMsg::pup(p);
	  p|k;
	  p|from_pe;
	  p|gid;
   }
};

class Main : public CBase_Main {
   int sum;
   public:
   Main(CkArgMsg* msg){
	  ckout<<"Numpes: "<<CkNumPes()<<endl;
	  checkGroup = CProxy_Check::ckNew();
	  checkGroup.createSection();
	  sum = 0;
	  mainProxy = thisProxy;
   }
   Main(CkMigrateMessage* msg){}
   void done(int k){
	  ckout<<"Sum : "<<k<<endl;
	  CkExit();
   }
};


class Check : public CBase_Check {
   CProxySection_Check secProxy;
   CkSectionInfo cookie;
   CkMulticastMgr *mymCastGrp;
   CkGroupID mCastGrpId;
   public:
   Check() {}
   Check(CkMigrateMessage* msg) {}
   void createSection(){
	  int numpes = CkNumPes(), step=1;
	  int me = CkMyPe();
	  if(CkMyPe() == 0 || CkMyPe() == 2){   //root
	  //if(CkMyPe() == 0){   //root
		 CkVec<int> elems;
		 for(int i=0; i<numpes; i+=step){
			elems.push_back(i);
		 }
		 //branching factor for the spanning tree
		 int bfactor = 4;
		 mCastGrpId = CProxy_CkMulticastMgr::ckNew();
		 secProxy = CProxySection_Check(checkGroup.ckGetGroupID(), elems.getVec(), elems.size(), bfactor);
		 mymCastGrp = CProxy_CkMulticastMgr(mCastGrpId).ckLocalBranch();
		 secProxy.ckSectionDelegate(mymCastGrp);
		 sectionBcastMsg *msg = new sectionBcastMsg(1, CkMyPe(), mCastGrpId);
		 secProxy.recvMsg(msg);
		 CkPrintf("Leader [%d] :: broadcast groupId %d CkMulticastMgr * %p\n", CkMyPe(), mCastGrpId.idx, mymCastGrp);
	  }
   }

   void repeat(int k){
      //if (CkMyPe() == 0){
      if (CkMyPe() == 0 || CkMyPe() == 2){
	 int ct = k / CkNumPes();
	 CkPrintf("Leader [%d] Iteration %d\n",CkMyPe(), ct);
	 sectionBcastMsg *msg = new sectionBcastMsg(ct+1, CkMyPe(), mCastGrpId);

	 if (ct >= 4){
	    secProxy.finishMsg(msg);
	 }else{
	    secProxy.recvMsg(msg);
	 }
      }
   }

   void recvMsg(sectionBcastMsg *msg){
	  int iteration = msg->k;
	  int from_pe = msg->from_pe;
	  CkGroupID gid = msg->gid;
	  CkMulticastMgr *mCastGrp = CProxy_CkMulticastMgr(gid).ckLocalBranch();
	  CkPrintf("PE [%d] :: sectionBcastMsg received from Leader [%d] @ iteration %d groupId %d CkMulticastMgr* %p\n", CkMyPe(), from_pe, iteration, gid, mCastGrp);
	  CkGetSectionInfo(cookie, msg);
	  mCastGrp->contribute(sizeof(int), &iteration, CkReduction::sum_int, cookie,
		  CkCallback(CkReductionTarget(Check, repeat), checkGroup[from_pe])
		  );
	  CkFreeMsg(msg);
   }

   void finishMsg(sectionBcastMsg *msg){
	  int iteration = msg->k;
	  int from_pe = msg->from_pe;
	  CkGroupID gid = msg->gid;
	  CkMulticastMgr *mCastGrp = CProxy_CkMulticastMgr(gid).ckLocalBranch();
	  CkPrintf("PE [%d] :: finishMsg received from Leader [%d] @ iteration %d groupId %d CkMulticastMgr* %p\n", CkMyPe(), from_pe, iteration, gid, mCastGrp);
	  CkGetSectionInfo(cookie, msg);
	  mCastGrp->contribute(sizeof(int), &iteration, CkReduction::sum_int, cookie,
		  CkCallback(CkReductionTarget(Main, done), mainProxy)
		  );
	  CkFreeMsg(msg);
   }
};


#include "check.def.h"
