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
   // Build Dominators Tree ////////////////////////////////////////////////////
   {  static constexpr auto intersect = [](const bblock *lhs, const bblock *rhs) noexcept RSN_INLINE{ // helper
         for (auto finger_lhs = lhs, finger_rhs = rhs; finger_lhs != finger_rhs;) {
            for (; finger_lhs->aux.postord_num < finger_rhs->aux.postord_num; finger_lhs = finger_lhs->aux.idom);
            for (; finger_rhs->aux.postord_num < finger_lhs->aux.postord_num; finger_rhs = finger_rhs->aux.idom);
         }
         return finger_lhs;
      };
      // determine postorder ordering and numbering
      lib::small_vec<bblock *, 510> postord(lib::size(pc));
      for (auto bb = pc->head(); bb = bb->next, bb;) bb->aux.visited = false;
      {  long num = {};
         static constexpr auto traverse = [](block *bb, decltype(postord) &postord, decltype(num) &num) noexcept RSN_NOINLINE{
            if (RSN_UNLIKELY(bb->aux.visited)) return;
            bb->aux.visited = true;
            for (auto &target: bb->rear()->targets()) traverse(target, postord, num);
            postord.push_back(bb), bb->aux.postord_num = num++;
         };
         traverse(pc->head(), postord, num);
      }
      // initial state
      pc->head()->aux.idom = pc->head();
      for (auto bb = pc->head(); bb = bb->next, bb;) bb->aux.idom = {};
      // state transition until a fixed point is achieved
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
   // Place Trivial Phi-functions //////////////////////////////////////////////
   {  // determine the set of virtual registers in the procedure
      std::unordered_set<vreg *> vregs;
      for (auto bb = pc->head(); bb; bb = bb->next) for (auto in = bb->head(); in; in = in->next) {
         for (auto &input: in->inputs()) if (is<vreg>(input)) vregs.insert(as<vreg>(input));
         for (auto &output: in->outputs()) vregs.insert(output);
      }
      // place phi-instructions
      for (auto vr: vregs)
      for (auto bb = pc->head(); bb; bb = bb->next) for (auto in = bb->head(); in; in = in->next)
      for (auto &output: in->outputs()) if (RSN_UNLIKELY(output == vr)) {
         lib::small_vec<bblock *, 510> worklist(lib::size(pc)); 
      }
   }


}
