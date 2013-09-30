#!/bin/bash

sim=../bin/m2s
configuraciones=configuraciones-prefetch/embedded
ruta_results=results/PFC


echo "Universe       = vanilla"
echo "Executable     = $sim"
echo "+GPBatchJob                 = true"
echo "+LongRunningJob             = true"

for cores in  64; do
	for ways in   4 ; do
		for mc in 1 2 8 16 64; do
			for c in 2 ; do
				for enlaceRed in 8 ; do

					configuracion_net=$configuraciones/netconfig-${cores}cores-${mc}mc-${enlaceRed}B
					configuracion_cache=$configuraciones/memconfig-${cores}core.32KB-L1-${mc}mc-${c}c
					configuracion_cpu=$configuraciones/cpuconfig.${cores}core.${ways}ways
				
					mkdir ${ruta_results}/${cores}cores_${mc}mc_${ways}ways_${c}channels_${enlaceRed}B
					results=$ruta_results/${cores}cores_${mc}mc_${ways}ways_${c}channels_${enlaceRed}B

					for i in ocean; do
	
						file=$( echo `basename $i`);
						
				 		echo "Arguments = --x86-sim detailed --x86-report $results/${file}_cpu  --main-mem-report $results/${file}_mm --net-report $results/${file}_net --mem-report $results/${file}_mem --net-report $results/${file}_net --mem-config $configuracion_cache --x86-config $configuracion_cpu --net-config $configuracion_net benchmarks/splash2/ocean/ocean.i386 -n130 -p${cores} -e1e-07 -r20000 -t28800";
						echo "Error = $results/${file}_simul"
						echo "log = $results/${file}_simul.log"
						echo "Queue"
					done;


				#	for i in cholesky; do
					
				#		file=$( echo `basename $i`);
						
				#		echo "Arguments = --x86-sim detailed --x86-report $results/${file}_cpu --net-report $results/${file}_net --mem-report $results/${file}_mem --net-report $results/${file}_net --mem-config $configuracion_cache --x86-config $configuracion_cpu --net-config $configuracion_net benchmarks/splash2/cholesky/cholesky.i386 -p8 -C65536";
				#		echo "Error = $results/${file}_simul"
				#		echo "log = $results/${file}_simul.log"
				#		echo "Input = benchmarks/splash2/cholesky/tk15.O"
				#		echo "Queue"
				#	done;


					for i in fft; do
						file=$( echo `basename $i`);
						
						echo "Arguments = --x86-sim detailed --main-mem-report $results/${file}_mm --x86-report $results/${file}_cpu --net-report $results/${file}_net --mem-report $results/${file}_mem --net-report $results/${file}_net --mem-config $configuracion_cache --x86-config $configuracion_cpu --net-config $configuracion_net benchmarks/splash2/fft/fft.i386 -m16 -p${cores} -n1024 -l6 -s -t";
						echo "Error = $results/${file}_simul"
						echo "log = $results/${file}_simul.log"
						echo "Queue"
					done;




					for i in radix; do
						file=$( echo `basename $i`);
						
						echo "Arguments = --x86-sim detailed --x86-report $results/${file}_cpu --main-mem-report $results/${file}_mm --net-report $results/${file}_net --mem-report $results/${file}_mem --net-report $results/${file}_net --mem-config $configuracion_cache --x86-config $configuracion_cpu --net-config $configuracion_net benchmarks/splash2/radix/radix.i386 -n524288 -p${cores}";
						echo "Error = $results/${file}_simul"
						echo "log = $results/${file}_simul.log"
						echo "Queue"
					done;
				done;
			done;
		done;
	done;
done

