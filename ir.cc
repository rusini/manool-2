// ir.cc

/*    Copyright (C) 2020 Alexey Protasov (AKA Alex or rusini)

   This is free software: you can redistribute it and/or modify it under the terms of the version 3 of the GNU General Public License
   as published by the Free Software Foundation (and only version 3).

   This software is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along with MANOOL.  If not, see <https://www.gnu.org/licenses/>.  */


# include "ir.hh"

using namespace rsn::opt;

bool insn_binop::try_to_fold() {
   if (RSN_LIKELY(!is<abs_imm>(lhs())) || RSN_LIKELY(!is<abs_imm>(rhs()))) return false;
   auto lhs = as<abs_imm>(insn_binop::lhs())->value, rhs = as<abs_imm>(insn_binop::rhs())->value;

   switch (op) {
   default:
      RSN_UNREACHABLE();
   case _add:
      insn_mov::make(this, std::move(dest), abs_imm::make(lhs + rhs));
      break;
   case _sub:
      insn_mov::make(this, std::move(dest), abs_imm::make(lhs - rhs));
      break;
   case _umul:
      insn_mov::make(this, std::move(dest), abs_imm::make(lhs * rhs));
      break;
   case _udiv: // x86 semantics
      if (RSN_UNLIKELY(!rhs)) insn_undefined::make(this); else
         insn_mov::make(this, std::move(dest), abs_imm::make(lhs / rhs));
      break;
   case _urem: // x86 semantics
      if (RSN_UNLIKELY(!rhs)) insn_undefined::make(this); else
         insn_mov::make(this, std::move(dest), abs_imm::make(lhs % rhs));
      break;
   case _smul:
      insn_mov::make(this, std::move(dest), abs_imm::make((long long)lhs * (long long)rhs));
      break;
   case _sdiv: // x86 semantics
      if (RSN_UNLIKELY(!rhs) || RSN_UNLIKELY(lhs == std::numeric_limits<long long>::min()) && RSN_UNLIKELY(rhs == -1)) insn_undefined::make(this); else
         insn_mov::make(this, std::move(dest), abs_imm::make((long long)lhs / (long long)rhs));
      break;
   case _srem: // x86 semantics
      if (RSN_UNLIKELY(!rhs) || RSN_UNLIKELY(lhs == std::numeric_limits<long long>::min()) && RSN_UNLIKELY(rhs == -1)) insn_undefined::make(this); else
         insn_mov::make(this, std::move(dest), abs_imm::make((long long)lhs % (long long)rhs));
      break;
   case _and:
      insn_mov::make(this, std::move(dest), abs_imm::make(lhs & rhs));
      break;
   case _or:
      insn_mov::make(this, std::move(dest), abs_imm::make(lhs | rhs));
      break;
   case _xor:
      insn_mov::make(this, std::move(dest), abs_imm::make(lhs ^ rhs));
      break;
   case _shl: // x86 semantics
      insn_mov::make(this, std::move(dest), abs_imm::make(lhs << (rhs & 0x3F)));
      break;
   case _ushr: // x86 semantics
      insn_mov::make(this, std::move(dest), abs_imm::make(lhs >> (rhs & 0x3F)));
      break;
   case _sshr: // x86 semantics
      insn_mov::make(this, std::move(dest), abs_imm::make((long long)lhs >> (rhs & 0x3F)));
      break;
   }
   remove(); return true;
}

bool insn_br::try_to_fold() {
   if (RSN_LIKELY(!is<abs_imm>(lhs())) || RSN_LIKELY(!is<abs_imm>(rhs()))) return false;
   auto lhs = as<abs_imm>(insn_binop::lhs())->value, rhs = as<abs_imm>(insn_binop::rhs())->value;

   switch (op) {
   default:
      RSN_UNREACHABLE();
   case _beq:
      insn_jmp::make(this, std::move(lhs == rhs ? dest1() : dest2()));
      break;
   case _bne:
      insn_jmp::make(this, std::move(lhs != rhs ? dest1() : dest2()));
      break;
   case _ublt:
      insn_jmp::make(this, std::move(lhs <  rhs ? dest1() : dest2()));
      break;
   case _uble:
      insn_jmp::make(this, std::move(lhs <= rhs ? dest1() : dest2()));
      break;
   case _sblt:
      insn_jmp::make(this, std::move((long long)lhs <  (long long)rhs ? dest1() : dest2()));
      break;
   case _sble:
      insn_jmp::make(this, std::move((long long)lhs <= (long long)rhs ? dest1() : dest2()));
      break;
   }
   remove(); return true;
}

bool insn_switch_br::try_to_fold() {
   if (RSN_LIKELY(!is<abs_imm>(index()))) return false;

   if (RSN_UNLIKELY(as<abs_imm>(index())->value >= dests().size())) insn_undefined::make(this); else
      insn_jmp::make(this, std::move(dests()[as<abs_imm>(index())->value]));
   remove(); return true;
}

bool insn_call::simplify() {
   if (RSN_LIKELY(!is<proc>(dest()))) return false;
   auto pr = as<proc>(dest());

   // integrate and expand the insn_entry
   if (RSN_UNLIKELY(as<insn_entry>(pr->head()->head())->params().size() != params().size()))
      insn_oops::make(this);
   else
   for (std::size_t sn = 0, size = params().size(); sn < size; ++sn)
      insn_mov::make(this, as<insn_entry>(pr->head()->head())->params()[sn], std::move(params()[sn]));
   // integrate the rest of entry BB
   for (auto in = pr->front()->front()->next(); in; in = in->next())
      in->clone(this);

   if (RSN_LIKELY(pr->size() == 1)) {
      // expand the insn_ret
      if (RSN_UNLIKELY(as<insn_ret>(prev())->results().size() != results().size()))
         insn_oops::make(prev());
      else
      for (std::size_t sn = 0, size = results().size(); sn < size; ++sn)
         insn_mov::make(prev(), std::move(results()[sn]), std::move(as<insn_ret>(prev())->results()[sn]));
      prev()->eliminate();
   } else {
      pr->head()->temp.bb = owner();
      {  // split the BB at the insn_call
         auto bb = RSN_LIKELY(owner()->next()) ? bblock::make(owner()->next()) : bblock::make(owner()->owner());
         for (auto in: all(this, {})) in->reattach(bb);
      }
      // integrate the rest of BBs
      for (auto bb = pr->head()->next(); bb; bb = bb->next()) {
         bb->temp.bb = bblock::make(owner());
         // integrate instructions
         for (auto in = bb->head(); in; in = in->next())
            in->clone(owner()->prev());
      }

      for (auto bb = pr->head(); bb; bb = bb->next())
      if (RSN_LIKELY(!is<insn_ret>(bb->temp.bb->back())))
         // fixup jump targets
         for (auto &target: bb->temp.bb->rear()->targets()) target = target->temp.bb;
      else {
         // expand an insn_ret
         if (RSN_UNLIKELY(as<insn_ret>(bb->temp.bb->rear())->results.size() != results.size()))
            insn_oops::make(bb->temp.bb->rear());
         else
         for (std::size_t sn = 0, size = results.size(); sn < size; ++sn)
            insn_mov::make(bb->temp.bb->rear(), results()[sn], std::move(as<insn_ret>(bb->temp.bb->rear())->results()[sn]));
         insn_jmp::make(bb->temp.bb->rear(), owner());
         bb->temp.bb->rear()->eliminate();
      }
   }

   eliminate(); return true;
}
