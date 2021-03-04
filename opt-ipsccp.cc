// ssa.cc -- construction of static single assignment form

/*    Copyright (C) 2020, 2021 Alexey Protasov (AKA Alex or rusini)

   This is free software: you can redistribute it and/or modify it under the terms of the version 3 of the GNU General Public License
   as published by the Free Software Foundation (and only version 3).

   This software is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along with this software.  If not, see <https://www.gnu.org/licenses/>.  */


# include "ir.hh"

namespace rsn::opt { void transform_ipsccp(proc *); }

void rsn::opt::transform_ipsccp(proc *pc) {
   std::size_t bb_count = 0, vr_count = 0;
   // Number BBs and VRs ///////////////////////////////////////////////////////////////////////////
   for (auto bb = pc->head(); bb; bb = bb->next()) {
      bb->sn = bb_count++;
      for (auto in = bb->head(); in; in = in->next()) {
         for (const auto &input: in->inputs()) if (is<vreg>(input)) as<vreg>(input)->sn = -1;
         for (const auto &output: in->outputs()) output->sn = -1;
      }
   }
   for (auto bb = pc->head(); bb; bb = bb->next()) for (auto in = bb->head(); in; in = in->next()) {
      for (const auto &input: in->inputs()) if (is<vreg>(input) && RSN_UNLIKELY(as<vreg>(input)->sn == -1)) as<vreg>(input)->sn = vr_count++;
      for (const auto &output: in->outputs()) if (RSN_UNLIKELY(output->sn == -1)) output->sn = vr_count++;
   }

   std::vector<std::vector<bblock *>> preds(bb_count), succs(bb_count);
   // Build BB Predecessor and Successor Lists (using preorder DFS) ////////////////////////////////
   {  std::vector<signed char> visited(bb_count);
      auto traverse = [&](auto &traverse, bblock *bb) RSN_NOINLINE{
         if (RSN_UNLIKELY(visited[bb->sn])) return;
         visited[bb->sn] = true;
         for (const auto &target: bb->rear()->targets())
         if (preds[target->sn].empty() || RSN_LIKELY(preds[target->sn].back() != bb))
            preds[target->sn].push_back(bb), succs[bb->sn].push_back(target);
         for (auto succ: succs[bb->sn]) traverse(traverse, succ);
      };
      traverse(traverse, pc->head());
   }

   std::vector<std::vector<operand *>> lattice(in_count);
   for (auto bb = pc->head(); bb; bb = bb->next()) for (auto in = bb->head(); in; in = in->next()) lattice[in->sn].resize(in->outputs().size());

   for (;;) {
      bool changed = false;
      for (auto bb = pc->head(); bb; bb = bb->next())
      for (auto in = bb->head(); in; in = in->next()) {
         std::vector<operand *> _input;
         for (const auto &input: in->inputs()) {
            if (is<imm>(input)) { _input.push_back(input); continue; }
            std::vector<signed char> visited(in_count);
            auto traverse = [&](auto &traverse, insn *in, vreg *vr) noexcept->vreg * RSN_NOINLINE{
               if (RSN_UNLIKELY(visited[in->sn])) return {};
               visited[in->sn] = true;
               for (auto _in = in->prev(); _in; _in = _in->prev()) {
                  std::size_t sn = 0; for (const auto &output: _in->outputs()) { if (RSN_UNLIKELY(output == vr)) return lattice[_in->sn][sn]; ++sn; }
               }
               if (RSN_UNLIKELY(preds[in->Owner()->sn].empty())) return {};
               auto res = traverse(traverse, lib::range_ref(preds[in->Owner()->sn]).first()->rear(), vr);
               for (auto pred: lib::range_ref(preds[in->Owner()->sn]).drop_first()) {
                  auto _res = traverse(traverse, pred->rear(), vr);
                  if (!_res) continue;
                  if (!res) { res = _res; continue; }
                  if (is<abs>(res) && is<abs>(_res) && as<abs>(res)->val == as<abs>(_res)->val) continue;
                  if (is<rel_base>(res) && (is<proc>(_res) || is<data>(_res)) && as<rel_base>(res)->id == as<rel_base>(_res)->id) { res = _res; continue; }
                  if (is<rel_base>(res) && is<rel_base>(_res) && as<rel_base>(res)->id == as<rel_base>(_res)->id) continue;
                  if (is<rel_disp>(res) && is<rel_disp>(_res) && as<rel_disp>(res)->base->id == as<rel_disp>(_res)->base->id &&
                     as<rel_disp>(res)->add == as<rel_disp>(_res)->add) continue;
                  res = vr;
               }
               return res;
            };
            _input.push_back(traverse(traverse, in, as<vreg>(input)));
         }
         auto _output = in->eval(_input);
         for (std::size_t sn = 0; sn < _output.size(); ++sn) {
            if (!_output[sn] && !lattice[in->sn][sn]) continue;
            if (is<abs>(_output[sn]) && is<abs>(lattice[in->sn][sn]) && as<abs>(_output[sn])->val == as<abs>(lattice[in->sn][sn])->val) continue;
            if (is<rel_base>(_output[sn]) && is<rel_base>(lattice[in->sn][sn]) && as<rel_base>(_output[sn])->id == as<rel_base>(lattice[in->sn][sn])->id) continue;
            if (is<rel_disp>(_output[sn]) && is<rel_disp>(lattice[in->sn][sn]) && as<rel_disp>(_output[sn])->base->id == as<rel_base>(lattice[in->sn][sn])->base->id &&
               as<rel_disp>(_output[sn])->add == as<rel_base>(lattice[in->sn][sn])->add) continue;
            if (is<vreg>(_output[sn]) && is<vreg>(lattice[in->sn][sn]) continue;
            changed |= true;
         }
         lattice[in->sn][sn] = _output;
      }
      if (RSN_UNLIKELY(!changed)) break;
   }

   for (auto &it: lattice) {
      bool flag = true; for (auto _it: it) flag &= _it && is<imm>(_it);
      if (RSN_UNLIKELY(flag))
   }
}


namespace rsn::opt {
   // Some absolute immediate operands to share
   static RSN_IF_WITH_MT(thread_local) const auto abs_0 = abs::make(0), abs_1 = abs::make(1);

   static RSN_INLINE inline void split(insn *in) { // split the BB at the insn
      auto bb = RSN_LIKELY(in->owner()->next()) ? bblock::make(in->owner()->next()) : bblock::make(in->owner()->owner());
      for (auto _in: all(in, {})) _in->reattach(bb);
   }
} // namespace rsn::opt


namespace rsn::opt {
   static inline std::vector<operand *> evaluate(decltype(insn_binop::op), std::vector<operand *> &&);
   std::vector<operand *> rsn::opt::insn_binop::evaluate(std::vector<operand *> &&inputs) { return opt::evaluate(op, std::move(inputs)); }
}

RSN_INLINE std::vector<operand *> rsn::opt::evaluate(decltype(insn_binop::op) op, std::vector<operand *> &&inputs) {
   // context shortcuts
   const decltype((inputs [0])) _lhs  = inputs [0]; auto dest = [&_lhs]() noexcept RSN_INLINE->auto &  { return _lhs; };
   const decltype((inputs [1])) _rhs  = inputs [1]; auto dest = [&_rhs]() noexcept RSN_INLINE->auto &  { return _rhs; };
   const decltype((outputs[0])) _dest = outputs[0]; auto dest = [&_dest]() noexcept RSN_INLINE->auto & { return _dest; };

   if (!lhs || !rhs)
      return {{}};
   switch (insn->op) {
   default:
      RSN_UNREACHABLE();
   case insn_binop::_add:
      if (is<abs>(lhs) && !is<abs>(rhs))
         swap(lhs, rhs);
      if (is<abs>(rhs) && RSN_UNLIKELY(as<abs>(rhs)->val == 0))
         return {std::move(lhs)};
      if (is<abs>(lhs) && is<abs>(rhs))
         return {abs::make(as<abs>(lhs)->val + as<abs>(rhs)->val)};
      if (is<rel_base>(lhs) && is<abs>(rhs))
         return {rel_disp::make(as<rel_base>(lhs), +as<abs>(rhs)->val)};
      if (is<rel_disp>(lhs) && is<abs>(rhs))
         return {!RSN_LIKELY(as<rel_disp>(lhs)->add + as<abs>(rhs)->val) ? (lib::smart_ptr<operand>)as<rel_disp>(lhs)->base :
            (lib::smart_ptr<operand>)rel_disp::make(as<rel_disp>(lhs)->base, as<rel_disp>(lhs)->add + as<abs>(rhs)->val)};
      return {insn->dest()};
   case insn_binop::_sub:
      if (lhs == rhs)
         return {abs_0};
      if (is<abs>(rhs) && RSN_UNLIKELY(as<abs>(rhs)->val == 0))
         return {std::move(lhs)};
      if (is<abs>(lhs) && is<abs>(rhs))
         return {abs::make(as<abs>(lhs)->val - as<abs>(rhs)->val)};
      if (is<rel_base>(lhs) && is<abs>(rhs))
         return {rel_disp::make(as<rel_base>(lhs), -as<abs>(rhs)->val)};
      if (is<rel_disp>(lhs) && is<abs>(rhs))
         return {!RSN_LIKELY(as<rel_disp>(lhs)->add + as<abs>(rhs)->val) ? (lib::smart_ptr<operand>)as<rel_disp>(lhs)->base :
            (lib::smart_ptr<operand>)rel_disp::make(as<rel_disp>(lhs)->base, as<rel_disp>(lhs)->add - as<abs>(rhs)->val)};
      if (is<rel_base>(lhs) && is<rel_base>(rhs) && as<rel_base>(lhs)->id == as<rel_base>(rhs)->id)
         return {abs_0};
      if (is<rel_disp>(lhs) && is<rel_base>(rhs) && as<rel_disp>(lhs)->base->id == as<rel_base>(rhs)->id)
         return {abs::make(+as<rel_disp>(lhs)->add)};
      if (is<rel_disp>(rhs) && is<rel_base>(lhs) && as<rel_disp>(rhs)->base->id == as<rel_base>(lhs)->id)
         return {abs::make(-as<rel_disp>(rhs)->add)};
      if (is<rel_disp>(lhs) && is<rel_disp>(rhs) && as<rel_disp>(lhs)->base->id == as<rel_disp>(rhs)->base->id)
         return {abs::make(as<rel_disp>(lhs)->add - as<rel_disp>(rhs)->add)};
      return {insn->dest()};
   case insn_binop::_mul:
      if (is<abs>(lhs) && !is<abs>(rhs))
         swap(lhs, rhs);
      if (is<abs>(rhs) && RSN_UNLIKELY(as<abs>(rhs)->val == 0))
         return {std::move(rhs)};
      if (is<abs>(rhs) && RSN_UNLIKELY(as<abs>(rhs)->val == 1))
         return {std::move(lhs)};
      if (is<abs>(lhs) && is<abs>(rhs))
         return {abs::make(as<abs>(lhs)->val * as<abs>(rhs)->val)};
      return {insn->dest()};
   case insn_binop::_udiv:
      if (lhs == rhs)
         return {abs_1};
      if (is<abs>(rhs) && as<abs>(rhs)->val == 0)
         return {bot};
      if (is<abs>(rhs) && as<abs>(rhs)->val == 1)
         return {std::move(lhs)};
      if (is<abs>(lhs) && is<abs>(rhs))
         return {abs::make(as<abs>(lhs)->val / as<abs>(rhs)->val)};
      if ( is<rel_base>(lhs) && is<rel_base>(rhs) && as<rel_base>(lhs)->id == as<rel_base>(rhs)->id ||
           is<rel_disp>(lhs) && is<rel_disp>(rhs) && as<rel_disp>(lhs)->base->id == as<rel_disp>(rhs)->base->id || as<rel_disp>(lhs)->add == as<rel_disp>(rhs)->add )
         return {abs_1};
      return {insn->dest()};
   case insn_binop::_urem:
      if (is<abs>(rhs) && as<abs>(rhs)->val == 0)
         return {bot};
      if (is<abs>(rhs) && as<abs>(rhs)->val == 1)
         return {std::move(lhs)};
      if (is<abs>(lhs) && is<abs>(rhs))
         return {abs::make(as<abs>(lhs)->val % as<abs>(rhs)->val)};
      return {insn->dest()};
   case insn_binop::_sdiv:
      if (is<abs>(rhs) && as<abs>(rhs)->val == 0)
         return {bot};
      if (is<abs>(rhs) && as<abs>(rhs)->val == 1)
         return {std::move(lhs)};
      if (is<abs>(lhs) && is<abs>(rhs) && (as<abs>(lhs)->val != std::numeric_limits<long long>::max || as<abs>(rhs)->val != -1))
         return {abs::make((long long)as<abs>(lhs)->val / (long long)as<abs>(rhs)->val)};
      return {insn->dest()};
   case insn_binop::_srem:
      if (is<abs>(rhs) && as<abs>(rhs)->val == 0)
         return {bot};
      if (is<abs>(rhs) && as<abs>(rhs)->val == 1)
         return {std::move(lhs)};
      if (is<abs>(lhs) && is<abs>(rhs) && (as<abs>(lhs)->val != std::numeric_limits<long long>::max || as<abs>(rhs)->val != -1))
         return {abs::make((long long)as<abs>(lhs)->val % (long long)as<abs>(rhs)->val)};
      return {insn->dest()};
   case insn_binop::_and:
      if (lhs == rhs)
         return {std::move(lhs)};
      if (is<abs>(lhs) && !is<abs>(rhs))
         swap(lhs, rhs);
      if (is<abs>(rhs) && RSN_UNLIKELY(as<abs>(rhs)->val == +0ull))
         return {std::move(rhs)};
      if (is<abs>(rhs) && RSN_UNLIKELY(as<abs>(rhs)->val == ~0ull))
         return {std::move(lhs)};
      if (is<abs>(lhs) && is<abs>(rhs))
         return {abs::make(as<abs>(lhs)->val & as<abs>(rhs)->val)};
      if ( is<rel_base>(lhs) && is<rel_base>(rhs) && as<rel_base>(lhs)->id == as<rel_base>(rhs)->id ||
           is<rel_disp>(lhs) && is<rel_disp>(rhs) && as<rel_disp>(lhs)->base->id == as<rel_disp>(rhs)->base->id && as<rel_disp>(lhs)->add == as<rel_disp>(rhs)->add )
         return {std::move(lhs)};
      return {insn->dest()};
   case insn_binop::_or:
      if (lhs == rhs)
         return {std::move(lhs)};
      if (is<abs>(lhs) && !is<abs>(rhs))
         swap(lhs, rhs);
      if (is<abs>(rhs) && RSN_UNLIKELY(as<abs>(rhs)->val == ~0ull))
         return {std::move(rhs)};
      if (is<abs>(rhs) && RSN_UNLIKELY(as<abs>(rhs)->val == +0ull))
         return {std::move(lhs)};
      if (is<abs>(lhs) && is<abs>(rhs))
         return {abs::make(as<abs>(lhs)->val | as<abs>(rhs)->val)};
      if ( is<rel_base>(lhs) && is<rel_base>(rhs) && as<rel_base>(lhs)->id == as<rel_base>(rhs)->id ||
           is<rel_disp>(lhs) && is<rel_disp>(rhs) && as<rel_disp>(lhs)->base->id == as<rel_disp>(rhs)->base->id && as<rel_disp>(lhs)->add == as<rel_disp>(rhs)->add )
         return {std::move(lhs)};
      return {insn->dest()};
   case insn_binop::_xor:
      if (lhs == rhs)
         return {std::move(lhs)};
      if (is<abs>(lhs) && !is<abs>(rhs))
         swap(lhs, rhs);
      if (is<abs>(rhs) && RSN_UNLIKELY(as<abs>(rhs)->val == +0ull))
         return {std::move(lhs)};
      if (is<abs>(lhs) && is<abs>(rhs))
         return {abs::make(as<abs>(lhs)->val | as<abs>(rhs)->val)};
      if ( is<rel_base>(lhs) && is<rel_base>(rhs) && as<rel_base>(lhs)->id == as<rel_base>(rhs)->id ||
           is<rel_disp>(lhs) && is<rel_disp>(rhs) && as<rel_disp>(lhs)->base->id == as<rel_disp>(rhs)->base->id && as<rel_disp>(lhs)->add == as<rel_disp>(rhs)->add )
         return {std::move(lhs)};
      return {insn->dest()};
   case insn_binop::_shl:
      if (is<abs>(rhs) && RSN_UNLIKELY((as<abs>(rhs)->val & 0x3F) == 0))
         return {std::move(lhs)};
      if (is<abs>(lhs) && RSN_UNLIKELY(as<abs>(lhs)->val == 0)
         return {std::move(lhs)};
      if (is<abs>(lhs) && is<abs>(rhs))
         return {abs::make(as<abs>(lhs)->val << (as<abs>(rhs)->val & 0x3F))};
      return {insn->dest()};
   case insn_binop::_ushr:
      if (is<abs>(rhs) && RSN_UNLIKELY((as<abs>(rhs)->val & 0x3F) == 0))
         return {std::move(lhs)};
      if (is<abs>(lhs) && RSN_UNLIKELY(as<abs>(lhs)->val == 0)
         return {std::move(lhs)};
      if (is<abs>(lhs) && is<abs>(rhs))
         return {abs::make(as<abs>(lhs)->val >> (as<abs>(rhs)->val & 0x3F))};
      return {insn->dest()};
   case insn_binop::_sshr:
      if (is<abs>(rhs) && RSN_UNLIKELY((as<abs>(rhs)->val & 0x3F) == 0))
         return {std::move(lhs)};
      if (is<abs>(lhs) && RSN_UNLIKELY(as<abs>(lhs)->val == 0)
         return {std::move(lhs)};
      if (is<abs>(lhs) && is<abs>(rhs))
         return {(long long)abs::make(as<abs>(lhs)->val >> (as<abs>(rhs)->val & 0x3F))};
      return {insn->dest()};






      if (is<abs>(lhs())) {
         if (!is<abs>(rhs())) // canonicalization
            changed = (lhs().swap(rhs()), true);
      } else
      if (is<imm>(lhs()) && !is<imm>(rhs())) // canonicalization
         return {dest()};
      if (is<abs>(rhs())) {
         if (as<abs>(rhs())->val == 0) // algebraic simplification
            return {lhs()};
         if (is<abs>(lhs())) // constant folding
            return {abs::make(as<abs>(lhs())->val + as<abs>(rhs())->val)};
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

namespace rsn::opt { static bool simplify(insn_br *); bool insn_br::simplify() { return opt::simplify(this); } }

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

namespace rsn::opt { static bool simplify(insn_switch_br *); bool insn_switch_br::simplify() { return opt::simplify(this); } }

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
      for (; count; --count) res.push_back(vreg::make());
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
