/***************************************************************************
 * RCS INFORMATION:
 *
 *	$RCSfile$
 *	$Author$	$Locker$		$State$
 *	$Revision$	$Date$
 *
 ***************************************************************************
 * DESCRIPTION:
 *
 ***************************************************************************
 * REVISION HISTORY:
 *
 * $Log$
 * Revision 1.11  1995-11-07 23:20:32  jyelon
 * removed neighbour_init residue.
 *
 * Revision 1.10  1995/11/07  18:24:53  jyelon
 * Corrected a bug in GetNodeNeighbours
 *
 * Revision 1.9  1995/11/07  18:16:45  jyelon
 * Corrected 'neighbour' functions (they now make a hypercube).
 *
 * Revision 1.8  1995/10/27  21:45:35  jyelon
 * Changed CmiNumPe --> CmiNumPes
 *
 * Revision 1.7  1995/10/18  22:22:53  jyelon
 * I forget.
 *
 * Revision 1.6  1995/10/13  22:36:29  jyelon
 * changed exit() --> exit(1)
 *
 * Revision 1.5  1995/10/13  22:34:42  jyelon
 * added CmiNext to CmiCallMain.
 *
 * Revision 1.4  1995/10/13  20:05:13  jyelon
 * *** empty log message ***
 *
 * Revision 1.3  1995/10/10  06:10:58  jyelon
 * removed program_name
 *
 * Revision 1.2  1995/09/30  15:44:59  jyelon
 * fixed a bug.
 *
 * Revision 1.1  1995/09/30  15:00:00  jyelon
 * Initial revision
 *
 * Revision 2.5  1995/09/29  09:50:07  jyelon
 * CmiGet-->CmiDeliver, added protos, etc.
 *
 * Revision 2.4  1995/09/20  15:56:29  gursoy
 * made the arg of CmiFree and CmiSize void*
 *
 * Revision 2.3  1995/09/07  22:33:07  gursoy
 * now the processor specific variables in machine files also accessed thru macros (because macros modifies the var names)
 *
 * Revision 2.2  1995/07/26  19:04:11  gursoy
 * fixed some timer-system-include-file related problems
 *
 * Revision 2.1  1995/07/11  16:53:57  gursoy
 * added CsdStopCount
 *
 * Revision 2.0  1995/07/05  23:37:59  gursoy
 * *** empty log message ***
 *
 *
 *
 ***************************************************************************/
static char ident[] = "@(#)$Header$";

#include <stdio.h>
#include <math.h>
#include "converse.h"

#ifdef CMK_TIMER_USE_TIMES
#include <sys/times.h>
#include <sys/unistd.h>
#endif

#ifdef CMK_TIMER_USE_GETRUSAGE
#include <sys/time.h>
#include <sys/resource.h>
#endif

static char *DeleteArg(argv)
  char **argv;
{
  char *res = argv[0];
  if (res==0) { CmiError("Bad arglist."); exit(1); }
  while (*argv) { argv[0]=argv[1]; argv++; }
  return res;
}

static void mycpy(dst, src, bytes)
    double *dst; double *src; int bytes;
{
        unsigned char *cdst, *csrc;

        while(bytes>8)
        {
                *dst++ = *src++;
                bytes -= 8;
        }
        cdst = (unsigned char *) dst;
        csrc = (unsigned char *) src;
        while(bytes)
        {
                *cdst++ = *csrc++;
                bytes--;
        }
}



/******************************************************************************
 *
 * CmiTimer
 *
 *****************************************************************************/
#ifdef CMK_TIMER_USE_TIMES

static struct tms inittime;

static void CmiTimerInit()
{
  times(&inittime);
}

double CmiTimer()
{
  double currenttime;
  int clk_tck;
    struct tms temp;

    times(&temp);
    clk_tck=sysconf(_SC_CLK_TCK);
    currenttime = 
     (((temp.tms_utime - inittime.tms_utime)+
       (temp.tms_stime - inittime.tms_stime))*1.0)/clk_tck;
    return (currenttime);
}

#endif

#ifdef CMK_TIMER_USE_GETRUSAGE

static struct rusage inittime;

static void CmiTimerInit()
{
  getrusage(0, &inittime); 
}

double CmiTimer() {
  double currenttime;

  struct rusage temp;
  getrusage(0, &temp);
  currenttime =
    (temp.ru_utime.tv_usec - inittime.ru_utime.tv_usec) * 0.000001+
      (temp.ru_utime.tv_sec - inittime.ru_utime.tv_sec) +
	(temp.ru_stime.tv_usec - inittime.ru_stime.tv_usec) * 0.000001+
	  (temp.ru_stime.tv_sec - inittime.ru_stime.tv_sec) ; 
  
  return (currenttime);
}

#endif

/*****************************************************************************
 *
 * Memory management.
 * 
 ****************************************************************************/

void *CmiAlloc(size)
int size;
{
char *res;
res =(char *)malloc(size+8);
if (res==0) printf("Memory allocation failed.");
((int *)res)[0]=size;
return (void *)(res+8);
}

int CmiSize(blk)
void *blk;
{
return ((int *)(((char *)blk)-8))[0];
}

void CmiFree(blk)
void *blk;
{
free( ((char *)blk)-8);
}

/*****************************************************************************
 *
 * Module variables
 * 
 ****************************************************************************/

typedef void *Fifo;

int        Cmi_mype;
int        Cmi_numpes;
int        Cmi_stacksize = 64000;
char     **CmiArgv;
CthThread *CmiThreads;
Fifo      *CmiQueues;
int       *CmiBarred;
int        CmiNumBarred=0;

CpvDeclare(Fifo, CmiLocalQueue);

/******************************************************************************
 *
 * Load-Balancer needs
 *
 * These neighbour functions impose a (possibly incomplete)
 * hypercube on the machine.
 *
 *****************************************************************************/


long CmiNumNeighbours(node)
int node;
{
  int bit, count=0;
  bit = 1;
  while (1) {
    int neighbour = node ^ bit;
    if (neighbour < Cmi_numpes) count++;
    bit<<1; if (bit > Cmi_numpes) break;
  }
  return count;
}

int CmiGetNodeNeighbours(node, neighbours)
int node, *neighbours;
{
  int bit, count=0;
  bit = 1;
  while (1) {
    int neighbour = node ^ bit;
    if (neighbour < Cmi_numpes) neighbours[count++] = neighbour;
    bit<<1; if (bit > Cmi_numpes) break;
  }
  return count;
}
 
int CmiNeighboursIndex(node, nbr)
int node, nbr;
{
  int bit, count=0;
  bit = 1;
  while (1) {
    int neighbour = node ^ bit;
    if (neighbour < Cmi_numpes) { if (nbr==neighbour) return count; count++; }
    bit<<=1; if (bit > Cmi_numpes) break;
  }
  return(-1);
}


/*****************************************************************************
 *
 * Comm handles are nonexistent in uth version
 *
 *****************************************************************************/

int CmiAsyncMsgSent(c)
CmiCommHandle c ;
{
  return 1;
}

void CmiReleaseCommHandle(c)
CmiCommHandle c ;
{
}

/********************* CONTEXT-SWITCHING FUNCTIONS ******************/

static void CmiNext()
{
  CthThread t; int index; int orig;
  index = (Cmi_mype+1) % Cmi_numpes;
  orig = index;
  while (1) {
    t = CmiThreads[index];
    if ((t)&&(!CmiBarred[index])) break;
    index = (index+1) % Cmi_numpes;
    if (index == orig) exit(0);
  }
  Cmi_mype = index;
  CthResume(t);
}

void CmiExit()
{
  CmiThreads[Cmi_mype] = 0;
  CmiFree(CthSelf());
  CmiNext();
}

void CmiYield()
{
  CmiThreads[Cmi_mype] = CthSelf();
  CmiNext();
}

void CmiNodeBarrier()
{
  int i;
  CmiNumBarred++;
  CmiBarred[Cmi_mype] = 1;
  if (CmiNumBarred == Cmi_numpes) {
    for (i=0; i<Cmi_numpes; i++) CmiBarred[i]=0;
    CmiNumBarred=0;
  }
  CmiYield();
}

/********************* MESSAGE RECEIVE FUNCTIONS ******************/

void *CmiGetNonLocal()
{
  CmiYield();
  return 0; /* Messages are always in local queue */
}

/********************* MESSAGE SEND FUNCTIONS ******************/

void CmiSyncSendFn(destPE, size, msg)
int destPE;
int size;
char * msg;
{
  char *buf = (char *)CmiAlloc(size);
  mycpy((double *)buf,(double *)msg,size);
  FIFO_EnQueue(CmiQueues[destPE],buf);
}

CmiCommHandle CmiAsyncSendFn(destPE, size, msg) 
int destPE;
int size;
char * msg;
{
  char *buf = (char *)CmiAlloc(size);
  mycpy((double *)buf,(double *)msg,size);
  FIFO_EnQueue(CmiQueues[destPE],buf);
}

void CmiFreeSendFn(destPE, size, msg)
int destPE;
int size;
char * msg;
{
  FIFO_EnQueue(CmiQueues[destPE], msg);
}

void CmiSyncBroadcastFn(size, msg)
int size;
char * msg;
{
  int i;
  for(i=0; i<Cmi_numpes; i++)
    if (i != Cmi_mype) CmiSyncSendFn(i,size,msg);
}

CmiCommHandle CmiAsyncBroadcastFn(size, msg)
int size;
char * msg;
{
  CmiSyncBroadcastFn(size, msg);
  return 0;
}

void CmiFreeBroadcastFn(size, msg)
int size;
char * msg;
{
  CmiSyncBroadcastFn(size, msg);
  CmiFree(msg);
}

void CmiSyncBroadcastAllFn(size, msg)
int size;
char * msg;
{
  int i;
  for(i=0; i<Cmi_numpes; i++)
    CmiSyncSendFn(i,size,msg);
}

CmiCommHandle CmiAsyncBroadcastAllFn(size, msg)
int size;
char * msg;
{
  CmiSyncBroadcastAllFn(size,msg);
  return 0 ;
}

void CmiFreeBroadcastAllFn(size, msg)
int size;
char * msg;
{
  int i;
  for(i=0; i<Cmi_numpes; i++)
    if (i!=Cmi_mype) CmiSyncSendFn(i,size,msg);
  FIFO_EnQueue(CpvAccess(CmiLocalQueue),msg);
}



/************************** SETUP ***********************************/

void CmiInitMc(argv)
char *argv[];
{
  CpvAccess(CmiLocalQueue) = CmiQueues[Cmi_mype];
  CmiSpanTreeInit();
  CmiTimerInit();
}

void CmiCallMain()
{
  int argc; char **argv;
  for (argc=0; CmiArgv[argc]; argc++);
  argv = (char **)CmiAlloc((argc+1)*sizeof(char *));
  memcpy(argv, CmiArgv, (argc+1)*sizeof(char *));
  user_main(argc, argv);
  CmiThreads[Cmi_mype] = 0;
  CmiNext();
}

static void CmiParseArgs(argv)
char **argv;
{
  char **argp;
  
  for (argp=argv; *argp; ) {
    if ((strcmp(*argp,"++stacksize")==0)&&(argp[1])) {
      DeleteArg(argp);
      Cmi_stacksize = atoi(*argp);
      DeleteArg(argp);
    } else if ((strcmp(*argp,"+p")==0)&&(argp[1])) {
      Cmi_numpes = atoi(argp[1]);
      argp+=2;
    } else if (sscanf(*argp, "+p%d", &Cmi_numpes) == 1) {
      argp+=1;
    } else argp++;
  }
  
  if (Cmi_numpes<1) {
    printf("Error: must specify number of processors to simulate with +pXXX\n",Cmi_numpes);
    exit(1);
  }
}

main(argc,argv)
int argc;
char *argv[];
{
  CthThread t; Fifo q; int stacksize, i;

  CmiArgv = argv;
  CmiParseArgs(argv);
  
  CthInit(argv);
  
  CpvInitialize(void*, CmiLocalQueue);
  CmiThreads = (CthThread *)CmiAlloc(Cmi_numpes*sizeof(CthThread));
  CmiBarred  = (int       *)CmiAlloc(Cmi_numpes*sizeof(int));
  CmiQueues  = (Fifo      *)CmiAlloc(Cmi_numpes*sizeof(Fifo));
  
  /* Create threads for all PE except PE 0 */
  for(i=0; i<Cmi_numpes; i++) {
    t = (i==0) ? CthSelf() : CthCreate(CmiCallMain, 0, Cmi_stacksize);
    CmiThreads[i] = t;
    CmiBarred[i] = 0;
    CmiQueues[i] = (Fifo)FIFO_Create();
  }
  Cmi_mype = 0;
  CmiCallMain();
}

