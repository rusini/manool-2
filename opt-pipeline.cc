// opt-pipeline.cc

/*    Copyright (C) 2020, 2021 Alexey Protasov (AKA Alex or rusini)

   This is free software: you can redistribute it and/or modify it under the terms of the version 3 of the GNU General Public License
   as published by the Free Software Foundation (and only version 3).

   This software is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along with this software.  If not, see <https://www.gnu.org/licenses/>.  */


# include "ir.hh"

namespace rsn::opt { void optimize(proc *); }

void rsn::opt::optimize(proc *tu) {
   bool transform_insn_simplify(proc *);
   void update_cfg_preds(proc *);
   bool transform_const_propag(proc *) noexcept;
   bool transform_copy_propag(proc *) noexcept;
   bool transform_dce(proc *) noexcept;
   bool transform_cfg_gc(proc *) noexcept;
   bool transform_cfg_merge(proc *tu) noexcept;

   update_cfg_preds(tu),
   transform_const_propag(tu);
   return;

   for (;;) {
      bool changed{};
      update_cfg_preds(tu),
      changed |= transform_const_propag(tu),
      changed |= transform_copy_propag(tu),
      changed |= transform_dce(tu),
      changed |= transform_cfg_gc(tu),
      changed |= transform_insn_simplify(tu),
      changed |= transform_cfg_merge(tu);
      if (!RSN_LIKELY(changed)) return;
   }
}
