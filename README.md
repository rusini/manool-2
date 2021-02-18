Compiler Optimizations Playground
=================================

This is (hopefully) the simplest implementation of the classic register-machine intermediate representation (IR) to undertake data and control flow analysis in
a compiler middle-end.

#### Project purpose

* For people who wish to expand their knowledge by playing around with and implementing various compiler analysis and transformation passes -- this is a classic
  IR infrastructure with a more accessible learning curve compared to LLVM and a simple, small code base.

* For people who look for a middle-end/back-end to implement an optimizing compiler for their programming languages -- eventually they will be able to fork and
  adapt the code base due to its simplicity (I only expect x86-64 and possibly aarch64 ISAs to be covered, in contrast to LLVM).

* For myself -- this IR infrastructure is a (hopefully) more flexible alternative to LLVM that I need to implement an optimizing compiler for my programming
  language MANOOL-2 (which requires static code analysis and optimizations, but a non-classic, just-in-time compilation scheme).

#### Changes from the first release

* A (rather general-purpose) support library is now officially endorsed (stuff under the namespace `::rsn::lib`).

* IR operand and instruction representation were optimized for fast access to operands and RTTI (even when that involves unusual programming techniques from the
  standpoint of common C++ programming practices).

* Branch instructions now involve a comparison operation (which might be more practical and/or convenient for the constant propagation pass).

* Output operands now appear after input operands in the dumps (after the `->` sign) and normally appear after input operands in the source code (arguably, this
  is more consistent than the previous approach of using the `:=` operator, for a number of reasons).

* Transient data are now associated with IR nodes indirectly, via a transient index, for a number of practical reasons.

* And finally: transformation to static single assignment (SSA) form is implemented.

#### Most important differences from LLVM

* Static typing at the IR level is not used.

* Virtual registers are supposed to be just unaliased variables -- an SSA form is expected by only some analysis/transformation passes but not others (and each
  pass can have its own preconditions, generally unexpressible, however, in terms of C++ static types for IR nodes).

* As a related characteristic: there is no "pass manager" -- pass ordering is supposed to be completely manual.

* Some IR instructions may have more than one output operands (including procedure call instructions).

* Virtual registers are supposed to have "infinite width" -- meaning that every register is capable to transfer any information produced by an IR instruction
  (e.g., 32/64-bit values, SIMD vectors, etc.).

* A translation unit is a single procedure, not a module as a collection of other stuff.

* As a related characteristic: ownership of some IR nodes (data operands, including procedures with a body available for inlining/IPO) is managed via reference
  counting, whereas for other IR nodes (basic blocks and instructions) there is a single owner.

* Link-time symbols are expressed as 128-bit hash values (identifying the pointed-to contents), instead of arbitrary names.

#### Sample piece of code using the API

    auto pc = opt::proc::make({1, 0}); // iterative version of Factorial
    auto r_arg = opt::vreg::make(), r_res = opt::vreg::make();
    auto b0 = opt::bblock::make(pc),
         b1 = opt::bblock::make(pc),
         b2 = opt::bblock::make(pc),
         b3 = opt::bblock::make(pc);

    opt::insn_entry::make(b0, {r_arg});
    opt::insn_mov::make(b0, opt::abs::make(1), r_res);
    opt::insn_jmp::make(b0, b1);

    opt::insn_br::make_bne(b1, r_arg, opt::abs::make(0), b2, b3);

    opt::insn_binop::make_umul(b2, r_res, r_arg, r_res);
    opt::insn_binop::make_sub(b2, r_arg, opt::abs::make(1), r_arg);
    opt::insn_jmp::make(b2, b1);

    opt::insn_ret::make(b3, {r_res});

    pc->dump();
    transform_to_ssa(pc);
    pc->dump();

#### Building the code in the repository

    clang++ -w -std=c++17 -{O3,s} -DRSN_USE_DEBUG {main,ir0,opt-simplify,ssa}.cc

On running, it displays an IR dump (or a number of them) on the standard error/log output. For instance:

    P3 = proc $0x00000001[0x00000000000000000000000000000001] as
    L6:
        entry -> R4
        mov N11#1[0x1] -> R5
        jmp to L7
    L7:
        beq R4, N14#0[0x0] to L9, L8
    L8:
        umul R5, R4 -> R5
        sub R4, N17#1[0x1] -> R4
        jmp to L7
    L9:
        ret R5
    end proc P3

    P3 = proc $0x00000001[0x00000000000000000000000000000001] as
    L6:
        entry -> R23
        mov N11#1[0x1] -> R24
        jmp to L7
    L7:
        phi R23, R28 -> R25
        phi R24, R27 -> R26
        beq R25, N14#0[0x0] to L9, L8
    L8:
        umul R26, R25 -> R27
        sub R25, N17#1[0x1] -> R28
        jmp to L7
    L9:
        ret R26
    end proc P3

---

*Alex Rusini* -- <mailto:rusini@manool.org>, <https://manool.org>  
*Compiler Optimizations Playground* -- <https://github.com/rusini/manool-2>
