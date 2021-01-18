// main.cc

# include "ir.hh"


int main() {
   namespace opt = rsn::opt;

   auto pr_int = opt::proc::make({123456, 1001});
   {  auto &&pr = pr_int;
      auto bb1 = opt::bblock::make(pr), bb2 = opt::bblock::make(pr), bb3 = opt::bblock::make(pr);
      auto r_lhs_t = opt::vreg::make(pr), r_lhs_v = opt::vreg::make(pr);
      auto r_rhs_t = opt::vreg::make(pr), r_rhs_v = opt::vreg::make(pr);
      auto r_cond  = opt::vreg::make(pr);

      opt::insn_entry::make(bb1, {r_lhs_t, r_lhs_v, r_rhs_t, r_rhs_v});
      opt::insn_binop::make_cmpeq(bb1, r_cond, r_rhs_t, pr_int);
      opt::insn_cond_jmp::make(bb1, r_cond, bb2, bb3);

      opt::insn_binop::make_add(bb2, r_lhs_v, r_lhs_v, r_rhs_v);
      opt::insn_ret::make(bb2, {r_lhs_t, r_lhs_v});

      opt::insn_undefined::make(bb3);
   }

   auto pr_fact = opt::proc::make({123456, 1002}); // iterative version of Factorial
   {  auto &&pr = pr_fact;
      auto bb1 = opt::bblock::make(pr), bb2 = opt::bblock::make(pr), bb3 = opt::bblock::make(pr), bb4 = opt::bblock::make(pr), bb5 = opt::bblock::make(pr);
      auto r_arg_t = opt::vreg::make(pr), r_arg_v = opt::vreg::make(pr);
      auto r_res_t = opt::vreg::make(pr), r_res_v = opt::vreg::make(pr);
      auto r_cond  = opt::vreg::make(pr);

      opt::insn_entry::make(bb1, {r_arg_t, r_arg_v});
      opt::insn_binop::make_cmpeq(bb1, r_cond, r_arg_t, pr_int);
      opt::insn_cond_jmp::make(bb1, r_cond, bb2, bb5);

      opt::insn_binop::make_cmpne(bb2, r_cond, r_arg_v, opt::abs_imm::make(0));
      opt::insn_cond_jmp::make(bb2, r_cond, bb3, bb4);

      opt::insn_binop::make_umul(bb3, r_res_v, r_res_v, r_arg_v);
      opt::insn_binop::make_sub(bb3, r_arg_v, r_arg_v, opt::abs_imm::make(1));
      opt::insn_jmp::make(bb3, bb2);

      opt::insn_ret::make(bb4, {r_res_t, r_res_v});

      opt::insn_undefined::make(bb5);
   }

   pr_int->dump();
   pr_fact->dump();

   return 0;
}
