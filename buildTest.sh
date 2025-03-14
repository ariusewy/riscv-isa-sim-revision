#! /bin/bash

mkdir -p build
cd build && ../configure --prefix=$PWD/../../toollocal/ && make -j4 && make install

# test

cd /home/ywangmu/Proj/ToolRef/toollocal/bin/ && \
./spike -d --debug-cmd=../../cascade-processorfuzz/shareddata/mylocaldata/dbgcmds/cmds_trace_regs_at_pc_locs_784213_rocket_8904673_34 --pc=2147483648 -l -c --log-commits --log=../../cascade-processorfuzz/shareddata/mylocaldata/Interspikeresol784213_rocket_8904673_34_csr.log ../../cascade-processorfuzz/shareddata/mylocaldata/spikeresol784213_rocket_8904673_34.elf



