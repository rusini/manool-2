// main.cc

# include "ir.hh"

int main() {
   namespace opt = rsn::opt;
   using opt::is, opt::as, opt::as_smart;

   auto pr = opt::proc::make({123, 456}); // iterative version of Factorial
   auto r_arg = opt::vreg::make(pr), r_res = opt::vreg::make(pr), r_cond = opt::vreg::make(pr);
   auto bb1 = opt::bblock::make(pr), bb2 = opt::bblock::make(pr), bb3 = opt::bblock::make(pr), bb4 = opt::bblock::make(pr);

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

   auto pr_test = opt::proc::make({123, 456});
   {  auto bb_test = opt::bblock::make(pr_test);
      auto r_test = opt::vreg::make(pr_test);
      opt::insn_entry::make(bb_test, {});
      auto in = opt::insn_call::make(bb_test, {r_test}, pr, {opt::abs_imm::make(20)});
      opt::insn_ret::make(bb_test, {});
   }
   pr_test->dump();
   for (auto bb: all(pr_test)) for (auto in: all(bb)) in->try_to_fold();
   pr_test->dump();
   return 0;
}
