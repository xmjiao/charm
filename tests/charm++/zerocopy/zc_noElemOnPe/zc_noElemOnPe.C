#include "zc_noElemOnPe.decl.h"

#define DEBUG(x) //x

int numElements;
CProxy_Main mProxy;

class RandomMap: public CkArrayMap {
  public:
  RandomMap() {}
  RandomMap(CkMigrateMessage *m) {}
  int procNum(int hdl, const CkArrayIndex &aid) {
    CkArrayIndex1D idx1d = *(CkArrayIndex1D *) &aid;
    //CkPrintf("[%d][%d][%d] elem=%d, %d\n", CmiMyPe(), CmiMyNode(), CmiMyRank(), idx1d.index[0], *(int *)idx1d.data());
    return CkNumPes() - 1;
  }
};

class Main : public CBase_Main {
  int testIndex;
  int srcCompletedCounter;
  CProxy_testArr arr1;
  bool reductionCompleted;

  int size1, size2, size3, doneCounter;
  char *buff1, *buff2, *buff3;

  CkCallback srcCompletionCb;

  public:
  Main(CkArgMsg *m) {
    numElements = CkNumPes();
    delete m;

    mProxy = thisProxy;

    reductionCompleted = false;

    srcCompletedCounter = 0;
    mProxy = thisProxy;

    testIndex = 0;

    size1 = 2001;
    size2 = 67;
    size3 = 4578;

    buff1 = new char[size1];
    buff2 = new char[size2];
    buff3 = new char[size3];

    CProxy_RandomMap randomMap = CProxy_RandomMap::ckNew();
    CkArrayOptions arrOpts(numElements);
    arrOpts.setMap(randomMap);
    arr1 = CProxy_testArr::ckNew(arrOpts);

    srcCompletionCb = CkCallback(CkIndex_Main::zcSrcCompleted(NULL), mProxy);

    arr1.recvEmSendApiBuffer(CkSendBuffer(buff1, srcCompletionCb), size1,
        CkSendBuffer(buff2, srcCompletionCb), size2,
        CkSendBuffer(buff3, srcCompletionCb), size3);
  };

  void zcSrcCompleted(CkDataMsg *m) {
    srcCompletedCounter++;
      delete m;

    if(srcCompletedCounter == 3) {
      srcCompletedCounter = 0;
      done();
    }
  }

  void done() {
    doneCounter++;
    if(doneCounter == 2) {
      doneCounter = 0;
      if(testIndex == 0) {
        CkPrintf("[%d][%d][%d] Test 1: EM Send API bcast and reduction completed\n", CmiMyPe(), CmiMyNode(), CmiMyRank());
        testIndex++;
        arr1.recvEmPostApiBuffer(CkSendBuffer(buff1, srcCompletionCb), size1,
            CkSendBuffer(buff2, srcCompletionCb), size2,
            CkSendBuffer(buff3, srcCompletionCb), size3);
      } else if(testIndex == 1) {
        CkPrintf("[%d][%d][%d] Test 2: EM Post API bcast and reduction completed\n", CmiMyPe(), CmiMyNode(), CmiMyRank());
        CkExit();
      } else {
        CkAbort("Invalid test index\n");
      }
    }
  }

};

class testArr : public CBase_testArr {
  int size1, size2, size3;
  char *buff1, *buff2, *buff3;

  CkCallback reductionCb;

  public:
  testArr() {
    CkPrintf("[%d][%d][%d] testArr element %d created on %d\n", CmiMyPe(), CmiMyNode(), CmiMyRank(), thisIndex, CmiMyPe());
    reductionCb = CkCallback(CkReductionTarget(Main, done), mProxy);
  }

  void recvEmSendApiBuffer(char *buff1, int size1, char *buff2, int size2, char *buff3, int size3) {
    //CkPrintf("[%d][%d][%d] EM Send API: Received nocopy buffers %d\n", CmiMyPe(), CmiMyNode(), CmiMyRank(), thisIndex);
    // Perform a reduction across all chare array elements to ensure that EM Send API has been received
    // by elements with indices > numElements/2
    contribute(reductionCb);
  }

  void recvEmPostApiBuffer(char *buff1, int size1, char *buff2, int size2, char *buff3, int size3, CkNcpyBufferPost *ncpyPost) {
    //CkPrintf("[%d][%d][%d] EM Post API: Posting buffers in the Post Entry Method %d\n", CmiMyPe(), CmiMyNode(), CmiMyRank(), thisIndex);
    this->buff1 = new char[size1];
    this->buff2 = new char[size2];
    this->buff3 = new char[size3];

    // use member variable buffers (buff1, buff2, buff3) as recipient buffers
    CkMatchBuffer(ncpyPost, 0, thisIndex*3);
    CkMatchBuffer(ncpyPost, 1, thisIndex*3 + 1);
    CkMatchBuffer(ncpyPost, 2, thisIndex*3 + 2);

    CkPostBuffer(this->buff1, size1, thisIndex*3);
    CkPostBuffer(this->buff2, size2, thisIndex*3 + 1);
    CkPostBuffer(this->buff3, size3, thisIndex*3 + 2);
  }

  void recvEmPostApiBuffer(char *buff1, int size1, char *buff2, int size2, char *buff3, int size3) {
    //CkPrintf("[%d][%d][%d] EM Post API: Received nocopy buffers %d\n", CmiMyPe(), CmiMyNode(), CmiMyRank(), thisIndex);
    // Perform a reduction across all chare array elements to ensure that EM Send API has been received
    // by elements with indices > numElements/2
    contribute(reductionCb);
  }
};

#include "zc_noElemOnPe.def.h"
