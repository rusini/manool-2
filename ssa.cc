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
   std::size_t bb_count = 0, vr_count = 0;
   // Number BBs and VRs ///////////////////////////////////////////////////////////////////////////
   for (auto bb = pc->head(); bb; bb = bb->next()) {
      bb->sn = bb_count++;
      for (auto in = bb->head(); in; in = in->next()) {
         for (const auto &input: in->inputs()) if (is<vreg>(input)) as<vreg>(input)->sn = -1;
         for (const auto &output: in->outputs()) output->sn = -1;
      }
   }
   for (auto bb = pc->head(); bb; bb = bb->next()) for (auto in = bb->head(); in; in = in->next()) {
      for (const auto &input: in->inputs()) if (is<vreg>(input) && RSN_UNLIKELY(as<vreg>(input)->sn == -1)) as<vreg>(input)->sn = vr_count++;
      for (const auto &output: in->outputs()) if (RSN_UNLIKELY(output->sn == -1)) output->sn = vr_count++;
   }

   std::vector<std::vector<bblock *>> preds(bb_count), succs(bb_count);
   // Build BB Predecessor and Successor Lists (using preorder DFS) ////////////////////////////////
   {  std::vector<signed char> visited(bb_count);
      auto traverse = [&](auto &traverse, bblock *bb) RSN_NOINLINE{
         if (RSN_UNLIKELY(visited[bb->sn])) return;
         visited[bb->sn] = true;
         for (const auto &target: bb->rear()->targets())
         if (preds[target->sn].empty() || RSN_LIKELY(preds[target->sn].back() != bb))
            preds[target->sn].push_back(bb), succs[bb->sn].push_back(target);
         for (auto succ: succs[bb->sn]) traverse(traverse, succ);
      };
      traverse(traverse, pc->head());
   }

   std::vector<bblock *> idom(bb_count);
   // Build Dominator Tree /////////////////////////////////////////////////////////////////////////
   {  std::vector<bblock *> postdfs; postdfs.reserve(bb_count);
      std::vector<long> postdfs_num(bb_count);
      // determine postorder DFS ordering and numbering for the CFG
      {  decltype(postdfs_num)::value_type num = {};
         std::vector<signed char> visited(bb_count);
         auto traverse = [&](auto &traverse, bblock *bb) noexcept RSN_NOINLINE{
            if (RSN_UNLIKELY(visited[bb->sn])) return;
            visited[bb->sn] = true;
            for (auto succ: succs[bb->sn]) traverse(traverse, succ);
            postdfs.push_back(bb), postdfs_num[bb->sn] = num++;
         };
         traverse(traverse, pc->head());
      }
      // helper routine for dominator set intersection
      const auto intersect = [&](bblock *lhs, bblock *rhs) noexcept RSN_INLINE{
         auto finger_lhs = lhs, finger_rhs = rhs;
         while (finger_lhs != finger_rhs) {
            while (postdfs_num[finger_lhs->sn] < postdfs_num[finger_rhs->sn]) finger_lhs = idom[finger_lhs->sn];
            while (postdfs_num[finger_rhs->sn] < postdfs_num[finger_lhs->sn]) finger_rhs = idom[finger_rhs->sn];
         }
         return finger_lhs;
      };
      // initial state
      idom[pc->head()->sn] = pc->head();
      // state transition until a fixed point is reached
      for (;;) {
         bool changed = false;
         for (auto bb: lib::range_ref(postdfs).drop_last().reverse()) {
            auto new_idom = lib::range_ref(preds[bb->sn]).first();
            for (auto pred: lib::range_ref(preds[bb->sn]).drop_first())
               if (RSN_LIKELY(idom[pred->sn])) new_idom = intersect(new_idom, pred);
            changed |= idom[bb->sn] != new_idom, idom[bb->sn] = new_idom;
         }
         if (RSN_UNLIKELY(!changed)) break;
      }
   }

   std::vector<std::vector<bblock *>> dom_front(bb_count);
   // Compute Dominance Frontiers //////////////////////////////////////////////////////////////////
   {  std::vector<std::vector<bool>> _dom_front(bb_count, std::vector<bool>(bb_count));
      for (auto bb = pc->head(); bb; bb = bb->next())
      if (RSN_UNLIKELY(preds[bb->sn].size() > 1))
      for (auto runner: preds[bb->sn])
      for (; runner != idom[bb->sn]; runner = idom[runner->sn])
      if (RSN_UNLIKELY(!_dom_front[runner->sn][bb->sn]))
         _dom_front[runner->sn][bb->sn] = (dom_front[runner->sn].push_back(bb), true);
   }

   idom.clear(), idom.shrink_to_fit();

   // Place Trivial Phi-functions for a Minimal (Non-pruned) SSA ///////////////////////////////////
   {  std::vector<std::vector<bool>> placed(bb_count, std::vector<bool>(vr_count));
      auto place = [&](auto &place, bblock *bb, vreg *vr)->void RSN_NOINLINE{
         for (auto _bb: dom_front[bb->sn]) if (RSN_UNLIKELY(!placed[_bb->sn][vr->sn])) {
            placed[_bb->sn][vr->sn] = (insn_phi::make(_bb->head(), std::vector<lib::smart_ptr<operand>>(preds[_bb->sn].size(), vr), vr), true);
            place(place, _bb, vr);
         }
      };
      for (auto bb = pc->head(); bb; bb = bb->next()) for (auto in = bb->head(); in; in = in->next())
         if (RSN_UNLIKELY(!is<insn_phi>(in))) for (const auto &output: in->outputs()) place(place, bb, output);
   }

   dom_front.clear(), dom_front.shrink_to_fit();
   preds.clear(), preds.shrink_to_fit();

   // Rename VRs ///////////////////////////////////////////////////////////////////////////////////
   {  std::vector<vreg *> vr_map(vr_count);
      std::vector<std::size_t> phi_arg_index(bb_count);
      std::vector<signed char> visited(bb_count);
      auto traverse = [&](auto &traverse, bblock *bb) RSN_NOINLINE{
         if (RSN_UNLIKELY(visited[bb->sn])) return;
         visited[bb->sn] = true;
         std::vector<std::pair<std::size_t, vreg *>> stack;
         auto in = bb->head();
         // rewrite phi destinations
         for (; is<insn_phi>(in); in = in->next()) {
            stack.reserve(7), stack.push_back({as<insn_phi>(in)->dest()->sn, vr_map[as<insn_phi>(in)->dest()->sn]}),
               vr_map[stack.back().first] = as<insn_phi>(in)->dest() = vreg::make(); // "reserve" speeds up in practice
         }
         // rewrite normal instructions
         for (; in; in = in->next()) {
            for (auto &input: in->inputs())
               if (is<vreg>(input)) input = vr_map[as<vreg>(input)->sn];
            for (auto &output: in->outputs())
               stack.reserve(7), stack.push_back({output->sn, vr_map[output->sn]}),
                  vr_map[stack.back().first] = output = vreg::make(); // ditto
         }
         // process successors
         for (auto succ: succs[bb->sn]) {
            // rewrite phi arguments
            for (auto in = succ->head(); is<insn_phi>(in); in = in->next())
               as<insn_phi>(in)->args()[phi_arg_index[succ->sn]] = vr_map[as<vreg>(as<insn_phi>(in)->args()[phi_arg_index[succ->sn]])->sn];
            ++phi_arg_index[succ->sn];
            // recur into the successor
            traverse(traverse, succ);
         }
         // restore VR mapping
         for (; !stack.empty(); stack.pop_back())
            vr_map[stack.back().first] = stack.back().second;
      };
      // initialization and start
      for (auto bb = pc->head(); bb; bb = bb->next()) for (auto in = bb->head(); in; in = in->next()) {
         for (const auto &input: in->inputs()) if (is<vreg>(input)) vr_map[as<vreg>(input)->sn] = as<vreg>(input);
         for (const auto &output: in->outputs()) vr_map[output->sn] = output;
      }
      traverse(traverse, pc->head());
   }

   // Prune SSA: DCE for Useless Phis //////////////////////////////////////////////////////////////
   for (;;) {
      std::vector<signed char> used(vr_count);
      for (auto bb = pc->head(); bb; bb = bb->next()) for (auto in = bb->head(); in; in = in->next())
         for (const auto &input: in->inputs()) if (is<vreg>(input)) used[as<vreg>(input)->sn] = true;
      bool changed = false;
      for (auto bb = pc->head(); bb; bb = bb->next()) for (auto in: all(bb)) {
         if (RSN_UNLIKELY(!is<insn_phi>(in))) break;
         if (RSN_UNLIKELY(!used[as<insn_phi>(in)->dest()->sn])) changed = (in->eliminate(), true);
      }
      if (RSN_UNLIKELY(!changed)) break;
   }
}
