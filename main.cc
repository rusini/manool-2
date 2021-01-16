// main.cc

# include "ir.hh"


namespace rsn::opt {
   using namespace lib;

   void update_cfg_preds(proc *pr) {
      for (auto bb: all(pr)) bb->temp.preds.clear();
      for (auto bb: all(pr)) for (auto &target: bb->back()->targets())
      if (target->temp.preds.empty() || target->temp.preds.back() != bb)
         target->temp.preds.push_back(bb);
      for (auto bb: all(pr)) bb->temp.preds.shrink_to_fit();
   }

   bool transform_cpropag(proc *pr) { // constant propagation (from mov and beq insns)
      using lib::is, lib::as;
      static const auto
      traverse = [](auto traverse, insn *in, const smart_ptr<vreg> &vr) noexcept->smart_ptr<operand>{
         for (auto _in = in->prev(); _in; _in = _in->prev()) {
            if (RSN_UNLIKELY(_in->temp.visited)) return vr;
            _in->temp.visited = true;
            if (RSN_UNLIKELY(is<insn_mov>(_in)) && RSN_UNLIKELY(as<insn_mov>(_in)->dest() == vr) && is<imm_val>(as<insn_mov>(_in)->src()))
               return as<insn_mov>(_in)->src();
            for (auto &output: _in->outputs()) if (RSN_UNLIKELY(output == vr))
               return vr;
         }
         if (RSN_UNLIKELY(in->owner()->temp.preds.empty())) return vr;
         static const auto
         _traverse = [traverse, in, &vr](insn *_in) noexcept{
            return RSN_UNLIKELY(is<insn_br>(_in)) && RSN_UNLIKELY(as<insn_br>(_in)->op == insn_br::_beq) && RSN_UNLIKELY(as<insn_br>(_in)->lhs() == vr) &&
               is<imm_val>(as<insn_br>(_in)->rhs()) && as<insn_br>(_in)->dest2() != in->owner() ? as<insn_br>(_in)->rhs()) : traverse(traverse, _in, vr);
         };
         auto res = _traverse(range_ref(in->owner()->temp.preds).first()->back());
         if (RSN_UNLIKELY(is<abs_imm>(res)))
         for (auto bb: range_ref(in->owner()->temp.preds).drop_first()) {
            auto _res = _traverse(bb->back());
            if (!is<abs_imm>(_res) || as<abs_imm>(_res)->value != as<abs_imm>(res)->value)
               return vr;
         } else
         if (RSN_UNLIKELY(is<proc>(res) || is<data>(res)))
         for (auto bb: range_ref(in->owner()->temp.preds).drop_first()) {
            auto _res = _traverse(bb->back());
            if (!is<rel_imm>(_res) || as<rel_imm>(_res)->id != as<rel_imm>(res)->id)
               return vr;
         } else
         if (RSN_UNLIKELY(is<rel_imm>(res)))
         for (auto bb: range_ref(in->owner()->temp.preds).drop_first()) {
            auto _res = _traverse(bb->back());
            if (!is<rel_imm>(_res) || as<rel_imm>(_res)->id != as<rel_imm>(res)->id)
               return vr;
            res = std::move(_res);
         } else
            RSN_UNREACHABLE();
         return res;
      };
      bool changed{};
      for (;;) {
         bool _changed{};
         for (auto bb: all(pr)) for (auto in: all(bb))
         for (auto &input: in->inputs()) if (is<vreg>(input)) {
            for (auto bb: all(pr)) for (auto in: all(bb)) in->temp.visited = {};
            auto res = traverse(traverse, in, lib::as_smart<vreg>(input));
            _changed |= res != input, input = std::move(res);
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


   template<typename It> struct range {
      It _begin, _end;
      template<typename Range> range(Range range): _begin(range.begin()), _end(range.end()) {}
      template<typename Range> range &operator=(Range range) { _begin = range.begin(), _end = range.end(); }
      range(It _begin, It _end): _begin(_begin), _end(_end) {}
      template<typename It_> range(It_ _begin, It_ _end): _begin(_begin), _end(_end) {}

      It begin() const noexcept { return _begin; }
      It end() const noexcept { return _end; }
   };

}

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
