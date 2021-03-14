// ssa.cc -- construction of static single assignment form

/*    Copyright (C) 2020, 2021 Alexey Protasov (AKA Alex or rusini)

   This is free software: you can redistribute it and/or modify it under the terms of the version 3 of the GNU General Public License
   as published by the Free Software Foundation (and only version 3).

   This software is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along with this software.  If not, see <https://www.gnu.org/licenses/>.  */


# include "ir.hh"

# if 0
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
# endif

namespace rsn::opt { // Forward declarations
   static inline lib::smart_ptr<operand> evaluate(const insn_binop *, lib::smart_ptr<operand> &&, lib::smart_ptr<operand> &&);
   static inline std::vector<bblock *> evaluate(const insn_br *, lib::smart_ptr<operand> &&, lib::smart_ptr<operand> &&);
   static inline std::vector<bblock *> evaluate(const insn_switch_br *, lib::smart_ptr<operand> &&);
}
namespace rsn::opt { // Interpret an operation over lattice values
   auto insn_entry
   ::evaluate(std::vector<lib::smart_ptr<operand>> &&inputs) const->std::tuple<std::vector<lib::smart_ptr<operand>>, std::vector<bblock *>>
      { return {std::vector<lib::smart_ptr<operand>>(params().begin(), params().end()), {}}; }
   auto insn_ret
   ::evaluate(std::vector<lib::smart_ptr<operand>> &&inputs) const->std::tuple<std::vector<lib::smart_ptr<operand>>, std::vector<bblock *>>
      { return {{}, {}}; }
   auto insn_call
   ::evaluate(std::vector<lib::smart_ptr<operand>> &&inputs) const->std::tuple<std::vector<lib::smart_ptr<operand>>, std::vector<bblock *>>
      { return {std::vector<lib::smart_ptr<operand>>(results().begin(), results().end()), {}}; }
   auto insn_mov
   ::evaluate(std::vector<lib::smart_ptr<operand>> &&inputs) const->std::tuple<std::vector<lib::smart_ptr<operand>>, std::vector<bblock *>>
      { std::vector<lib::smart_ptr<operand>> outputs; outputs.push_back(std::move(inputs[0]));
        return {std::move(outputs), {}}; }
   auto insn_load
   ::evaluate(std::vector<lib::smart_ptr<operand>> &&inputs) const->std::tuple<std::vector<lib::smart_ptr<operand>>, std::vector<bblock *>>
      { return {{dest()}, {}}; }
   auto insn_store
   ::evaluate(std::vector<lib::smart_ptr<operand>> &&inputs) const->std::tuple<std::vector<lib::smart_ptr<operand>>, std::vector<bblock *>>
      { return {{}, {}}; }
   auto insn_binop
   ::evaluate(std::vector<lib::smart_ptr<operand>> &&inputs) const->std::tuple<std::vector<lib::smart_ptr<operand>>, std::vector<bblock *>>
      { std::vector<lib::smart_ptr<operand>> outputs; outputs.push_back(opt::evaluate(this, std::move(inputs[0]), std::move(inputs[1])));
        return {std::move(outputs), {}}; }
   auto insn_jmp
   ::evaluate(std::vector<lib::smart_ptr<operand>> &&inputs) const->std::tuple<std::vector<lib::smart_ptr<operand>>, std::vector<bblock *>>
      { return {{}, {dest()}}; }
   auto insn_br
   ::evaluate(std::vector<lib::smart_ptr<operand>> &&inputs) const->std::tuple<std::vector<lib::smart_ptr<operand>>, std::vector<bblock *>>
      { return {{}, opt::evaluate(this, std::move(inputs[0]), std::move(inputs[1]))}; }
   auto insn_switch_br
   ::evaluate(std::vector<lib::smart_ptr<operand>> &&inputs) const->std::tuple<std::vector<lib::smart_ptr<operand>>, std::vector<bblock *>>
      { return {{}, opt::evaluate(this, std::move(inputs[0]))}; }
}

namespace rsn::opt { static RSN_IF_WITH_MT(thread_local) const auto abs_0 = abs::make(0), abs_1 = abs::make(1); } // some absolute immediate operands to share

RSN_INLINE auto rsn::opt::evaluate(const insn_binop *insn, lib::smart_ptr<operand> &&lhs, lib::smart_ptr<operand> &&rhs)->lib::smart_ptr<operand>
   if (!lhs || !rhs) return {}; // see "Engineering a Compiler", p. 517
   switch (insn->op) {
   default:
      RSN_UNREACHABLE();
   case insn_binop::_add: // the result on overflow is reduced to modulo 2**64 (for unsigned interpretation)
      if (is<abs>(lhs) && !is<abs>(rhs))
         lhs.swap(rhs); // canonicalize to simplify analysis below
      if (is<abs>(rhs)) {
         if (RSN_UNLIKELY(as<abs>(rhs)->val == 0))
            return std::move(lhs);
         if (is<abs>(lhs))
            return abs::make(as<abs>(lhs)->val + as<abs>(rhs)->val);
         if (is<rel_base>(lhs))
            return rel_disp::make(as_smart<rel_base>(std::move(lhs)), +as<abs>(rhs)->val);
         if (is<rel_disp>(lhs))
            return !RSN_LIKELY(as<rel_disp>(lhs)->add + as<abs>(rhs)->val) ? (lib::smart_ptr<operand>)as<rel_disp>(lhs)->base :
               (lib::smart_ptr<operand>)rel_disp::make(as<rel_disp>(lhs)->base, as<rel_disp>(lhs)->add + as<abs>(rhs)->val);
      }
      return insn->dest();
   case insn_binop::_sub: // the result on overflow is reduced to modulo 2**64 (for unsigned interpretation)
      if (is<abs>(rhs)) {
         if (RSN_UNLIKELY(as<abs>(rhs)->val == 0))
            return std::move(lhs);
         if (is<abs>(lhs))
            return abs::make(as<abs>(lhs)->val - as<abs>(rhs)->val);
         if (is<rel_base>(lhs))
            return rel_disp::make(as_smart<rel_base>(std::move(lhs)), -as<abs>(rhs)->val);
         if (is<rel_disp>(lhs))
            return !RSN_LIKELY(as<rel_disp>(lhs)->add + as<abs>(rhs)->val) ? (lib::smart_ptr<operand>)as<rel_disp>(lhs)->base :
               (lib::smart_ptr<operand>)rel_disp::make(as<rel_disp>(lhs)->base, as<rel_disp>(lhs)->add - as<abs>(rhs)->val);
      }
      if (is<rel_base>(lhs) && is<rel_base>(rhs) && as<rel_base>(lhs)->id == as<rel_base>(rhs)->id)
         return abs_0;
      if (is<rel_disp>(lhs) && is<rel_base>(rhs) && as<rel_disp>(lhs)->base->id == as<rel_base>(rhs)->id)
         return abs::make(+as<rel_disp>(lhs)->add);
      if (is<rel_disp>(rhs) && is<rel_base>(lhs) && as<rel_disp>(rhs)->base->id == as<rel_base>(lhs)->id)
         return abs::make(-as<rel_disp>(rhs)->add);
      if (is<rel_disp>(lhs) && is<rel_disp>(rhs) && as<rel_disp>(lhs)->base->id == as<rel_disp>(rhs)->base->id)
         return abs::make(as<rel_disp>(lhs)->add - as<rel_disp>(rhs)->add);
      if (lhs == rhs) // can hold only for VRs at this point
         return abs_0; // _|_ neutralization
      return insn->dest();
   case insn_binop::/*_mul*/_umul: // the result on overflow is reduced to modulo 2**64 (for unsigned interpretation)
      if (is<abs>(lhs) && !is<abs>(rhs))
         lhs.swap(rhs); // canonicalize to simplify analysis below
      if (is<abs>(rhs)) {
         if (RSN_UNLIKELY(as<abs>(rhs)->val == 0))
            return std::move(rhs); // possible _|_ neutralization
         if (RSN_UNLIKELY(as<abs>(rhs)->val == 1))
            return std::move(lhs);
         if (is<abs>(lhs))
            return abs::make(as<abs>(lhs)->val * as<abs>(rhs)->val);
      }
      return insn->dest();
   case insn_binop::_udiv: // division by 0 causes UB
      if (is<abs>(rhs) && !RSN_LIKELY(as<abs>(rhs)->val))
         return insn->dest(); // _|_ for further lowering to insn_oops
      if (is<abs>(lhs) && RSN_UNLIKELY(as<abs>(lhs)->val == 0))
         return std::move(lhs); // possible _|_ neutralization; UB when RHS == 0
      if (is<abs>(rhs) && RSN_UNLIKELY(as<abs>(rhs)->val == 1))
         return std::move(lhs);
      if (is<abs>(lhs) && is<abs>(rhs))
         return abs::make(as<abs>(lhs)->val / as<abs>(rhs)->val);
      if ( is<rel_base>(lhs) && is<rel_base>(rhs) && as<rel_base>(lhs)->id == as<rel_base>(rhs)->id ||
           is<rel_disp>(lhs) && is<rel_disp>(rhs) && as<rel_disp>(lhs)->base->id == as<rel_disp>(rhs)->base->id || as<rel_disp>(lhs)->add == as<rel_disp>(rhs)->add )
         return abs_1;
      if (lhs == rhs) // can hold only for VRs at this point
         return abs_1; // _|_ neutralization; UB when RHS == 0
      return insn->dest(); // UB (trap) when RHS == 0
   case insn_binop::_urem: // division by 0 causes UB
      if (is<abs>(rhs) && !RSN_LIKELY(as<abs>(rhs)->val))
         return insn->dest(); // _|_ for further lowering to insn_oops
      if (is<abs>(lhs) && RSN_UNLIKELY(as<abs>(lhs)->val == 0))
         return std::move(lhs); // possible _|_ neutralization; UB when RHS == 0
      if (is<abs>(rhs) && RSN_UNLIKELY(as<abs>(rhs)->val == 1))
         return abs_0; // possible _|_ neutralization
      if (is<abs>(lhs) && is<abs>(rhs))
         return abs::make(as<abs>(lhs)->val % as<abs>(rhs)->val);
      if ( is<rel_base>(lhs) && is<rel_base>(rhs) && as<rel_base>(lhs)->id == as<rel_base>(rhs)->id ||
           is<rel_disp>(lhs) && is<rel_disp>(rhs) && as<rel_disp>(lhs)->base->id == as<rel_disp>(rhs)->base->id || as<rel_disp>(lhs)->add == as<rel_disp>(rhs)->add )
         return abs_0;
      if (lhs == rhs) // can hold only for VRs at this point
         return abs_0; // _|_ neutralization; UB when RHS == 0
      return insn->dest(); // UB (trap) when RHS == 0
   case insn_binop::_sdiv: // division by 0 and division of the minimum negative number by -1 causes UB
      if (is<abs>(rhs) && !RSN_LIKELY(as<abs>(rhs)->val))
         return insn->dest(); // _|_ for further lowering to insn_oops [*]
      if (is<abs>(lhs) && RSN_UNLIKELY(as<abs>(lhs)->val == 0))
         return std::move(lhs); // possible _|_ neutralization; UB when RHS == 0
      if (is<abs>(rhs) && RSN_UNLIKELY(as<abs>(rhs)->val == 1))
         return std::move(lhs);
      if (is<abs>(lhs) && is<abs>(rhs))
         return as<abs>(lhs)->val == std::numeric_limits<long long>::max() && as<abs>(rhs)->val == -1 ? (lib::smart_ptr<operand>)insn->dest() /*[*]*/ :
            (lib::smart_ptr<operand>)abs::make((long long)as<abs>(lhs)->val / (long long)as<abs>(rhs)->val);
      if ( is<rel_base>(lhs) && is<rel_base>(rhs) && as<rel_base>(lhs)->id == as<rel_base>(rhs)->id ||
           is<rel_disp>(lhs) && is<rel_disp>(rhs) && as<rel_disp>(lhs)->base->id == as<rel_disp>(rhs)->base->id || as<rel_disp>(lhs)->add == as<rel_disp>(rhs)->add )
         return abs_1;
      if (lhs == rhs) // can hold only for VRs at this point
         return abs_1; // _|_ neutralization; UB when RHS == 0
      return insn->dest(); // potentially UB (trap)
   case insn_binop::_srem: // division by 0 and division of the minimum negative number by -1 causes UB
      if (is<abs>(rhs) && !RSN_LIKELY(as<abs>(rhs)->val))
         return insn->dest(); // _|_ for further lowering to insn_oops [*]
      if (is<abs>(lhs) && RSN_UNLIKELY(as<abs>(lhs)->val == 0))
         return std::move(lhs); // possible _|_ neutralization; UB when RHS == 0
      if (is<abs>(rhs) && RSN_UNLIKELY(as<abs>(rhs)->val == 1))
         return abs_0; // possible _|_ neutralization
      if (is<abs>(lhs) && is<abs>(rhs))
         return as<abs>(lhs)->val == std::numeric_limits<long long>::max() && as<abs>(rhs)->val == -1 ? (lib::smart_ptr<operand>)insn->dest() /*[*]*/ :
            (lib::smart_ptr<operand>)abs::make((long long)as<abs>(lhs)->val % (long long)as<abs>(rhs)->val);
      if ( is<rel_base>(lhs) && is<rel_base>(rhs) && as<rel_base>(lhs)->id == as<rel_base>(rhs)->id ||
           is<rel_disp>(lhs) && is<rel_disp>(rhs) && as<rel_disp>(lhs)->base->id == as<rel_disp>(rhs)->base->id || as<rel_disp>(lhs)->add == as<rel_disp>(rhs)->add )
         return abs_0;
      if (lhs == rhs) // can hold only for VRs at this point
         return abs_0; // _|_ neutralization; UB when RHS == 0
      return insn->dest(); // potentially UB (trap)
   case insn_binop::_and:
      if (lhs == rhs)
         return std::move(lhs);
      if (is<abs>(lhs) && !is<abs>(rhs))
         lhs.swap(rhs); // canonicalize to simplify analysis below
      if (is<abs>(rhs)) {
         if (RSN_UNLIKELY(as<abs>(rhs)->val == +0ull))
            return std::move(rhs); // possible _|_ neutralization
         if (RSN_UNLIKELY(as<abs>(rhs)->val == ~0ull))
            return std::move(lhs);
         if (is<abs>(lhs))
            return abs::make(as<abs>(lhs)->val & as<abs>(rhs)->val);
      }
      if ( is<rel_base>(lhs) && is<rel_base>(rhs) && as<rel_base>(lhs)->id == as<rel_base>(rhs)->id ||
           is<rel_disp>(lhs) && is<rel_disp>(rhs) && as<rel_disp>(lhs)->base->id == as<rel_disp>(rhs)->base->id && as<rel_disp>(lhs)->add == as<rel_disp>(rhs)->add )
         return std::move(lhs);
      return insn->dest();
   case insn_binop::_or:
      if (lhs == rhs)
         return std::move(lhs);
      if (is<abs>(lhs) && !is<abs>(rhs))
         lhs.swap(rhs); // canonicalize to simplify analysis below
      if (is<abs>(rhs)) {
         if (RSN_UNLIKELY(as<abs>(rhs)->val == ~0ull))
            return std::move(rhs); // possible _|_ neutralization
         if (RSN_UNLIKELY(as<abs>(rhs)->val == +0ull))
            return std::move(lhs);
         if (is<abs>(lhs))
            return abs::make(as<abs>(lhs)->val | as<abs>(rhs)->val);
      }
      if ( is<rel_base>(lhs) && is<rel_base>(rhs) && as<rel_base>(lhs)->id == as<rel_base>(rhs)->id ||
           is<rel_disp>(lhs) && is<rel_disp>(rhs) && as<rel_disp>(lhs)->base->id == as<rel_disp>(rhs)->base->id && as<rel_disp>(lhs)->add == as<rel_disp>(rhs)->add )
         return std::move(lhs);
      return insn->dest();
   case insn_binop::_xor:
      if (lhs == rhs)
         return std::move(lhs);
      if (is<abs>(lhs) && !is<abs>(rhs))
         lhs.swap(rhs); // canonicalize to simplify analysis below
      if (is<abs>(rhs)) {
         if (RSN_UNLIKELY(as<abs>(rhs)->val == +0ull))
            return std::move(lhs);
         if (is<abs>(lhs))
            return abs::make(as<abs>(lhs)->val ^ as<abs>(rhs)->val);
      }
      if ( is<rel_base>(lhs) && is<rel_base>(rhs) && as<rel_base>(lhs)->id == as<rel_base>(rhs)->id ||
           is<rel_disp>(lhs) && is<rel_disp>(rhs) && as<rel_disp>(lhs)->base->id == as<rel_disp>(rhs)->base->id && as<rel_disp>(lhs)->add == as<rel_disp>(rhs)->add )
         return std::move(lhs);
      return insn->dest();
   case insn_binop::_shl:  // shift counter is taken by modulo 64 (as on X86)
      if (is<abs>(lhs) && RSN_UNLIKELY(as<abs>(lhs)->val == 0))
         return std::move(lhs); // possible _|_ neutralization
      if (is<abs>(rhs) && RSN_UNLIKELY((as<abs>(rhs)->val & 0x3F) == 0))
         return std::move(lhs);
      if (is<abs>(lhs) && is<abs>(rhs))
         return abs::make(as<abs>(lhs)->val << (as<abs>(rhs)->val & 0x3F));
      return insn->dest();
   case insn_binop::_ushr: // shift counter is taken by modulo 64 (as on X86)
      if (is<abs>(lhs) && RSN_UNLIKELY(as<abs>(lhs)->val == 0))
         return std::move(lhs); // possible _|_ neutralization
      if (is<abs>(rhs) && RSN_UNLIKELY((as<abs>(rhs)->val & 0x3F) == 0))
         return std::move(lhs);
      if (is<abs>(lhs) && is<abs>(rhs))
         return abs::make(as<abs>(lhs)->val >> (as<abs>(rhs)->val & 0x3F));
      return insn->dest();
   case insn_binop::_sshr: // shift counter is taken by modulo 64 (as on X86)
      if (is<abs>(lhs) && RSN_UNLIKELY(as<abs>(lhs)->val == 0))
         return std::move(lhs); // possible _|_ neutralization
      if (is<abs>(rhs) && RSN_UNLIKELY((as<abs>(rhs)->val & 0x3F) == 0))
         return std::move(lhs);
      if (is<abs>(lhs) && is<abs>(rhs))
         return abs::make((long long)as<abs>(lhs)->val >> (as<abs>(rhs)->val & 0x3F));
      return insn->dest();
   }
}

RSN_INLINE auto rsn::opt::evaluate(const insn_br *insn, lib::smart_ptr<operand> &&lhs, lib::smart_ptr<operand> &&rhs)->std::vector<bblock *> {
   if (!lhs || !rhs) return {}; // see "Engineering a Compiler", p. 517
   switch (insn->op) {
   default:
      RSN_UNREACHABLE();
   case insn_binop::_beq:
      if (lhs == rhs)
         return {insn->dest1()};
      if (is<abs>(lhs) && is<abs>(rhs))
         return {as<abs>(lhs)->val == as<abs>(val)->val ? insn->dest1() : insn->dest2()};
      if (is<rel_base>(lhs) && is<rel_base>(rhs) && as<rel_base>(lhs)->id == as<rel_base>(rhs)->id)
         return {insn->dest1()};
      if (is<rel_disp>(lhs) && is<rel_disp>(rhs) && as<rel_disp>(lhs)->base->id == as<rel_disp>(rhs)->base->id)
         return {as<rel_disp>(lhs)->add == as<rel_disp>(rhs)->add ? insn->dest1() : insn->dest2()};
      if (is<imm>(lhs) && is<imm>(rhs))
         return {insn->dest2()};
      return {insn->dest1(), insn->dest2()};
   case insn_binop::_bult:
      if (lhs == rhs)
         return {insn->dest2()};
      if (is<abs>(lhs) && is<abs>(rhs))
         return {as<abs>(lhs)->val < as<abs>(val)->val ? insn->dest1() : insn->dest2()};
      if (is<rel_base>(lhs) && is<rel_base>(rhs) && as<rel_base>(lhs)->id == as<rel_base>(rhs)->id)
         return {insn->dest2()};
      if (is<rel_base>(lhs) && is<rel_disp>(rhs) && as<rel_base>(lhs)->id == as<rel_disp>(rhs)->base->id)
         return {(long long)as<rel_disp>(rhs)->add > 0 ? insn->dest1() : insn->dest2()};
      if (is<rel_base>(rhs) && is<rel_disp>(lhs) && as<rel_base>(rhs)->id == as<rel_disp>(lhs)->base->id)
         return {(long long)as<rel_disp>(lhs)->add < 0 ? insn->dest1() : insn->dest2()};
      if (is<rel_disp>(lhs) && is<rel_disp>(rhs) && as<rel_disp>(lhs)->base->id == as<rel_disp>(rhs)->base->id)
         return {(long long)as<rel_disp>(lhs)->add < (long long)as<rel_disp>(rhs)->add ? insn->dest1() : insn->dest2()};
      return {insn->dest1(), insn->dest2()};
   case insn_binop::_bslt:
      if (lhs == rhs)
         return {insn->dest2()};
      if (is<abs>(lhs) && is<abs>(rhs))
         return {(long long)as<abs>(lhs)->val < (long long)as<abs>(val)->val ? insn->dest1() : insn->dest2()};
      if (is<rel_base>(lhs) && is<rel_base>(rhs) && as<rel_base>(lhs)->id == as<rel_base>(rhs)->id)
         return {insn->dest2()};
      if (is<rel_base>(lhs) && is<rel_disp>(rhs) && as<rel_base>(lhs)->id == as<rel_disp>(rhs)->base->id)
         return {(long long)as<rel_disp>(rhs)->add > 0 ? insn->dest1() : insn->dest2()};
      if (is<rel_base>(rhs) && is<rel_disp>(lhs) && as<rel_base>(rhs)->id == as<rel_disp>(lhs)->base->id)
         return {(long long)as<rel_disp>(lhs)->add < 0 ? insn->dest1() : insn->dest2()};
      if (is<rel_disp>(lhs) && is<rel_disp>(rhs) && as<rel_disp>(lhs)->base->id == as<rel_disp>(rhs)->base->id)
         return {(long long)as<rel_disp>(lhs)->add < (long long)as<rel_disp>(rhs)->add ? insn->dest1() : insn->dest2()};
      return {insn->dest1(), insn->dest2()};
   }
}

RSN_INLINE auto rsn::opt::evaluate(const insn_switch_br *insn, lib::smart_ptr<operand> &&index)->std::vector<bblock *> {
   if (!index) return {}; // see "Engineering a Compiler", p. 517
   if (is<abs>(index)) {
      if (RSN_UNLIKELY(as<abs>(index)->val >= dests().size())) return {};
      return {insn->dests()[as<abs>(index)->val]};
   }
   return std::vector<bblock *>(insn->dests().begin(), insn->dests().end());
}
