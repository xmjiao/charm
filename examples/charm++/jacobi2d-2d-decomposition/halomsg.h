#ifndef HALO_MSG_
#define HALO_MSG_

struct HaloMsg {
  int dir;
  int size;
  double* data;

  HaloMsg() : data(NULL) {}

  void init(int dir_, int size_) {
    dir = dir_;
    size = size_;
    data = new double[size];
  }

  ~HaloMsg() {
    if (data) delete data;
  }

  void pup(PUP::er &p) {
    p|dir;
    p|size;
    if (p.isUnpacking()) {
      data = new double[size];
      PUParray(p, data, size);
    }
  }
};

#endif // HALO_MSG_
