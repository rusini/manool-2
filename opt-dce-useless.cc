// opt-dce-useless.cc -- dead code elimination of useless code

/*    Copyright (C) 2020, 2021 Alexey Protasov (AKA Alex or rusini)

   This is free software: you can redistribute it and/or modify it under the terms of the version 3 of the GNU General Public License
   as published by the Free Software Foundation (and only version 3).

   This software is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along with this software.  If not, see <https://www.gnu.org/licenses/>.  */


# include "ir.hh"

namespace rsn::opt { bool transform_dce_useless(proc *); }

bool rsn::opt::transform_dce_useless(proc *pc) {
   std::size_t in_count = 0, vr_count = 0;
   // Number instructions and VRs //////////////////////////////////////////////////////////////////
   for (auto bb = pc->head(); bb; bb = bb->next()) for (auto in = bb->head(); in; in = in->next()) {
      for (const auto &input: in->inputs()) if (is<vreg>(input)) as<vreg>(input)->sn = -1;
      for (const auto &output: in->outputs()) output->sn = -1;
   }
   for (auto bb = pc->head(); bb; bb = bb->next()) for (auto in = bb->head(); in; in = in->next()) {
      in->sn = in_count++;
      for (const auto &input: in->inputs()) if (is<vreg>(input) && RSN_UNLIKELY(as<vreg>(input)->sn == -1)) as<vreg>(input)->sn = vr_count++;
      for (const auto &output: in->outputs()) if (RSN_UNLIKELY(output->sn == -1)) output->sn = vr_count++;
   }

   std::vector<insn *> def(vr_count);
   // Determine definition instruction for each VR (if any) ////////////////////////////////////////
   for (auto bb = pc->head(); bb; bb = bb->next()) for (auto in = bb->head(); in; in = in->next())
      for (const auto &output: in->outputs()) def[output->sn] = in;

   std::vector<signed char> visited(in_count);
   // Traverse SSA graph in use->def direction starting from impure instructions ///////////////////
   auto traverse = [&](auto &traverse, insn *in) noexcept RSN_NOINLINE{
      if (RSN_UNLIKELY(visited[in->sn])) return;
      visited[in->sn] = true;
      for (const auto &input: in->inputs()) if (is<vreg>(input) && RSN_LIKELY(def[as<vreg>(input)->sn])) traverse(traverse, def[as<vreg>(input)->sn]);
   };
   for (auto bb = pc->head(); bb; bb = bb->next()) for (auto in = bb->head(); in; in = in->next())
      if (RSN_UNLIKELY(is<impure_insn>(in))) traverse(traverse, in);

   def.clear(), def.shrink_to_fit();

   // Eliminate unvisited instructions /////////////////////////////////////////////////////////////
   bool changed = false;
   for (auto bb = pc->head(); bb; bb = bb->next()) for (auto in: all(bb))
      if (RSN_UNLIKELY(!visited[in->sn])) in->eliminate(), changed = true;
   return changed;
}
