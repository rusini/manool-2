// ir0.cc

/*    Copyright (C) 2020, 2021 Alexey Protasov (AKA Alex or rusini)

   This is free software: you can redistribute it and/or modify it under the terms of the version 3 of the GNU General Public License
   as published by the Free Software Foundation (and only version 3).

   This software is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along with this software.  If not, see <https://www.gnu.org/licenses/>.  */


# if RSN_USE_DEBUG

# include "ir0.hh"

# include <unordered_set>

void rsn::opt::proc::dump() const noexcept {
   std::fprintf(stderr, "P%u = proc $0x%08X[0x%016llX%016llX] as\n", node::sn, (unsigned)id.first, id.second, id.first);
   {  std::unordered_set<bblock *> bblocks;
      for (auto bb = head(); bb; bb = bb->next()) for (auto in = bb->head(); in; in = in->next()) {
         for (auto &target: in->targets()) if (target->owner() != this) bblocks.insert(target);
      }
      for (auto it: bblocks) log << "  ; Error: Reference to foreign bblock " << it << '\n';
   }
   for (auto bb = head(); bb; bb = bb->next()) bb->dump();
   std::fprintf(stderr, "end proc P%u\n\n", node::sn);
}

void rsn::opt::bblock::dump() const noexcept {
   std::fprintf(stderr, "L%u:\n", node::sn);
   for (auto in = head(); in; in = in->next())
      std::fputs("    ", stderr), in->dump(), std::fputc('\n', stderr);
}

# endif // # if RSN_USE_DEBUG
