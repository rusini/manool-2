*Dear community,*

I have started a new open-source project: **Compiler Optimizations Playground**: <https://github.com/rusini/manool-2>

This is (hopefully) the simplest implementation (under 1 KLOC in modern C++) of the classic register-machine intermediate representation (IR) to undertake data
and control flow analysis in a compiler middle-end (and inadvertently, the underlying infrastructure can be reused even on the code-generation stage, in a
back-end, as a basis for a "machine IR" in LLVM terminology, due to its open architecture).

#### Project purpose

* For people who wish to expand their knowledge by playing around with and implementing various compiler analysis and transformation passes -- this is a classic
  IR infrastructure with a more accessible learning curve compared to LLVM and a simple, tiny code base.

* For people who look for a middle-end/back-end to implement an optimizing compiler for their programming languages -- eventually they will be able to fork and
  adapt the code base due to its simplicity (I only expect x86-64 and possibly aarch64 ISAs to be covered, in contrast to LLVM).

* For myself -- this IR infrastructure is a (hopefully) more flexible alternative to LLVM that I need to implement an optimizing compiler for my programming
  language MANOOL-2 (which requires static code analysis and optimizations, but a non-classic, just-in-time compilation scheme).

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

    auto pr = opt::proc::make({}); // iterative version of Factorial

    auto r_arg  = opt::vreg::make(pr),
         r_res  = opt::vreg::make(pr),
         r_cond = opt::vreg::make(pr);
    auto bb1 = opt::bblock::make(pr),
         bb2 = opt::bblock::make(pr),
         bb3 = opt::bblock::make(pr),
         bb4 = opt::bblock::make(pr);

    opt::insn_entry::make(bb1, {r_arg});
    opt::insn_mov::make(bb1, r_res, opt::abs_imm::make(1));
    opt::insn_jmp::make(bb1, bb2);

    opt::insn_binop::make_cmpne(bb2, r_cond, r_arg, opt::abs_imm::make(0));
    opt::insn_cond_jmp::make(bb2, r_cond, bb3, bb4);

    opt::insn_binop::make_umul(bb3, r_res, r_res, r_arg);
    opt::insn_binop::make_sub(bb3, r_arg, r_arg, opt::abs_imm::make(1));
    opt::insn_jmp::make(bb3, bb2);

    opt::insn_ret::make(bb4, {r_res});

    pr->dump();

#### Building the code in the repository

    clang++ -w -std=c++17 -{O3,s} -DRSN_USE_DEBUG {main,ir0,ir}.cc

On running, it displays an IR dump (or a number of them) on the standard error/log output.

#### Further observations

* For the most part, all IR nodes allow for in-place updates and can be referred to by using (raw) pointers, so using (hash-based or order-based) sets of nodes,
  tuples of nodes, or mappings with nodes or tuples of nodes as keys is quite efficient.

* Alternatively, there are also places (class members) to inject temporary data in an intrussive manner directly into IR nodes. Some instruction-kind-dependent
  optimizations are also expected to have implementations injected into the IR infrastructure description, namely constant folding (including inlining for a
  single call site as a particular case of constant folding).

* All constant-folding logic is in fact already implemented. In general, analysis and transformation passes should be implemented apart, though.

* You can traverse the IR hierarchy in any direction, including bottom-up ("slots" for data operands and jump targets are not considered to be IR nodes, and
  this would be an overengineering).

* The IR instruction set is actually user-extensible via subclassing in C++ (but you can examine any kind of instructions by examining its output/input operands
  and/or jump targets in a general way).

---

This code is meant to constitute a playground, not a complete open-source product or even a library like LLVM, where otherwise most code would be typically
contributed by a few project leaders -- I invite everyone (compiler technology enthusiasts and language designers) to play around with it and share your
findings. I have experience with working on commercial compiler projects, but my experience with actually implementing advanced compiler optimizations is still
limited...

Thank you,

*Alex Rusini* -- <rusini@manool.org>, <https://manool.org>
