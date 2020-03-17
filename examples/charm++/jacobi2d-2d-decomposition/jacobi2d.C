/** \file jacobi2d.C
 *  Author: Yanhua Sun, Eric Bohm and Abhinav S Bhatele
 *
 *  This is jacobi 2d problem using 2d decomposition 
 *
 */

#include "NDMeshStreamer.h"
#include "jacobi2d.decl.h"

#if defined OMP
#include <omp.h>
// do we need sched header?
#endif // OMP

#if defined BIGSIM
#include "cktiming.h"
#endif // defined BIGSIM

// See README for documentation

/*readonly*/ CProxy_Main mainProxy;
/*readonly*/ int arrayDimX;
/*readonly*/ int arrayDimY;
/*readonly*/ int blockDimX;
/*readonly*/ int blockDimY;

// specify the number of worker chares in each dimension
/*readonly*/ int num_chare_x;
/*readonly*/ int num_chare_y;

/*readonly*/ int maxiterations;
/*readonly*/ CProxy_ArrayMeshStreamer<double, CkArrayIndex2D, Jacobi, SimpleMeshRouter> tram_proxy;

static unsigned long next = 1;


#define MAX_ITER		100
#define LEFT			1
#define RIGHT			2
#define TOP			3
#define BOTTOM			4
#define DIVIDEBY5       	0.2
const double THRESHOLD   =  0.004;


/** \class Main
 *
 */
class Main : public CBase_Main {

  double startTime;

public:
  CProxy_Jacobi array;
  double max_error;
  Main(CkArgMsg* m) {
    if ( (m->argc < 3) || (m->argc > 8)) {
      CkPrintf("%s [array_size] [block_size]\n", m->argv[0]);
      CkPrintf("OR %s [array_size] [block_size] maxiterations\n", m->argv[0]);
      CkPrintf("OR %s [array_size_X] [array_size_Y] [block_size_X] [block_size_Y] \n", m->argv[0]);
      CkPrintf("OR %s [array_size_X] [array_size_Y] [block_size_X] [block_size_Y] maxiterations\n", m->argv[0]);
      CkPrintf("OR %s [array_size_X] [array_size_Y] [block_size_X] [block_size_Y] maxiterations numthreads\n", m->argv[0]);
      CkPrintf("OR %s [array_size_X] [array_size_Y] [block_size_X] [block_size_Y] maxiterations numthreads spread\n", m->argv[0]);
      CkAbort("Abort");
    }

    // store the main proxy
    mainProxy = thisProxy;

    if(m->argc <= 4) {
      arrayDimX = arrayDimY = atoi(m->argv[1]);
      blockDimX = blockDimY = atoi(m->argv[2]);
    }
    else if (m->argc >= 5) {
      arrayDimX = atoi(m->argv[1]);
      arrayDimY = atoi(m->argv[2]);
      blockDimX = atoi(m->argv[3]); 
      blockDimY = atoi(m->argv[4]); 
    }
    maxiterations = MAX_ITER;
    if(m->argc==4)
      maxiterations = atoi(m->argv[3]); 
    if(m->argc > 5)
      maxiterations = atoi(m->argv[5]); 

    int numthreads = 1;
    if (m->argc > 6) {
      numthreads = atoi(m->argv[6]);
    }

    int spread = 1;
    if (m->argc > 7) {
      spread = atoi(m->argv[7]);
    }

    if (arrayDimX < blockDimX || arrayDimX % blockDimX != 0)
      CkAbort("array_size_X % block_size_X != 0!");
    if (arrayDimY < blockDimY || arrayDimY % blockDimY != 0)
      CkAbort("array_size_Y % block_size_Y != 0!");

    num_chare_x = arrayDimX / blockDimX;
    num_chare_y = arrayDimY / blockDimY;

    CkPrintf("\nSTENCIL COMPUTATION WITH NO BARRIERS\n");
    CkPrintf("Running Jacobi on %d processors with (%d, %d) chares\n", CkNumPes(), num_chare_x, num_chare_y);
    CkPrintf("Array Dimensions: %d %d\n", arrayDimX, arrayDimY);
    CkPrintf("Block Dimensions: %d %d\n", blockDimX, blockDimY);
    CkPrintf("max iterations %d\n", maxiterations);
    CkPrintf("Threshold %.10g\n", THRESHOLD);

    array = CProxy_Jacobi::ckNew(num_chare_x, num_chare_y);

    // TRAM
    int tram_dims[1] = {CkNumPes()};
    tram_proxy = CProxy_ArrayMeshStreamer<double, CkArrayIndex2D, Jacobi, SimpleMeshRouter>
      ::ckNew(1024, 1, tram_dims, array);
    CkCallback start_cb(CkIndex_Jacobi::send(), array);
    tram_proxy.init(start_cb, -1);

#if defined AFFINITY
    CProxy_SetThreads::ckNew(numthreads, spread);
#else // AFFINITY
    // start measuring execution time
    startTime = CkWallTimer();
#endif // AFFINITY
  }

  void done(int totalIter) {
    if(totalIter >= maxiterations)
      CkPrintf("Finish due to max iterations %d, total time %.3f seconds. \n", totalIter, CkWallTimer()-startTime); 
    else
      CkPrintf("Finish due to convergence, iterations %d, total time %.3f seconds. \n", totalIter, CkWallTimer()-startTime); 
    CkExit();
  }

  void start(CkReductionMsg* msg) {
#if defined DEBUG
    array.debug();
#else // DEBUG
    // start measuring execution time
    startTime = CkWallTimer();

    // start computation
    array.run();
#endif // DEBUG
  }
};

/** \class Jacobi
 *
 */

class Jacobi: public CBase_Jacobi {
  Jacobi_SDAG_CODE

public:
    double **temperature;
    double **new_temperature;
    int imsg;
    int iterations;
    int neighbors;
    int istart,ifinish,jstart,jfinish;
    double max_error;
    bool leftBound, rightBound, topBound, bottomBound;
    int converged;
    // Constructor, initialize values
    Jacobi() {
      int i, j;
      temperature = new double*[blockDimX+2];
      new_temperature = new double*[blockDimX+2];

      for (i=0; i<blockDimX+2; i++) {
        temperature[i] = new double[blockDimY+2];
        new_temperature[i] = new double[blockDimY+2];
      }

      for(i=0; i<blockDimX+2; ++i) {
        for(j=0; j<blockDimY+2; ++j) {
          temperature[i][j] = 0.;
        } 
      }

      converged = 0;
      imsg = 0;
      iterations = 0;
      neighbors = 0;
      max_error = 0.;
      // determine border conditions
      leftBound = rightBound = topBound = bottomBound = false;
      istart = jstart = 1;
      ifinish = blockDimX+1;
      jfinish = blockDimY+1;

      if(thisIndex.x==0)
      {
        leftBound = true;
        istart++;
      }
      else
        neighbors++;

      if(thisIndex.x==num_chare_x-1)
      {
        rightBound = true;
        ifinish--;
      }
      else
        neighbors++;

      if(thisIndex.y==0)
      {
        topBound = true;
        jstart++;
      }
      else
        neighbors++;

      if(thisIndex.y==num_chare_y-1)
      {
        bottomBound = true;
        jfinish--;
      }
      else
        neighbors++;
      constrainBC();
    }

    void pup(PUP::er &p)
    {
      int i,j;
      p|imsg;
      p|iterations;
      p|neighbors;
      p|istart; p|ifinish; p|jstart; p|jfinish;
      p|leftBound; p|rightBound; p|topBound; p|bottomBound;
      p|converged;
      p|max_error;

      if (p.isUnpacking()) {
        temperature = new double*[blockDimX+2];
        new_temperature = new double*[blockDimX+2];
        for (i=0; i<blockDimX+2; i++) {
          temperature[i] = new double[blockDimY];
          new_temperature[i] = new double[blockDimY];
        }
      }
      for(i=0;i<blockDimX+2; i++) {
        for(j=0;j<blockDimY+2; j++) {
          p|temperature[i][j];
          p|new_temperature[i][j];
        }
      }
    }

    Jacobi(CkMigrateMessage* m) { }

    ~Jacobi() { 
      for (int i=0; i<blockDimX+2; i++) {
        delete [] temperature[i];
        delete [] new_temperature[i];
      }
      delete [] temperature; 
      delete [] new_temperature; 
    }

    // Send ghost faces to the six neighbors
    void send(void) {
#if defined BIGSIM
      startTraceBigSim();
#endif // defined BIGSIM
#if defined ITERATION
      if (thisIndex.x + thisIndex.y == 0) CkPrintf("Iteration %d\n", iterations);
#endif // defined ITERATION
      iterations++;

      ArrayMeshStreamer<double, CkArrayIndex2D, Jacobi, SimpleMeshRouter>* local_tram_proxy = tram_proxy.ckLocalBranch();

      if(!leftBound)
      {
        double *leftGhost =  new double[blockDimY];
        for(int j=0; j<blockDimY; ++j) 
          leftGhost[j] = temperature[1][j+1];
        thisProxy(thisIndex.x-1, thisIndex.y).receiveGhosts(iterations, RIGHT, blockDimY, leftGhost);
        delete [] leftGhost;
      }
      if(!rightBound)
      {
        double *rightGhost =  new double[blockDimY];
        for(int j=0; j<blockDimY; ++j) 
          rightGhost[j] = temperature[blockDimX][j+1];
        thisProxy(thisIndex.x+1, thisIndex.y).receiveGhosts(iterations, LEFT, blockDimY, rightGhost);
        delete [] rightGhost;
      }
      if(!topBound)
      {
        double *topGhost =  new double[blockDimX];
        for(int i=0; i<blockDimX; ++i) 
          topGhost[i] = temperature[i+1][1];
        thisProxy(thisIndex.x, thisIndex.y-1).receiveGhosts(iterations, BOTTOM, blockDimX, topGhost);
        delete [] topGhost;
      }
      if(!bottomBound)
      {
        double *bottomGhost =  new double[blockDimX];
        for(int i=0; i<blockDimX; ++i) 
          bottomGhost[i] = temperature[i+1][blockDimY];
        thisProxy(thisIndex.x, thisIndex.y+1).receiveGhosts(iterations, TOP, blockDimX, bottomGhost);
        delete [] bottomGhost;
      }
#if defined BIGSIM
      endTraceBigSim("begin_iteration", iterations, 0.);
#endif // defined BIGSIM
    }

    void processGhosts(int dir, int size, double gh[]) {
#if defined BIGSIM
      startTraceBigSim();
#endif // defined BIGSIM
      switch(dir) {
      case LEFT:
        for(int j=0; j<size; ++j) {
          temperature[0][j+1] = gh[j];
        }
        break;
      case RIGHT:
        for(int j=0; j<size; ++j) {
          temperature[blockDimX+1][j+1] = gh[j];
        }
        break;
      case TOP:
        for(int i=0; i<size; ++i) {
          temperature[i+1][0] = gh[i];
        }
        break;
      case BOTTOM:
        for(int i=0; i<size; ++i) {
          temperature[i+1][blockDimY+1] = gh[i];
        }
        break;
      default:
        CkAbort("ERROR\n");
      }
#if defined BIGSIM
      endTraceBigSim("processGhost", iterations, dir, size);
#endif // defined BIGSIM
    }

    void check_and_compute() {
#if defined BIGSIM
      startTraceBigSim();
#endif // defined BIGSIM
      double temperatureIth = 0.;
      double difference = 0.;
      double **tmp;

      max_error = 0.;
      // When all neighbor values have been received, we update our values and proceed
      #pragma omp parallel for private(temperatureIth, difference) schedule(static)
      for(int i=istart; i<ifinish; ++i) {
        for(int j=jstart; j<jfinish; ++j) {
          temperatureIth=(temperature[i][j] 
            + temperature[i-1][j] 
            +  temperature[i+1][j]
            +  temperature[i][j-1]
            +  temperature[i][j+1]) * 0.2;

#if !defined PERF
          // update relative error
          difference = temperatureIth-temperature[i][j];
          // fix sign without fabs overhead
          if(difference<0) difference *= -1.0; 
          max_error=(max_error>difference) ? max_error : difference;
#endif // PERF
          new_temperature[i][j] = temperatureIth;
        }
      }

      tmp = temperature;
      temperature = new_temperature;
      new_temperature = tmp;
      //dumpMatrix(temperature);
#if defined BIGSIM
      endTraceBigSim("check_and_compute", iterations, max_error);
#endif // defined BIGSIM
    }

    // Enforce some boundary conditions
    void constrainBC() {
      if(topBound)
        for(int i=0; i<blockDimX+2; ++i)
        {
          temperature[i][1] = 1.0;
          new_temperature[i][1] = 1.0;
        }

      if(leftBound)
        for(int j=0; j<blockDimY+2; ++j){
          temperature[1][j] = 1.0;
          new_temperature[1][j] = 1.0;
        }

      if(bottomBound)
        for(int i=0; i<blockDimX+2; ++i){
          temperature[i][blockDimY] = 1.;
          new_temperature[i][blockDimY] = 1.;
        }

      if(rightBound)
        for(int j=0; j<blockDimY+2; ++j){
          temperature[blockDimX][j] = 1.;
          new_temperature[blockDimX][j] = 1.;
        }
    }

    // for debugging
    void dumpMatrix(double **matrix)
    {
      CkPrintf("\n\n[%d,%d]\n",thisIndex.x, thisIndex.y);
      for(int i=0; i<blockDimX+2;++i)
      {
        for(int j=0; j<blockDimY+2;++j)
        {
          CkPrintf("%0.3lf ",matrix[i][j]);
        }
        CkPrintf("\n");
      }
    }

    void debug() {
      #pragma omp parallel
      {
        CkPrintf("[%d]: %d:%d\n", thisIndex, CkMyPe(), sched_getcpu());
      }

      contribute(CkCallback(CkCallback::ckExit));
    }
};

/** \class SetThreads
 *
 */
class SetThreads : public CBase_SetThreads {
public:
  SetThreads(int numThreads, int spread) {
    int myIdx = CmiPhysicalRank(CmiMyPe());

    cpu_set_t set;
    CPU_ZERO(&set);

    for (int i =  myIdx      * numThreads * spread;
             i < (myIdx + 1) * numThreads * spread; i += spread) {
      CPU_SET(i, &set);
    }

    if (sched_setaffinity(0, sizeof(cpu_set_t), &set)) {
      CkAbort("Error setting affinity.\n");
    }

#if defined OMP
    omp_set_num_threads(numThreads);
#endif // OMP

    CkCallback cb(CkIndex_Main::start(NULL), mainProxy);
    contribute(cb);
  }
};

#include "jacobi2d.def.h"
