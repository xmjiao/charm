#default gm dir
test -z "$CMK_INCDIR" && CMK_INCDIR="-I /usr/gm/include"
test -z "$CMK_LIBDIR" && CMK_LIBDIR="-L /usr/gm/lib"

CMK_CPP_C="$CMK_CPP_C -E  $CMK_INCDIR "
CMK_CC="$CMK_CC $CMK_INCDIR "
CMK_CC_RELIABLE="$CMK_CC_RELIABLE $CMK_INCDIR "
CMK_CC_FASTEST="$CMK_CC_FASTEST $CMK_INCDIR "
CMK_CXX="$CMK_CXX $CMK_INCDIR "
CMK_CXXPP="$CMK_CXXPP -E $CMK_INCDIR "
CMK_LD="$CMK_LD $CMK_LIBDIR "
CMK_LDXX="$CMK_LDXX $CMK_LIBDIR "

CMK_LIBS="$CMK_LIBS -lgm"
