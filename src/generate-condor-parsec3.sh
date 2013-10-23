#!/bin/bash

sim=../bin/m2s
configuraciones=configuraciones-prefetch
ruta_results=results/parsec3

configuracion_net=$configuraciones/netconfig-64B.mesh
configuracion_cache=$configuraciones/cacheconfig.8nodes.L2-256KB-64B-8ways
configuracion_cpu=$configuraciones/cpuconfig.fu.prefetchwork

mkdir ${ruta_results}

echo "Universe       = vanilla"
echo "Executable     = $sim"
echo "+GPBatchJob                 = true"
echo "+LongRunningJob             = true"

for ctxs in benchmarks/parsec3/ctxfiles/*8threads; do
  i=$( echo $ctxs | cut -d'.' -f2 | cut -d'_' -f1 )
  echo "Arguments = --x86-sim detailed --x86-max-cycles 1000000000 --x86-report ${ruta_results}/${i}_cpu --main-mem-report ${ruta_results}/${i}_mm --net-report ${ruta_results}/${i}_net --mem-report ${ruta_results}/${i}_mem --net-report ${ruta_results}/${i}_net --mem-config $configuracion_cache --x86-config $configuracion_cpu --net-config $configuracion_net --ctx-config $ctxs";
  echo "Error = ${ruta_results}/${i}_simul"
  echo "log = ${ruta_results}/${i}_simul.log"
  echo "Queue"
done

