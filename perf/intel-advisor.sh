#!/bin/bash

cd /u/dwagner5/femera-mini-develop

CPUMODEL=`./cpumodel.sh`
CPUCOUNT=`./cpucount.sh`

export OMP_SCHEDULE=static
export OMP_PLACES=cores
export OMP_PROC_BIND=spread
export OMP_NUM_THREADS=$CPUCOUNT

module purge
module load gcc_8.3.0
makw clean
make -j$CPUCOUNT mini-all
module load intel
make -j$CPUCOUNT mini-all

XDIR=/u/dwagner5/femera-mini-develop
MDIR=/hpnobackup1/dwagner5/femera-test/cube

#CSTR=icc
#YSTR=iso
for YSTR in iso ort;

for CSTR in icc; do
for SIZE in medium large; do

case "$SIZE" in
  small)
    #FIXME This will not work with ittnotify around the iter loop.
    ZSTR=3kdof
    MSTR=uhxt4p2
    # PART must be 1
    PART=1
    NMODEL=1000
    ITER=2000
    GSTR=$MSTR"n"$PART
    SSTR=""
    ;;
  medium)
    ZSTR=1mdof
    MSTR=uhxt33p2
    PART=240
    NMODEL=1
    ITER=5000
    GSTR=$MSTR"n"$PART
    SSTR=""
    ;;
  large)
    ZSTR=50mdof
    MSTR=uhxt123p2
    PART=1920
    NMODEL=1
    ITER=100
    GSTR=$MSTR"n"
    SSTR="-xS16 -yS12 -zS10"
    ;;
esac
case "$YSTR" in
  iso)
    $XDIR/"gmsh2fmr-"$CPUMODEL"-gcc" -v1 \
    -x@0.0 -x0 -y@0.0 -y0 -z@0.0 -z0 -x@1.0 -xu0.001 -x@1.0 $SSTR \
    -M0 -E100e9 -N0.3 \
    -a $MDIR/$MSTR/$GSTR
    ;;
  ort)
    $XDIR/"gmsh2fmr-"$CPUMODEL"-gcc" -v1 \
    -x@0.0 -x0 -y@0.0 -y0 -z@0.0 -z0 -x@1.0 -xu0.001 -x@1.0 $SSTR \
    -M0 -E100e9 -N0.3 -R \
    -a $MDIR/$MSTR/$GSTR
    ;;
esac
if [ "$NMODEL" == "1" ]; then
  EXESTR=$XDIR/"femerq-"$CPUMODEL"-"$CSTR" -c"$CPUCOUNT" -r0 -i"$ITER\
" -p "$MDIR/$MSTR/$MSTR"n"$PART
else
  EXESTR=$XDIR/"femera-mmq-"$CPUMODEL"-"$CSTR" -c"$PART" -n"$CPUCOUNT" -m"$NMODEL\
" -r0 -i"$ITER" -p "$MDIR/$MSTR/$MSTR"n"$PART
fi
advixe-cl --collect survey\
  --project-dir /u/dwagner5/intel/advixe/projects/"femera-"$ZSTR"-tet10-"$YSTR"-"$CPUMODEL"-"$CSTR --\
  $EXESTR

advixe-cl --collect tripcounts --flop --stacks\
  --project-dir /u/dwagner5/intel/advixe/projects/"femera-"$ZSTR"-tet10-"$YSTR"-"$CPUMODEL"-"$CSTR --\
  $EXESTR

done
done
done