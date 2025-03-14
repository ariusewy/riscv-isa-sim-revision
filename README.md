Spike RISC-V ISA Simulator
============================

About
-------------

Spike, the RISC-V ISA Simulator, implements a functional model of one or more
RISC-V harts.  It is named after the golden spike used to celebrate the
completion of the US transcontinental railway.

Spike supports the following RISC-V ISA features:
  - RV32I and RV64I base ISAs, v2.1
  - RV32E and RV64E base ISAs, v1.9
  - Zifencei extension, v2.0
  - Zicsr extension, v2.0
  - M extension, v2.0
  - A extension, v2.1
  - F extension, v2.2
  - D extension, v2.2
  - Q extension, v2.2
  - C extension, v2.0
  - Zbkb, Zbkc, Zbkx, Zknd, Zkne, Zknh, Zksed, Zksh scalar cryptography extensions (Zk, Zkn, and Zks groups), v1.0
  - Zkr virtual entropy source emulation, v1.0
  - V extension, v1.0 (_requires a 64-bit host_)
  - P extension, v0.9.2
  - Zba extension, v1.0
  - Zbb extension, v1.0
  - Zbc extension, v1.0
  - Zbs extension, v1.0
  - Zfh and Zfhmin half-precision floating-point extensions, v1.0
  - Zfinx extension, v1.0
  - Zmmul integer multiplication extension, v1.0
  - Zicbom, Zicbop, Zicboz cache-block maintenance extensions, v1.0
  - Conformance to both RVWMO and RVTSO (Spike is sequentially consistent)
  - Machine, Supervisor, and User modes, v1.11
  - Hypervisor extension, v1.0
  - Svnapot extension, v1.0
  - Svpbmt extension, v1.0
  - Svinval extension, v1.0
  - Sdext extension, v1.0-STABLE
  - Sdtrig extension, v1.0-STABLE
    - 4 triggers support type={2, 3, 4, 5, 6, 15} (mcontrol, icount, itrigger, etrigger, mcontrol6, disabled)
  - Smepmp extension v1.0
  - Smstateen extension, v1.0
  - Sscofpmf v0.5.2
  - Zca extension, v1.0
  - Zcb extension, v1.0
  - Zcf extension, v1.0
  - Zcd extension, v1.0
  - Zcmp extension, v1.0
  - Zcmt extension, v1.0
  - Zfbfmin extension, v0.6
  - Zvfbfmin extension, v0.6
  - Zvfbfwma extension, v0.6

As a Spike extension, the remainder of the proposed
[Bit-Manipulation Extensions](https://github.com/riscv/riscv-bitmanip)
is provided under the Spike-custom extension name _Xbitmanip_.
These instructions (and, of course, the extension name) are not RISC-V
standards.

These proposed bit-manipulation extensions can be split into further
groups: Zbp, Zbs, Zbe, Zbf, Zbc, Zbm, Zbr, Zbt. Note that Zbc is
ratified, but the original proposal contained some extra instructions
(64-bit carryless multiplies) which are captured here.

To enable these extensions individually, use the Spike-custom
extension names _XZbp_, _XZbs_, _XZbc_, and so on.

Versioning and APIs
-------------------

Projects are versioned primarily to indicate when the API has been extended or
rendered incompatible.  In that spirit, Spike aims to follow the
[SemVer](https://semver.org/spec/v2.0.0.html) versioning scheme, in which
major version numbers are incremented when backwards-incompatible API changes
are made; minor version numbers are incremented when new APIs are added; and
patch version numbers are incremented when bugs are fixed in
a backwards-compatible manner.

Spike's principal public API is the RISC-V ISA.  _The C++ interface to Spike's
internals is **not** considered a public API at this time_, and
backwards-incompatible changes to this interface _will_ be made without
incrementing the major version number.

Build Steps
---------------

We assume that the RISCV environment variable is set to the RISC-V tools
install path.

    $ apt-get install device-tree-compiler
    $ mkdir build
    $ cd build
    $ ../configure --prefix=$RISCV
    $ make
    $ [sudo] make install

If your system uses the `yum` package manager, you can substitute
`yum install dtc` for the first step.

Build Steps on OpenBSD
----------------------

Install bash, gmake, dtc, and use clang.

    $ pkg_add bash gmake dtc
    $ exec bash
    $ export CC=cc; export CXX=c++
    $ mkdir build
    $ cd build
    $ ../configure --prefix=$RISCV
    $ gmake
    $ [doas] make install

Compiling and Running a Simple C Program
-------------------------------------------

Install spike (see Build Steps), riscv-gnu-toolchain, and riscv-pk.

Write a short C program and name it hello.c.  Then, compile it into a RISC-V
ELF binary named hello:

    $ riscv64-unknown-elf-gcc -o hello hello.c

Now you can simulate the program atop the proxy kernel:

    $ spike pk hello

Simulating a New Instruction
------------------------------------

Adding an instruction to the simulator requires two steps:

  1.  Describe the instruction's functional behavior in the file
      riscv/insns/<new_instruction_name>.h.  Examine other instructions
      in that directory as a starting point.

  2.  Add the opcode and opcode mask to riscv/opcodes.h.  Alternatively,
      add it to the riscv-opcodes package, and it will do so for you:
        ```
         $ cd ../riscv-opcodes
         $ vi opcodes       // add a line for the new instruction
         $ make install
        ```

  3.  Rebuild the simulator.

Interactive Debug Mode
---------------------------

To invoke interactive debug mode, launch spike with -d:

    $ spike -d pk hello

To see the contents of an integer register (0 is for core 0):

    : reg 0 a0

To see the contents of a floating point register:

    : fregs 0 ft0

or:

    : fregd 0 ft0

depending upon whether you wish to print the register as single- or double-precision.

To see the contents of a memory location (physical address in hex):

    : mem 2020

To see the contents of memory with a virtual address (0 for core 0):

    : mem 0 2020

You can advance by one instruction by pressing the enter key. You can also
execute until a desired equality is reached:

    : until pc 0 2020                   (stop when pc=2020)
    : until reg 0 mie a                 (stop when register mie=0xa)
    : until mem 2020 50a9907311096993   (stop when mem[2020]=50a9907311096993)

Alternatively, you can execute as long as an equality is true:

    : while mem 2020 50a9907311096993

You can continue execution indefinitely by:

    : r

At any point during execution (even without -d), you can enter the
interactive debug mode with `<control>-<c>`.

To end the simulation from the debug prompt, press `<control>-<c>` or:

    : q

Debugging With Gdb
------------------

An alternative to interactive debug mode is to attach using gdb. Because spike
tries to be like real hardware, you also need OpenOCD to do that. OpenOCD
doesn't currently know about address translation, so it's not possible to
easily debug programs that are run under `pk`. We'll use the following test
program:
```
$ cat rot13.c 
char text[] = "Vafgehpgvba frgf jnag gb or serr!";

// Don't use the stack, because sp isn't set up.
volatile int wait = 1;

int main()
{
    while (wait)
        ;

    // Doesn't actually go on the stack, because there are lots of GPRs.
    int i = 0;
    while (text[i]) {
        char lower = text[i] | 32;
        if (lower >= 'a' && lower <= 'm')
            text[i] += 13;
        else if (lower > 'm' && lower <= 'z')
            text[i] -= 13;
        i++;
    }

done:
    while (!wait)
        ;
}
$ cat spike.lds 
OUTPUT_ARCH( "riscv" )

SECTIONS
{
  . = 0x10110000;
  .text : { *(.text) }
  .data : { *(.data) }
}
$ riscv64-unknown-elf-gcc -g -Og -o rot13-64.o -c rot13.c
$ riscv64-unknown-elf-gcc -g -Og -T spike.lds -nostartfiles -o rot13-64 rot13-64.o
```

To debug this program, first run spike telling it to listen for OpenOCD:
```
$ spike --rbb-port=9824 -m0x10100000:0x20000 rot13-64
Listening for remote bitbang connection on port 9824.
```

In a separate shell run OpenOCD with the appropriate configuration file:
```
$ cat spike.cfg 
adapter driver remote_bitbang
remote_bitbang host localhost
remote_bitbang port 9824

set _CHIPNAME riscv
jtag newtap $_CHIPNAME cpu -irlen 5 -expected-id 0xdeadbeef

set _TARGETNAME $_CHIPNAME.cpu
target create $_TARGETNAME riscv -chain-position $_TARGETNAME

gdb_report_data_abort enable

init
halt
$ openocd -f spike.cfg
Open On-Chip Debugger 0.10.0-dev-00002-gc3b344d (2017-06-08-12:14)
...
riscv.cpu: target state: halted
```

In yet another shell, start your gdb debug session:
```
tnewsome@compy-vm:~/SiFive/spike-test$ riscv64-unknown-elf-gdb rot13-64
GNU gdb (GDB) 8.0.50.20170724-git
Copyright (C) 2017 Free Software Foundation, Inc.
License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>
This is free software: you are free to change and redistribute it.
There is NO WARRANTY, to the extent permitted by law.  Type "show copying"
and "show warranty" for details.
This GDB was configured as "--host=x86_64-pc-linux-gnu --target=riscv64-unknown-elf".
Type "show configuration" for configuration details.
For bug reporting instructions, please see:
<http://www.gnu.org/software/gdb/bugs/>.
Find the GDB manual and other documentation resources online at:
<http://www.gnu.org/software/gdb/documentation/>.
For help, type "help".
Type "apropos word" to search for commands related to "word"...
Reading symbols from rot13-64...done.
(gdb) target remote localhost:3333
Remote debugging using localhost:3333
0x0000000010010004 in main () at rot13.c:8
8	    while (wait)
(gdb) print wait
$1 = 1
(gdb) print wait=0
$2 = 0
(gdb) print text
$3 = "Vafgehpgvba frgf jnag gb or serr!"
(gdb) b done 
Breakpoint 1 at 0x10110064: file rot13.c, line 22.
(gdb) c
Continuing.
Disabling abstract command writes to CSRs.

Breakpoint 1, main () at rot13.c:23
23	    while (!wait)
(gdb) print wait
$4 = 0
(gdb) print text
...
```


My Revisions
------------------

1. Add CSR print info in log file, in `riscv-isa-sim/riscv/processor.cc` 

2. Used In `riscv-isa-sim/riscv/execute.cc`

3. spike.cc main() -> sim_t::run() -> processor_t::step() -> execute_insn_xxx()
    - !debug && log still will open the debug mode
4. sim_t::step()
  -> processor_t::step()
    -> execute_insn_fast() 或 execute_insn_logged()
      -> fetch.func(p, fetch.insn, pc) 

5. mstatus，frm (浮点舍入模式)，fflags (浮点标志)，mcause (机器异常原因)，scause (监管者异常原因)，medeleg (机器异常委托)，mcounteren (机器计数器使能)，scounteren (监管者计数器使能)，dcsr (调试控制和状态寄存器)
dcsr (调试控制和状态寄存器)
5. --log-commits 
  0: 3 0x0000000000001000 (0x00000297) x5  0x0000000000001000
  │  │ │                   │             │   │
  │  │ │                   │             │   └── 写入的值
  │  │ │                   │             └────── 目标寄存器(x5)
  │  │ │                   └───────────────── 指令的机器码
  │  │ └───────────────────────────────── PC值(程序计数器)
  │  └─────────────────────────────────── 特权级(3=M模式)
  └────────────────────────────────────── 核心ID(core 0)

- Debug Cmd
(spike) help
# Interactive commands:
- `reg <core> [reg]` - Display [reg] (all if omitted) in <core>
- `freg <core> <reg>` - Display float <reg> in <core> as hex
- `fregh <core> <reg>` - Display half precision <reg> in <core>
- `fregs <core> <reg>` - Display single precision <reg> in <core>
- `fregd <core> <reg>` - Display double precision <reg> in <core>
- `vreg <core> [reg]` - Display vector [reg] (all if omitted) in <core>
- `pc <core>` - Show current PC in <core>
- `priv <core>` - Show current privilege level in <core>
- `mem [core] <hex addr>` - Show contents of virtual memory <hex addr> in [core] (physical memory <hex addr> if omitted)
- `str [core] <hex addr>` - Show NUL-terminated C string at virtual address <hex addr> in [core] (physical address <hex addr> if omitted)
- `dump` - Dump physical memory to binary files
- `mtime` - Show mtime
- `mtimecmp <core>` - Show mtimecmp for <core>
- `until reg <core> <reg> <val>` - Stop when <reg> in <core> hits <val>
- `untiln reg <core> <reg> <val>` - Run noisy and stop when <reg> in <core> hits <val>
- `until pc <core> <val>` - Stop when PC in <core> hits <val>
- `untiln pc <core> <val>` - Run noisy and stop when PC in <core> hits <val>
- `until mem [core] <addr> <val>` - Stop when virtual memory <addr> in [core] (physical address <addr> if omitted) becomes <val>
- `untiln mem [core] <addr> <val>` - Run noisy and stop when virtual memory <addr> in [core] (physical address <addr> if omitted) becomes <val>
- `while reg <core> <reg> <val>` - Run while <reg> in <core> is <val>
- `while pc <core> <val>` - Run while PC in <core> is <val>
- `while mem [core] <addr> <val>` - Run while virtual memory <addr> in [core] (physical memory <addr> if omitted) is <val>
- `run [count]` - Resume noisy execution (until CTRL+C, or [count] insns)
- `r [count]` - Alias for run
- `rs [count]` - Resume silent execution (until CTRL+C, or [count] insns)
- `quit` - End the simulation
- `q` - Alias for quit
- `help` - This screen!
- `h` - Alias for help

**Note**: Hitting enter is the same as: `run 1`
- Test Spike cmd demo 
--log-commits 
./spike -d --debug-cmd=/home/ywangmu/Proj/ToolRef/cascade-processorfuzz/shareddata/dbgcmds/debug_cmd_0.txt --pc=2147483648 -l --log=spike_cascade_test_rocket0.log /home/ywangmu/Proj/ToolRef/cascade-processorfuzz/shareddata/manyelfs/rocket_0.elf

./spike -d --debug-cmd=/cascade-meta/fuzzer/mylocaldata/rtl784213_rocket_8904673_34.cmd --pc=2147483648 -l -c --log-commits --log=/cascade-meta/fuzzer/mylocaldata/rtl784213_rocket_8904673_34_csr.log /cascade-meta/fuzzer/mylocaldata/rtl784213_rocket_8904673_34.elf

./spike -d --debug-cmd=../../cascade-processorfuzz/shareddata/mylocaldata/dbgcmds/cmds_trace_regs_at_pc_locs_784213_rocket_8904673_34 --pc=2147483648 -l -c --log-commits --log=/../../cascade-processorfuzz/shareddata/mylocaldata/Interspikeresol784213_rocket_8904673_34_csr.log ../../cascade-processorfuzz/shareddata/mylocaldata/spikeresol784213_rocket_8904673_34.elf

./spike -d --debug-cmd=/cascade-meta/fuzzer/mylocaldata/dbgcmds/cmds_trace_regs_at_pc_locs_784213_rocket_8904673_34 --pc=2147483648 -l --log-commits --log=/cascade-meta/fuzzer/mylocaldata/Interspikeresol784213_rocket_8904673_34_full.log /cascade-meta/fuzzer/mylocaldata/spikeresol784213_rocket_8904673_34.elf

./spike -d --debug-cmd=/cascade-meta/fuzzer/mylocaldata/rtl784213_rocket_8904673_34.cmd --pc=2147483648 -l --log-commits --log=/cascade-meta/fuzzer/mylocaldata/Interspikeresol784213_rocket_8904673_34_all_simple.log /cascade-meta/fuzzer/mylocaldata/spikeresol784213_rocket_8904673_34.elf

./spike -d --pc=2147483648 -l --log-commits --log=/cascade-meta/fuzzer/mylocaldata/Interspikeresol784213_rocket_8904673_34_all_simple.log /cascade-meta/fuzzer/mylocaldata/spikeresol784213_rocket_8904673_34.elf



- Cached Prompt

现在根据这个函数，给我一个take_trap_new函数，可以字符串s中追加打印state. 类似这样的CSR的值以及执行的指令信息