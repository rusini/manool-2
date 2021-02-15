// main.cc

# include "ir.hh"

namespace rsn::opt {
   bool transform_to_ssa(proc *);
}

int main() {
   namespace opt = rsn::opt;
   using opt::is, opt::as, opt::as_smart;

   auto pc = opt::proc::make({1, 0}); // iterative version of Factorial
   auto r_arg = opt::vreg::make(), r_res = opt::vreg::make();
   auto b0 = opt::bblock::make(pc), b1 = opt::bblock::make(pc), b2 = opt::bblock::make(pc), b3 = opt::bblock::make(pc);

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

   return {};
}
