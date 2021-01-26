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
static RSN_IF_WITH_MT(thread_local) const auto abs_ones = abs_imm::make(~0ull);

static RSN_INLINE inline void split(insn *in) { // split the BB at the insn
   auto bb = RSN_LIKELY(in->owner()->next()) ? bblock::make(in->owner()->next()) : bblock::make(in->owner()->owner());
   for (auto _in: all(in, {})) _in->reattach(bb);
}

bool insn_binop::simplify() {
   bool changed{};
   switch (op) {
   default:
      RSN_UNREACHABLE();
   case _add:
      if (is<abs_imm>(lhs())) {
         if (!is<abs_imm>(rhs())) // canonicalization
            changed = (lhs().swap(rhs()), true);
      } else
      if (is<imm_val>(lhs()) && !is<imm_val>(rhs())) // canonicalization
         return lhs().swap(rhs()), true;
      if (is<abs_imm>(rhs())) {
         if (as<abs_imm>(rhs())->value == 0) // algebraic simplification
            return insn_mov::make(this, std::move(dest()), std::move(lhs())), eliminate(), true;
         if (is<abs_imm>(lhs())) // constant folding
            return insn_mov::make(this, std::move(dest()), abs_imm::make(as<abs_imm>(lhs())->value + as<abs_imm>(rhs())->value)), eliminate(), true;
         if (is<base_rel>(lhs())) // constant folding
            return insn_mov::make(this, std::move(dest()),
               disp_rel::make(as<base_rel>(lhs()), as<abs_imm>(rhs())->value), eliminate(), true;
         if (is<disp_rel>(lhs())) // constant folding
            return insn_mov::make(this, std::move(dest()),
               as<disp_rel>(lhs())->offset + as<abs_imm>(rhs())->value ?
               disp_rel::make(as<disp_rel>(lhs())->base, as<disp_rel>(lhs())->offset + as<abs_imm>(rhs())->value) : as<disp_rel>(lhs())->base,
               eliminate(), true;
      }
      return changed;
   case _sub:
      if (is<abs_imm>(rhs())) {
         if (as<abs_imm>(rhs())->value == 0) // algebraic simplification
            return insn_mov::make(this, std::move(dest()), std::move(lhs())), eliminate(), true;
         if (is<abs_imm>(lhs())) // constant folding
            return insn_mov::make(this, std::move(dest()), abs_imm::make(as<abs_imm>(lhs())->value - as<abs_imm>(rhs())->value)), eliminate(), true;
         if (is<base_rel>(lhs())) // constant folding
            return insn_mov::make(this, std::move(dest()),
               disp_rel::make(as<base_rel>(lhs()), -as<abs_imm>(rhs())->value), eliminate(), true;
         if (is<disp_rel>(lhs())) // constant folding
            return insn_mov::make(this, std::move(dest()),
               disp_rel::make(as<disp_rel>(lhs())->base, as<disp_rel>(lhs())->offset - as<abs_imm>(rhs())->value), eliminate(), true;
      }
      if (lhs()->equals(rhs())) // algebraic simplification
         return insn_mov::make(this, std::move(dest()), abs_0), eliminate(), true;
      return {};
   case _umul:
      if (is<abs_imm>(lhs())) {
         if (!is<abs_imm>(rhs())) // canonicalization
            changed = (lhs().swap(rhs()), true);
      } else
      if (is<imm_val>(lhs()) && !is<imm_val>(rhs())) // canonicalization
         return lhs().swap(rhs()), true;
      if (is<abs_imm>(rhs())) {
         if (as<abs_imm>(rhs())->value == 1) // algebraic simplification
            return insn_mov::make(this, std::move(dest()), std::move(lhs())), eliminate(), true;
         if (as<abs_imm>(rhs())->value == 0) // algebraic simplification
            return insn_mov::make(this, std::move(dest()), abs_0), eliminate(), true;
         if (is<abs_imm>(lhs())) // constant folding
            return insn_mov::make(this, std::move(dest()), abs_imm::make(as<abs_imm>(lhs())->value * as<abs_imm>(rhs())->value)), eliminate(), true;
      }
      return changed;
   case _udiv:
      if (is<abs_imm>(rhs())) {
         if (!RSN_LIKELY(as<abs_imm>(rhs())->value)) // x86 semantics
            return insn_oops::make(this), eliminate(), true;
         if (as<abs_imm>(rhs())->value == 1) // algebraic simplification
            return insn_mov::make(this, std::move(dest()), std::move(lhs())), eliminate(), true;
         if (is<abs_imm>(lhs())) // constant folding
            return insn_mov::make(this, std::move(dest()), abs_imm::make(as<abs_imm>(lhs())->value / as<abs_imm>(rhs())->value)), eliminate(), true;
      }
      if (is<abs_imm>(lhs()) && !as<abs_imm>(lhs())->value) { // algebraic simplification
         auto bb = bblock::make(owner()->owner()); insn_oops::make(bb); insn_br::make_bne(this, std::move(rhs()), abs_0, owner()->next(), bb), split(this);
         insn_mov::make(this, std::move(dest()), abs_0);
         return eliminate(), true;
      }
      if (lhs()->equals(rhs())) { // algebraic simplification
         auto bb = bblock::make(owner()->owner()); insn_oops::make(bb); insn_br::make_bne(this, std::move(rhs()), abs_0, owner()->next(), bb), split(this);
         return insn_mov::make(this, std::move(dest()), abs_1), eliminate(), true;
      }
      return {};
   case _urem:
      if (is<abs_imm>(rhs())) {
         if (!RSN_LIKELY(as<abs_imm>(rhs())->value)) // x86 semantics
            return insn_oops::make(this), eliminate(), true;
         if (as<abs_imm>(rhs())->value == 1) // algebraic simplification
            return insn_mov::make(this, std::move(dest()), abs_0), eliminate(), true;
         if (is<abs_imm>(lhs())) // constant folding
            return insn_mov::make(this, std::move(dest()), abs_imm::make(as<abs_imm>(lhs())->value % as<abs_imm>(rhs())->value)), eliminate(), true;
      }
      if (is<abs_imm>(lhs()) && !as<abs_imm>(lhs())->value || lhs()->equals(rhs())) { // algebraic simplification
         auto bb = bblock::make(owner()->owner()); insn_oops::make(bb); insn_br::make_bne(this, std::move(rhs()), abs_0, owner()->next(), bb), split(this);
         insn_mov::make(this, std::move(dest()), abs_0);
         return eliminate(), true;
      }
      return {};
   case _smul:
      if (is<abs_imm>(lhs())) {
         if (!is<abs_imm>(rhs())) // canonicalization
            changed = (lhs().swap(rhs()), true);
      } else
      if (is<imm_val>(lhs()) && !is<imm_val>(rhs())) // canonicalization
         return lhs().swap(rhs()), true;
      if (is<abs_imm>(rhs())) {
         if (as<abs_imm>(rhs())->value == 1) // algebraic simplification
            return insn_mov::make(this, std::move(dest()), std::move(lhs())), eliminate(), true;
         if (as<abs_imm>(rhs())->value == 0) // algebraic simplification
            return insn_mov::make(this, std::move(dest()), abs_0), eliminate(), true;
         if (is<abs_imm>(lhs())) // constant folding
            return insn_mov::make(this, std::move(dest()),
               abs_imm::make((long long)as<abs_imm>(lhs())->value * (long long)as<abs_imm>(rhs())->value)), eliminate(), true;
      }
      return changed;
   case _sdiv:
      if (is<abs_imm>(rhs())) {
         if (!RSN_LIKELY(as<abs_imm>(rhs())->value)) // x86 semantics
            return insn_oops::make(this), eliminate(), true;
         if (as<abs_imm>(rhs())->value == 1) // algebraic simplification
            return insn_mov::make(this, std::move(dest()), std::move(lhs())), eliminate(), true;
         if (is<abs_imm>(lhs())) { // constant folding
            if (RSN_UNLIKELY(as<abs_imm>(lhs())->value == std::numeric_limits<long long>::min()) && RSN_UNLIKELY(as<abs_imm>(rhs())->value == -1))
               insn_oops::make(this); // x86 semantics
            else
               insn_mov::make(this, std::move(dest()), abs_imm::make((long long)as<abs_imm>(lhs())->value / (long long)as<abs_imm>(rhs())->value));
            return eliminate(), true;
         }
      }
      if (is<abs_imm>(lhs()) && !as<abs_imm>(lhs())->value) { // algebraic simplification
         auto bb = bblock::make(owner()->owner()); insn_oops::make(bb); insn_br::make_bne(this, std::move(rhs()), abs_0, owner()->next(), bb), split(this);
         insn_mov::make(this, std::move(dest()), abs_0), eliminate();
         return true;
      }
      if (lhs()->equals(rhs())) { // algebraic simplification
         auto bb = bblock::make(owner()->owner()); insn_oops::make(bb); insn_br::make_bne(this, std::move(rhs()), abs_0, owner()->next(), bb), split(this);
         insn_mov::make(this, std::move(dest()), abs_1);
         return eliminate(), true;
      }
      return {};
   case _srem:
      if (is<abs_imm>(rhs())) {
         if (!RSN_LIKELY(as<abs_imm>(rhs())->value)) // x86 semantics
            return insn_oops::make(this), eliminate(), true;
         if (as<abs_imm>(rhs())->value == 1) // algebraic simplification
            return insn_mov::make(this, std::move(dest()), abs_0), eliminate(), true;
         if (is<abs_imm>(lhs())) { // constant folding
            if (RSN_UNLIKELY(as<abs_imm>(lhs())->value == std::numeric_limits<long long>::min()) && RSN_UNLIKELY(as<abs_imm>(rhs())->value == -1))
               insn_oops::make(this); // x86 semantics
            else
               insn_mov::make(this, std::move(dest()), abs_imm::make((long long)as<abs_imm>(lhs())->value % (long long)as<abs_imm>(rhs())->value));
            return eliminate(), true;
         }
      }
      if (is<abs_imm>(lhs()) && !as<abs_imm>(lhs())->value || lhs()->equals(rhs())) { // algebraic simplification
         auto bb = bblock::make(owner()->owner()); insn_oops::make(bb); insn_br::make_bne(this, std::move(rhs()), abs_0, owner()->next(), bb), split(this);
         insn_mov::make(this, std::move(dest()), abs_0);
         return eliminate(), true;
      }
      return {};


















      if (is<abs_imm>(lhs())) {
         if (is<abs_imm>(rhs())) // constant folding
            return insn_mov::make(this, std::move(dest()), abs_imm::make(as<abs_imm>(lhs())->value * as<abs_imm>(rhs())->value)), eliminate(), true;
         changed = (lhs().swap(rhs()), true); // canonicalization
      } else
      if (is<imm_val>(lhs()) && !is<imm_val>(rhs())) // canonicalization
         return lhs().swap(rhs()), true;
      if (is<abs_imm>(rhs())) {
         if (as<abs_imm>(rhs())->value == 1) // algebraic simplification
            return insn_mov::make(this, std::move(dest()), std::move(lhs())), eliminate(), true;
         if (as<abs_imm>(rhs())->value == 0) // algebraic simplification
            return insn_mov::make(this, std::move(dest()), abs_0), eliminate(), true;
      }
      return changed;







      if (is<abs_imm>(lhs()) && !is<abs_imm>(rhs()))
         changed = (lhs().swap(rhs()), true); // canonicalization
      else
      if (is<imm_val>(lhs()) && !is<imm_val>(rhs())) // canonicalization
         return lhs().swap(rhs()), true;
      if (is<abs_imm>(rhs())) {
         if (as<abs_imm>(rhs())->value == 0) // algebraic simplification
            return insn_mov::make(this, std::move(dest()), std::move(lhs())), eliminate(), true;
         if (is<abs_imm>(lhs())) // constant folding
            return insn_mov::make(this, std::move(dest()), abs_imm::make(as<abs_imm>(lhs())->value + as<abs_imm>(rhs())->value)), eliminate(), true;
         if (is<base_rel>(lhs())) // constant folding
            return insn_mov::make(this, std::move(dest()), disp_rel::make(as<base_rel>(lhs()), as<abs_imm>(rhs())->value), eliminate(), true;
         if (is<disp_rel>(lhs())) // constant folding
            return insn_mov::make(this, std::move(dest()), disp_rel::make(as<disp_rel>(lhs())->base, as<disp_rel>(lhs())->offset + as<abs_imm>(rhs())->value), eliminate(), true;
      }
      return changed;




      // algebraic simplification
         return insn_mov::make(this, std::move(dest()), std::move(lhs())), eliminate(), true;


      if (is<abs_imm>(rhs()) && as<abs_imm>(rhs())->value == 0) // algebraic simplification
         return insn_mov::make(this, std::move(dest()), std::move(lhs())), eliminate(), true;


   
      if (is<abs_imm>(lhs())) {
         if (is<abs_imm>(rhs())) // constant folding
            return insn_mov::make(this, std::move(dest()), abs_imm::make(as<abs_imm>(lhs())->value + as<abs_imm>(rhs())->value)), eliminate(), true;
         changed = (lhs().swap(rhs()), true); // canonicalization
      } else
      if (is<imm_val>(lhs()) && !is<imm_val>(rhs())) // canonicalization
         return lhs().swap(rhs()), true;
      if (is<abs_imm>(rhs()) && as<abs_imm>(rhs())->value == 0) // algebraic simplification
         return insn_mov::make(this, std::move(dest()), std::move(lhs())), eliminate(), true;
# if 0
      if (is<abs_imm>(rhs()) {
         if (as<abs_imm>(rhs())->value == 0) // algebraic simplification
            return insn_mov::make(this, std::move(dest()), std::move(lhs())), eliminate(), true;
         if (is<off_rel>(lhs())) // constant folding
            return insn_mov::make(this, std::move(dest()),
               off_rel::make(as<off_rel>(lhs())->base, as<off_rel>(lhs())->disp + as<abs_imm>(rhs())->value)), eliminate(), true;
         else
         if (is<rel_imm>(lhs())) // constant folding
            return insn_mov::make(this, std::move(dest()),
               off_rel::make(std::move(lhs()), as<abs_imm>(rhs())->value)), eliminate(), true;
      }



      if (is<rel_imm>(lhs())) {
         return is<off_rel>(lhs())
      }
      return changed;


      if (is<off_rel>(lhs())) {
         if (is<imm_val>(rhs())) off_rel::make();
            
      } else
      if (is<rel_imm>(lhs()) && is<imm_val>(lhs())) // constant folding
# endif
      return changed;
   case _sub:
      if (is<abs_imm>(rhs())) {
         if (is<abs_imm>(lhs())) // constant folding
            return insn_mov::make(this, std::move(dest()), abs_imm::make(as<abs_imm>(lhs())->value - as<abs_imm>(rhs())->value)), eliminate(), true;
         if (as<abs_imm>(rhs())->value == 0) // algebraic simplification
            return insn_mov::make(this, std::move(dest()), std::move(lhs())), eliminate(), true;
      }
      if (lhs()->equals(rhs())) // algebraic simplification
         return insn_mov::make(this, std::move(dest()), abs_0), eliminate(), true;
      return {};
   case _umul:
      if (is<abs_imm>(lhs())) {
         if (is<abs_imm>(rhs())) // constant folding
            return insn_mov::make(this, std::move(dest()), abs_imm::make(as<abs_imm>(lhs())->value * as<abs_imm>(rhs())->value)), eliminate(), true;
         changed = (lhs().swap(rhs()), true); // canonicalization
      } else
      if (is<imm_val>(lhs()) && !is<imm_val>(rhs())) // canonicalization
         return lhs().swap(rhs()), true;
      if (is<abs_imm>(rhs())) {
         if (as<abs_imm>(rhs())->value == 1) // algebraic simplification
            return insn_mov::make(this, std::move(dest()), std::move(lhs())), eliminate(), true;
         if (as<abs_imm>(rhs())->value == 0) // algebraic simplification
            return insn_mov::make(this, std::move(dest()), abs_0), eliminate(), true;
      }
      return changed;
   case _udiv:
      if (is<abs_imm>(rhs())) {
         if (!RSN_LIKELY(as<abs_imm>(rhs())->value)) // x86 semantics
            return insn_oops::make(this), eliminate(), true;
         if (is<abs_imm>(lhs())) // constant folding
            return insn_mov::make(this, std::move(dest()), abs_imm::make(as<abs_imm>(lhs())->value / as<abs_imm>(rhs())->value)), eliminate(), true;
         if (as<abs_imm>(rhs())->value == 1) // algebraic simplification
            return insn_mov::make(this, std::move(dest()), std::move(lhs())), eliminate(), true;
      }
      if (is<abs_imm>(lhs()) && !as<abs_imm>(lhs())->value) { // algebraic simplification
         auto bb = bblock::make(owner()->owner()); insn_oops::make(bb); insn_br::make_bne(this, std::move(rhs()), abs_0, owner()->next(), bb), split(this);
         insn_mov::make(this, std::move(dest()), abs_0);
         return eliminate(), true;
      }
      if (lhs()->equals(rhs())) { // algebraic simplification
         auto bb = bblock::make(owner()->owner()); insn_oops::make(bb); insn_br::make_bne(this, std::move(rhs()), abs_0, owner()->next(), bb), split(this);
         return insn_mov::make(this, std::move(dest()), abs_1), eliminate(), true;
      }
      return {};
   case _urem:
      if (is<abs_imm>(rhs())) {
         if (!RSN_LIKELY(as<abs_imm>(rhs())->value)) // x86 semantics
            return insn_oops::make(this), eliminate(), true;
         if (is<abs_imm>(lhs())) // constant folding
            return insn_mov::make(this, std::move(dest()), abs_imm::make(as<abs_imm>(lhs())->value % as<abs_imm>(rhs())->value)), eliminate(), true;
         if (as<abs_imm>(rhs())->value == 1) // algebraic simplification
            return insn_mov::make(this, std::move(dest()), abs_0), eliminate(), true;
      }
      if (is<abs_imm>(lhs()) && !as<abs_imm>(lhs())->value || lhs()->equals(rhs())) { // algebraic simplification
         auto bb = bblock::make(owner()->owner()); insn_oops::make(bb); insn_br::make_bne(this, std::move(rhs()), abs_0, owner()->next(), bb), split(this);
         insn_mov::make(this, std::move(dest()), abs_0);
         return eliminate(), true;
      }
      return {};
   case _smul:
      if (is<abs_imm>(lhs())) {
         if (is<abs_imm>(rhs())) // constant folding
            return insn_mov::make(this, std::move(dest()),
               abs_imm::make((long long)as<abs_imm>(lhs())->value * (long long)as<abs_imm>(rhs())->value)), eliminate(), true;
         changed = (lhs().swap(rhs()), true); // canonicalization
      } else
      if (is<imm_val>(lhs()) && !is<imm_val>(rhs())) // canonicalization
         return lhs().swap(rhs()), true;
      if (is<abs_imm>(rhs())) {
         if (as<abs_imm>(rhs())->value == 1) // algebraic simplification
            return insn_mov::make(this, std::move(dest()), std::move(lhs())), eliminate(), true;
         if (as<abs_imm>(rhs())->value == 0) // algebraic simplification
            return insn_mov::make(this, std::move(dest()), abs_0), eliminate(), true;
      }
      return changed;
   case _sdiv:
      if (is<abs_imm>(rhs())) {
         if (!RSN_LIKELY(as<abs_imm>(rhs())->value)) // x86 semantics
            return insn_oops::make(this), eliminate(), true;
         if (is<abs_imm>(lhs())) { // constant folding
            if (RSN_UNLIKELY(as<abs_imm>(lhs())->value == std::numeric_limits<long long>::min()) && RSN_UNLIKELY(as<abs_imm>(rhs())->value == -1))
               insn_oops::make(this); // x86 semantics
            else
               insn_mov::make(this, std::move(dest()), abs_imm::make((long long)as<abs_imm>(lhs())->value / (long long)as<abs_imm>(rhs())->value));
            return eliminate(), true;
         }
         if (as<abs_imm>(rhs())->value == 1) // algebraic simplification
            return insn_mov::make(this, std::move(dest()), std::move(lhs())), eliminate(), true;
      }
      if (is<abs_imm>(lhs()) && !as<abs_imm>(lhs())->value) { // algebraic simplification
         auto bb = bblock::make(owner()->owner()); insn_oops::make(bb); insn_br::make_bne(this, std::move(rhs()), abs_0, owner()->next(), bb), split(this);
         insn_mov::make(this, std::move(dest()), abs_0), eliminate();
         return true;
      }
      if (lhs()->equals(rhs())) { // algebraic simplification
         auto bb = bblock::make(owner()->owner()); insn_oops::make(bb); insn_br::make_bne(this, std::move(rhs()), abs_0, owner()->next(), bb), split(this);
         insn_mov::make(this, std::move(dest()), abs_1);
         return eliminate(), true;
      }
      return {};
   case _srem:
      if (is<abs_imm>(rhs())) {
         if (!RSN_LIKELY(as<abs_imm>(rhs())->value)) // x86 semantics
            return insn_oops::make(this), eliminate(), true;
         if (is<abs_imm>(lhs())) { // constant folding
            if (RSN_UNLIKELY(as<abs_imm>(lhs())->value == std::numeric_limits<long long>::min()) && RSN_UNLIKELY(as<abs_imm>(rhs())->value == -1))
               insn_oops::make(this); // x86 semantics
            else
               insn_mov::make(this, std::move(dest()), abs_imm::make((long long)as<abs_imm>(lhs())->value % (long long)as<abs_imm>(rhs())->value));
            return eliminate(), true;
         }
         if (as<abs_imm>(rhs())->value == 1) // algebraic simplification
            return insn_mov::make(this, std::move(dest()), abs_0), eliminate(), true;
      }
      if (is<abs_imm>(lhs()) && !as<abs_imm>(lhs())->value || lhs()->equals(rhs())) { // algebraic simplification
         auto bb = bblock::make(owner()->owner()); insn_oops::make(bb); insn_br::make_bne(this, std::move(rhs()), abs_0, owner()->next(), bb), split(this);
         insn_mov::make(this, std::move(dest()), abs_0);
         return eliminate(), true;
      }
      return {};
   case _and:
      if (is<abs_imm>(lhs())) {
         if (is<abs_imm>(rhs())) // constant folding
            return insn_mov::make(this, std::move(dest()), abs_imm::make(as<abs_imm>(lhs())->value & as<abs_imm>(rhs())->value)), eliminate(), true;
         changed = (lhs().swap(rhs()), true); // canonicalization
      } else
      if (is<imm_val>(lhs()) && !is<imm_val>(rhs())) // canonicalization
         return lhs().swap(rhs()), true;
      if (is<abs_imm>(rhs())) {
         if (as<abs_imm>(rhs())->value == ~0ull || lhs()->equals(rhs())) // algebraic simplification
            return insn_mov::make(this, std::move(dest()), std::move(lhs())), eliminate(), true;
         if (as<abs_imm>(rhs())->value == +0ull) // algebraic simplification
            return insn_mov::make(this, std::move(dest()), abs_0), eliminate(), true;
      }
      return changed;
   case _or:
      if (is<abs_imm>(lhs())) {
         if (is<abs_imm>(rhs())) // constant folding
            return insn_mov::make(this, std::move(dest()), abs_imm::make(as<abs_imm>(lhs())->value | as<abs_imm>(rhs())->value)), eliminate(), true;
         changed = (lhs().swap(rhs()), true); // canonicalization
      } else
      if (is<imm_val>(lhs()) && !is<imm_val>(rhs())) // canonicalization
         return lhs().swap(rhs()), true;
      if (is<abs_imm>(rhs())) {
         if (as<abs_imm>(rhs())->value == ~0ull || lhs()->equals(rhs())) // algebraic simplification
            return insn_mov::make(this, std::move(dest()), std::move(lhs())), eliminate(), true;
         if (as<abs_imm>(rhs())->value == +0ull) // algebraic simplification
            return insn_mov::make(this, std::move(dest()), abs_ones), eliminate(), true;
      }
      return changed;
   case _xor:
      if (is<abs_imm>(lhs())) {
         if (is<abs_imm>(rhs())) // constant folding
            return insn_mov::make(this, std::move(dest()), abs_imm::make(as<abs_imm>(lhs())->value ^ as<abs_imm>(rhs())->value)), eliminate(), true;
         changed = (lhs().swap(rhs()), true); // canonicalization
      } else
      if (is<imm_val>(lhs()) && !is<imm_val>(rhs())) // canonicalization
         return lhs().swap(rhs()), true;
      if (is<abs_imm>(rhs()) && as<abs_imm>(rhs())->value == +0ull) // algebraic simplification
         return insn_mov::make(this, std::move(dest()), std::move(lhs())), eliminate(), true;
      if (lhs()->equals(rhs())) // algebraic simplification
         return insn_mov::make(this, std::move(dest()), abs_0), eliminate(), true;
      return changed;
   case _shl:
      if (!is<abs_imm>(rhs())) {
         if (is<abs_imm>(lhs())) // constant folding (x86 semantics)
            return insn_mov::make(this, std::move(dest()), abs_imm::make(as<abs_imm>(lhs())->value << as<abs_imm>(rhs())->value & 0x3F)), eliminate(), true;
         if (!as<abs_imm>(rhs())->value) // algebraic simplification
            return insn_mov::make(this, std::move(dest()), std::move(lhs())), eliminate(), true;
      }
      if (is<abs_imm>(lhs()) && !as<abs_imm>(lhs())->value) // algebraic simplification
         return insn_mov::make(this, std::move(dest()), abs_0), eliminate(), true;
      return {};
   case _ushr:
      if (is<abs_imm>(rhs())) {
         if (is<abs_imm>(lhs())) // constant folding (x86 semantics)
            return insn_mov::make(this, std::move(dest()), abs_imm::make(as<abs_imm>(lhs())->value >> as<abs_imm>(rhs())->value & 0x3F)), eliminate(), true;
         if (!as<abs_imm>(rhs())->value) // algebraic simplification
            return insn_mov::make(this, std::move(dest()), std::move(lhs())), eliminate(), true;
      }
      if (is<abs_imm>(lhs()) && !as<abs_imm>(lhs())->value) // algebraic simplification
         return insn_mov::make(this, std::move(dest()), abs_0), eliminate(), true;
      return {};
   case _sshr:
      if (is<abs_imm>(rhs())) {
         if (is<abs_imm>(lhs())) // constant folding (x86 semantics)
            return insn_mov::make(this, std::move(dest()),
               abs_imm::make((long long)as<abs_imm>(lhs())->value >> as<abs_imm>(rhs())->value & 0x3F)), eliminate(), true;
         if (!as<abs_imm>(rhs())->value) // algebraic simplification
            return insn_mov::make(this, std::move(dest()), std::move(lhs())), eliminate(), true;
      }
      if (is<abs_imm>(lhs()) && !as<abs_imm>(lhs())->value) // algebraic simplification
         return insn_mov::make(this, std::move(dest()), abs_0), eliminate(), true;
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
