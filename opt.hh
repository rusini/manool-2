// opt.hh -- analysis and transformation passes

/*    Copyright (C) 2020 Alexey Protasov (AKA Alex or rusini)

   This is free software: you can redistribute it and/or modify it under the terms of the version 3 of the GNU General Public License
   as published by the Free Software Foundation (and only version 3).

   This software is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along with MANOOL.  If not, see <https://www.gnu.org/licenses/>.  */


# ifndef RSN_INCLUDED_OPT
# define RSN_INCLUDED_OPT

# include "ir.hh"

namespace rsn::opt {
   void update_cfg_preds(proc *);             // update control-flow predecessors
   bool transform_cpropag(proc *) noexcept;   // constant propagation (from mov and beq insns)
   bool transform_cfold(proc *);              // constant folding (including inlining)
   bool transform_cfg_merge(proc *) noexcept; // merge basic blocks (for any B1 and B2 where B1 is the only predecessor or B2)
   bool transform_cfg_gc(proc *) noexcept;    // eliminate basic blocks unreachable from the entry basic block
}

# endif // # ifndef RSN_INCLUDED_OPT
