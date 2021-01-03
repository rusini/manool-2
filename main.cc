// main.cc

# include "ir.hh"


namespace rsn::opt {

   void update_cfg_preds(proc *pr) {
      for (auto bb: all(pr))
         bb->temp.preds.clear();
      for (auto bb: all(pr)) for (auto targets: bb->tail()->targets())
         (*targets)->temp.preds.insert(bb);
   }

   smart_ptr<operand> propag(bblock *bb, insn *in, const smart_ptr<vreg> &vr) {
      if (in) for (auto _in: rev_from(in)) {
         if (RSN_UNLIKELY(_in->temp.visited)) return vr;
         _in->temp.visited = true;
         if (RSN_UNLIKELY(is<insn_mov>(_in)) && RSN_UNLIKELY(as<insn_mov>(_in)->dest == vr) && is<imm_val>(as<insn_mov>(_in)->src))
            return as<insn_mov>(_in)->src;
         for (auto output: _in->outputs()) if (RSN_UNLIKELY(*output == vr))
            return vr;
      }
      if (bb->temp.preds.empty()) return vr;
      auto res = propag(*bb->temp.preds.begin(), (*bb->temp.preds.begin())->tail(), vr);
      if (is<abs_imm>(res))
      for (auto it = bb->temp.preds.begin(); ++it != bb->temp.preds.end();) {
         auto temp = propag(*it, (*it)->tail(), smart_ptr<vreg>(vr));
         if (!is<abs_imm>(temp) || as<abs_imm>(temp)->value != as<abs_imm>(res)->value)
            return vr;
      } else
      if (is<proc>(res) || is<data>(res))
      for (auto it = bb->temp.preds.begin(); ++it != bb->temp.preds.end();) {
         auto temp = propag(*it, (*it)->tail(), smart_ptr<vreg>(vr));
         if (!is<rel_imm>(temp) || as<rel_imm>(temp)->id != as<rel_imm>(res)->id)
            return vr;
      } else
      if (is<rel_imm>(res))
      for (auto it = bb->temp.preds.begin(); ++it != bb->temp.preds.end();) {
         auto temp = propag(*it, (*it)->tail(), smart_ptr<vreg>(vr));
         if (!is<rel_imm>(temp) || as<rel_imm>(temp)->id != as<rel_imm>(res)->id)
            return vr;
         res = std::move(temp);
      } else
         RSN_UNREACHABLE();
      return std::move(res);
   };

   bool transform_cpropag(proc *pr) {
      bool changed{}, _changed;
      do {
         _changed = {};
         for (auto bb: all(pr)) for (auto in: all(bb))
         for (auto input: in->inputs()) if (is<vreg>(*input)) {
            auto res = propag(in->owner(), in->prev(), as_smart<vreg>(*input));
            for (auto bb: all(pr)) for (auto in: all(bb)) in->temp.visited = false;
            _changed |= res != *input, *input = std::move(res);
         }
         changed |= _changed;
      } while (_changed);
      return changed;
   }

   bool transform_cfold(proc *pr) {
      bool changed = false;
      for (auto bb: all(pr)) for (auto in: all(bb))
         if (in->try_to_fold()) changed = true;
      return changed;
   }

}

int main() {
   namespace opt = rsn::opt;
   using opt::is, opt::as, opt::as_smart;

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
   pr->dump();

   return 0;
}
