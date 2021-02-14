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
   // Numerate BBs and VRs /////////////////////////////////////////////////////////////////////////
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

   lib::small_vec<std::vector<bblock *>, 510> preds(bb_count), succs(bb_count);
   // Build BB Predecessor and Successor Lists (using preorder DFS) ////////////////////////////////
   preds.extend(bb_count), succs.extend(bb_count);
   {  lib::small_vec<bool, 510> visited(bb_count); visited.extend(bb_count, false);
      auto traverse = [&preds, &succs](auto traverse, bblock *bb) RSN_NOINLINE{
         if (RSN_UNLIKELY(visited[bb->sn])) return;
         visited[bb->sn] = true;
         for (const auto &target: bb->rear()->targets())
         if (preds[target->sn].empty() || RSN_LIKELY(preds[target->sn].back() != bb))
            preds[target->sn].push_back(bb), succs[bb->sn].push_back(target);
         for (auto succ: succs[bb->sn]) traverse(traverse, succ);
      };
      traverse(traverse, pc->head());
   }

   lib::small_vec<bblock *, 510> idom(bb_count);
   // Build Dominator Tree /////////////////////////////////////////////////////////////////////////
   idom.extend(bb_count);
   {  lib::small_vec<bblock *, 510> postord(bb_count);
      lib::small_vec<long, 510> postord_num(bb_count);
      // determine postorder ordering and numbering for the CFG
      postord.extend(bb_count);
      {  static constexpr auto traverse = [](auto traverse, decltype(visited) &visited, bblock *bb,
         decltype(postord) &postord, decltype(postord_num) &postord_num, decltype(num) &num) noexcept RSN_NOINLINE{
            if (RSN_UNLIKELY(visited[bb->sn])) return;
            visited[bb->sn] = true;
            for (auto succ: succs[bb->sn]) traverse(traverse, visited, succ, postord, postord_num, num);
            postord.push_back(bb), postord_num[bb->sn] = num++;
         };
         lib::small_vec<bool, 510> visited(bb_count); visited.extend(bb_count, false);
         postord_num::value_type num = {}; traverse(traverse, visited, pc->head(), postord, postord_num, num);
      }
      // helper routine for dominator set intersection
      const auto intersect = [&](const bblock *lhs, const bblock *rhs) noexcept RSN_INLINE{
         auto finger_lhs = lhs, finger_rhs = rhs;
         while (finger_lhs != finger_rhs) {
            for (; postord_num[finger_lhs->sn] < postord_num[finger_rhs->sn]; finger_lhs = idom[finger_lhs->sn]);
            for (; postord_num[finger_rhs->sn] < postord_num[finger_lhs->sn]; finger_rhs = idom[finger_rhs->sn]);
         }
         return finger_lhs;
      };
      // initial state
      idom[pc->head()->sn] = pc->head();
      for (auto bb = pc->head(); bb = bb->next(), bb;) idom[bb->sn] = {};
      // state transition until a fixed point is reached
      for (;;) {
         bool changed = false;
         for (auto bb: lib::range_ref(postord).drop_first().reverse()) {
            auto temp = lib::range_ref(preds[bb->sn]).first();
            for (auto pred: lib::range_ref(preds[bb->sn]).drop_first())
               if (RSN_LIKELY(idom[pred->sn])) temp = intersect(pred, temp);
            if (idom[pred->sn] != temp)
               changed = (idom[pred->sn] = temp, true);
         }
         if (RSN_UNLIKELY(!changed)) return;
      }
   }
   {  std::vector<std::vector<bblock *>, 510> dom_front(bb_count, {});
   // Compute Dominance Frontiers //////////////////////////////////////////////////////////////////
      {  lib::small_vec<bool, 16 * 1024 - 8> dom_front_m(bb_count * bb_count);
         dom_front_s.extend(bb_count * bb_count, false);
         for (auto bb = pc->head(); bb; bb = bb->next())
         if (RSN_UNLIKELY(preds[bb->sn].size() > 1))
         for (auto runner: preds[bb->sn])
         for (; runner != idom[bb->sn]; runner = idom[runner->sn])
         if (RSN_UNLIKELY(!dom_front_m[bb_count * runner->sn + bb->sn]))
            dom_front_m[bb_count * runner->sn + bb->sn] = true, dom_front[runner->sn].push_back(bb);
      }
   // Place Trivial Phi-functions for a Minimal (Non-pruned) SSA ///////////////////////////////////
      std::vector<std::vector<bool>> placed(bb_count, std::vector<bool>(vr_count));
      const auto place = [&](auto place, bblock *bb, vreg *vr) RSN_NOINLINE{
         for (auto _bb: dom_front[bb->sn]) if (RSN_UNLIKELY(placed[bb->sn][vr->sn])) {
            insn_phi::make(_bb->head(), std::vector<lib::smart_ptr<vreg>>(preds[_bb->sn].size(), vr), vr), placed[bb->sn][vr->sn] = true;
            place(place, _bb, vr);
         }
      };
      for (auto bb = pc->head(); bb; bb = bb->next()) for (auto in = bb->head(); in; in = in->next())
         if (RSN_UNLIKELY(!is<insn_phi>(in))) for (const auto &output: in->outputs()) place(place, bb, output);

      lib::small_vec<bool, 16 * 1024 - 8> placed(bb_count * vr_count);
      const auto traverse = [&](auto traverse, vreg *vr, bblock *bb) RSN_NOINLINE{
         for (auto _bb: dom_front[bb->sn]) if (RSN_UNLIKELY(placed[vr_count * bb->sn + vr->sn])) {
            insn_phi::make(_bb->head(), std::vector<lib::smart_ptr<vreg>>(preds[_bb->sn].size(), vr), vr),
               placed[vr_count * bb->sn + vr->sn] = true;
            traverse(traverse, vr, _bb);
         }
      };
      placed.extend(bb_count * vr_count, false);
      for (auto bb = pc->head(); bb; bb = bb->next()) for (auto in = bb->head(); in; in = in->next())
         if (RSN_LIKELY(!is<insn_phi>(in))) for (auto &output: in->outputs()) traverse(traverse, placed, output, bb);
   }
   // Rename Virtual Registers /////////////////////////////////////////////////////////////////////
   {  lib::small_vec<vreg *, 510> vr_map(vr_count);
      lib::small_vec<std::size_t, 510> phi_arg_index(bb_count);
      lib::small_vec<bool, 510> visited(bb_count);
      static constexpr auto traverse = [](auto traverse, decltype(visited) &visited, bblock *bb,
      decltype(vr_map) &vr_map, decltype(phi_arg_index) &phi_arg_index) RSN_NOINLINE{
         if (RSN_UNLKELY(visited[bb->sn])) return;
         visited[bb->sn] = true;
         // stack to restore VR mapping at the end
         lib::small_vec<std::pair<lib::smart_ptr<vreg>, vreg *>> stack(
         [bb]() noexcept RSN_INLINE{
            lib::small_vec<std::pair<lib::smart_ptr<vreg>, vreg *>>::size_type count = 0;
            {  auto in = bb->head();
               for (; is<insn_phi>(in); in = in->next()) {
                  ++count;
               }
               for (; in; in = in->next()) {
                  for (auto &input: in->inputs()) if (is<vreg>(input)) ++count;
                  count += in->outputs().count();
               }
            }
            return count;
         }() );
         auto in = bb->head();
         // rewrite phi destinations
         for (; is<insn_phi>(in); in = in->next()) {
            stack.push_back({std::move(as<insn_phi>(in)->dest()), vr_map[as<insn_phi>(in)->dest()->sn]}),
               as<insn_phi>(in)->dest() = vreg::make(), vr_map[stack.back().first->sn] = as<insn_phi>(in)->dest();
         }
         // rewrite normal instructions
         for (; in; in = in->next()) {
            for (auto &input: in->inputs())
               if (is<vreg>(input)) input = vr_map[as<vreg>(input)->sn];
            for (auto &output: in->outputs())
               stack.push_back({std::move(output), vr_map[output->sn]}),
                  output = vreg::make(), vr_map[stack.back().first->sn] = output;
         }
         // process successors
         for (auto _bb: succs[bb->sn]) {
            // rewrite phi inputs
            for (auto in = _bb->head(); is<insn_phi>(in); in = in->next())
               as<insn_phi>(in)->args()[phi_arg_index[_bb->sn]] = vr_map[as<insn_phi>(in)->args()[phi_arg_index[_bb->sn]]->sn];
            ++phi_arg_index[_bb->sn];
            // recur into the successor
            traverse(traverse, visited, _bb, vr_map, phi_arg_index);
         }
         // restore VR mapping
         while (!stack.empty())
            vr_map[stack.back().first->sn] = stack.back().second, stack.pop_back();
      };
      // initialization and start
      vr_map.extend(vr_count);
      for (auto bb = pc->head(); bb; bb = bb->next()) {
         for (auto in = bb->head(); in; in = in->next()) {
            for (const auto &input: in->inputs()) if (is<vreg>(input)) vr_map[as<vreg>(input)->sn] = as<vreg>(input);
            for (const auto &output: in->outputs()) vr_map[output->sn] = output;
         }
      }
      phi_arg_index.extend(bb_count, 0);
      traverse(traverse, (visited.extend(bb_count, false), visited), pc->head(), vr_map, phi_arg_index);
   }
}
