#!/bin/bash

sim=./m2s

echo "Universe       = vanilla"
echo "Executable     = $sim"

for i in benchmarks/wcet/*; do 
	for ways in 2 4; do 
		for size in 16 32; do 
			file=$( echo `basename $i`_${ways}way_cpu_${size}kb_l1); 
			echo "Arguments = --x86-sim detailed --x86-report results/embedded/${file}_cpu --mem-report results/embedded/${file}_mem --mem-config configuraciones-prefetch/embedded/memconfig-1core.${size}KB-L1 --x86-config configuraciones-prefetch/embedded/cpuconfig.1core.${ways}ways $i &> results/embedded/${file}_simul";
			echo "Error = results/embedded/${file}_simul.err"
			echo "Queue"
		done;
	done;
done

for i in coremark; do 
	for ways in 2 4; do 
		for size in 16 32; do 
			file=$( echo `basename $i`_${ways}way_cpu_${size}kb_l1);
			echo "Arguments = --x86-sim detailed --x86-report results/embedded/${file}_cpu --mem-report results/embedded/${file}_mem --mem-config configuraciones-prefetch/embedded/memconfig-1core.${size}KB-L1 --x86-config configuraciones-prefetch/embedded/cpuconfig.1core.${ways}ways benchmarks/coremark_v1.0/coremark.exe 0x3415 0x3415 0x66 0 7 1 10000 &> results/embedded/${file}_simul";
			echo "Error = results/embedded/${file}_simul.err"
			echo "Queue"
		done;
	done;
done

for i in ocean; do 
	for ways in 2 4; do 
		for size in 16 32; do 
			file=$( echo `basename $i`_${ways}way_cpu_${size}kb_l1);
			echo "Arguments = --x86-sim detailed --x86-report results/embedded/${file}_cpu --mem-report results/embedded/${file}_mem --mem-config configuraciones-prefetch/embedded/memconfig-1core.${size}KB-L1 --x86-config configuraciones-prefetch/embedded/cpuconfig.1core.${ways}ways benchmarks/splash2/ocean/ocean.i386 -n130 -p1 -e1e-07 -r20000 -t28800 &> results/embedded/${file}_simul";
			echo "Error = results/embedded/${file}_simul.err"
			echo "Queue"
		done;
	done;
done

