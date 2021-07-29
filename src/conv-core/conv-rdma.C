/* Support for Direct Nocopy API (Generic Implementation)
 * Specific implementations are in arch/layer/machine-onesided.{h,c}
 */
#include "converse.h"
#include "conv-rdma.h"
#include <algorithm>
#include <vector>

bool useCMAForZC;

CpvExtern(std::vector<NcpyOperationInfo *>, newZCPupGets);
static int zc_pup_handler_idx;
static int ncpy_handler_idx;

// Methods required to keep the Nocopy Direct API functional on non-LRTS layers
#if !CMK_USE_LRTS
void CmiSetNcpyAckSize(int ackSize) {}

void CmiForwardNodeBcastMsg(int size, char *msg) {}

void CmiForwardProcBcastMsg(int size, char *msg) {}
#endif

/****************************** Zerocopy Direct API For non-RDMA layers *****************************/
/* Support for generic implementation */

// Function Pointer to Acknowledement handler function for the Direct API
RdmaAckCallerFn ncpyDirectAckHandlerFn;

// An Rget initiator PE sends this message to the target PE that will be the source of the data
typedef struct _converseRdmaMsg {
  char cmicore[CmiMsgHeaderSizeBytes];
} ConverseRdmaMsg;

static int get_request_handler_idx;
static int put_data_handler_idx;

// Invoked when this PE has to send a large array for an Rget
static void getRequestHandler(ConverseRdmaMsg *getReqMsg){

  NcpyOperationInfo *ncpyOpInfo = (NcpyOperationInfo *)((char *)(getReqMsg) + sizeof(ConverseRdmaMsg));

  resetNcpyOpInfoPointers(ncpyOpInfo);

  ncpyOpInfo->freeMe = CMK_DONT_FREE_NCPYOPINFO;

  // Get is implemented internally using a call to Put
  CmiIssueRput(ncpyOpInfo);
}

// Invoked when this PE receives a large array as the target of an Rput or the initiator of an Rget
static void putDataHandler(ConverseRdmaMsg *payloadMsg) {

  NcpyOperationInfo *ncpyOpInfo = (NcpyOperationInfo *)((char *)payloadMsg + sizeof(ConverseRdmaMsg));

  resetNcpyOpInfoPointers(ncpyOpInfo);

  // copy the received messsage into the user's destination address
  memcpy((char *)ncpyOpInfo->destPtr,
         (char *)payloadMsg + sizeof(ConverseRdmaMsg) + ncpyOpInfo->ncpyOpInfoSize,
         std::min(ncpyOpInfo->srcSize, ncpyOpInfo->destSize));

  // Invoke the destination ack
  ncpyOpInfo->ackMode = CMK_DEST_ACK; // Only invoke the destination ack
  ncpyOpInfo->freeMe  = CMK_DONT_FREE_NCPYOPINFO;
  ncpyDirectAckHandlerFn(ncpyOpInfo);

  CmiFree(payloadMsg);
}


void CmiIssueRgetCopyBased(NcpyOperationInfo *ncpyOpInfo) {

  int ncpyOpInfoSize = ncpyOpInfo->ncpyOpInfoSize;

  // Send a ConverseRdmaMsg to other PE requesting it to send the array
  ConverseRdmaMsg *getReqMsg = (ConverseRdmaMsg *)CmiAlloc(sizeof(ConverseRdmaMsg) + ncpyOpInfoSize);

  // copy the additional Info into the getReqMsg
  memcpy((char *)getReqMsg + sizeof(ConverseRdmaMsg),
         (char *)ncpyOpInfo,
         ncpyOpInfoSize);

  CmiSetHandler(getReqMsg, get_request_handler_idx);
  CmiSyncSendAndFree(ncpyOpInfo->srcPe, sizeof(ConverseRdmaMsg) + ncpyOpInfoSize, getReqMsg);

  // free original ncpyOpinfo
  if(ncpyOpInfo->freeMe == CMK_FREE_NCPYOPINFO)
    CmiFree(ncpyOpInfo);
}

void CmiIssueRputCopyBased(NcpyOperationInfo *ncpyOpInfo) {

  int ncpyOpInfoSize = ncpyOpInfo->ncpyOpInfoSize;
  int size = ncpyOpInfo->srcSize;
  int destPe = ncpyOpInfo->destPe;

  // Send a ConverseRdmaMsg to the other PE sending the array
  ConverseRdmaMsg *payloadMsg = (ConverseRdmaMsg *)CmiAlloc(sizeof(ConverseRdmaMsg) + ncpyOpInfoSize + size);

  // copy the ncpyOpInfo into the recvMsg
  memcpy((char *)payloadMsg + sizeof(ConverseRdmaMsg),
         (char *)ncpyOpInfo,
         ncpyOpInfoSize);

  // copy the large array into the recvMsg
  memcpy((char *)payloadMsg + sizeof(ConverseRdmaMsg) + ncpyOpInfoSize,
         ncpyOpInfo->srcPtr,
         size);

  // Invoke the source ack
  ncpyOpInfo->ackMode = CMK_SRC_ACK; // only invoke the source ack

  ncpyDirectAckHandlerFn(ncpyOpInfo);

  CmiSetHandler(payloadMsg, put_data_handler_idx);
  CmiSyncSendAndFree(destPe,
                     sizeof(ConverseRdmaMsg) + ncpyOpInfoSize + size,
                     payloadMsg);
}


// Rget/Rput operations are implemented as normal converse messages
// This method is invoked during converse initialization to initialize these message handlers
void CmiOnesidedDirectInit(void) {
  get_request_handler_idx = CmiRegisterHandler((CmiHandler)getRequestHandler);
  put_data_handler_idx = CmiRegisterHandler((CmiHandler)putDataHandler);
  zc_pup_handler_idx = CmiRegisterHandler((CmiHandler)zcPupHandler);

  ncpy_handler_idx = CmiRegisterHandler((CmiHandler)_ncpyAckHandler);

  CmiSetDirectNcpyAckHandler(CmiRdmaDirectAckHandler);
}

/****************************** Zerocopy Direct API *****************************/

// Get Methods
void CmiNcpyBuffer::memcpyGet(CmiNcpyBuffer &source) {
  // memcpy the data from the source buffer into the destination buffer
  memcpy((void *)ptr, source.ptr, std::min(cnt, source.cnt));
}

#if CMK_USE_CMA
void CmiNcpyBuffer::cmaGet(CmiNcpyBuffer &source) {
  CmiIssueRgetUsingCMA(source.ptr,
         source.layerInfo,
         source.pe,
         ptr,
         layerInfo,
         pe,
         std::min(cnt, source.cnt));
}
#endif


void CmiNcpyBuffer::rdmaGet(CmiNcpyBuffer &source, int ackSize, char *srcAck, char *destAck) {

  //int ackSize = sizeof(CmiCallback);

  if(regMode == CMK_BUFFER_UNREG) {
    // register it because it is required for RGET
    CmiSetRdmaBufferInfo(layerInfo + CmiGetRdmaCommonInfoSize(), ptr, cnt, regMode);

    isRegistered = true;
  }

  int rootNode = -1; // -1 is the rootNode for p2p operations

  NcpyOperationInfo *ncpyOpInfo = createNcpyOpInfo(source, *this, ackSize, srcAck, destAck, rootNode, CMK_DIRECT_API, (void *)ref);

  CmiIssueRget(ncpyOpInfo);
}

void invokeCallbackHandler(CmiNcpyBuffer &buffer) {
  if(buffer.handler != CMK_IGNORE_HANDLER) {
    ncpyCallbackMsg *msg = (ncpyCallbackMsg *)CmiAlloc(sizeof(ncpyCallbackMsg));
    CmiSetHandler(msg, buffer.handler);
    msg->buff = buffer;
    CmiSyncSendAndFree(buffer.pe, sizeof(ncpyCallbackMsg), msg);
  }
}

void invokeCallback(CmiNcpyBuffer *buffer) {
  if(buffer->handler != CMK_IGNORE_HANDLER) {
    ncpyCallbackMsg *msg = (ncpyCallbackMsg *)CmiAlloc(sizeof(ncpyCallbackMsg));
    CmiSetHandler(msg, buffer->handler);
    msg->buff = *buffer;
    CmiSyncSendAndFree(buffer->pe, sizeof(ncpyCallbackMsg), msg);
  }
//#if CMK_SMP
//    //call to callbackgroup to call the callback when calling from comm thread
//    //this adds one more trip through the scheduler
//    _ckcallbackgroup[buff.pe].call(buff.cb, sizeof(CkNcpyBuffer), (const char *)(&buff));
//#else
//    //Invoke the callback
//    buff.cb.send(sizeof(CkNcpyBuffer), &buff);
//#endif
}

#if CMK_USE_CMA && CMK_REG_REQUIRED
void CkRdmaEMDeregAndAckDirectHandler(void *ack) {

  CmiNcpyBuffer buffInfo = *(CmiNcpyBuffer *)ack;

  CmiPrintf("[%d] CkRdmaEMDeregAndAckDirectHandler\n", CmiMyPe());
  buffInfo.print();

  // De-register source buffer
  deregisterBuffer(buffInfo);

  // Invoke Callback
  invokeCallback(&buffInfo);
}
#endif

inline void _ncpyAckHandler(ncpyHandlerMsg *msg) {
  //QdProcess(1);

  switch(msg->opMode) {
#if CMK_USE_CMA && CMK_REG_REQUIRED
    case ncpyHandlerIdx::CMA_DEREG_ACK_DIRECT  : CkRdmaEMDeregAndAckDirectHandler((char *)msg + sizeof(ncpyHandlerMsg));
                                                 break;
#endif
    default                                    : CmiAbort("_ncpyAckHandler: Invalid OpMode\n");
                                                 break;
  }

  CmiFree(msg); // Allocated in invokeRemoteNcpyAckHandler
}


inline void invokeCmaDirectRemoteDeregAckHandler(CmiNcpyBuffer &buffInfo, ncpyHandlerIdx opMode) {

  CmiPrintf("[%d] invokeCmaDirectRemoteDeregAckHandler\n", CmiMyPe());
  buffInfo.print();
  ncpyHandlerMsg *msg = (ncpyHandlerMsg *)CmiAlloc(sizeof(ncpyHandlerMsg) + sizeof(CmiNcpyBuffer));

  memcpy((char *)msg + sizeof(ncpyHandlerMsg), (char *)&buffInfo, sizeof(CmiNcpyBuffer));

  msg->opMode = opMode;
  CmiSetHandler(msg, ncpy_handler_idx);
  //QdCreate(1); // Matching QdProcess in _ncpyAckHandler
  CmiSyncSendAndFree(buffInfo.pe, sizeof(ncpyHandlerMsg) + sizeof(CmiNcpyBuffer), (char *)msg);
}

// Perform a nocopy get operation into this destination using the passed source
CmiNcpyStatus CmiNcpyBuffer::get(CmiNcpyBuffer &source){

  if(regMode == CMK_BUFFER_NOREG || source.regMode == CMK_BUFFER_NOREG) {
    CmiAbort("Cannot perform RDMA operations in CMK_BUFFER_NOREG mode\n");
  }

  // Check that the source buffer fits into the destination buffer
  if(cnt < source.cnt)
    CmiAbort("CmiNcpyBuffer::get (destination.cnt < source.cnt) Destination buffer is smaller than the source buffer\n");

  // Check that this object is local when get is called
  CmiAssert(CmiNodeOf(pe) == CmiMyNode());

  CmiNcpyMode transferMode = findTransferMode(source.pe, pe);

  //Check if it is a within-process sending
  if(transferMode == CmiNcpyMode::MEMCPY) {
    memcpyGet(source);

#if CMK_REG_REQUIRED
    // De-register source
    if(source.isRegistered && source.deregMode == CMK_BUFFER_DEREG)
      deregisterBuffer(source);
#endif

    //Invoke the source callback
    //source.cb.send(sizeof(CmiNcpyBuffer), &source);
    invokeCallbackHandler(source);

#if CMK_REG_REQUIRED
    // De-register destination
    if(isRegistered && deregMode == CMK_BUFFER_DEREG)
      deregisterBuffer(*this);
#endif

    //Invoke the destination callback
    //cb.send(sizeof(CmiNcpyBuffer), this);
    invokeCallbackHandler(*this);

    // rdma data transfer complete
    return CmiNcpyStatus::complete;

#if CMK_USE_CMA
  } else if(transferMode == CmiNcpyMode::CMA) {

    cmaGet(source);

#if CMK_REG_REQUIRED
    // De-register source and invoke cb
    if(source.isRegistered && source.deregMode == CMK_BUFFER_DEREG)
      invokeCmaDirectRemoteDeregAckHandler(source, ncpyHandlerIdx::CMA_DEREG_ACK_DIRECT); // Send a message to de-register source buffer and invoke callback
    else
#endif
      //source.cb.send(sizeof(CmiNcpyBuffer), &source); //Invoke the source callback
      invokeCallbackHandler(source);

#if CMK_REG_REQUIRED
    // De-register destination
    if(isRegistered && deregMode == CMK_BUFFER_DEREG)
      deregisterBuffer(*this);
#endif

    //Invoke the destination callback
    //cb.send(sizeof(CmiNcpyBuffer), this);
    //CmiHandlerInfo *h = &(CmiHandlerToInfo(handler));
    //h->hdlr(nullptr, h->userPtr);
    invokeCallbackHandler(*this);

    // rdma data transfer complete
    return CmiNcpyStatus::complete;

#endif // end of CMK_USE_CMA
  } else if (transferMode == CmiNcpyMode::RDMA) {

    //zcQdIncrement();

    //rdmaGet(source, sizeof(CkCallback), (char *)&source.cb, (char *)&cb);
    rdmaGet(source, sizeof(int), (char *)(&source.handler), (char *)(&handler));

    // rdma data transfer incomplete
    return CmiNcpyStatus::incomplete;

  } else {
    CmiAbort("CmiNcpyBuffer::get : Invalid CmiNcpyMode");
  }
}

void constructSourceBufferObject(NcpyOperationInfo *info, CmiNcpyBuffer &src) {
  src.ptr = info->srcPtr;
  src.pe  = info->srcPe;
  src.cnt = info->srcSize;
  src.ref = info->srcRef;
  src.regMode = info->srcRegMode;
  src.deregMode = info->srcDeregMode;
  src.isRegistered = info->isSrcRegistered;
  memcpy((char *)(&src.handler), info->srcAck, info->srcAckSize); // initialize cb
  memcpy((char *)(src.layerInfo), info->srcLayerInfo, info->srcLayerSize); // initialize layerInfo
}

void constructDestinationBufferObject(NcpyOperationInfo *info, CmiNcpyBuffer &dest) {
  dest.ptr = info->destPtr;
  dest.pe  = info->destPe;
  dest.cnt = info->destSize;
  dest.ref = info->destRef;
  dest.regMode = info->destRegMode;
  dest.deregMode = info->destDeregMode;
  dest.isRegistered = info->isDestRegistered;
  memcpy((char *)(&dest.handler), info->destAck, info->destAckSize); // initialize cb
  memcpy((char *)(dest.layerInfo), info->destLayerInfo, info->destLayerSize); // initialize layerInfo
}

void invokeSourceCallback(NcpyOperationInfo *info) {
  int handler = *(int *)(info->srcAck);
  if(handler != CMK_IGNORE_HANDLER) {
    if(info->ackMode == CMK_SRC_DEST_ACK || info->ackMode == CMK_SRC_ACK) {
      ncpyCallbackMsg *msg = (ncpyCallbackMsg *)CmiAlloc(sizeof(ncpyCallbackMsg));
      CmiSetHandler(msg, handler);

      CmiNcpyBuffer src;
      constructSourceBufferObject(info, src);
      msg->buff = src;
      CmiSyncSendAndFree(info->srcPe, sizeof(ncpyCallbackMsg), msg);
    }
  }
}

void invokeDestinationCallback(NcpyOperationInfo *info) {
  int handler = *(int *)(info->destAck);
  if(handler != CMK_IGNORE_HANDLER) {
    if(info->ackMode == CMK_SRC_DEST_ACK || info->ackMode == CMK_DEST_ACK) {
      ncpyCallbackMsg *msg = (ncpyCallbackMsg *)CmiAlloc(sizeof(ncpyCallbackMsg));
      CmiSetHandler(msg, handler);

      CmiNcpyBuffer dest;
      constructDestinationBufferObject(info, dest);
      msg->buff = dest;
      CmiSyncSendAndFree(info->destPe, sizeof(ncpyCallbackMsg), msg);
    }
  }
}

inline void deregisterDestBuffer(NcpyOperationInfo *ncpyOpInfo) {
  CmiDeregisterMem(ncpyOpInfo->destPtr, ncpyOpInfo->destLayerInfo + CmiGetRdmaCommonInfoSize(), ncpyOpInfo->destPe, ncpyOpInfo->destRegMode);
  ncpyOpInfo->isDestRegistered = 0;
}

inline void deregisterSrcBuffer(NcpyOperationInfo *ncpyOpInfo) {
  CmiDeregisterMem(ncpyOpInfo->srcPtr, ncpyOpInfo->srcLayerInfo + CmiGetRdmaCommonInfoSize(), ncpyOpInfo->srcPe, ncpyOpInfo->srcRegMode);
  ncpyOpInfo->isSrcRegistered = 0;
}

void handleDirectApiCompletion(NcpyOperationInfo *info) {

  int freeMe = info->freeMe;

  if(CmiMyNode() == CmiNodeOf(info->destPe)) {
#if CMK_REG_REQUIRED
    if(info->isDestRegistered == 1 && info->destDeregMode == CMK_BUFFER_DEREG)
      deregisterDestBuffer(info);
#endif

    // Invoke the destination callback
    invokeDestinationCallback(info);

#if CMK_REG_REQUIRED
    // send a message to the source to de-register and invoke callback
    if(info->isSrcRegistered == 1 && info->srcDeregMode == CMK_BUFFER_DEREG) {
      freeMe = CMK_DONT_FREE_NCPYOPINFO; // don't free info here, it'll be freed by the machine layer
      //QdCreate(1); // Matching QdProcess in CkRdmaDirectAckHandler
      CmiInvokeRemoteDeregAckHandler(info->srcPe, info);
    } else
#endif
      invokeSourceCallback(info);
  }

  if(CmiMyNode() == CmiNodeOf(info->srcPe)) {
#if CMK_REG_REQUIRED
    if(info->isSrcRegistered == 1 && info->srcDeregMode == CMK_BUFFER_DEREG)
      deregisterSrcBuffer(info);
#endif

    // Invoke the source callback
    invokeSourceCallback(info);

#if CMK_REG_REQUIRED
    // send a message to the destination to de-register and invoke callback
    if(info->isDestRegistered == 1 && info->destDeregMode == CMK_BUFFER_DEREG) {
      freeMe = CMK_DONT_FREE_NCPYOPINFO; // don't free info here, it'll be freed by the machine layer
      //QdCreate(1); // Matching QdProcess in CkRdmaDirectAckHandler
      CmiInvokeRemoteDeregAckHandler(info->destPe, info);
    } else
#endif
      invokeDestinationCallback(info);
  }

  if(freeMe == CMK_FREE_NCPYOPINFO)
    CmiFree(info);
}

// Ack handler function which invokes the callback
void CmiRdmaDirectAckHandler(void *ack) {
  NcpyOperationInfo *info = (NcpyOperationInfo *)(ack);

  switch(info->opMode) {
    case CMK_DIRECT_API             : //CmiPrintf("[%d] CMK_DIRECT_API completed\n", CmiMyPe());
                                      handleDirectApiCompletion(info); // Ncpy Direct API
                                      break;
    case CMK_EM_API_SRC_ACK_INVOKE  : invokeSourceCallback(info);
                                      break;
    case CMK_EM_API_DEST_ACK_INVOKE : invokeDestinationCallback(info);
                                      break;
    default                         : CmiAbort("CmiRdmaDirectAckHandler: Unknown ncpyOpInfo->opMode %d", info->opMode);
                                      break;
  }
}





NcpyOperationInfo *CmiNcpyBuffer::createNcpyOpInfo(CmiNcpyBuffer &source, CmiNcpyBuffer &destination, int ackSize, char *srcAck, char *destAck, int rootNode, int opMode, void *refPtr) {

  int layerInfoSize = CMK_COMMON_NOCOPY_DIRECT_BYTES + CMK_NOCOPY_DIRECT_BYTES;

  // Create a general object that can be used across layers and can store the state of the CmiNcpyBuffer objects
  int ncpyObjSize = getNcpyOpInfoTotalSize(
                      layerInfoSize,
                      ackSize,
                      layerInfoSize,
                      ackSize);

  NcpyOperationInfo *ncpyOpInfo = (NcpyOperationInfo *)CmiAlloc(ncpyObjSize);

  setNcpyOpInfo(source.ptr,
                (char *)(source.layerInfo),
                layerInfoSize,
                srcAck,
                ackSize,
                source.cnt,
                source.regMode,
                source.deregMode,
                source.isRegistered,
                source.pe,
                source.ref,
                destination.ptr,
                (char *)(destination.layerInfo),
                layerInfoSize,
                destAck,
                ackSize,
                destination.cnt,
                destination.regMode,
                destination.deregMode,
                destination.isRegistered,
                destination.pe,
                destination.ref,
                rootNode,
                ncpyOpInfo);

  ncpyOpInfo->opMode = opMode;
  ncpyOpInfo->refPtr = refPtr;

  return ncpyOpInfo;
}

// Put Methods
void CmiNcpyBuffer::memcpyPut(CmiNcpyBuffer &destination) {
  // memcpy the data from the source buffer into the destination buffer
  memcpy((void *)destination.ptr, ptr, std::min(cnt, destination.cnt));
}

#if CMK_USE_CMA
void CmiNcpyBuffer::cmaPut(CmiNcpyBuffer &destination) {
  CmiIssueRputUsingCMA(destination.ptr,
                       destination.layerInfo,
                       destination.pe,
                       ptr,
                       layerInfo,
                       pe,
                       std::min(cnt, destination.cnt));
}
#endif

void CmiNcpyBuffer::rdmaPut(CmiNcpyBuffer &destination, int ackSize, char *srcAck, char *destAck) {

  int layerInfoSize = CMK_COMMON_NOCOPY_DIRECT_BYTES + CMK_NOCOPY_DIRECT_BYTES;

  if(regMode == CMK_BUFFER_UNREG) {
    // register it because it is required for RPUT
    CmiSetRdmaBufferInfo(layerInfo + CmiGetRdmaCommonInfoSize(), ptr, cnt, regMode);

    isRegistered = true;
  }

  int rootNode = -1; // -1 is the rootNode for p2p operations

  NcpyOperationInfo *ncpyOpInfo = createNcpyOpInfo(*this, destination, ackSize, srcAck, destAck, rootNode, CMK_DIRECT_API, (void *)ref);

  CmiIssueRput(ncpyOpInfo);
}

// Returns CmiNcpyMode::MEMCPY if both the PEs are the same and memcpy can be used
// Returns CmiNcpyMode::CMA if both the PEs are in the same physical node and CMA can be used
// Returns CmiNcpyMode::RDMA if RDMA needs to be used
CmiNcpyMode findTransferMode(int srcPe, int destPe) {
  if(CmiNodeOf(srcPe)==CmiNodeOf(destPe))
    return CmiNcpyMode::MEMCPY;
#if CMK_USE_CMA
  else if(useCMAForZC && CmiDoesCMAWork() && CmiPeOnSamePhysicalNode(srcPe, destPe))
    return CmiNcpyMode::CMA;
#endif
  else
    return CmiNcpyMode::RDMA;
}

CmiNcpyMode findTransferModeWithNodes(int srcNode, int destNode) {
  if(srcNode==destNode)
    return CmiNcpyMode::MEMCPY;
#if CMK_USE_CMA
  else if(useCMAForZC && CmiDoesCMAWork() && CmiPeOnSamePhysicalNode(CmiNodeFirst(srcNode), CmiNodeFirst(destNode)))
    return CmiNcpyMode::CMA;
#endif
  else
    return CmiNcpyMode::RDMA;
}

zcPupSourceInfo *zcPupAddSource(CmiNcpyBuffer &src) {
  zcPupSourceInfo *srcInfo = new zcPupSourceInfo();
  srcInfo->src = src;
  srcInfo->deallocate = free;
  return srcInfo;
}

zcPupSourceInfo *zcPupAddSource(CmiNcpyBuffer &src, std::function<void (void *)> deallocate) {
  zcPupSourceInfo *srcInfo = new zcPupSourceInfo();
  srcInfo->src = src;
  srcInfo->deallocate = deallocate;
  return srcInfo;
}

void zcPupDone(void *ref) {
  zcPupSourceInfo *srcInfo = (zcPupSourceInfo *)(ref);
#if CMK_REG_REQUIRED
  deregisterBuffer(srcInfo->src);
#endif

  srcInfo->deallocate((void *)srcInfo->src.ptr);
  delete srcInfo;
}

void zcPupHandler(ncpyHandlerMsg *msg) {
  zcPupDone(msg->ref);
}

void invokeZCPupHandler(void *ref, int pe) {
  ncpyHandlerMsg *msg = (ncpyHandlerMsg *)CmiAlloc(sizeof(ncpyHandlerMsg));
  msg->ref = (void *)ref;

  CmiSetHandler(msg, zc_pup_handler_idx);
  CmiSyncSendAndFree(pe, sizeof(ncpyHandlerMsg), (char *)msg);
}

void zcPupGet(CmiNcpyBuffer &src, CmiNcpyBuffer &dest) {
  CmiNcpyMode transferMode = findTransferMode(src.pe, dest.pe);
  if(transferMode == CmiNcpyMode::MEMCPY) {
    CmiAbort("zcPupGet: memcpyGet should not happen\n");
  }
#if CMK_USE_CMA
  else if(transferMode == CmiNcpyMode::CMA) {
    dest.cmaGet(src);

#if CMK_REG_REQUIRED
    // De-register destination buffer
    deregisterBuffer(dest);
#endif

    if(src.ref)
      invokeZCPupHandler((void *)src.ref, src.pe);
    else
      CmiAbort("zcPupGet - src.ref is NULL\n");
  }
#endif
  else {
    int ackSize = 0;
    int rootNode = -1; // -1 is the rootNode for p2p operations
    NcpyOperationInfo *ncpyOpInfo = dest.createNcpyOpInfo(src, dest, ackSize, NULL, NULL, rootNode, CMK_ZC_PUP, NULL);
    CpvAccess(newZCPupGets).push_back(ncpyOpInfo);
  }
}

#if CMK_USE_LRTS
#include "machine-rdma.h"
#endif


/* Perform an RDMA Get operation into the local destination address from the remote source address*/
void CmiIssueRget(NcpyOperationInfo *ncpyOpInfo) {
#if CMK_USE_LRTS && CMK_ONESIDED_IMPL
  // Use network RDMA for a PE on a remote host
  LrtsIssueRget(ncpyOpInfo);
#else
  CmiIssueRgetCopyBased(ncpyOpInfo);
#endif
}

/* Perform an RDMA Put operation into the remote destination address from the local source address */
void CmiIssueRput(NcpyOperationInfo *ncpyOpInfo) {
#if CMK_USE_LRTS && CMK_ONESIDED_IMPL
  // Use network RDMA for a PE on a remote host
  LrtsIssueRput(ncpyOpInfo);
#else
  CmiIssueRputCopyBased(ncpyOpInfo);
#endif
}

/* De-register registered memory for pointer */
void CmiDeregisterMem(const void *ptr, void *info, int pe, unsigned short int mode){
#if CMK_USE_LRTS && CMK_ONESIDED_IMPL
  LrtsDeregisterMem(ptr, info, pe, mode);
#endif
}

#if CMK_REG_REQUIRED
void CmiInvokeRemoteDeregAckHandler(int pe, NcpyOperationInfo *ncpyOpInfo) {
#if CMK_USE_LRTS && CMK_ONESIDED_IMPL
  LrtsInvokeRemoteDeregAckHandler(pe, ncpyOpInfo);
#endif
}
#endif

/* Set the machine specific information for a nocopy pointer */
void CmiSetRdmaBufferInfo(void *info, const void *ptr, int size, unsigned short int mode){
#if CMK_USE_LRTS && CMK_ONESIDED_IMPL
  LrtsSetRdmaBufferInfo(info, ptr, size, mode);
#endif
}

/* Set the ack handler function used in the Direct API */
void CmiSetDirectNcpyAckHandler(RdmaAckCallerFn fn){
  ncpyDirectAckHandlerFn = fn;
}

#if CMK_USE_CMA
#include <unistd.h>
#endif

/* Support for Nocopy Direct API */
typedef struct _cmi_common_rdma_info {
#if CMK_USE_CMA
  pid_t pid;
#elif defined _MSC_VER
  char empty;
#endif
} CmiCommonRdmaInfo_t;

/* Set the generic converse/LRTS information */
void CmiSetRdmaCommonInfo(void *info, const void *ptr, int size) {
#if CMK_USE_CMA
  CmiCommonRdmaInfo_t *cmmInfo = (CmiCommonRdmaInfo_t *)info;
  cmmInfo->pid = getpid();
#endif
}

int CmiGetRdmaCommonInfoSize() {
#if CMK_USE_CMA
  return sizeof(CmiCommonRdmaInfo_t);
#else
  return 0; // If CMK_USE_CMA is false, sizeof(CmiCommonRdmaInfo_t) is 1 (size of an empty structure in C++)
            // However, 0 is returned since CMK_COMMON_NOCOPY_DIRECT_BYTES is set to 0 when CMK_USE_CMA is false
            // because the offset (returned by CmiGetRdmaCommonInfoSize) should equal CMK_COMMON_NOCOPY_DIRECT_BYTES
#endif
}

#if CMK_USE_CMA
#include <unistd.h>
#include <sys/uio.h> // for struct iovec
extern int cma_works;
int readShmCma(pid_t, char*, char*, size_t);
int writeShmCma(pid_t, char *, char *, size_t);

// These methods are also used by the generic layer implementation of the Direct API
void CmiIssueRgetUsingCMA(
  const void* srcAddr,
  void *srcInfo,
  int srcPe,
  const void* destAddr,
  void *destInfo,
  int destPe,
  size_t size) {

  // get remote process id
  CmiCommonRdmaInfo_t *remoteCommInfo = (CmiCommonRdmaInfo_t *)srcInfo;
  pid_t pid = remoteCommInfo->pid;
  readShmCma(pid, (char *)destAddr, (char *)srcAddr, size);
}

void CmiIssueRputUsingCMA(
  const void* destAddr,
  void *destInfo,
  int destPe,
  const void* srcAddr,
  void *srcInfo,
  int srcPe,
  size_t size) {

  // get remote process id
  CmiCommonRdmaInfo_t *remoteCommInfo = (CmiCommonRdmaInfo_t *)destInfo;
  pid_t pid = remoteCommInfo->pid;
  writeShmCma(pid, (char *)srcAddr, (char *)destAddr, size);
}
#endif

void CmiInvokeNcpyAck(void *ack) {
  ncpyDirectAckHandlerFn(ack);
}
