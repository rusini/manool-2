// main.cc

# include "ir.hh"


namespace rsn::opt {

   void update_cfg_preds(proc *pr) {
      for (auto bb: all(pr))
         bb->temp.preds.clear();
      for (auto bb: all(pr)) for (auto targets: bb->tail()->targets())
         (*targets)->temp.preds.insert(bb);
   }

   bool transform_cpropag(proc *pr) {
      static constexpr auto
      traverse = [](auto traverse, insn *in, const smart_ptr<vreg> &vr, bool skip = {}) noexcept->smart_ptr<operand>{
         for (auto _in: skip ? rev_after(in) : rev_from(in)) {
            if (RSN_UNLIKELY(_in->temp.visited)) return vr;
            _in->temp.visited = true;
            if (RSN_UNLIKELY(is<insn_mov>(_in)) && RSN_UNLIKELY(as<insn_mov>(_in)->dest == vr) && is<imm_val>(as<insn_mov>(_in)->src))
               return as<insn_mov>(_in)->src;
            for (auto output: _in->outputs()) if (RSN_UNLIKELY(*output == vr))
               return vr;
         }
         if (in->owner()->temp.preds.empty()) return vr;
         auto res = traverse(traverse, (*in->owner()->temp.preds.begin())->tail(), vr);
         if (is<abs_imm>(res))
         for (auto it = in->owner()->temp.preds.begin(); ++it != in->owner()->temp.preds.end();) {
            auto temp = traverse(traverse, (*it)->tail(), smart_ptr<vreg>(vr));
            if (!is<abs_imm>(temp) || as<abs_imm>(temp)->value != as<abs_imm>(res)->value)
               return vr;
         } else
         if (is<proc>(res) || is<data>(res))
         for (auto it = in->owner()->temp.preds.begin(); ++it != in->owner()->temp.preds.end();) {
            auto temp = traverse(traverse, (*it)->tail(), smart_ptr<vreg>(vr));
            if (!is<rel_imm>(temp) || as<rel_imm>(temp)->id != as<rel_imm>(res)->id)
               return vr;
         } else
         if (is<rel_imm>(res))
         for (auto it = in->owner()->temp.preds.begin(); ++it != in->owner()->temp.preds.end();) {
            auto temp = traverse(traverse, (*it)->tail(), smart_ptr<vreg>(vr));
            if (!is<rel_imm>(temp) || as<rel_imm>(temp)->id != as<rel_imm>(res)->id)
               return vr;
            res = std::move(temp);
         } else
            RSN_UNREACHABLE();
         return std::move(res);
      };
      bool changed{};
      for (;;) {
         bool _changed{};
         for (auto bb: all(pr)) for (auto in: all(bb))
         for (auto input: in->inputs()) if (is<vreg>(*input)) {
            for (auto bb: all(pr)) for (auto in: all(bb)) in->temp.visited = {};
            auto res = traverse(traverse, in, as_smart<vreg>(*input), true);
            _changed |= res != *input, *input = std::move(res);
         }
         changed |= _changed;
         if (!RSN_LIKELY(_changed)) break;
      }
      return changed;
   }

   bool transform_cfold(proc *pr) {
      bool changed{};
      for (auto bb: all(pr)) for (auto in: all(bb))
         changed |= in->try_to_fold();
      return changed;
   }

   bool transform_dse(proc *pr) {
      static constexpr auto
      traverse = [](auto traverse, insn *in, vreg *vr, bool skip = {}) noexcept->bool{
         for (auto _in: skip ? all_after(in) : all_from(in)) {
            if (RSN_UNLIKELY(_in->temp.visited)) return false;
            _in->temp.visited = true;
            for (auto input: _in->inputs()) if (RSN_UNLIKELY(*input == vr))
               return true;
         }
         for (auto target: in->owner()->tail()->targets())
            if (traverse(traverse, (*target)->head(), vr)) return true;
         return false;
      };
      bool changed{};
      for (auto bb: all(pr)) for (auto in: all(bb)) {
         if (RSN_UNLIKELY(is<insn_entry>(in)) || RSN_UNLIKELY(is<insn_call>(in)) || RSN_UNLIKELY(in == bb->tail())) goto next_insn;
         for (auto output: in->outputs()) {
            for (auto bb: all(pr)) for (auto in: all(bb)) in->temp.visited = {};
            if (traverse(traverse, in, *output, true)) goto next_insn;
         }
         in->remove(), changed = true;
      next_insn:;
      }
      return changed;
   }

   bool transform_cfg_merge(proc *pr) noexcept {
      for (auto bb: all(pr)) if (RSN_UNLIKELY(bb->temp.preds.size() == 1)) {
         (*bb->temp.preds.begin())->remove();
         for (auto in: all(bb)) in->reattach(*bb->temp.preds.begin());
         bb->remove();
      }
   }

}

int main() {
   namespace opt = rsn::opt;

   auto pr_int = opt::proc::make({123456, 1});
   {  auto bb1 = opt::bblock::make(pr_int), bb2 = opt::bblock::make(pr_int), bb3 = opt::bblock::make(pr_int);
      auto r_lhs_t = opt::vreg::make(pr_int), r_lhs_v = opt::vreg::make(pr_int);
      auto r_rhs_t = opt::vreg::make(pr_int), r_rhs_v = opt::vreg::make(pr_int);
      auto r_cond  = opt::vreg::make(pr_int);

      opt::insn_entry::make(bb1, {r_lhs_t, r_lhs_v, r_rhs_t, r_rhs_v});
      opt::insn_binop::make_cmpeq(bb1, r_cond, r_rhs_t, pr_int});
      opt::insn_cond_jmp::make(bb1, r_cond, bb2, bb3);

      opt::insn_binop::make_add(bb2, r_lhs_v, r_lhs_v, r_rhs_v});
      opt::insn_ret::make(bb2, {r_lhs_t, r_lhs_v});

      opt::insn_undefined::make(bb3);
   }


   auto pr = opt::proc::make({123, 456}); // iterative version of Factorial
   auto r1 = opt::vreg::make(pr), r2 = opt::vreg::make(pr), r3 = opt::vreg::make(pr);
   auto bb1 = opt::bblock::make(pr)/*, bb2 = opt::bblock::make(pr), bb3 = opt::bblock::make(pr), bb4 = opt::bblock::make(pr)*/;

   opt::insn_entry::make(bb1, {});
   opt::insn_mov::make(bb1, r1, opt::abs_imm::make(123));
   opt::insn_mov::make(bb1, r2, opt::abs_imm::make(456));
   opt::insn_binop::make_add(bb1, r3, r1, r2);
   opt::insn_ret::make(bb1, {r3});

   pr->dump();
   opt::update_cfg_preds(pr);
   while (opt::transform_cfold(pr) || opt::transform_cpropag(pr));
   opt::transform_dse(pr);
   opt::transform_cfg_merge(pr);
   pr->dump();

   return 0;
}
