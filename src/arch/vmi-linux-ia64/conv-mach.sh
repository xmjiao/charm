VMI_INCDIR="-I/home/koenig/THESIS/VMI20-install/include" 
VMI_LIBDIR="-L/home/koenig/THESIS/VMI20-install/lib"
#
CMK_CPP_CHARM="/lib/cpp -P"
CMK_CPP_C="gcc -E $CMK_INCDIR $VMI_INCDIR "
CMK_CC="gcc $CMK_INCDIR $VMI_INCDIR "
CMK_CXX="g++ $CMK_INCDIR $VMI_INCDIR "
CMK_CXXPP="$CMK_CC -x c++ -E "
CMK_CF77="f77"
CMK_CF90="f90"
CMK_LD="$CMK_CC -rdynamic -pthread $VMI_LIBDIR "
CMK_LDXX="$CMK_CXX -rdynamic -pthread $VMI_LIBDIR"
CMK_RANLIB='ranlib'
CMK_LIBS='-lckqt -lvmi20 -lcurl -ldl -lexpat -lssl -lcrypto'
CMK_QT='generic64'
CMK_XIOPTS=''
CMK_F90LIBS='-lvast90 -lg2c'
CMK_MOD_EXT="vo"
