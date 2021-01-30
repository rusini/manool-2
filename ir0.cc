// ir0.cc

/*    Copyright (C) 2020 Alexey Protasov (AKA Alex or rusini)

   This is free software: you can redistribute it and/or modify it under the terms of the version 3 of the GNU General Public License
   as published by the Free Software Foundation (and only version 3).

   This software is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along with MANOOL.  If not, see <https://www.gnu.org/licenses/>.  */


# if RSN_USE_DEBUG

# include "ir0.hh"

# include <unordered_set>

using namespace rsn::opt;

void proc::dump() const noexcept {
   std::fprintf(stderr, "P%u = proc $0x%08X[0x%016llX%016llX] as\n", sn, (unsigned)id.first, id.second, id.first);
   {  std::unordered_set<bblock *> bblocks;
      for (auto bb: all(this)) for (auto in: all(bb)) {
         for (auto target: in->targets()) if (target->owner() != this) bblocks.insert(target);
      }
      for (auto it: bblocks) log << "  ; Error: Reference to foreign bblock " << it << '\n';
   }
   for (auto it: all(this)) it->dump();
   std::fprintf(stderr, "end proc P%u\n\n", sn);
}

void bblock::dump() const noexcept {
   std::fprintf(stderr, "L%u:\n", sn);
   for (auto insn: all(this)) std::fputs("    ", stderr), insn->dump(), std::fputc('\n', stderr);
}

# endif // # if RSN_USE_DEBUG
