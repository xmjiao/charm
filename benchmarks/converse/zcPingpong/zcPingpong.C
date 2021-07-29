#include <stdlib.h>
#include <converse.h>
#include <conv-rdma.h>

CpvDeclare(int,nCycles);
CpvDeclare(int,minMsgSize);
CpvDeclare(int,maxMsgSize);
CpvDeclare(int,factor);
CpvDeclare(bool,warmUp);
CpvDeclare(int,msgSize);
CpvDeclare(int,cycleNum);
CpvDeclare(int,count);

CpvDeclare(void *,buffer);
CpvDeclare(int, bufferSize);
CpvDeclare(CmiNcpyBuffer, mySrcBuff);
CpvDeclare(CmiNcpyBuffer, myDestBuff);

CpvDeclare(int,setupHandler);
CpvDeclare(int,startHandler);
CpvDeclare(int,node0Handler);
CpvDeclare(int,node1Handler);

CpvDeclare(int,setupZcHandler);
CpvDeclare(int,startZcHandler);

CpvDeclare(int,node0ZCHandler);
CpvDeclare(int,node1ZCHandler);
CpvDeclare(int,node0ZCCompHandler);
CpvDeclare(int,node1ZCCompHandler);

CpvDeclare(int,exitHandler);

CpvStaticDeclare(double,startTime1);
CpvStaticDeclare(double,endTime1);

CpvStaticDeclare(double,startTime2);
CpvStaticDeclare(double,endTime2);

struct regularMsg {
  char core[CmiMsgHeaderSizeBytes];
  int bufferSize;
};

void setupPingpong(regularMsg *setupMsg) {

  //Free previous buffer
  //CmiPrintf("[%d] setupPingpong prev message size is %d and buffer is %p\n", CmiMyPe(), CpvAccess(msgSize), CpvAccess(buffer));
  //CmiPrintf("[%d] setupPingpong msgSize=%d\n", CmiMyPe(), CpvAccess(msgSize));

  if(CpvAccess(buffer) != nullptr) {
    CpvAccess(mySrcBuff).deregisterMem();
    CpvAccess(myDestBuff).deregisterMem();
    //CmiPrintf("[%d] freeing buffer %p\n", CmiMyPe(), CpvAccess(buffer));
    CmiFree(CpvAccess(buffer));
  }

  CpvAccess(msgSize) = setupMsg->bufferSize;
  CpvAccess(buffer) = (void *)CmiAlloc(CpvAccess(msgSize));

  if(CpvAccess(msgSize) >= 1 << 19)
    CpvAccess(nCycles) = 100;

  //CmiPrintf("[%d] allocating buffer %p\n", CmiMyPe(), CpvAccess(buffer));

  //CmiPrintf("[%d] setupPingpong new message size is %d and buffer is %p\n", CmiMyPe(), CpvAccess(msgSize), CpvAccess(buffer));

  CmiFree(setupMsg);

  char *startMsg = (char *)CmiAlloc(CmiMsgHeaderSizeBytes);
  CmiSetHandler(startMsg, CpvAccess(startHandler));
  CmiSyncSendAndFree(0, CmiMsgHeaderSizeBytes, startMsg);
}

// Start the pingpong for each message size
void startPingpong(char *startMsg)
{
  CpvAccess(count)++;
  CmiFree(startMsg);

  if(CpvAccess(count) == CmiNumPes()) { // ready to start Pingpong

    CpvAccess(count) = 0;
    //CmiPrintf("[%d] startPingpong message size is %d and buffer is %p\n", CmiMyPe(), CpvAccess(msgSize), CpvAccess(buffer));
    CpvAccess(cycleNum) = 0;

    // send the first message to node 1
    CpvAccess(startTime1) = CmiWallTimer();

    char *msg = (char *)CmiAlloc(sizeof(regularMsg) + CpvAccess(msgSize));
    ((regularMsg *)msg)->bufferSize = CpvAccess(msgSize);
    memcpy(msg + sizeof(regularMsg), CpvAccess(buffer), CpvAccess(msgSize));
    CmiSetHandler(msg, CpvAccess(node1Handler));
    CmiSyncSendAndFree(1, sizeof(regularMsg) + CpvAccess(msgSize), msg);
  }
}

CmiHandler node1HandlerFunc(regularMsg *rMsg)
{
  CpvAccess(msgSize) = rMsg->bufferSize;

  //CmiPrintf("[%d] node1HandlerFunc and message size is %d and buffer is %p\n", CmiMyPe(), CpvAccess(msgSize), CpvAccess(buffer));
  // copy from received message into user buffer
  memcpy(CpvAccess(buffer), (char *)rMsg + sizeof(regularMsg), CpvAccess(msgSize));

  // send a message to node 0
  char *msg = (char *)CmiAlloc(sizeof(regularMsg) + CpvAccess(msgSize));
  ((regularMsg *)msg)->bufferSize = CpvAccess(msgSize);
  memcpy(msg + sizeof(regularMsg), CpvAccess(buffer), CpvAccess(msgSize));
  CmiSetHandler(msg, CpvAccess(node0Handler));
  CmiSyncSendAndFree(0, sizeof(regularMsg) + CpvAccess(msgSize), msg);

  CmiFree(rMsg);
  return 0;
}

CmiHandler node0HandlerFunc(regularMsg *rMsg)
{
  CpvAccess(cycleNum)++;

  CpvAccess(msgSize) = rMsg->bufferSize;
  //CmiPrintf("[%d] node0HandlerFunc and message size is %d aCmiMyPe(), CpvAccess(msgSize), CpvAccess(buffer));

  if (CpvAccess(cycleNum) == CpvAccess(nCycles)) {
    CpvAccess(cycleNum) = 0;
    CpvAccess(endTime1) = CmiWallTimer();

    void *setupMsg = CmiAlloc(CmiMsgHeaderSizeBytes);
    CmiSetHandler(setupMsg, CpvAccess(setupZcHandler));
    CmiSyncBroadcastAllAndFree(CmiMsgHeaderSizeBytes, setupMsg);

  } else {
    // copy from received message into user buffer
    memcpy(CpvAccess(buffer), (char *)rMsg + sizeof(regularMsg), CpvAccess(msgSize));

    // send a message to node 1
    char *msg = (char *)CmiAlloc(sizeof(regularMsg) + CpvAccess(msgSize));
    memcpy(msg + sizeof(regularMsg), CpvAccess(buffer), CpvAccess(msgSize));
    ((regularMsg *)msg)->bufferSize = CpvAccess(msgSize);
    CmiSetHandler(msg, CpvAccess(node1Handler));
    CmiSyncSendAndFree(1, sizeof(regularMsg) + CpvAccess(msgSize), msg);
  }
  CmiFree(rMsg);
  return 0;
}

struct zcMsg {
  char core[CmiMsgHeaderSizeBytes];
  CmiNcpyBuffer buffer;
};

void setupZcPingpong(char *setupMsg) {

  //CpvAccess(msgSize) = setupMsg->bufferSize;
  CmiFree(setupMsg);

  //CmiPrintf("[%d] setupZcPingpong message size is %d and buffer is %p\n", CmiMyPe(), CpvAccess(msgSize), CpvAccess(buffer));

  CpvAccess(mySrcBuff) = CmiNcpyBuffer(CpvAccess(buffer), CpvAccess(msgSize), CMK_IGNORE_HANDLER, CMK_BUFFER_REG, CMK_BUFFER_NODEREG);

  if(CmiMyPe() == 0)
    CpvAccess(myDestBuff) = CmiNcpyBuffer(CpvAccess(buffer), CpvAccess(msgSize), CpvAccess(node0ZCCompHandler), CMK_BUFFER_REG, CMK_BUFFER_NODEREG);
  else
    CpvAccess(myDestBuff) = CmiNcpyBuffer(CpvAccess(buffer), CpvAccess(msgSize), CpvAccess(node1ZCCompHandler), CMK_BUFFER_REG, CMK_BUFFER_NODEREG);


  char *startMsg = (char *)CmiAlloc(CmiMsgHeaderSizeBytes);
  CmiSetHandler(startMsg, CpvAccess(startZcHandler));
  CmiSyncSendAndFree(0, CmiMsgHeaderSizeBytes, startMsg);
}


void startZcPingpong(char *startMsg) {

  CpvAccess(count)++;
  CmiFree(startMsg);

  if(CpvAccess(count) == CmiNumPes()) { // ready to start Pingpong

    CpvAccess(count) = 0;

    CpvAccess(startTime2) = CmiWallTimer();
    zcMsg *msg = (zcMsg *)CmiAlloc(sizeof(zcMsg));
    CmiSetHandler(msg, CpvAccess(node1ZCHandler));
    msg->buffer = CpvAccess(mySrcBuff);
    //CmiPrintf("[%d] startZcPingpong with message size %d\n", CmiMyPe(), CpvAccess(msgSize));
    //CpvAccess(mySrcBuff).print();
    CmiSyncSendAndFree(1, sizeof(zcMsg), msg);
  }
}

void finishZcPingpong() {
  //CmiPrintf("[%d] finishZcPingpong\n", CmiMyPe());
  CmiPrintf("%zu,%zu,%lf,%lf\n",
      CpvAccess(msgSize),
      CpvAccess(nCycles),
      (1e6*(CpvAccess(endTime1)-CpvAccess(startTime1)))/(2.*CpvAccess(nCycles)),
      (1e6*(CpvAccess(endTime2)-CpvAccess(startTime2)))/(2.*CpvAccess(nCycles)));

  if (CpvAccess(msgSize) <= CpvAccess(maxMsgSize)) {
    void *setupMsg = CmiAlloc(sizeof(regularMsg));
    CmiSetHandler(setupMsg, CpvAccess(setupHandler));
    ((regularMsg *)setupMsg)->bufferSize = CpvAccess(msgSize) * CpvAccess(factor);
    CmiSyncBroadcastAllAndFree(sizeof(regularMsg), setupMsg);
  } else {
    //exit
    void *exitMsg = CmiAlloc(CmiMsgHeaderSizeBytes);
    CmiSetHandler(exitMsg, CpvAccess(exitHandler));
    CmiSyncBroadcastAllAndFree(CmiMsgHeaderSizeBytes, exitMsg);
  }
}



void node0ZCHandlerFunc(zcMsg *msg) {
  //CmiPrintf("[%d] node0ZCHandlerFunc\n", CmiMyPe());
  CmiNcpyBuffer *src1Buff = &(msg->buffer);
  //src1Buff->print();
  CpvAccess(myDestBuff).get(*src1Buff);
  CmiFree(msg);
}

void node0ZCCompHandlerFunc(char *compMsg) {
  CpvAccess(cycleNum)++;
  CmiFree(compMsg);

  //CmiPrintf("[%d] node0ZCCompHandlerFunc num completed = %d, num total = %d\n", CmiMyPe(), CpvAccess(cycleNum), CpvAccess(nCycles));

  if (CpvAccess(cycleNum) == CpvAccess(nCycles)) {
    CpvAccess(cycleNum) = 0;
    CpvAccess(endTime2) = CmiWallTimer();
    //CmiPrintf("[%d] node0ZCCompHandlerFunc one size done, moving onto next size\n", CmiMyPe());

    finishZcPingpong();
  } else {
    zcMsg *msg = (zcMsg *)CmiAlloc(sizeof(zcMsg));
    CmiSetHandler(msg, CpvAccess(node1ZCHandler));
    msg->buffer = CpvAccess(mySrcBuff);
    CmiSyncSendAndFree(1, sizeof(zcMsg), msg);
  }
}

void node1ZCHandlerFunc(zcMsg *msg) {
  //CmiPrintf("[%d] node1ZCHandlerFunc\n", CmiMyPe());
  CmiNcpyBuffer *src0Buff = &(msg->buffer);
  //src0Buff->print();
  CpvAccess(myDestBuff).get(*src0Buff);
  CmiFree(msg);
}

void node1ZCCompHandlerFunc(char *compMsg) {
  CmiFree(compMsg);
  //CmiPrintf("[%d] node1ZCCompHandlerFunc\n", CmiMyPe());
  zcMsg *msg = (zcMsg *)CmiAlloc(sizeof(zcMsg));
  CmiSetHandler(msg, CpvAccess(node0ZCHandler));
  msg->buffer = CpvAccess(mySrcBuff);
  //CpvAccess(mySrcBuff).print();
  CmiSyncSendAndFree(0, sizeof(zcMsg), msg);
}

//We finished for all message sizes. Exit now
CmiHandler exitHandlerFunc(char *msg)
{
  CmiFree(msg);
  CmiFree(CpvAccess(buffer));
  CsdExitScheduler();
  return 0;
}

//Converse main. Initialize variables and register handlers
CmiStartFn mymain(int argc, char *argv[])
{
  CpvInitialize(int,msgSize);
  CpvInitialize(int,cycleNum);

  CpvInitialize(int,nCycles);
  CpvInitialize(int,minMsgSize);
  CpvInitialize(int,maxMsgSize);
  CpvInitialize(int,factor);
  CpvInitialize(bool,warmUp);

  CpvInitialize(int,count);

  CpvInitialize(CmiNcpyBuffer, mySrcBuff);
  CpvInitialize(CmiNcpyBuffer, myDestBuff);

  // Register Handlers
  CpvInitialize(int,exitHandler);
  CpvAccess(exitHandler) = CmiRegisterHandler((CmiHandler) exitHandlerFunc);

  CpvInitialize(int,node0Handler);
  CpvAccess(node0Handler) = CmiRegisterHandler((CmiHandler) node0HandlerFunc);
  CpvInitialize(int,node1Handler);
  CpvAccess(node1Handler) = CmiRegisterHandler((CmiHandler) node1HandlerFunc);

  CpvInitialize(int,node0ZCHandler);
  CpvAccess(node0ZCHandler) = CmiRegisterHandler((CmiHandler) node0ZCHandlerFunc);
  CpvInitialize(int,node1ZCHandler);
  CpvAccess(node1ZCHandler) = CmiRegisterHandler((CmiHandler) node1ZCHandlerFunc);

  CpvInitialize(int,node0ZCCompHandler);
  CpvAccess(node0ZCCompHandler) = CmiRegisterHandler((CmiHandler) node0ZCCompHandlerFunc);
  CpvInitialize(int,node1ZCCompHandler);
  CpvAccess(node1ZCCompHandler) = CmiRegisterHandler((CmiHandler) node1ZCCompHandlerFunc);


  CpvInitialize(int,startHandler);
  CpvAccess(startHandler) = CmiRegisterHandler((CmiHandler) startPingpong);

  CpvInitialize(int,setupHandler);
  CpvAccess(setupHandler) = CmiRegisterHandler((CmiHandler) setupPingpong);

  CpvInitialize(int,startZcHandler);
  CpvAccess(startZcHandler) = CmiRegisterHandler((CmiHandler) startZcPingpong);

  CpvInitialize(int,setupZcHandler);
  CpvAccess(setupZcHandler) = CmiRegisterHandler((CmiHandler) setupZcPingpong);



  //set warmup run
  CpvAccess(warmUp) = true;

  CpvInitialize(double,startTime1);
  CpvInitialize(double,endTime1);

  CpvInitialize(double,startTime2);
  CpvInitialize(double,endTime2);

  int otherPe = CmiMyPe() ^ 1;

  // Set runtime cpuaffinity
  CmiInitCPUAffinity(argv);

  // Initialize CPU topology
  CmiInitCPUTopology(argv);

  // Update the argc after runtime parameters are extracted out
  argc = CmiGetArgc(argv);
  if(argc == 5){
    CpvAccess(nCycles)=atoi(argv[1]);
    CpvAccess(minMsgSize)= atoi(argv[2]);
    CpvAccess(maxMsgSize)= atoi(argv[3]);
    CpvAccess(factor)= atoi(argv[4]);
  } else if(argc == 1) {
    // use default arguments
    CpvAccess(nCycles) = 1000;
    CpvAccess(minMsgSize) = 1 << 4;
    CpvAccess(maxMsgSize) = 1 << 25;
    CpvAccess(factor) = 2;
  } else {
    if(CmiMyPe() == 0)
      CmiAbort("Usage: ./pingpong <ncycles> <minsize> <maxsize> <increase factor> \nExample: ./pingpong 100 2 128 2\n");
  }

  if(CmiMyPe() == 0) {
    CmiPrintf("Pingpong with iterations = %d, minMsgSize = %d, maxMsgSize = %d, increase factor = %d\n",
        CpvAccess(nCycles), CpvAccess(minMsgSize), CpvAccess(maxMsgSize), CpvAccess(factor));
  }

  if(CmiNumPes() != 2 && CmiMyPe() == 0) {
    CmiAbort("This test is designed for only 2 pes and cannot be run on %d pe(s)!\n", CmiNumPes());
  }


  CpvAccess(msgSize)= CpvAccess(minMsgSize);

  CpvInitialize(void *, buffer);
  CpvAccess(buffer) = nullptr;

  // Wait for all PEs of the node to complete topology init
  CmiNodeAllBarrier();

  CpvAccess(count) = 0;

  if(CmiMyPe() == 0) {
    void *setupMsg = CmiAlloc(sizeof(regularMsg));
    CmiSetHandler(setupMsg, CpvAccess(setupHandler));
    ((regularMsg *)setupMsg)->bufferSize = CpvAccess(msgSize);
    CmiSyncBroadcastAllAndFree(sizeof(regularMsg), setupMsg);
  }
  return 0;
}

int main(int argc,char *argv[])
{
  ConverseInit(argc,argv,(CmiStartFn)mymain,0,0);
  return 0;
}
