#!/bin/bash
CPUMODEL=`./cpumodel.sh`
CPUCOUNT=`./cpucount.sh`
CSTR=gcc
EXEDIR="."
GMSH2FMR=gmsh2fmr-$CPUMODEL-$CSTR
#
if [ -d "/hpnobackup1/dwagner5/femera-test/cube" ];then
  MESHDIR=/hpnobackup1/dwagner5/femera-test/cube
else
  MESHDIR=cube
fi
#
HAS_GNUPLOT=`which gnuplot`
MEM=`free -b  | grep Mem | awk '{print $7}'`
echo `free -g  | grep Mem | awk '{print $7}'` GB Available Memory
#
UDOF_MAX=$(( $MEM / 120 ))
NODE_MAX=$(( $UDOF_MAX / 3))
TET10_MAX=$(( $UDOF_MAX / 4 ))
echo Max. Model: $TET10_MAX Tet10, $NODE_MAX Nodes, $UDOF_MAX Elastic DOF
#
MESH_N=21
LIST_H=(2 3 4 5 7 9   12 15 21 26 33   38 45 56 72 96   123 156 205 265 336)
NOMI_UDOF=(500 1000 2500 5000 10000 25000\
 50000 100000 250000 500000 1000000\
 1500000 2500000 5000000 10000000 25000000\
 50000000 100000000 250000000 500000000 1000000000)
#TRY_H='';
TRY_COUNT=0;
for I in $(seq 0 20); do
  if [ ${NOMI_UDOF[I]} -lt $UDOF_MAX ]; then
    TRY_COUNT=$(( $TRY_COUNT + 1))
  fi
done
#
PERFDIR="perf"
PROFILE=$PERFDIR/"uhxt-tet10-elas-ort-"$CPUMODEL"-"$CSTR".pro"
CSVFILE=$PERFDIR/"uhxt-tet10-elas-ort-"$CPUMODEL"-"$CSTR".csv"
#
if [ ! -f $PROFILE ];then
  P=2; C=$CPUCOUNT; N=$C; RTOL=1e-24;
  TARGET_TEST_S=6;# Try for S sec/run
  REPEAT_TEST_N=5;# Repeat each test N times
  ITERS_MIN=10;
  #
  # First, get a rough idea of DOF/sec to estimate time
  # with 10 iterations of the second-to-largest model
  if [ ! -f $CSVFILE ]; then
    ITERS=10; H=${LIST_H[$(( $TRY_COUNT - 2 ))]};
    MESH=$MESHDIR"/uhxt"$H"p"$P"/uhxt"$H"p"$P"n"$N
    echo Running $ITERS iterations of $MESH...
    $PERFDIR/mesh-uhxt.sh $H $P $N "$MESHDIR" "$EXEDIR/$GMSH2FMR"
    $EXEDIR"/femerq-"$CPUMODEL"-"$CSTR -v1 -c$C -i$ITERS -r$RTOL\
    -p $MESH >> $CSVFILE
  fi
  DOFS=`head -n1 $CSVFILE | awk -F, '{ print $13 }'`
  for I in $(seq 0 $(( $TRY_COUNT - 1)) ); do
    H=${LIST_H[I]}
    MESH=$MESHDIR"/uhxt"$H"p"$P"/uhxt"$H"p"$P"n"$N
    $PERFDIR/mesh-uhxt.sh $H $P $N "$MESHDIR" "$EXEDIR/$GMSH2FMR"
    NNODE=`grep -m1 -A1 -i node $MESH".msh" | tail -n1`
    NDOF=$(( $NNODE * 3 ))
    ITERS=`printf '%f*%f/%f\n' $TARGET_TEST_S $DOFS $NDOF | bc`
    if [ $ITERS -lt $ITERS_MIN ];then ITERS=10; fi
    echo "Running "$ITERS" iterations of "$MESH" ("$NDOF" DOF), "$REPEAT_TEST_N" times..."
    for I in $(seq 0 $REPEAT_TEST_N ); do
      $EXEDIR"/femerq-"$CPUMODEL"-"$CSTR -v1 -c$C -i$ITERS -r$RTOL\
      -p $MESH >> $CSVFILE
    done
  done
else
  echo "Reading profile: "$PROFILE"..."
fi
if [ ! -z "$HAS_GNUPLOT" ]; then
  echo "Reading basiic profile data: "$CSVFILE"..."
gnuplot -e  "\
set terminal dumb noenhanced size 79,25;\
set datafile separator ',';\
set tics scale 0,0;\
set logscale x;\
set xrange [1e3:1.05e9];\
set yrange [0:];\
set key inside top right;\
set title 'Femera Performance ["$CPUCOUNT" Partitions]';\
set xlabel 'System Size [MDOF]';\
plot 'perf/uhxt-tet10-elas-ort-"$CPUMODEL"-"$CSTR".csv'\
 using 3:(\$13/1e6)\
 with points pointtype 24 \
 title '[DOF/s]';"
fi
#
#
#