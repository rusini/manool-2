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

bool insn_call::try_to_fold() {
   if (RSN_LIKELY(!is<proc>(dest()))) return false;
   auto pr = as<proc>(dest());

   // integrate and expand the insn_entry
   if (RSN_UNLIKELY(as<insn_entry>(pr->front()->front())->params().size() != params().size()))
      insn_undefined::make(this);
   else
   for (std::size_t sn = 0, size = params().size(); sn < size; ++sn) {
      if (RSN_UNLIKELY(!as<insn_entry>(pr->front()->front())->params()[sn]->temp.vr))
         as<insn_entry>(pr->front()->front())->params()[sn]->temp.vr = vreg::make(owner()->owner());
      insn_mov::make(this, as<insn_entry>(pr->front()->front())->params()[sn]->temp.vr, std::move(params()[sn]));
   }
   // integrate the rest of entry BB
   for (auto in: all_from(pr->front()->front()->next())) {
      in->clone(this);
      // fixup vreg references
      for (auto &output: prev()->outputs()) {
         if (RSN_UNLIKELY(!output->temp.vr)) output->temp.vr = vreg::make(owner()->owner());
         output = output->temp.vr;
      }
      for (auto &input: prev()->inputs()) if (is<vreg>(input)) {
         if (RSN_UNLIKELY(!as<vreg>(input)->temp.vr)) as<vreg>(input)->temp.vr = vreg::make(owner()->owner());
         input = as<vreg>(input)->temp.vr;
      }
   }

   if (RSN_LIKELY(pr->size() == 1)) {
      // expand the insn_ret
      if (RSN_UNLIKELY(as<insn_ret>(prev())->results().size() != results().size()))
         insn_undefined::make(prev());
      else
      for (std::size_t sn = 0, size = results().size(); sn < size; ++sn)
         insn_mov::make(prev(), std::move(results()[sn]), std::move(as<insn_ret>(prev())->results()[sn]));
      prev()->remove();
   } else {
      pr->head()->temp.bb = owner();
      {  // split the BB at the insn_call
         auto bb = RSN_LIKELY(owner()->next()) ? bblock::make(owner()->next()) : bblock::make(owner()->owner());
         for (auto in: all_from(this)) in->reattach(bb);
      }
      // integrate the rest of BBs
      for (auto bb: all_from(pr->head()->next())) {
         bb->temp.bb = bblock::make(owner());
         // integrate instructions
         for (auto in: all(bb)) {
            in->clone(owner()->prev());
            // fixup vreg references
            for (auto &output: owner()->prev()->back()->outputs()) {
               if (RSN_UNLIKELY(!output->temp.vr)) output->temp.vr = vreg::make(owner()->owner());
               output = output->temp.vr;
            }
            for (auto &input: owner()->prev()->back()->inputs()) if (is<vreg>(input)) {
               if (RSN_UNLIKELY(!as<vreg>(input)->temp.vr)) as<vreg>(input)->temp.vr = vreg::make(owner()->owner());
               input = as<vreg>(input)->temp.vr;
            }
         }
      }

      for (auto bb: all(pr))
      if (RSN_LIKELY(!is<insn_ret>(bb->temp.bb->back())))
         // fixup jump targets
         for (auto &target: bb->temp.bb->back()->targets()) target = target->temp.bb;
      else {
         // expand an insn_ret
         if (RSN_UNLIKELY(as<insn_ret>(bb->temp.bb->back())->results.size() != results.size()))
            insn_undefined::make(bb->temp.bb->back());
         else
         for (std::size_t sn = 0, size = results.size(); sn < size; ++sn)
            insn_mov::make(bb->temp.bb->back(), results()[sn], std::move(as<insn_ret>(bb->temp.bb->back())->results()[sn]));
         insn_jmp::make(bb->temp.bb->back(), owner());
         bb->temp.bb->back()->remove();
      }
   }

   // reset instrussive temporaries
   for (auto bb: all(pr)) for (auto in: all(bb)) {
      for (auto &output: in->outputs()) output->temp.vr = {};
      for (auto &input: in->inputs()) if (is<vreg>(input)) as<vreg>(input)->temp.vr = {};
   }

   remove(); return true;
}
