#!/bin/bash

plots_dir=./plots
mkdir -p ${plots_dir}

for ipc_file in *.ipc; do
	bench=${ipc_file%.*}
	gnuplot <<- EOP
		set key left top
		set title "${bench}"
		set xlabel "Cycles"
		set ylabel "IPC"
		set format x "%2.0t{/Symbol \327}10^{%L}"
		set terminal pngcairo enhanced font "Verdana,10"
		set output "${plots_dir}/${bench}.png"
		plot "${ipc_file}" every ::25 using 1:3 with lines title "Global", "${ipc_file}" every ::25 using 1:4 with lines title "Interval"
	EOP
done

