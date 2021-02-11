// ssa.cc -- construction of static single assignment form

/*    Copyright (C) 2020, 2021 Alexey Protasov (AKA Alex or rusini)

   This is free software: you can redistribute it and/or modify it under the terms of the version 3 of the GNU General Public License
   as published by the Free Software Foundation (and only version 3).

   This software is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along with this software.  If not, see <https://www.gnu.org/licenses/>.  */


# include "ir.hh"

namespace rsn::opt { void transform_to_ssa(proc *); }

/* References:
   - A Simple, Fast Dominance Algorithm by Keith D. Cooper, Timothy J. Harvey, and Ken Kennedy
*/
void rsn::opt::transform_to_ssa(proc *pc) {
   // Build Dominator Tree /////////////////////////////////////////////////////
   {  static constexpr auto intersect = [](const bblock *lhs, const bblock *rhs) noexcept RSN_INLINE{ // helper
         for (auto finger_lhs = lhs, finger_rhs = rhs; finger_lhs != finger_rhs;) {
            for (; finger_lhs->aux.postord_num < finger_rhs->aux.postord_num; finger_lhs = finger_lhs->aux.idom);
            for (; finger_rhs->aux.postord_num < finger_lhs->aux.postord_num; finger_rhs = finger_rhs->aux.idom);
         }
         return finger_lhs;
      };
      // determine postorder ordering and numbering for the CFG
      lib::small_vec<bblock *, 510> postord(lib::size(pc));
      for (auto bb = pc->head(); bb = bb->next, bb;) bb->aux.visited = false;
      {  long num = {};
         static constexpr auto traverse = [](auto traverse, block *bb, decltype(postord) &postord, decltype(num) &num) noexcept RSN_NOINLINE{
            if (RSN_UNLIKELY(bb->aux.visited)) return;
            bb->aux.visited = true;
            for (auto &target: bb->rear()->targets()) traverse(traverse, target, postord, num);
            postord.push_back(bb), bb->aux.postord_num = num++;
         };
         traverse(traverse, pc->head(), postord, num);
      }
      // initial state
      pc->head()->aux.idom = pc->head();
      for (auto bb = pc->head(); bb = bb->next, bb;) bb->aux.idom = {};
      // state transition until a fixed point is reached
      for (;;) {
         bool changed = false;
         for (auto bb: lib::range_ref(postord).drop_first().reverse()) {
            auto idom = bb->aux.preds.head();
            for (auto pred: lib::range_ref(bb->aux.preds).drop_first())
               if (RSN_LIKELY(pred->aux.idom)) idom = intersect(pred, idom);
            if (bb->aux.idom != idom)
               changed = (bb->aux.idom = idom, true);
         }
         if (RSN_UNLIKELY(!changed)) return;
      }
   }
   // Compute Dominance Frontiers //////////////////////////////////////////////
   const auto _finally_dom_front = lib::finally([pc]() noexcept RSN_INLINE{
      for (auto bb = pc->head(); bb; bb = bb->next) bb->aux.dom_front.clear();
   });
   for (auto bb = pc->head(); bb; bb = bb->next)
   if (RSN_UNLIKELY(bb->aux.preds.size() > 1))
   for (auto runner: bb->aux.preds)
   for (; runner != bb->aux.idom; runner = runner->aux.idom)
      runner->aux.dom_front.insert(bb);
   // Place Trivial Phi-functions for a Minimal (Non-pruned) SSA ///////////////
   {  std::set<std::pair<vreg *, bblock *>> placed;
      static constexpr auto place = [](auto place, decltype(placed) &placed, vreg *vr, bblock *bb) noexcept RSN_NOINLINE{
         for (auto _bb: bb->aux.dom_front) if (RSN_LIKELY(placed.find({vr, _bb}) == placed.end())) {
            insn_phi::make(_bb->head(), std::vector<lib::smart_ptr<vreg>>(_bb->aux.preds.size(), vr));
            placed.insert({vr, _bb}), place(place, placed, vr, bb->aux.dom_front);
         }
      };
      for (auto bb = pc->head(); bb; bb = bb->next()) for (auto in = bb->head(); in; in = in->next())
         for (auto &output: in->outputs()) place(place, placed, output, bb);
   }
   // Rename Virtual Registers /////////////////////////////////////////////////
   {  static constexpr auto traverse =
      [](auto traverse, bblock *bb) noexcept RSN_NOINLINE{
         if (RSN_UNLKELY(bb->aux.visited)) return;
         bb->aux.visited = true;
         lib::small_vec<std::pair<lib::smart_ptr<vreg>, vreg *>> stack(
         [bb]() noexcept RSN_INLINE{
            lib::small_vec<std::pair<vreg *, vreg *>>::size_type count = 0;
            auto in = bb->head();
            for (; is<insn_phi>(in); in = in->next()) {
               ++count;
            }
            for (; in; in = in->next()) {
               for (auto &input: in->inputs()) if (is<vreg>(input)) ++count;
               count += in->outputs().count();
            }
            return count;
         }() );
         auto in = bb->head();
         for (; is<insn_phi>(in); in = in->next()) {
            stack.push_back({std::move(as<insn_phi>(in)->dest()), as<insn_phi>(in)->dest()->aux.vr}),
               as<insn_phi>(in)->dest() = vreg::make(), stack.back().first->aux.vr = as<insn_phi>(in)->dest();
         }
         for (; in; in = in->next()) {
            for (auto &input: in->inputs())
               if (is<vreg>(input)) input = as<vreg>(input)->aux.vr;
            for (auto &output: in->outputs())
               stack.push_back({std::move(output), output->aux.vr}),
                  output = vreg::make(), stack.back().first->aux.vr = output;
         }
         for (auto _bb: bb->aux.succs) {
            for (auto in = _bb->head(); is<insn_phi>(in); in = in->next())
               as<insn_phi>(in)->args()[_bb->aux.phi_arg_index] = as<insn_phi>(in)->args()[_bb->aux.phi_arg_index]->aux.vr;
            ++_bb->aux.phi_arg_index;
            traverse(traverse, _bb);
         }
         while (!stack.empty())
            stack.back().first->aux.vr = stack.back().second, stack.pop_back();
      };
      for (auto bb = pc->head(); bb; bb = bb->next()) {
         bb->aux.visited = false, bb->aux.phi_arg_index;
         for (auto in = bb->head(); in; in = in->next()) {
            for (auto &input: in->inputs()) if (is<vreg>(input)) as<vreg>(input)->aux.vr = as<vreg>(input);
            for (auto &output: in->outputs()) output->aux.vr = output;
         }
      }
      traverse(traverse, pc->head());
   }
}
