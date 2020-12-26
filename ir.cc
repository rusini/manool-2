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
   if (RSN_LIKELY(!is<abs_imm>(lhs)) || RSN_LIKELY(!is<abs_imm>(rhs))) return false;
   auto lhs = as<abs_imm>(insn_binop::lhs)->value, rhs = as<abs_imm>(insn_binop::rhs)->value;

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
   case _cmpeq:
      insn_mov::make(this, std::move(dest), abs_imm::make(lhs == rhs));
      break;
   case _cmpne:
      insn_mov::make(this, std::move(dest), abs_imm::make(lhs != rhs));
      break;
   case _ucmplt:
      insn_mov::make(this, std::move(dest), abs_imm::make(lhs <  rhs));
      break;
   case _ucmple:
      insn_mov::make(this, std::move(dest), abs_imm::make(lhs <= rhs));
      break;
   case _scmplt:
      insn_mov::make(this, std::move(dest), abs_imm::make((long long)lhs <  (long long)rhs));
      break;
   case _scmple:
      insn_mov::make(this, std::move(dest), abs_imm::make((long long)lhs <= (long long)rhs));
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

bool insn_cond_jmp::try_to_fold() {
   if (RSN_LIKELY(!is<abs_imm>(cond))) return false;

   insn_jmp::make(this, std::move(as<abs_imm>(cond)->value ? dest1 : dest2));
   remove(); return true;
}

bool insn_switch_jmp::try_to_fold() {
   if (RSN_LIKELY(!is<abs_imm>(index))) return false;

   if (RSN_UNLIKELY(as<abs_imm>(index)->value >= dests.size())) insn_undefined::make(this); else
      insn_jmp::make(this, std::move(dests[as<abs_imm>(index)->value]));
   remove(); return true;
}
