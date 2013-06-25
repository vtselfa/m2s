#!/bin/bash

echo "cpufile/cpuways/l1size/cores/mcs/stall_rob/stall_iq/stall_lsq/stall_rename/stall_mem/stall_uop_queue/stall_spec/stall_used/Misses l2/Misses dl1/Misses il1/#insts/IPC/Avg.Lat./Cycles"

for i in $1/*cpu;
do
  cpufile=$( echo $i);
  memfile=$( echo $i | sed 's/cpu$/mem/' );
  netfile=$( echo $i | sed 's/cpu$/net/' );
  simulfile=$( echo $i | sed 's/cpu$/simul/' );
  stall_rob=$( grep "Dispatch.Stall.Cycles.rob" $cpufile | cut -d'=' -f2 | awk '{ SUM += $1} END { print SUM }' );
  stall_iq=$( grep "Dispatch.Stall.Cycles.iq" $cpufile | cut -d'=' -f2 | awk '{ SUM += $1} END { print SUM }' );
  stall_lsq=$( grep "Dispatch.Stall.Cycles.lsq" $cpufile | cut -d'=' -f2 | awk '{ SUM += $1} END { print SUM }' );
  stall_rename=$( grep "Dispatch.Stall.Cycles.rename" $cpufile | cut -d'=' -f2 | awk '{ SUM += $1} END { print SUM }' );
  stall_mem=$( grep "Dispatch.Stall.Cycles.mem" $cpufile | cut -d'=' -f2 | awk '{ SUM += $1} END { print SUM }' );
  stall_uop_queue=$( grep "Dispatch.Stall.Cycles.uop_queue" $cpufile | cut -d'=' -f2 | awk '{ SUM += $1} END { print SUM }' );
  stall_spec=$( grep "Dispatch.Stall.spec" $cpufile | cut -d'=' -f2 | awk '{ SUM += $1} END { print SUM }' );
  stall_used=$( grep "Dispatch.Stall.Cycles.used" $cpufile | cut -d'=' -f2 | awk '{ SUM += $1} END { print SUM }' );
  latency=$( grep "AverageLatency" $netfile | cut -d'=' -f2 | awk '{ SUM += $1} END { print SUM }' );
  cpuways=$( echo $i | cut -d'_' -f2 );
  l1size=$( echo $i | cut -d'_' -f4 );
  cores=$( echo $i | cut -d'_' -f6 );
  mcs=$( echo $i | cut -d'_' -f7 );
  bench=$( echo $i | cut -d'_' -f1 );
  cycles=$( grep -w -m1 "^Cycles" $simulfile | tail -n1 | cut -d'=' -f 2 | awk '{ SUM += $1} END { print SUM }' );
  l2=$( grep -w -m1 "^Misses" $memfile | tail -n1 | cut -d'=' -f 2 | awk '{ SUM += $1} END { print SUM }' );
  dl1=$( grep -w -m2 "^Misses" $memfile | tail -n1 | cut -d'=' -f 2 | awk '{ SUM += $1} END { print SUM }' );
  il1=$( grep -w -m3 "^Misses" $memfile | tail -n1 | cut -d'=' -f 2 | awk '{ SUM += $1} END { print SUM }' );
  inst=$( grep -w "CommittedInstructions" $simulfile | cut -d'=' -f 2 | awk '{ SUM += $1} END { print SUM }' );
  ipc=$( grep -w "CommittedInstructionsPerCycle" $simulfile | cut -d'=' -f 2 | awk '{ SUM += $1} END { print SUM }' );
  echo `basename $bench`/$cpuways/$l1size/$cores/$mcs/$stall_rob/$stall_iq/$stall_lsq/$stall_rename/$stall_mem/$stall_uop_queue/$stall_spec/$stall_used/$l2/$dl1/$il1/$inst/$ipc/$latency/$cycles;
done;

