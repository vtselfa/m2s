#!/bin/bash

echo "_________________________ondemand memory distribuion. OD10 y OD50 MM ACCESSES RANKING_________________________"
echo
echo "_______________Ejecucion de Benchmarks SPEC2006 y Stream en m2s______________"
echo
echo "_________________El Condor Pasa por Cell_________________"
echo

ruta_de_la_ula=.
ruta_resultats=results/con_MC_MM_pref_OBL
id="prefNormColaXBank_oblL1L2"
#today=$(date +%d%b[%X])
mkdir ${ruta_de_la_ula}/${ruta_resultats}/${id}
mkdir ${ruta_de_la_ula}/${ruta_resultats}/${id}/result_output
mkdir ${ruta_de_la_ula}/${ruta_resultats}/${id}/result_log
mkdir ${ruta_de_la_ula}/${ruta_resultats}/${id}/result_err
mkdir ${ruta_de_la_ula}/${ruta_resultats}/${id}/mm_ranking
mkdir ${ruta_de_la_ula}/${ruta_resultats}/${id}/cpu
mkdir ${ruta_de_la_ula}/${ruta_resultats}/${id}/main_memory
mkdir ${ruta_de_la_ula}/${ruta_resultats}/${id}/mem
mkdir ${ruta_de_la_ula}/${ruta_resultats}/${id}/net


###############El_Condor_Pasa###############
echo > ${ruta_de_la_ula}/lanzar/lanzar_${id}
echo "+GPBatchJob = true" >> ${ruta_de_la_ula}/lanzar/lanzar_${id}
echo "+LongRunningJob = true" >> ${ruta_de_la_ula}/lanzar/lanzar_${id}
echo >> ${ruta_de_la_ula}/lanzar/lanzar_${id}
echo "Executable = ${ruta_de_la_ula}/m2s" >> ${ruta_de_la_ula}/lanzar/lanzar_${id}
echo >> ${ruta_de_la_ula}/lanzar/lanzar_${id}
echo "Universe = vanilla" >> ${ruta_de_la_ula}/lanzar/lanzar_${id}
echo >> ${ruta_de_la_ula}/lanzar/lanzar_${id}
echo "Rank = -LoadAvg" >> ${ruta_de_la_ula}/lanzar/lanzar_${id}
echo >> ${ruta_de_la_ula}/lanzar/lanzar_${id}
###############El_Condor_Pasa###############

config="${ruta_de_la_ula}/configuraciones-prefetch/cpuconfig.prefetchwork"

# BENCHMARK


for bench in tonto specrand gromacs namd perlbench bzip2 gcc bwaves mcf milc zeusmp cactusADM leslie3d gobmk dealII soplex povray calculix hmmer sjeng GemsFDTD libquantum h264ref lbm omnetpp astar wrf sphinx3 xalancbmk

#perlbench bzip2 gcc bwaves gamess mcf milc zeusmp gromacs cactusADM leslie3d namd gobmk dealII soplex povray calculix hmmer sjeng GemsFDTD libquantum h264ref tonto lbm omnetpp astar wrf sphinx3 xalancbmk specrand
	do

		ctxconfig="${ruta_de_la_ula}/ctxconfigs/ctxconfig.${bench}"



									###############El_Condor_Pasa ###############

		cacheconfig="${ruta_de_la_ula}/configuraciones-prefetch/cacheconfig.8nodes.L2-2MB-64B-32ways"

		netconfig="${ruta_de_la_ula}/configuraciones-prefetch/netconfig-64B.mesh"

		echo "Arguments = --x86-max-inst 500000000 --x86-sim detailed  --x86-config $config --net-config $netconfig  --mem-config $cacheconfig --report-mem-main-memory  ${ruta_de_la_ula}/${ruta_resultats}/${id}/main_memory/$bench.main_memory  --net-report ${ruta_de_la_ula}/${ruta_resultats}/${id}/net/$bench.net.stats  --mem-report ${ruta_de_la_ula}/${ruta_resultats}/${id}/mem/$bench.mem.stats  --x86-report ${ruta_de_la_ula}/${ruta_resultats}/${id}/cpu/$bench.cpu.stats   --ctx-config $ctxconfig" >> ${ruta_de_la_ula}/lanzar/lanzar_${id} 

		echo >> ${ruta_de_la_ula}/lanzar/lanzar_${id}

		echo "Output = ${ruta_resultats}/${id}/result_output/${id}_${bench}_process.out" >> ${ruta_de_la_ula}/lanzar/lanzar_${id}

		echo >> ${ruta_de_la_ula}/lanzar/lanzar_${id}

		echo "Log = ${ruta_resultats}/${id}/result_log/${id}_${bench}_process.log" >> ${ruta_de_la_ula}/lanzar/lanzar_${id}

		echo >> ${ruta_de_la_ula}/lanzar/lanzar_${id}

		echo "Error = ${ruta_resultats}/${id}/result_err/${id}_${bench}_process.err" >> ${ruta_de_la_ula}/lanzar/lanzar_${id}

		echo >> ${ruta_de_la_ula}/lanzar/lanzar_${id}

		echo "Queue" >> "$ruta_de_la_ula""/lanzar/lanzar_${id}"

		echo >> ${ruta_de_la_ula}/lanzar/lanzar_${id}

	done





										###############El_Condor_Pasa###############

condor_submit ${ruta_de_la_ula}/lanzar/lanzar_${id}

###############El_Condor_Pasa###############   
