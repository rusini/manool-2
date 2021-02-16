// opt-simplify.cc

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
   static RSN_IF_WITH_MT(thread_local) const auto abs_0 = abs::make(0), abs_1 = abs::make(1);

   static RSN_INLINE inline void split(insn *in) { // split the BB at the insn
      auto bb = RSN_LIKELY(in->owner()->next()) ? bblock::make(in->owner()->next()) : bblock::make(in->owner()->owner());
      for (auto _in: all(in, {})) _in->reattach(bb);
   }
} // namespace rsn::opt


namespace rsn::opt { static bool simplify(insn_binop *); }
bool rsn::opt::insn_binop::simplify() { return opt::simplify(this); }

RSN_INLINE static inline bool rsn::opt::simplify(insn_binop *insn) {
   const auto eliminate = [insn]() noexcept RSN_INLINE -> void   { insn->eliminate(); };
   const auto owner     = [insn]() noexcept RSN_INLINE -> auto   { return insn->owner(); };
   const auto lhs       = [insn]() noexcept RSN_INLINE -> auto & { return insn->lhs(); };
   const auto rhs       = [insn]() noexcept RSN_INLINE -> auto & { return insn->rhs(); };
   const auto dest      = [insn]() noexcept RSN_INLINE -> auto & { return insn->dest(); };

   bool changed{};
   switch (insn->op) {
   default:
      RSN_UNREACHABLE();
   case insn_binop::_add:
      if (is<abs>(lhs())) {
         if (!is<abs>(rhs())) // canonicalization
            changed = (lhs().swap(rhs()), true);
      } else
      if (is<imm>(lhs()) && !is<imm>(rhs())) // canonicalization
         return lhs().swap(rhs()), true;
      if (is<abs>(rhs())) {
         if (as<abs>(rhs())->val == 0) // algebraic simplification
            return insn_mov::make(insn, std::move(lhs()), std::move(dest())), eliminate(), true;
         if (is<abs>(lhs())) // constant folding
            return insn_mov::make(insn, abs::make(as<abs>(lhs())->val + as<abs>(rhs())->val), std::move(dest())), eliminate(), true;
         if (is<rel_base>(lhs())) // constant folding
            return insn_mov::make(insn, rel_disp::make(as_smart<rel_base>(std::move(lhs())), +as<abs>(rhs())->val), std::move(dest())), eliminate(), true;
         if (is<rel_disp>(lhs())) // constant folding
            return insn_mov::make(insn, as<rel_disp>(lhs())->add + as<abs>(rhs())->val == 0 ? (lib::smart_ptr<operand>)as<rel_disp>(lhs())->base :
               (lib::smart_ptr<operand>)rel_disp::make(as<rel_disp>(lhs())->base, as<rel_disp>(lhs())->add + as<abs>(rhs())->val), std::move(dest())),
               eliminate(), true;
      }
      return changed;
   case insn_binop::_sub:
      if (lhs() == rhs()) // algebraic simplification
         return insn_mov::make(insn, abs_0, std::move(dest())), eliminate(), true;
      if (is<abs>(rhs())) {
         if (as<abs>(rhs())->val == 0) // algebraic simplification
            return insn_mov::make(insn, std::move(lhs()), std::move(dest())), eliminate(), true;
         if (is<abs>(lhs())) // constant folding
            return insn_mov::make(insn, abs::make(as<abs>(lhs())->val - as<abs>(rhs())->val), std::move(dest())), eliminate(), true;
         if (is<rel_base>(lhs())) // constant folding
            return insn_mov::make(insn, rel_disp::make(as_smart<rel_base>(std::move(lhs())), -as<abs>(rhs())->val), std::move(dest())), eliminate(), true;
         if (is<rel_disp>(lhs())) // constant folding
            return insn_mov::make(insn, as<rel_disp>(lhs())->add - as<abs>(rhs())->val == 0 ? (lib::smart_ptr<operand>)as<rel_disp>(lhs())->base :
               (lib::smart_ptr<operand>)rel_disp::make(as<rel_disp>(lhs())->base, as<rel_disp>(lhs())->add - as<abs>(rhs())->val), std::move(dest())),
               eliminate(), true;
         // canonicalization
         return insn_binop::make_add(insn, std::move(lhs()), abs::make(-as<abs>(rhs())->val), std::move(dest())), eliminate(), true;
      }
      if (is<rel_base>(lhs())) {
         if (is<rel_base>(rhs())) {
            if (as<rel_base>(lhs())->id == as<rel_base>(rhs())->id) // constant folding
               return insn_mov::make(insn, abs_0, std::move(dest())), eliminate(), true;
         } else
         if (is<rel_disp>(rhs())) {
            if (as<rel_base>(lhs())->id == as<rel_disp>(rhs())->base->id) // constant folding
               return insn_mov::make(insn, abs::make(-as<rel_disp>(rhs())->add), std::move(dest())), eliminate(), true;
         }
      } else
      if (is<rel_disp>(lhs())) {
         if (is<rel_base>(rhs())) {
            if (as<rel_disp>(lhs())->base->id == as<rel_base>(rhs())->id) // constant folding
               return insn_mov::make(insn, abs::make(+as<rel_disp>(lhs())->add), std::move(dest())), eliminate(), true;
         } else
         if (is<rel_disp>(rhs())) {
            if (as<rel_disp>(lhs())->base->id == as<rel_disp>(rhs())->base->id) // constant folding
               return insn_mov::make(insn, abs::make(as<rel_disp>(lhs())->add - as<rel_disp>(rhs())->add), std::move(dest())), eliminate(), true;
         }
      }
      return {};
   case insn_binop::_umul:
      if (is<abs>(lhs())) {
         if (!is<abs>(rhs())) // canonicalization
            changed = (lhs().swap(rhs()), true);
      } else
      if (is<imm>(lhs()) && !is<imm>(rhs())) // canonicalization
         return lhs().swap(rhs()), true;
      if (is<abs>(rhs())) {
         if (as<abs>(rhs())->val == 1) // algebraic simplification
            return insn_mov::make(insn, std::move(lhs()), std::move(dest())), eliminate(), true;
         if (as<abs>(rhs())->val == 0) // algebraic simplification
            return insn_mov::make(insn, std::move(rhs()), std::move(dest())), eliminate(), true;
         if (is<abs>(lhs())) // constant folding
            return insn_mov::make(insn, abs::make(as<abs>(lhs())->val * as<abs>(rhs())->val), std::move(dest())), eliminate(), true;
      }
      return changed;
   case insn_binop::_udiv:
      if (lhs() == rhs()) // algebraic simplification
         return insn_oops::make(bblock::make(owner()->owner())),
            split(insn), insn_br::make_bne(owner()->prev(), std::move(rhs()), abs_0, owner(), owner()->owner()->rear()),
            insn_mov::make(insn, abs_1, std::move(dest())), eliminate(), true;
      if (is<abs>(rhs())) {
         if (!RSN_LIKELY(as<abs>(rhs())->val)) // x86 semantics
            return insn_oops::make(insn), eliminate(), true;
         if (as<abs>(rhs())->val == 1) // algebraic simplification
            return insn_mov::make(insn, std::move(lhs()), std::move(dest())), eliminate(), true;
         if (is<abs>(lhs())) // constant folding
            return insn_mov::make(insn, abs::make(as<abs>(lhs())->val / as<abs>(rhs())->val), std::move(dest())), eliminate(), true;
      }
      if (is<abs>(lhs()) && as<abs>(lhs())->val == 0) // algebraic simplification
         return insn_oops::make(bblock::make(owner()->owner())),
            split(insn), insn_br::make_bne(owner()->prev(), std::move(rhs()), abs_0, owner(), owner()->owner()->rear()),
            insn_mov::make(insn, std::move(lhs()), std::move(dest())), eliminate(), true;
      if ( is<rel_base>(lhs()) && is<rel_base>(rhs()) && as<rel_base>(lhs())->id == as<rel_base>(rhs())->id ||
           is<rel_disp>(lhs()) && is<rel_disp>(rhs()) && as<rel_disp>(lhs())->base->id == as<rel_disp>(rhs())->base->id &&
           as<rel_disp>(lhs())->add == as<rel_disp>(rhs())->add ) // algebraic simplification
         return insn_oops::make(bblock::make(owner()->owner())),
            split(insn), insn_br::make_bne(owner()->prev(), std::move(rhs()), abs_0, owner(), owner()->owner()->rear()),
            insn_mov::make(insn, abs_1, std::move(dest())), eliminate(), true;
      return {};
   case insn_binop::_urem:
      if (lhs() == rhs()) // algebraic simplification
         return insn_oops::make(bblock::make(owner()->owner())),
            split(insn), insn_br::make_bne(owner()->prev(), std::move(rhs()), abs_0, owner(), owner()->owner()->rear()),
            insn_mov::make(insn, abs_0, std::move(dest())), eliminate(), true;
      if (is<abs>(rhs())) {
         if (!RSN_LIKELY(as<abs>(rhs())->val)) // x86 semantics
            return insn_oops::make(insn), eliminate(), true;
         if (as<abs>(rhs())->val == 1) // algebraic simplification
            return insn_mov::make(insn, abs_0, std::move(dest())), eliminate(), true;
         if (is<abs>(lhs())) // constant folding
            return insn_mov::make(insn, abs::make(as<abs>(lhs())->val % as<abs>(rhs())->val), std::move(dest())), eliminate(), true;
      }
      if (is<abs>(lhs()) && as<abs>(lhs())->val == 0) // algebraic simplification
         return insn_oops::make(bblock::make(owner()->owner())),
            split(insn), insn_br::make_bne(owner()->prev(), std::move(rhs()), abs_0, owner(), owner()->owner()->rear()),
            insn_mov::make(insn, std::move(lhs()), std::move(dest())), eliminate(), true;
      if ( is<rel_base>(lhs()) && is<rel_base>(rhs()) && as<rel_base>(lhs())->id == as<rel_base>(rhs())->id ||
           is<rel_disp>(lhs()) && is<rel_disp>(rhs()) && as<rel_disp>(lhs())->base->id == as<rel_disp>(rhs())->base->id &&
           as<rel_disp>(lhs())->add == as<rel_disp>(rhs())->add ) // algebraic simplification
         return insn_oops::make(bblock::make(owner()->owner())),
            split(insn), insn_br::make_bne(owner()->prev(), std::move(rhs()), abs_0, owner(), owner()->owner()->rear()),
            insn_mov::make(insn, abs_0, std::move(dest())), eliminate(), true;
      return {};
   case insn_binop::_smul:
      if (is<abs>(lhs())) {
         if (!is<abs>(rhs())) // canonicalization
            changed = (lhs().swap(rhs()), true);
      } else
      if (is<imm>(lhs()) && !is<imm>(rhs())) // canonicalization
         return lhs().swap(rhs()), true;
      if (is<abs>(rhs())) {
         if (as<abs>(rhs())->val == 1) // algebraic simplification
            return insn_mov::make(insn, std::move(lhs()), std::move(dest())), eliminate(), true;
         if (as<abs>(rhs())->val == 0) // algebraic simplification
            return insn_mov::make(insn, std::move(rhs()), std::move(dest())), eliminate(), true;
         if (is<abs>(lhs())) // constant folding
            return insn_mov::make(insn, abs::make((long long)as<abs>(lhs())->val * (long long)as<abs>(rhs())->val), std::move(dest())), eliminate(), true;
      }
      return changed;
   case insn_binop::_sdiv:
      if (lhs() == rhs()) // algebraic simplification
         return insn_oops::make(bblock::make(owner()->owner())),
            split(insn), insn_br::make_bne(owner()->prev(), std::move(rhs()), abs_0, owner(), owner()->owner()->rear()),
            insn_mov::make(insn, abs_1, std::move(dest())), eliminate(), true;
      if (is<abs>(rhs())) {
         if (!RSN_LIKELY(as<abs>(rhs())->val)) // x86 semantics
            return insn_oops::make(insn), eliminate(), true;
         if (as<abs>(rhs())->val == 1) // algebraic simplification
            return insn_mov::make(insn, std::move(lhs()), std::move(dest())), eliminate(), true;
         if (is<abs>(lhs())) { // constant folding
            if (RSN_UNLIKELY(as<abs>(lhs())->val == std::numeric_limits<long long>::min()) && RSN_UNLIKELY(as<abs>(rhs())->val == -1)) // x86 semantics
               return insn_oops::make(insn), eliminate(), true;
            return insn_mov::make(insn, abs::make((long long)as<abs>(lhs())->val / (long long)as<abs>(rhs())->val), std::move(dest())), eliminate(), true;
         }
      }
      if (is<abs>(lhs()) && as<abs>(lhs())->val == 0) // algebraic simplification
         return insn_oops::make(bblock::make(owner()->owner())),
            split(insn), insn_br::make_bne(owner()->prev(), std::move(rhs()), abs_0, owner(), owner()->owner()->rear()),
            insn_mov::make(insn, std::move(lhs()), std::move(dest())), eliminate(), true;
      if ( is<rel_base>(lhs()) && is<rel_base>(rhs()) && as<rel_base>(lhs())->id == as<rel_base>(rhs())->id ||
           is<rel_disp>(lhs()) && is<rel_disp>(rhs()) && as<rel_disp>(lhs())->base->id == as<rel_disp>(rhs())->base->id &&
           as<rel_disp>(lhs())->add == as<rel_disp>(rhs())->add ) // algebraic simplification
         return insn_oops::make(bblock::make(owner()->owner())),
            split(insn), insn_br::make_bne(owner()->prev(), std::move(rhs()), abs_0, owner(), owner()->owner()->rear()),
            insn_mov::make(insn, abs_1, std::move(dest())), eliminate(), true;
      return {};
   case insn_binop::_srem:
      if (lhs() == rhs()) // algebraic simplification
         return insn_oops::make(bblock::make(owner()->owner())),
            split(insn), insn_br::make_bne(owner()->prev(), std::move(rhs()), abs_0, owner(), owner()->owner()->rear()),
            insn_mov::make(insn, abs_0, std::move(dest())), eliminate(), true;
      if (is<abs>(rhs())) {
         if (!RSN_LIKELY(as<abs>(rhs())->val)) // x86 semantics
            return insn_oops::make(insn), eliminate(), true;
         if (as<abs>(rhs())->val == 1) // algebraic simplification
            return insn_mov::make(insn, abs_0, std::move(dest())), eliminate(), true;
         if (is<abs>(lhs())) { // constant folding
            if (RSN_UNLIKELY(as<abs>(lhs())->val == std::numeric_limits<long long>::min()) && RSN_UNLIKELY(as<abs>(rhs())->val == -1)) // x86 semantics
               return insn_oops::make(insn), eliminate(), true;
            return insn_mov::make(insn, abs::make((long long)as<abs>(lhs())->val % (long long)as<abs>(rhs())->val), std::move(dest())), eliminate(), true;
         }
      }
      if (is<abs>(lhs()) && as<abs>(lhs())->val == 0) // algebraic simplification
         return insn_oops::make(bblock::make(owner()->owner())),
            split(insn), insn_br::make_bne(owner()->prev(), std::move(rhs()), abs_0, owner(), owner()->owner()->rear()),
            insn_mov::make(insn, std::move(lhs()), std::move(dest())), eliminate(), true;
      if ( is<rel_base>(lhs()) && is<rel_base>(rhs()) && as<rel_base>(lhs())->id == as<rel_base>(rhs())->id ||
           is<rel_disp>(lhs()) && is<rel_disp>(rhs()) && as<rel_disp>(lhs())->base->id == as<rel_disp>(rhs())->base->id &&
           as<rel_disp>(lhs())->add == as<rel_disp>(rhs())->add ) // algebraic simplification
         return insn_oops::make(bblock::make(owner()->owner())),
            split(insn), insn_br::make_bne(owner()->prev(), std::move(rhs()), abs_0, owner(), owner()->owner()->rear()),
            insn_mov::make(insn, abs_0, std::move(dest())), eliminate(), true;
      return {};
   case insn_binop::_and:
      if (lhs() == rhs()) // algebraic simplification
         return insn_mov::make(insn, std::move(lhs()), std::move(dest())), eliminate(), true;
      if (is<abs>(lhs())) {
         if (!is<abs>(rhs())) // canonicalization
            changed = (lhs().swap(rhs()), true);
      } else
      if (is<imm>(lhs()) && !is<imm>(rhs())) // canonicalization
         return lhs().swap(rhs()), true;
      if (is<abs>(rhs())) {
         if (as<abs>(rhs())->val == ~0ull) // algebraic simplification
            return insn_mov::make(insn, std::move(lhs()), std::move(dest())), eliminate(), true;
         if (as<abs>(rhs())->val == +0ull) // algebraic simplification
            return insn_mov::make(insn, std::move(rhs()), std::move(dest())), eliminate(), true;
         if (is<abs>(lhs())) // constant folding
            return insn_mov::make(insn, abs::make(as<abs>(lhs())->val & as<abs>(rhs())->val), std::move(dest())), eliminate(), true;
      } else
      if ( is<rel_base>(lhs()) && is<rel_base>(rhs()) && as<rel_base>(lhs())->id == as<rel_base>(rhs())->id ||
           is<rel_disp>(lhs()) && is<rel_disp>(rhs()) && as<rel_disp>(lhs())->base->id == as<rel_disp>(rhs())->base->id &&
           as<rel_disp>(lhs())->add == as<rel_disp>(rhs())->add ) // algebraic simplification
         return insn_mov::make(insn, std::move(lhs()), std::move(dest())), eliminate(), true;
      return changed;
   case insn_binop::_or:
      if (lhs() == rhs()) // algebraic simplification
         return insn_mov::make(insn, std::move(lhs()), std::move(dest())), eliminate(), true;
      if (is<abs>(lhs())) {
         if (!is<abs>(rhs())) // canonicalization
            changed = (lhs().swap(rhs()), true);
      } else
      if (is<imm>(lhs()) && !is<imm>(rhs())) // canonicalization
         return lhs().swap(rhs()), true;
      if (is<abs>(rhs())) {
         if (as<abs>(rhs())->val == +0ull) // algebraic simplification
            return insn_mov::make(insn, std::move(lhs()), std::move(dest())), eliminate(), true;
         if (as<abs>(rhs())->val == ~0ull) // algebraic simplification
            return insn_mov::make(insn, std::move(rhs()), std::move(dest())), eliminate(), true;
         if (is<abs>(lhs())) // constant folding
            return insn_mov::make(insn, abs::make(as<abs>(lhs())->val | as<abs>(rhs())->val), std::move(dest())), eliminate(), true;
      } else
      if ( is<rel_base>(lhs()) && is<rel_base>(rhs()) && as<rel_base>(lhs())->id == as<rel_base>(rhs())->id ||
           is<rel_disp>(lhs()) && is<rel_disp>(rhs()) && as<rel_disp>(lhs())->base->id == as<rel_disp>(rhs())->base->id &&
           as<rel_disp>(lhs())->add == as<rel_disp>(rhs())->add ) // algebraic simplification
         return insn_mov::make(insn, std::move(lhs()), std::move(dest())), eliminate(), true;
      return changed;
   case insn_binop::_xor:
      if (lhs() == rhs()) // algebraic simplification
         return insn_mov::make(insn, abs_0, std::move(dest())), eliminate(), true;
      if (is<abs>(lhs())) {
         if (!is<abs>(rhs())) // canonicalization
            changed = (lhs().swap(rhs()), true);
      } else
      if (is<imm>(lhs()) && !is<imm>(rhs())) // canonicalization
         return lhs().swap(rhs()), true;
      if (is<abs>(rhs())) {
         if (as<abs>(rhs())->val == +0ull) // algebraic simplification
            return insn_mov::make(insn, std::move(lhs()), std::move(dest())), eliminate(), true;
         if (is<abs>(lhs())) // constant folding
            return insn_mov::make(insn, abs::make(as<abs>(lhs())->val ^ as<abs>(rhs())->val), std::move(dest())), eliminate(), true;
      } else
      if ( is<rel_base>(lhs()) && is<rel_base>(rhs()) && as<rel_base>(lhs())->id == as<rel_base>(rhs())->id ||
           is<rel_disp>(lhs()) && is<rel_disp>(rhs()) && as<rel_disp>(lhs())->base->id == as<rel_disp>(rhs())->base->id &&
           as<rel_disp>(lhs())->add == as<rel_disp>(rhs())->add ) // algebraic simplification
         return insn_mov::make(insn, abs_0, std::move(dest())), eliminate(), true;
      return changed;
   case insn_binop::_shl:
      if (is<abs>(rhs())) {
         if ((as<abs>(rhs())->val & 0x3F) == 0) // algebraic simplification (x86 semantics)
            return insn_mov::make(insn, std::move(lhs()), std::move(dest())), eliminate(), true;
         if (is<abs>(lhs())) // constant folding (x86 semantics)
            return insn_mov::make(insn, abs::make(as<abs>(lhs())->val << (as<abs>(rhs())->val & 0x3F)), std::move(dest())), eliminate(), true;
      }
      if (is<abs>(lhs()) && as<abs>(lhs())->val == 0) // algebraic simplification
         return insn_mov::make(insn, std::move(lhs()), std::move(dest())), eliminate(), true;
      return {};
   case insn_binop::_ushr:
      if (is<abs>(rhs())) {
         if ((as<abs>(rhs())->val & 0x3F) == 0) // algebraic simplification (x86 semantics)
            return insn_mov::make(insn, std::move(lhs()), std::move(dest())), eliminate(), true;
         if (is<abs>(lhs())) // constant folding (x86 semantics)
            return insn_mov::make(insn, abs::make(as<abs>(lhs())->val >> (as<abs>(rhs())->val & 0x3F)), std::move(dest())), eliminate(), true;
      }
      if (is<abs>(lhs()) && as<abs>(lhs())->val == 0) // algebraic simplification
         return insn_mov::make(insn, std::move(lhs()), std::move(dest())), eliminate(), true;
      return {};
   case insn_binop::_sshr:
      if (is<abs>(rhs())) {
         if ((as<abs>(rhs())->val & 0x3F) == 0) // algebraic simplification (x86 semantics)
            return insn_mov::make(insn, std::move(lhs()), std::move(dest())), eliminate(), true;
         if (is<abs>(lhs())) // constant folding (x86 semantics)
            return insn_mov::make(insn, abs::make((long long)as<abs>(lhs())->val >> (as<abs>(rhs())->val & 0x3F)), std::move(dest())), eliminate(), true;
      }
      if (is<abs>(lhs()) && as<abs>(lhs())->val == 0) // algebraic simplification
         return insn_mov::make(insn, std::move(lhs()), std::move(dest())), eliminate(), true;
      return {};
   }
}

namespace rsn::opt { static bool simplify(insn_br *); }
bool rsn::opt::insn_br::simplify() { return opt::simplify(this); }

RSN_INLINE static inline bool rsn::opt::simplify(insn_br *in) {
   const auto owner = [in]()RSN_INLINE{ return in->owner(); };
   const auto eliminate = [in]()RSN_INLINE{ in->eliminate(); };
   decltype(auto) lhs = in->lhs(), rhs = in->rhs();
   decltype(auto) dest1 = in->dest1(), dest2 = in->dest2();
   bool changed{};
   switch (in->op) {
   default:
      RSN_UNREACHABLE();
   case insn_br::_beq:
      if (is<abs>(lhs)) {
         if (!is<abs>(rhs)) // canonicalization
            changed = (lhs.swap(rhs), true);
      } else
      if (is<imm>(lhs) && !is<imm>(rhs)) // canonicalization
         return lhs.swap(rhs), true;
      if (is<abs>(lhs) && is<abs>(rhs)) // constant folding
         return insn_jmp::make(in, std::move(as<abs>(lhs)->val == as<abs>(rhs)->val ? dest1 : dest2)), eliminate(), true;
      return changed;
   case insn_br::_bult:
      if (is<abs>(lhs) && is<abs>(rhs)) // constant folding
         return insn_jmp::make(in, std::move(as<abs>(lhs)->val < as<abs>(rhs)->val ? dest1 : dest2)), eliminate(), true;
      return {};
   case insn_br::_bslt:
      if (is<abs>(lhs) && is<abs>(rhs)) // constant folding
         return insn_jmp::make(in, std::move((long long)as<abs>(lhs)->val < (long long)as<abs>(rhs)->val ? dest1 : dest2)), eliminate(), true;
      return {};
   }
}

namespace rsn::opt { static bool simplify(insn_switch_br *); }
bool rsn::opt::insn_switch_br::simplify() { return opt::simplify(this); }

RSN_INLINE static inline bool rsn::opt::simplify(insn_switch_br *in) {
   if (!is<abs>(in->index())) return {};
   // constant folding
   if (RSN_UNLIKELY(as<abs>(in->index())->val >= in->dests().size())) insn_oops::make(in), in->eliminate(), true;
   return insn_jmp::make(in, in->dests()[as<abs>(in->index())->val]), in->eliminate(), true;
}

namespace rsn::opt { static bool simplify(insn_call *); bool insn_call::simplify() { return opt::simplify(this); } }

RSN_INLINE inline bool rsn::opt::simplify(insn_call *insn) {
   // context shortcuts
   auto owner = [insn]() noexcept RSN_INLINE{ return insn->owner(); };
   auto next  = [insn]() noexcept RSN_INLINE{ return insn->next(); };
   auto prev  = [insn]() noexcept RSN_INLINE{ return insn->prev(); };
   auto eliminate = [insn]() noexcept RSN_INLINE{ insn->eliminate(); };
   const decltype(insn->dest())    _dest    = insn->dest();    auto dest    = [&_dest]() noexcept RSN_INLINE->auto &    { return _dest; };
   const decltype(insn->params())  _params  = insn->params();  auto params  = [&_params]() noexcept RSN_INLINE->auto &  { return _params; };
   const decltype(insn->results()) _results = insn->results(); auto results = [&_results]() noexcept RSN_INLINE->auto & { return _results; };

   if (RSN_LIKELY(!is<proc>(dest()))) return false;
   auto pc = as<proc>(dest());

   // temporary mappings
   auto bbmap = [&]() RSN_INLINE{
      std::vector<bblock *>::size_type count = 0;
      for (auto bb = pc->head(); bb; bb = bb->next()) ++count;
      std::vector<bblock *> res(count);
      return res;
   }();
   auto vrmap = [&]() RSN_INLINE{
      std::vector<lib::smart_ptr<vreg>>::size_type count = 0;
      for (auto bb = pc->head(); bb; bb = bb->next()) for (auto in = bb->head(); in; in = in->next()) {
         for (auto &input: in->inputs()) if (is<vreg>(input)) if (RSN_UNLIKELY(as<vreg>(input)->sn >= count)) count = as<vreg>(input)->sn + 1;
         for (auto &output: in->outputs()) if (RSN_UNLIKELY(output->sn >= count)) count = output->sn + 1;
      }
      std::vector<lib::smart_ptr<vreg>> res(count);
      for (; count; --count) res.emplace_back(vreg::make());
      return res;
   }();

   // integrate and expand the insn_entry
   if (RSN_UNLIKELY(as<insn_entry>(pc->head()->head())->params().size() != params().size()))
      insn_oops::make(insn);
   else for (std::size_t sn = 0; sn < params().size(); ++sn)
      insn_mov::make(insn, std::move(params()[sn]), vrmap[as<insn_entry>(pc->head()->head())->params()[sn]->sn]);
   // integrate the rest of entry BB
   for (auto in = pc->head()->head()->next(); in; in = in->next()) {
      in->clone(insn);
      for (auto &input: prev()->inputs()) if (is<vreg>(input)) input = vrmap[as<vreg>(input)->sn];
      for (auto &output: prev()->outputs()) output = vrmap[output->sn];
   }

   if (RSN_LIKELY(!pc->head()->next())) {
      // expand the insn_ret
      if (RSN_UNLIKELY(as<insn_ret>(prev())->results().size() != results().size()))
         insn_oops::make(prev());
      else
      for (std::size_t sn = 0; sn < results().size(); ++sn)
         insn_mov::make(prev(), std::move(as<insn_ret>(prev())->results()[sn]), std::move(results()[sn]));
      prev()->eliminate();
   } else {
      bbmap[pc->head()->sn] = owner();
      {  // split the BB at the insn_call
         auto bb = RSN_LIKELY(owner()->next()) ? bblock::make(owner()->next()) : bblock::make(owner()->owner());
         for (auto in: all(insn, {})) in->reattach(bb);
      }
      // integrate the rest of BBs
      for (auto bb = pc->head()->next(); bb; bb = bb->next()) {
         bbmap[bb->sn] = bblock::make(owner());
         // integrate instructions
         for (auto in = bb->head(); in; in = in->next()) {
            in->clone(owner()->prev());
            for (auto &input: owner()->prev()->rear()->inputs()) if (is<vreg>(input)) input = vrmap[as<vreg>(input)->sn];
            for (auto &output: owner()->prev()->rear()->outputs()) output = vrmap[output->sn];
         }
      }

      for (auto bb = pc->head(); bb; bb = bb->next())
      if (RSN_LIKELY(!is<insn_ret>(bbmap[bb->sn]->rear())))
         // fixup jump targets
         for (auto &target: bbmap[bb->sn]->rear()->targets()) target = bbmap[target->sn];
      else {
         // expand an insn_ret
         if (RSN_UNLIKELY(as<insn_ret>(bbmap[bb->sn]->rear())->results().size() != results().size()))
            insn_oops::make(bbmap[bb->sn]->rear());
         else
         for (std::size_t sn = 0; sn < results().size(); ++sn)
            insn_mov::make(bbmap[bb->sn]->rear(), std::move(as<insn_ret>(bbmap[bb->sn]->rear())->results()[sn]), results()[sn]);
         insn_jmp::make(bbmap[bb->sn]->rear(), owner());
         bbmap[bb->sn]->rear()->eliminate();
      }
   }

   return eliminate(), true;
}
