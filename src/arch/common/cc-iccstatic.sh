# test version
ICC_ver=`icc -v 2>&1 | grep Version`
ICC_ver=`echo $ICC_ver | awk '{ print $2; }' | awk 'BEGIN {FS="."}; { print $1; }'`
test -z "$ICC_ver" && echo "ICC compiler not found!" && exit 1
#echo version:$ICC_ver

if test $ICC_ver  -ge 10
then
  ICCOPTS="-static-intel"
elif test $ICC_ver -eq 9
then
  ICCOPTS="-i-static"
else
  ICCOPTS="-static-libcxa"
fi

CMK_CPP_C='icc -E '
CMK_CC='icc '
CMK_CXX='icpc '
CMK_CXXPP='icpc -E '
CMK_LD="icc $ICCOPTS"
CMK_LDXX="icpc $ICCOPTS"
CMK_LD_LIBRARY_PATH="-Wl,-rpath,$CHARMLIBSO/"
CMK_CF90='ifc -auto '
CMK_CF90_FIXED="$CMK_CF90 -132 -FI "
CMK_NATIVE_F90="$CMK_CF90"
CMK_NATIVE_CC="$CMK_CC"
CMK_NATIVE_CXX="$CMK_CXX"
CMK_NATIVE_LD="$CMK_LD"
CMK_NATIVE_LDXX="$CMK_LDXX"
CMK_F90LIBS='-L/usr/local/intel/compiler70/ia32/lib -L/opt/intel/compiler70/ia32/lib -lintrins -lIEPCF90 -lF90 -lintrins -limf  '
CMK_F90_USE_MODDIR=""
