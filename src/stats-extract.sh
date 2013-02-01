#!/bin/bash

echo "cpufile,cpuways,l1size,stall_rob,stall_iq,stall_lsq,stall_rename,stall_mem,stall_uop_queue,stall_spec,stall_used,Misses l2,Misses dl1,Misses il1,#insts,IPC"

for i in $1/*cpu;
do
  cpufile=$( echo $i);
  memfile=$( echo $i | sed 's/cpu$/mem/' );
  simulfile=$( echo $i | sed 's/cpu$/simul/' );
  stall_rob=$( grep "Dispatch.Stall.rob" $cpufile | cut -d'=' -f2 );
  stall_iq=$( grep "Dispatch.Stall.iq" $cpufile | cut -d'=' -f2 );
  stall_lsq=$( grep "Dispatch.Stall.lsq" $cpufile | cut -d'=' -f2 );
  stall_rename=$( grep "Dispatch.Stall.rename" $cpufile | cut -d'=' -f2 );
  stall_mem=$( grep "Dispatch.Stall.Mem" $cpufile | cut -d'=' -f2 );
  stall_uop_queue=$( grep "Dispatch.Stall.uop_queue" $cpufile | cut -d'=' -f2 );
  stall_spec=$( grep "Dispatch.Stall.spec" $cpufile | cut -d'=' -f2 );
  stall_used=$( grep "Dispatch.Stall.used" $cpufile | cut -d'=' -f2 );
  cpuways=$( echo $i | cut -d'_' -f2 );
  l1size=$( echo $i | cut -d'_' -f4 );
  l2=$( grep -w -m1 "^Misses" $memfile | tail -n1 | cut -d'=' -f 2 );
  dl1=$( grep -w -m2 "^Misses" $memfile | tail -n1 | cut -d'=' -f 2 );
  il1=$( grep -w -m3 "^Misses" $memfile | tail -n1 | cut -d'=' -f 2 );
  inst=$( grep -w "CommittedInstructions" $simulfile | cut -d'=' -f 2 );
  ipc=$( grep -w "CommittedInstructionsPerCycle" $simulfile | cut -d'=' -f 2 );
  echo `basename $cpufile`,$cpuways,$l1size,$stall_rob,$stall_iq,$stall_lsq,$stall_rename,$stall_mem,$stall_uop_queue,$stall_spec,$stall_used,$l2,$dl1,$il1,$inst,$ipc;
done;
  
