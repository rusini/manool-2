// main.cc

# include "ir.hh"


int main() {
   namespace opt = rsn::opt;

   enum { op_add, op_mul, op_cmpeq };

   auto p_int  = opt::proc::make    ({0, 123llu << 48}); // Integer datatype
   auto p_bool = opt::rel_base::make({0, 456llu << 48}); // Boolean datatype
   auto p_oops = opt::rel_base::make({0, 999llu << 48}); // signaling exceptions

   {  auto p = p_int;
      auto b0 = opt::bblock::make(p), b1 = opt::bblock::make(p), b2 = opt::bblock::make(p),
         b_add_op = opt::bblock::make(p), b_add_ret = opt::bblock::make(p),
         b_mul_op = opt::bblock::make(p), b_mul_ret = opt::bblock::make(p),
         b_cmpeq_op = opt::bblock::make(p), b_cmpeq_ret = opt::bblock::make(p),
         b_cmpeq_ret1 = opt::bblock::make(p), b_cmpeq_ret0 = opt::bblock::make(p),
         b_oops = opt::bblock::make(p);
      auto r_op = opt::reg::make(), r_lhs_v = opt::reg::make(), r_rhs_t = opt::reg::make(), r_rhs_v = opt::reg::make();

      opt::insn_entry::make(b0, {r_op, r_lhs_v, r_rhs_t, r_rhs_v});

      opt::insn_br::make_beq(b0, r_op, opt::abs::make(op_add), b_add_op, b1);
      opt::insn_br::make_beq(b1, r_op, opt::abs::make(op_mul), b_mul_op, b2);
      opt::insn_br::make_beq(b2, r_op, opt::abs::make(op_cmpeq), b_cmpeq_op, b_oops);

      opt::insn_br::make_beq(b_add_op, r_rhs_t, p_int, b_add_ret, b_oops);
      opt::insn_binop::make_add(b_add_ret, r_lhs_v, r_lhs_v, r_rhs_v);
      opt::insn_ret::make(b_add_ret, {p_int, r_lhs_v});

      opt::insn_br::make_beq(b_mul_op, r_rhs_t, p_int, b_mul_ret, b_oops);
      opt::insn_binop::make_umul(b_mul_ret, r_lhs_v, r_lhs_v, r_rhs_v);
      opt::insn_ret::make(b_mul_ret, {p_int, r_lhs_v});

      opt::insn_br::make_beq(b_cmpeq_op, r_rhs_t, p_int, b_mul_ret, b_oops);
      opt::insn_br::make_beq(b_cmpeq_ret, r_lhs_v, r_rhs_v, b_cmpeq_ret1, b_cmpeq_ret0);
      opt::insn_ret::make(b_cmpeq_ret1, {p_bool, opt::abs::make(true)});
      opt::insn_ret::make(b_cmpeq_ret0, {p_bool, opt::abs::make(false)});

      opt::insn_call::make(b_oops, {}, p_oops, {});
      opt::insn_oops::make(b_oops);

      p->dump();
   }

   auto p_fact = opt::proc::make({0, 1 << 48}); // iterative version of Factorial
   {  auto p = p_fact;
      auto b0 = opt::bblock::make(p), b1 = opt::bblock::make(p), b2 = opt::bblock::make(p), b3 = opt::bblock::make(p),
         b4 = opt::bblock::make(p), b5 = opt::bblock::make(p), b_oops = opt::bblock::make(p);
      auto r_n_t = opt::reg::make(), r_n_v = opt::reg::make();
      auto r_res_t = opt::reg::make(), r_res_v = opt::reg::make();
      auto r_tmp_t = opt::reg::make(), r_tmp_v = opt::reg::make();

      opt::insn_entry::make(b0, {r_n_t, r_n_v});
      opt::insn_br::make_beq(b0, r_n_t, p_int, b1, b_oops);

      opt::insn_mov::make(b1, r_res_t, p_int);
      opt::insn_mov::make(b1, r_res_v, opt::abs::make(1));
      opt::insn_jmp::make(b1, b2);

      opt::insn_call::make(b2, {r_tmp_t, r_tmp_v}, r_n_t, {opt::abs::make(1), r_n_t, r_n_v, p_int, 0});
      opt::insn_br::make_beq(b2, r_tmp_t, p_bool, b3, b_oops);

      opt::insn_br::make_bne(b3, r_tmp_v, opt::abs::make(0), b4, b5);

      opt::insn_call::make(b4, {r_res_t, r_res_v}, r_res_t, {opt::abs::make(2), r_res_t, r_res_v, r_n_t, r_n_v});
      opt::insn_call::make(b4, {r_n_t, r_n_v}, r_n_t, {opt::abs::make(3), r_n_t, r_n_v, p_int, opt::abs::make(1)});
      opt::insn_jmp::make(b4, b2);

      opt::insn_ret::make(b5, {r_res_t, r_res_v});

      opt::insn_call::make(b_oops, {}, p_oops, {});
      opt::insn_oops::make(b_oops);

      p->dump();
   }

   return {};
}
