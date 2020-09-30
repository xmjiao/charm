/*
   Simple Charm++ collision detection test program--
   Orion Sky Lawlor, olawlor@acm.org, 2003/3/18
   */
#include <stdio.h>
#include <string.h>
#include "collidecharm.h"
#include "hello.decl.h"

CProxy_main mid;
CProxy_Hello arr;
int nElements;

void printCollisionHandler(void *param,int nColl,Collision *colls)
{
  CkPrintf("**********************************************\n");
  CkPrintf("*** Final collision handler called-- %d records:\n",nColl);
  int nPrint=nColl;
  const int maxPrint=30;
  if (nPrint>maxPrint) nPrint=maxPrint;
  for (int c=0;c<nPrint;c++) {
    CkPrintf("%d:%d hits %d:%d\n",
        colls[c].A.chunk,colls[c].A.number,
        colls[c].B.chunk,colls[c].B.number);
  }
  CkPrintf("**********************************************\n");
  mid.maindone();
}

class main : public CBase_main
{
  public:
    main(CkMigrateMessage *m) {}
    main(CkArgMsg* m)
    {
      nElements=2;
      if(m->argc > 1) nElements = atoi(m->argv[1]);
      delete m;
      CkPrintf("Running Hello on %d processors for %d elements\n",
          CkNumPes(),nElements);
      mid = thishandle;

      CollideGrid3d gridMap(CkVector3d(0,0,0),CkVector3d(2,100,2));
      CollideHandle collide=CollideCreate(gridMap,
          CollideSerialClient(printCollisionHandler,0));

      arr = CProxy_Hello::ckNew(collide,nElements);

      arr.DoIt();
    };

    void maindone(void)
    {
      CkPrintf("All done\n");
      CkExit();
    };
};

class Hello : public CBase_Hello
{
  CollideHandle collide;
  int nTimes;
  public:
  Hello(const CollideHandle &collide_) :collide(collide_)
  {
    CkPrintf("Creating element %d on PE %d and registering chunk as %d\n",thisIndex,CkMyPe(), thisIndex);
    nTimes=0;
    CollideRegister(collide,thisIndex);
  }

  Hello(CkMigrateMessage *m) : CBase_Hello(m) {}
  void pup(PUP::er &p) {
    p|collide;
    if (p.isUnpacking())
      CollideRegister(collide,thisIndex);
  }
  ~Hello() {
    CollideUnregister(collide,thisIndex);
  }

  void DoIt(void)
  {
    int nBoxes=3;
    bbox3d *box=new bbox3d[nBoxes];

    int *prio=new int[nBoxes];

    CkVector3d p1(3,3,3), p2(-2,-2,-2), p3(5,5,5), p4(6,6,6), p5(-8,-8,-8);

    box[0].add(p1);
    box[1].add(p2);
    box[2].add(p3);

    //box[3].add(p4);
    //box[4].add(p5);

    for (int i=0;i<nBoxes;i++) {
      if(thisIndex < nElements/2)
        prio[i] = 20;
      else
        prio[i] = 100;
    }

    CollideBoxesPrio(collide,thisIndex,nBoxes,box, NULL);
    //CollideBoxesPrio(collide,thisIndex,nBoxes,box, prio);

    delete[] box;
    nTimes++;
  }
};

#include "hello.def.h"

