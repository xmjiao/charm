#ifndef __PAIRUTIL_H__
#define __PAIRUTIL_H__

// some codes are borrowed from Orion's serial version.

#include <stdio.h>
#include <stdlib.h>
#include <fftw.h>
#include <charm++.h>

#if defined(__INTEL_COMPILER)
#include <emmintrin.h>
#define PAIR_USE_SSE 0
#endif

struct Index4D {unsigned short w,x,y,z;};

class CkArrayIndexIndex4D: public CkArrayIndex {
 public:
  unsigned short index[4];
  CkArrayIndexIndex4D(unsigned short i0,
		      unsigned short i1,
		      unsigned short i2,
		      unsigned short i3)
    {index[0] = i0; index[1] = i1; index[2] = i2; index[3] = i3; nInts = 2;}
};

typedef ArrayElementT<Index4D> ArrayElement4D;

// ComplexPoint: real and imaginary components and 3 coordinates
struct ComplexPt {
  double re, im;
  int x, y, z;
  inline ComplexPt() {re=im=0.0; x=y=z=0;}
  inline ComplexPt(double re_,double im_,int x_,int y_,int z_) {
    re=re_; im=im_;
    x=x_; y=y_; z=z_;
  }
};

struct complex {
    double re;
    double im;
    
    inline complex() {re=0; im=0;}
    inline complex(fftw_real r) {re=r; im=0;}
    inline complex(fftw_real r,fftw_real i) {re=r; im=i;}
    
    inline double getMagSqr(void) const { 
#if PAIR_USE_SSE      
        __m128d dreg1 = _mm_loadu_pd(this);
        __m128d dreg2 = _mm_loadu_pd(this);

        dreg1 = _mm_mul_pd(dreg1, dreg2);
        return dreg1[0] + dreg1[1];
#else
        return re*re+im*im; 
#endif
    }

    inline complex operator+(complex a) { 
#if PAIR_USE_SSE    
        __m128d dreg1 = _mm_loadu_pd(this);
        __m128d dreg2 = _mm_loadu_pd(&a);
  
        return (complex)_mm_add_pd(dreg1, dreg2);
#else         
        return complex(re+a.re,im+a.im); 
#endif
    }


    inline complex conj(void) { 
        return complex(re, -im); 
    }

    inline void operator+=(complex a) { 
        
#if PAIR_USE_SSE
        __m128d dreg1 = _mm_loadu_pd(this);
        __m128d dreg2 = _mm_loadu_pd(&a);
        
        _mm_storeu_pd(this, _mm_add_pd(dreg1, dreg2));
#else
        re+=a.re; im+=a.im; 
#endif

    }
    
    inline complex operator*(double a) { 
#if PAIR_USE_SSE
        __m128d dreg1 = _mm_loadu_pd(this);
        __m128d dreg2 = _mm_loadh_pd(dreg1, &a);
        dreg2 = _mm_loadl_pd(dreg2, &a);

        return (complex)_mm_mul_pd(dreg1, dreg2);
#else
        return complex(re*a, im*a); 
#endif
    } 
 
    inline bool notzero() const { return( (0.0 != re) ? true : (0.0 != im)); }

    inline void operator*=(complex a) {        
#if PAIR_USE_SSE
         register __m128d dreg = {a.re,a.im};
         
#else
        double treal, tim;
        treal = re * a.re - im * a.im;
        tim = re * a.im + im * a.re;
        re = treal;
        im = tim;
#endif
    }

    inline complex operator*(complex a) {
        return complex( re * a.re - im * a.im, re * a.im + im * a.re); }


    inline void operator -= (complex a) {re -= a.re; im -= a.im;}

    inline void multiplyByi () {
        double tmp = re; 
        re = -im; 
        im = tmp;
    }
    
    void pup(PUP::er &p) {
        p|re;
        p|im;
    }
    
    void * operator new[] (size_t size){
        void *buf = malloc(size);
        return buf;
    }
    
    void operator delete[] (void *buf){
        free(buf);
    }
};



#endif //__PAIRUTIL_H__


