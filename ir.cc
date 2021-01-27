// ir.cc

/*    Copyright (C) 2020, 2021 Alexey Protasov (AKA Alex or rusini)

   This is free software: you can redistribute it and/or modify it under the terms of the version 3 of the GNU General Public License
   as published by the Free Software Foundation (and only version 3).

   This software is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along with this software.  If not, see <https://www.gnu.org/licenses/>.  */


# include "ir.hh"

# include <limits> // numeric_limits

namespace rsn::opt {

// Some absolute immediate operands to share
static RSN_IF_WITH_MT(thread_local) const auto abs_0 = abs_imm::make(0), abs_1 = abs_imm::make(1);

static RSN_INLINE inline void split(insn *in) { // split the BB at the insn
   auto bb = RSN_LIKELY(in->owner()->next()) ? bblock::make(in->owner()->next()) : bblock::make(in->owner()->owner());
   for (auto _in: all(in, {})) _in->reattach(bb);
}

bool insn_binop::simplify() {
   bool changed{};
   switch (op) {
      typedef lib::smart_ptr<operand> operand;
   default:
      RSN_UNREACHABLE();
   case _add:
      if (lhs()->is_abs()) {
         if (!rhs()->is_abs()) // canonicalization
            changed = (lhs().swap(rhs()), true);
      } else
      if (!lhs()->is_reg() && rhs()->is_reg()) // canonicalization
         return lhs().swap(rhs()), true;
      if (rhs()->is_abs()) {
         if (rhs()->as_abs()->val == 0) // algebraic simplification
            return insn_mov::make(this, std::move(dest()), std::move(lhs())), eliminate(), true;
         if (lhs()->is_abs()) // constant folding
            return insn_mov::make(this, std::move(dest()), abs::make(lhs()->as_abs()->val + rhs()->as_abs()->val)), eliminate(), true;
         if (lhs()->is_rel_base()) // constant folding
            return insn_mov::make(this, std::move(dest()), rel_disp::make(std::move(lhs())->as_smart_rel_base(), +rhs()->as_abs()->val), eliminate(), true;
         if (lhs()->is_rel_disp()) // constant folding
            return insn_mov::make(this, std::move(dest()), lhs()->as_rel_disp()->add + rhs()->as_abs()->val == 0 ? (operand)lhs()->as_rel_disp()->base :
               (operand)rel_disp::make(lhs()->as_rel_disp()->base, lhs()->as_rel_disp()->add + rhs()->as_abs()->val), eliminate(), true;
      }
      return changed;
   case _sub:
      if (rhs()->is_abs()) {
         if (rhs()->as_abs()->val == 0) // algebraic simplification
            return insn_mov::make(this, std::move(dest()), std::move(lhs())), eliminate(), true;
         if (lhs()->is_abs()) // constant folding
            return insn_mov::make(this, std::move(dest()), abs::make(lhs()->as_abs()->val - rhs()->as_abs()->val)), eliminate(), true;
         if (lhs()->is_rel_base()) // constant folding
            return insn_mov::make(this, std::move(dest()), rel_disp::make(std::move(lhs())->as_smart_rel_base(), -rhs()->as_abs()->val), eliminate(), true;
         if (lhs()->is_rel_disp()) // constant folding
            return insn_mov::make(this, std::move(dest()), lhs()->as_rel_disp()->add - rhs()->as_abs()->val == 0 ? (operand)lhs()->as_rel_disp()->base :
               (operand)rel_disp::make(lhs()->as_rel_disp()->base, lhs()->as_rel_disp()->add - rhs()->as_abs()->val), eliminate(), true;
         // canonicalization
         return insn_binop::make_add(this, std::move(dest()), std::move(lhs()), abs::make(-rhs()->as_abs()->val)), eliminate(), true;
      }
      if (lhs() == rhs()) // algebraic simplification
         return insn_mov::make(this, std::move(dest()), abs_0), eliminate(), true;
      if (lhs()->is_rel_base()) {
         if (rhs()->is_rel_base()) {
            if (lhs()->is_rel_base()->id == rhs()->is_rel_base()->id) // constant folding
               return insn_mov::make(this, std::move(dest()), abs_0), eliminate(), true;
         } else
         if (rhs()->is_rel_disp()) {
            if (lhs()->is_rel_base()->id == rhs()->is_rel_disp()->base->id) // constant folding
               return insn_mov::make(this, std::move(dest()), abs::make(-rhs()->is_rel_disp()->add)), eliminate(), true;
         }
      } else
      if (lhs()->is_rel_disp()) {
         if (rhs()->is_rel_base()) {
            if (lhs()->is_rel_disp()->base->id == rhs()->is_rel_base()->id) // constant folding
               return insn_mov::make(this, std::move(dest()), abs::make(+lhs()->is_rel_disp()->add)), eliminate(), true;
         } else
         if (rhs()->is_rel_disp()) {
            if (lhs()->is_rel_disp()->base->id == rhs()->is_rel_disp()->base->id) // constant folding
               return insn_mov::make(this, std::move(dest()), abs::make(lhs()->is_rel_disp()->add - rhs()->is_rel_disp()->add)), eliminate(), true;
         }
      }
      return {};
   case _umul:
      if (lhs()->is_abs()) {
         if (!rhs()->is_abs()) // canonicalization
            changed = (lhs().swap(rhs()), true);
      } else
      if (!lhs()->is_reg() && rhs()->is_reg()) // canonicalization
         return lhs().swap(rhs()), true;
      if (rhs()->is_abs()) {
         if (rhs()->as_abs()->val == 1) // algebraic simplification
            return insn_mov::make(this, std::move(dest()), std::move(lhs())), eliminate(), true;
         if (rhs()->as_abs()->val == 0) // algebraic simplification
            return insn_mov::make(this, std::move(dest()), std::move(rhs())), eliminate(), true;
         if (lhs()->is_abs()) // constant folding
            return insn_mov::make(this, std::move(dest()), abs::make(lhs()->as_abs()->val * rhs()->as_abs()->val)), eliminate(), true;
      }
      return changed;
   case _udiv:
      if (rhs()->is_abs()) {
         if (!RSN_LIKELY(rhs()->as_abs()->val)) // x86 semantics
            return insn_oops::make(this), eliminate(), true;
         if (rhs()->as_abs()->val == 1) // algebraic simplification
            return insn_mov::make(this, std::move(dest()), std::move(lhs())), eliminate(), true;
         if (lhs()->is_abs()) // constant folding
            return insn_mov::make(this, std::move(dest()), abs::make(lhs()->as_abs()->val / rhs()->as_abs()->val)), eliminate(), true;
      }
      if (lhs()->is_abs() && lhs()->as_abs()->val == 0) // algebraic simplification
         return bblock::make(owner()->owner()), insn_oops::make(owner()->owner()->rear()),
            split(this), insn_br::make_bne(owner()->prev, std::move(rhs()), abs_0, owner(), owner()->owner()->rear()),
            insn_mov::make(this, std::move(dest()), std::move(lhs())), eliminate(), true;
      if ( lhs()->is_rel_base() && rhs()->is_rel_base() && lhs()->as_rel_base()->id == rhs()->as_rel_base()->id ||
           lhs()->is_rel_disp() && rhs()->is_rel_disp() && lhs()->as_rel_disp()->base->id == rhs()->as_rel_disp()->base->id &&
           lhs()->as_rel_disp()->add == rhs()->as_rel_disp()->add || lhs() == rhs() ) // algebraic simplification
         return bblock::make(owner()->owner()), insn_oops::make(owner()->owner()->rear()),
            split(this), insn_br::make_bne(owner()->prev, std::move(rhs()), abs_0, owner(), owner()->owner()->rear()),
            insn_mov::make(this, std::move(dest()), abs_1), eliminate(), true;
      return {};
   case _urem:
      if (rhs()->is_abs()) {
         if (!RSN_LIKELY(rhs()->as_abs()->val)) // x86 semantics
            return insn_oops::make(this), eliminate(), true;
         if (rhs()->as_abs()->val == 1) // algebraic simplification
            return insn_mov::make(this, std::move(dest()), abs_0), eliminate(), true;
         if (lhs()->is_abs()) // constant folding
            return insn_mov::make(this, std::move(dest()), abs::make(lhs()->as_abs()->val % rhs()->as_abs()->val)), eliminate(), true;
      }
      if ( lhs()->is_abs() && lhs()->as_abs()->val == 0 ||
           lhs()->is_rel_base() && rhs()->is_rel_base() && lhs()->as_rel_base()->id == rhs()->as_rel_base()->id ||
           lhs()->is_rel_disp() && rhs()->is_rel_disp() && lhs()->as_rel_disp()->base->id == rhs()->as_rel_disp()->base->id &&
           lhs()->as_rel_disp()->add == rhs()->as_rel_disp()->add || lhs() == rhs() ) // algebraic simplification
         return bblock::make(owner()->owner()), insn_oops::make(owner()->owner()->rear()),
            split(this), insn_br::make_bne(owner()->prev, std::move(rhs()), abs_0, owner(), owner()->owner()->rear()),
            insn_mov::make(this, std::move(dest()), abs_0), eliminate(), true;
      return {};
   case _smul:
      if (lhs()->is_abs()) {
         if (!rhs()->is_abs()) // canonicalization
            changed = (lhs().swap(rhs()), true);
      } else
      if (!lhs()->is_reg() && rhs()->is_reg()) // canonicalization
         return lhs().swap(rhs()), true;
      if (rhs()->is_abs()) {
         if (rhs()->as_abs()->val == 1) // algebraic simplification
            return insn_mov::make(this, std::move(dest()), std::move(lhs())), eliminate(), true;
         if (rhs()->as_abs()->val == 0) // algebraic simplification
            return insn_mov::make(this, std::move(dest()), abs_0), eliminate(), true;
         if (lhs()->is_abs()) // constant folding
            return insn_mov::make(this, std::move(dest()), abs::make((long long)lhs()->as_abs()->val * (long long)rhs()->as_abs()->val)), eliminate(), true;
      }
      return changed;
   case _sdiv:
      if (rhs()->is_abs()) {
         if (!RSN_LIKELY(rhs()->as_abs()->val)) // x86 semantics
            return insn_oops::make(this), eliminate(), true;
         if (rhs()->as_abs()->val == 1) // algebraic simplification
            return insn_mov::make(this, std::move(dest()), std::move(lhs())), eliminate(), true;
         if (lhs()->is_abs()) { // constant folding
            if (RSN_UNLIKELY(lhs()->as_abs()->val == std::numeric_limits<long long>::min()) && RSN_UNLIKELY(rhs()->as_abs()->val == -1)) // x86 semantics
               insn_oops::make(this);
            else
               insn_mov::make(this, std::move(dest()), abs::make((long long)lhs()->as_abs()->val / (long long)rhs()->as_abs()->val));
            return eliminate(), true;
         }
      }
      if (lhs()->is_abs() && lhs()->as_abs()->val == 0) // algebraic simplification
         return bblock::make(owner()->owner()), insn_oops::make(owner()->owner()->rear()),
            split(this), insn_br::make_bne(owner()->prev, std::move(rhs()), abs_0, owner(), owner()->owner()->rear()),
            insn_mov::make(this, std::move(dest()), std::move(lhs())), eliminate(), true;
      if ( lhs()->is_rel_base() && rhs()->is_rel_base() && lhs()->as_rel_base()->id == rhs()->as_rel_base()->id ||
           lhs()->is_rel_disp() && rhs()->is_rel_disp() && lhs()->as_rel_disp()->base->id == rhs()->as_rel_disp()->base->id &&
           lhs()->as_rel_disp()->add == rhs()->as_rel_disp()->add || lhs() == rhs() ) // algebraic simplification
         return bblock::make(owner()->owner()), insn_oops::make(owner()->owner()->rear()),
            split(this), insn_br::make_bne(owner()->prev, std::move(rhs()), abs_0, owner(), owner()->owner()->rear()),
            insn_mov::make(this, std::move(dest()), abs_1), eliminate(), true;
      return {};
   case _srem:
      if (rhs()->is_abs()) {
         if (!RSN_LIKELY(rhs()->as_abs()->val)) // x86 semantics
            return insn_oops::make(this), eliminate(), true;
         if (rhs()->as_abs()->val == 1) // algebraic simplification
            return insn_mov::make(this, std::move(dest()), abs_0), eliminate(), true;
         if (lhs()->is_abs()) { // constant folding
            if (RSN_UNLIKELY(lhs()->as_abs()->val == std::numeric_limits<long long>::min()) && RSN_UNLIKELY(rhs()->as_abs()->val == -1)) // x86 semantics
               insn_oops::make(this);
            else
               insn_mov::make(this, std::move(dest()), abs::make((long long)lhs()->as_abs()->val % (long long)rhs()->as_abs()->val));
            return eliminate(), true;
         }
      }
      if ( lhs()->is_abs() && lhs()->as_abs()->val == 0 ||
           lhs()->is_rel_base() && rhs()->is_rel_base() && lhs()->as_rel_base()->id == rhs()->as_rel_base()->id ||
           lhs()->is_rel_disp() && rhs()->is_rel_disp() && lhs()->as_rel_disp()->base->id == rhs()->as_rel_disp()->base->id &&
           lhs()->as_rel_disp()->add == rhs()->as_rel_disp()->add || lhs() == rhs() ) // algebraic simplification
         return bblock::make(owner()->owner()), insn_oops::make(owner()->owner()->rear()),
            split(this), insn_br::make_bne(owner()->prev, std::move(rhs()), abs_0, owner(), owner()->owner()->rear()),
            insn_mov::make(this, std::move(dest()), abs_0), eliminate(), true;
      return {};
   case _and:
      if (lhs()->is_abs()) {
         if (!rhs()->is_abs()) // canonicalization
            changed = (lhs().swap(rhs()), true);
      } else
      if (!lhs()->is_reg() && rhs()->is_reg()) // canonicalization
         return lhs().swap(rhs()), true;
      if (rhs()->is_abs()) {
         if (rhs()->as_abs()->val == ~0ull) // algebraic simplification
            return insn_mov::make(this, std::move(dest()), std::move(lhs())), eliminate(), true;
         if (rhs()->as_abs()->val == +0ull) // algebraic simplification
            return insn_mov::make(this, std::move(dest()), std::move(rhs())), eliminate(), true;
         if (lhs()->is_abs()) // constant folding
            return insn_mov::make(this, std::move(dest()), abs::make(lhs()->as_abs()->val & rhs()->as_abs()->val)), eliminate(), true;
      } else
      if ( lhs->is_rel_base() && rhs->is_rel_base() && lhs->as_rel_base()->id == rhs->as_rel_base()->id ||
           lhs->is_rel_disp() && rhs->is_rel_disp() && lhs->as_rel_disp()->base->id == rhs->as_rel_disp()->base->id &&
           lhs->as_rel_disp()->add == rhs->as_rel_disp()->add || lhs == rhs ) // algebraic simplification
         return insn_mov::make(this, std::move(dest()), std::move(lhs())), eliminate(), true;
      return changed;
   case _or:
      if (lhs()->is_abs()) {
         if (!rhs()->is_abs()) // canonicalization
            changed = (lhs().swap(rhs()), true);
      } else
      if (!lhs()->is_reg() && rhs()->is_reg()) // canonicalization
         return lhs().swap(rhs()), true;
      if (rhs()->is_abs()) {
         if (rhs()->as_abs()->val == +0ull) // algebraic simplification
            return insn_mov::make(this, std::move(dest()), std::move(lhs())), eliminate(), true;
         if (rhs()->as_abs()->val == ~0ull) // algebraic simplification
            return insn_mov::make(this, std::move(dest()), std::move(rhs())), eliminate(), true;
         if (lhs()->is_abs()) // constant folding
            return insn_mov::make(this, std::move(dest()), abs::make(lhs()->as_abs()->val | rhs()->as_abs()->val)), eliminate(), true;
      } else
      if ( lhs->is_rel_base() && rhs->is_rel_base() && lhs->as_rel_base()->id == rhs->as_rel_base()->id ||
           lhs->is_rel_disp() && rhs->is_rel_disp() && lhs->as_rel_disp()->base->id == rhs->as_rel_disp()->base->id &&
           lhs->as_rel_disp()->add == rhs->as_rel_disp()->add || lhs == rhs ) // algebraic simplification
         return insn_mov::make(this, std::move(dest()), std::move(lhs())), eliminate(), true;
      return changed;
   case _xor:
      if (lhs()->is_abs()) {
         if (!rhs()->is_abs()) // canonicalization
            changed = (lhs().swap(rhs()), true);
      } else
      if (!lhs()->is_reg() && rhs()->is_reg()) // canonicalization
         return lhs().swap(rhs()), true;
      if (rhs()->is_abs()) {
         if (rhs()->as_abs()->val == +0ull) // algebraic simplification
            return insn_mov::make(this, std::move(dest()), std::move(lhs())), eliminate(), true;
         if (lhs()->is_abs()) // constant folding
            return insn_mov::make(this, std::move(dest()), abs::make(lhs()->as_abs()->val ^ rhs()->as_abs()->val)), eliminate(), true;
      } else
      if ( lhs->is_rel_base() && rhs->is_rel_base() && lhs->as_rel_base()->id == rhs->as_rel_base()->id ||
           lhs->is_rel_disp() && rhs->is_rel_disp() && lhs->as_rel_disp()->base->id == rhs->as_rel_disp()->base->id &&
           lhs->as_rel_disp()->add == rhs->as_rel_disp()->add || lhs == rhs ) // algebraic simplification
         return insn_mov::make(this, std::move(dest()), std::move(lhs())), eliminate(), true;
      return changed;
   case _shl:
      if (rhs()->is_abs()) {
         if ((rhs()->as_abs()->val & 0x3F) == 0) // algebraic simplification (x86 semantics)
            return insn_mov::make(this, std::move(dest()), std::move(lhs())), eliminate(), true;
         if (lhs()->is_abs()) // constant folding (x86 semantics)
            return insn_mov::make(this, std::move(dest()), abs::make(lhs()->as_abs()->val << (rhs()->as_abs()->val & 0x3F))), eliminate(), true;
      }
      if (lhs()->is_abs() && lhs()->as_abs()->val == 0) // algebraic simplification
         return insn_mov::make(this, std::move(dest()), std::move(lhs()))), eliminate(), true;
      return {};
   case _ushr:
      if (rhs()->is_abs()) {
         if ((rhs()->as_abs()->val & 0x3F) == 0) // algebraic simplification (x86 semantics)
            return insn_mov::make(this, std::move(dest()), std::move(lhs())), eliminate(), true;
         if (lhs()->is_abs()) // constant folding (x86 semantics)
            return insn_mov::make(this, std::move(dest()), abs::make(lhs()->as_abs()->val >> (rhs()->as_abs()->val & 0x3F))), eliminate(), true;
      }
      if (lhs()->is_abs() && lhs()->as_abs()->val == 0) // algebraic simplification
         return insn_mov::make(this, std::move(dest()), std::move(lhs()))), eliminate(), true;
      return {};
   case _sshr:
      if (rhs()->is_abs()) {
         if ((rhs()->as_abs()->val & 0x3F) == 0) // algebraic simplification (x86 semantics)
            return insn_mov::make(this, std::move(dest()), std::move(lhs())), eliminate(), true;
         if (lhs()->is_abs()) // constant folding (x86 semantics)
            return insn_mov::make(this, std::move(dest()),
               abs::make((long long)lhs()->as_abs()->val >> (rhs()->as_abs()->val & 0x3F))), eliminate(), true;
      }
      if (lhs()->is_abs() && lhs()->as_abs()->val == 0) // algebraic simplification
         return insn_mov::make(this, std::move(dest()), std::move(lhs()))), eliminate(), true;
      return {};
   }
}

# if 0
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
# endif

} // namespace rsn::opt
