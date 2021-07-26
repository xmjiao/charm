#include <stdlib.h>
#include <converse.h>
#include <conv-rdma.h>

CpvDeclare(void *,buffer);
CpvDeclare(int, bufferSize);

CpvDeclare(int,node1Handler);
CpvDeclare(int,node0CompHandler);
CpvDeclare(int,node1CompHandler);

void startTest() { // called on pe 0
  CmiPrintf("[%d] startTest\n", CmiMyPe());

  // declare source buffer info
  CmiNcpyBuffer srcBuff(CpvAccess(buffer), CpvAccess(bufferSize), CpvAccess(node0CompHandler));

  char *msg = (char *)CmiAlloc(CmiMsgHeaderSizeBytes + sizeof(CmiNcpyBuffer));
  CmiSetHandler(msg,CpvAccess(node1Handler));
  memcpy(msg + CmiMsgHeaderSizeBytes, (char *)(&srcBuff), sizeof(CmiNcpyBuffer));
  CmiSyncSendAndFree(1, CmiMsgHeaderSizeBytes + sizeof(CmiNcpyBuffer), msg);
}

CmiHandler node1HandlerFunc(char *msg) { // called on pe 1
  CmiPrintf("[%d] node1HandlerFunc\n", CmiMyPe());
  CmiNcpyBuffer *srcBuff = (CmiNcpyBuffer *)( (char *)msg + CmiMsgHeaderSizeBytes);
  CmiNcpyBuffer destBuff(CpvAccess(buffer), CpvAccess(bufferSize), CpvAccess(node1CompHandler));
  destBuff.get(*srcBuff);
  return 0;
}

CmiHandler node1CompHandlerFunc(char *msg) {
  CmiPrintf("[%d] node1CompHandlerFunc\n", CmiMyPe());
  ConverseExit();
  return 0;
}

CmiHandler node0CompHandlerFunc(char *msg) {
  CmiPrintf("[%d] node0CompHandlerFunc\n", CmiMyPe());
  ConverseExit();
  return 0;
}

//Converse main. Initialize variables and register handlers
CmiStartFn mymain(int argc, char *argv[])
{
  // Register Handlers
  CpvInitialize(int,node1Handler);
  CpvAccess(node1Handler) = CmiRegisterHandler((CmiHandler) node1HandlerFunc);

  CpvInitialize(int, node0CompHandler);
  CpvAccess(node0CompHandler) = CmiRegisterHandler((CmiHandler) node0CompHandlerFunc);

  CpvInitialize(int, node1CompHandler);
  CpvAccess(node1CompHandler) = CmiRegisterHandler((CmiHandler) node1CompHandlerFunc);

  // Set runtime cpuaffinity
  CmiInitCPUAffinity(argv);

  // Initialize CPU topology
  CmiInitCPUTopology(argv);

  // Wait for all PEs of the node to complete topology init
  CmiNodeAllBarrier();

  if(CmiNumPes() != 2 && CmiMyPe() == 0) {
    CmiAbort("This test is designed for only 2 pes and cannot be run on %d pe(s)!\n", CmiNumPes());
  }

  CpvInitialize(int, bufferSize);
  CpvAccess(bufferSize) = 1000;

  CpvInitialize(void *, buffer);
  CpvAccess(buffer) = (void *)CmiAlloc(CpvAccess(bufferSize));

  // Node 0 waits till all processors finish their topology processing
  if(CmiMyPe() == 0) {
    startTest();
  }

  return 0;
}

int main(int argc,char *argv[])
{
  ConverseInit(argc,argv,(CmiStartFn)mymain,0,0);
  return 0;
}
