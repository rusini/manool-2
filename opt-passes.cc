// opt-passes.cc -- analysis and transformation passes

/*    Copyright (C) 2020, 2021 Alexey Protasov (AKA Alex or rusini)

   This is free software: you can redistribute it and/or modify it under the terms of the version 3 of the GNU General Public License
   as published by the Free Software Foundation (and only version 3).

   This software is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along with this software.  If not, see <https://www.gnu.org/licenses/>.  */


# include "ir.hh"

namespace rsn::opt {
   using namespace lib;

   bool transform_insn_simplify(proc *tu) { // constant folding (including inlining as a particular case), algebraic simplification, and canonicalization
      bool changed{};
      for (auto bb: lib::all(tu)) for (auto in: lib::all(bb))
         changed |= in->simplify();
      return changed;
   }

   void update_cfg_preds(proc *tu) { // CFG predecessors
      for (auto bb = tu->head(); bb; bb = bb->next()) bb->temp.preds.clear();
      for (auto bb = tu->head(); bb; bb = bb->next()) for (auto &target: bb->rear()->targets())
      if (target->temp.preds.empty() || target->temp.preds.back() != bb)
         target->temp.preds.push_back(bb);
      for (auto bb = tu->head(); bb; bb = bb->next()) bb->temp.preds.shrink_to_fit();
   }

   bool transform_const_propag(proc *tu) { // constant propagation (from mov and beq insns)
      static constexpr auto
      traverse = [](auto traverse, insn *in, vreg *vr) noexcept->operand *{
         for (auto _in = in->prev(); _in; _in = _in->prev()) {
            if (RSN_UNLIKELY(_in->temp.visited)) return vr;
            _in->temp.visited = true;
            if (RSN_UNLIKELY(is<insn_mov>(_in)) && RSN_UNLIKELY(as<insn_mov>(_in)->dest() == vr) && is<imm>(as<insn_mov>(_in)->src()))
               return as<insn_mov>(_in)->src();
            for (auto &output: _in->outputs()) if (RSN_UNLIKELY(output == vr))
               return vr;
         }
         if (RSN_UNLIKELY(in->owner()->temp.preds.empty())) return vr;
         const auto
         _traverse = [traverse, in, &vr](insn *_in) noexcept{
            return RSN_UNLIKELY(is<insn_br>(_in)) && RSN_UNLIKELY(as<insn_br>(_in)->op == insn_br::_beq) && RSN_UNLIKELY(as<insn_br>(_in)->lhs() == vr) &&
               is<imm>(as<insn_br>(_in)->rhs()) && as<insn_br>(_in)->dest2() != in->owner() ? (operand *)as<insn_br>(_in)->rhs() : traverse(traverse, _in, vr);
         };
         auto res = _traverse(range_ref(in->owner()->temp.preds).first()->rear());
         if (RSN_UNLIKELY(is<abs>(res)))
         for (auto bb: range_ref(in->owner()->temp.preds).drop_first()) {
            auto _res = _traverse(bb->rear());
            if (!is<abs>(_res) || as<abs>(_res)->val != as<abs>(res)->val)
               return vr;
         } else
         if (RSN_UNLIKELY(is<proc>(res) || is<data>(res)))
         for (auto bb: range_ref(in->owner()->temp.preds).drop_first()) {
            auto _res = _traverse(bb->rear());
            if (!is<rel_base>(_res) || as<rel_base>(_res)->id != as<rel_base>(res)->id)
               return vr;
         } else
         if (RSN_UNLIKELY(is<rel_base>(res)))
         for (auto bb: range_ref(in->owner()->temp.preds).drop_first()) {
            auto _res = _traverse(bb->rear());
            if (!is<rel_base>(_res) || as<rel_base>(_res)->id != as<rel_base>(res)->id)
               return vr;
            res = std::move(_res);
         } else
            RSN_UNREACHABLE();
         return res;
      };
      bool changed{};
      for (;;) {
         bool _changed{};
         for (auto bb = tu->head(); bb; bb = bb->next()) for (auto in = bb->head(); in; in = in->next())
         for (auto &input: in->inputs()) if (is<vreg>(input)) {
            for (auto bb = tu->head(); bb; bb = bb->next()) for (auto in = bb->head(); in; in = in->next()) in->temp.visited = {};
            auto res = traverse(traverse, in, lib::as<vreg>(input));
            _changed |= res != input, input = std::move(res);
         }
         changed |= _changed;
         if (!RSN_LIKELY(_changed)) break;
      }
      return changed;
   }

   bool transform_copy_propag(proc *tu) { // copy propagation
      static constexpr auto
      traverse = [](auto traverse, insn *in, vreg *vr) noexcept->vreg *{
         for (auto _in = in->prev(); _in; _in = _in->prev()) {
            if (RSN_UNLIKELY(_in->temp.visited)) return vr;
            _in->temp.visited = true;
            if (RSN_UNLIKELY(is<insn_mov>(_in)) && RSN_UNLIKELY(as<insn_mov>(_in)->dest() == vr) && is<vreg>(as<insn_mov>(_in)->src())) {
               for (auto _in2 = _in->next(); _in2 != in; _in2 = _in2->next()) for (auto &output: _in2->outputs())
                  if (RSN_UNLIKELY(output == as<insn_mov>(_in)->src())) return vr;
               return as<vreg>(as<insn_mov>(_in)->src());
            }
            for (auto &output: _in->outputs())
               if (RSN_UNLIKELY(output == vr)) return vr;
         }
         if (RSN_UNLIKELY(in->owner()->temp.preds.empty())) return vr;
         auto res = traverse(traverse, in->owner()->temp.preds.front()->rear(), vr);
         for (auto bb: range_ref(in->owner()->temp.preds).drop_first())
            if (traverse(traverse, in->owner()->rear(), vr) != res) return vr;
         for (auto _in2 = in->next(); _in2 != in; _in2 = _in2->next()) for (auto &output: _in2->outputs())
            if (RSN_UNLIKELY(output == res)) return vr;
         return res;
      };
      bool changed{};
      for (;;) {
         bool _changed{};
         for (auto bb = tu->head(); bb; bb = bb->next()) for (auto in = bb->head(); in; in = in->next())
         for (auto &input: in->inputs()) if (is<vreg>(input)) {
            for (auto bb = tu->head(); bb; bb = bb->next()) for (auto in = bb->head(); in; in = in->next()) in->temp.visited = {};
            auto res = traverse(traverse, in, as<vreg>(input));
            _changed |= res != input, input = std::move(res);
         }
         changed |= _changed;
         if (!RSN_LIKELY(_changed)) break;
      }
      return changed;
   }

   bool transform_dce(proc *tu) { // eliminate instructions whose only effect is to produce dead values
      static constexpr auto
      traverse = [](auto traverse, insn *in, vreg *vr) noexcept{
         for (auto _in = in; _in; _in = _in->next()) {
            if (RSN_UNLIKELY(_in->temp.visited)) return false;
            _in->temp.visited = true;
            for (auto &input: _in->inputs())
               if (RSN_UNLIKELY(input == vr)) return true;
         }
         for (auto &target: in->owner()->rear()->targets())
            if (RSN_UNLIKELY(traverse(traverse, target->head(), vr))) return true;
         return false;
      };
      bool changed{};
      for (auto bb = tu->head(); bb; bb = bb->next()) for (auto in: lib::all(bb->head(), bb->rear())) {
         if (RSN_UNLIKELY(is<insn_call>(in)) || RSN_UNLIKELY(is<insn_entry>(in))) goto next;
         for (auto &output: in->outputs()) {
            for (auto bb = tu->head(); bb; bb = bb->next()) for (auto in = bb->head(); in; in = in->next()) in->temp.visited = {};
            if (traverse(traverse, in->next(), output)) goto next;
         }
         changed = (in->eliminate(), true);
      next:;
      }
      return changed;
   }

   bool transform_cfg_gc(proc *tu) noexcept { // eliminate basic blocks unreachable from the entry basic block
      static constexpr auto traverse = [](auto traverse, bblock *bb) noexcept{
         if (RSN_UNLIKELY(bb->temp.visited)) return;
         bb->temp.visited = true;
         for (auto &target: bb->rear()->targets()) traverse(traverse, target);
      };
      for (auto bb = tu->head(); bb; bb = bb->next()) bb->temp.visited = {};
      traverse(traverse, tu->head());
      bool changed{};
      for (auto bb: lib::all(tu)) if (!RSN_LIKELY(bb->temp.visited))
         changed = (bb->eliminate(), true);
      return changed;
   }

   bool transform_cfg_merge(proc *tu) noexcept {
      bool changed{};
      for (auto bb: all(tu))
      if (RSN_UNLIKELY(bb->temp.preds.size() == 1) && RSN_UNLIKELY(is<insn_jmp>(bb->temp.preds.front()->rear()))) {
         bb->temp.preds.front()->rear()->eliminate();
         for (auto in: all(bb)) in->reattach(bb->temp.preds.front());
         bb->eliminate();
         changed = true;
      }
      return changed;
   }

# if 0
   bool transform_jump_threading(proc *tu) { // only considers the most trivial case for now: jumping or branching to a jumping BB
   }


   bool transform_copypropag(proc *tu) { // copy propagation
      static const auto
      traverse = [](auto traverse, bblock *bb, insn *in, const smart_ptr<vreg> &vr) noexcept->smart_ptr<vreg>{
         for (auto _in = in->prev(); _in; _in = _in->prev()) {
            if (RSN_UNLIKELY(_in->temp.visited)) return vr;
            _in->temp.visited = true;
            if (RSN_UNLIKELY(is<insn_mov>(_in)) && RSN_UNLIKELY(as<insn_mov>(_in)->dest() == vr) && is<vreg>(as<insn_mov>(_in)->src())) {
               for (auto _in2 = _in->next(); _in2 != in; _in2 = _in2->next()) for (auto &output: _in2->outputs())
                  if (RSN_UNLIKELY(output == as<insn_mov>(_in)->src())) return vr;
               return as<insn_mov>(_in)->src();
            }
            for (auto &output: _in->outputs())
               if (RSN_UNLIKELY(output == vr)) return vr;
         }
         if (RSN_UNLIKELY(in->owner()->temp.preds.empty())) return vr;
         auto res = traverse(traverse, bb->temp.preds.front(), bb->temp.preds.front()->rear(), vr);
         for (auto bb: range_ref(in->owner()->temp.preds).drop_first())
            if (traverse(traverse, bb, bb->rear(), vr) != res) return vr;
         for (auto _in2 = _in->next(); _in2 != in; _in2 = _in2->next()) for (auto &output: _in2->outputs())
            if (RSN_UNLIKELY(output == res)) return vr;
         return res;
      };
      bool changed{};
      for (;;) {
         bool _changed{};
         for (auto bb = tu->head(); bb; bb = bb->next()) for (auto in = bb->head(); in; in = in->next())
         for (auto &input: in->inputs()) if (is<vreg>(input)) {
            for (auto bb = tu->head(); bb; bb = bb->next()) for (auto in = bb->head(); in; in = in->next()) in->temp.visited = {};
            auto res = traverse(traverse, bb, in, as_smart<vreg>(input));
            _changed |= res != input, input = std::move(res);
         }
         changed |= _changed;
         if (!RSN_LIKELY(_changed)) break;
      }
      return changed;
   }

#endif

}
