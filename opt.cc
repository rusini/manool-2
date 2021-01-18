// opt.cc -- analysis and transformation passes

/*    Copyright (C) 2020 Alexey Protasov (AKA Alex or rusini)

   This is free software: you can redistribute it and/or modify it under the terms of the version 3 of the GNU General Public License
   as published by the Free Software Foundation (and only version 3).

   This software is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along with MANOOL.  If not, see <https://www.gnu.org/licenses/>.  */


# include "opt.hh"

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

   bool transform_dce(proc *tu) { // eliminate instructions whose only effect is to produce dead values
      static constexpr auto
      traverse = [](auto traverse, bblock *bb, insn *in, vreg *vr) noexcept{
         for (auto _in = in; _in; _in = _in->next()) {
            if (RSN_UNLIKELY(_in->temp.visited)) return false;
            _in->temp.visited = true;
            for (auto &input: _in->inputs())
               if (RSN_UNLIKELY(input == vr)) return true;
         }
         for (auto &target: bb->back()->targets())
            if (RSN_UNLIKELY(traverse(traverse, target, target->front(), vr))) return true;
         return false;
      };
      bool changed{};
      for (auto bb = tu->front(); bb; bb = bb->next()) for (auto in: all(bb->front(); bb->back())) {
         if (RSN_UNLIKELY(is<insn_call>(in)) || RSN_UNLIKELY(is<insn_entry>(in))) goto next;
         for (auto &output: in->outputs()) {
            for (auto bb = tu->front(); bb; bb = bb->next()) for (auto in = bb->front(); in; in = in->next()) in->temp.visited = {};
            if (traverse(traverse, bb, in->next(), output)) goto next;
         }
         in->remove(), changed = true;
      next:;
      }
      return changed;
   }

   bool transform_cfg_merge(proc *tu) noexcept {
      bool changed{};
      for (auto bb: all(tu))
      if (RSN_UNLIKELY(bb->temp.preds.size() == 1) && RSN_UNLIKELY(is<insn_jmp>(bb->temp.preds.front()->back()))) {
         bb->temp.preds.front()->back()->remove();
         for (auto in: all(bb)) in->reattach(bb->temp.preds.front());
         bb->remove();
         changed = true;
      }
      return changed;
   }

   bool transform_cfg_gc(proc *tu) noexcept { // eliminate basic blocks unreachable from the entry basic block
      static const auto traverse = [](auto traverse, bblock *bb) noexcept{
         if (RSN_UNLIKELY(bb->temp.visited)) return;
         bb->temp.visited = true;
         for (auto &target: bb->back()->targets()) traverse(traverse, target);
      };
      for (auto bb: all(tu)) bb->temp.visited = {};
      traverse(traverse, tu->front());
      bool changed{};
      for (auto bb: all(tu)) if (!RSN_LIKELY(bb->temp.visited))
         bb->remove(), changed = true;
      return changed;
   }

   bool transform_cfg_gc(proc *tu) noexcept { // eliminate basic blocks unreachable from the entry basic block
      static const auto traverse = [](auto traverse, bblock *bb) noexcept{
         if (RSN_UNLIKELY(bb->temp.visited)) return;
         bb->temp.visited = true;
         for (auto &target: bb->back()->targets()) traverse(traverse, target);
      };
      for (auto bb = tu->front(); bb; bb = bb->next()) bb->temp.visited = {};
      traverse(traverse, tu->front());
      bool changed{};
      for (auto bb: all(tu->front()) if (!RSN_LIKELY(bb->temp.visited))
         bb->remove(), changed = true;
      return changed;
   }

   template<typename Obj> RSN_NOINLINE auto all(Obj *begin, Obj *end = {})->std::enable_if_t<std::is_base_of_v<aux::node, Obj>, lib::small_vec<Obj *>> {
      std::size_t size = 0; for (auto node = begin; node != end; node = node->next()) ++size;
      {  lib::small_vec<Obj *> res(size); for (auto node = begin; node != end; node = node->next()) res.push_back(node);
         return res;
      }
   }
   template<typename Obj> RSN_NOINLINE auto rev(Obj *begin, Obj *end = {})->std::enable_if_t<std::is_base_of_v<aux::node, Obj>, lib::small_vec<Obj *>> {
      std::size_t size = 0; for (auto node = begin; node != end; node = node->prev()) ++size;
      {  lib::small_vec<Obj *> res(size); for (auto node = begin; node != end; node = node->prev()) res.push_back(node);
         return res;
      }
   }

}
