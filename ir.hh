// ir.hh

/*    Copyright (C) 2020 Alexey Protasov (AKA Alex or rusini)

   This is free software: you can redistribute it and/or modify it under the terms of the version 3 of the GNU General Public License
   as published by the Free Software Foundation (and only version 3).

   This software is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along with MANOOL.  If not, see <https://www.gnu.org/licenses/>.  */


# ifndef RSN_INCLUDED_IR
# define RSN_INCLUDED_IR

# include <limits> // numeric_limits

# include "ir0.hh"

namespace rsn::opt {

   // IR Instructions //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

   class insn_entry final: public insn {
   public: // public data members
      std::vector<smart_ptr<vreg>> params;
   public: // construction/destruction
      RSN_INLINE static auto make(bblock *owner, decltype(params) params)
         { return new insn_entry{owner, std::move(params)}; }
      RSN_INLINE static auto make(insn *next, decltype(params) params)
         { return new insn_entry{next, std::move(params)}; }
      RSN_NOINLINE insn_entry *clone(bblock *owner) const override
         { return make(owner, params); }
      RSN_NOINLINE insn_entry *clone(insn *next) const override
         { return make(next, params); }
   public: // querying arguments and performing transformations
      RSN_NOINLINE auto outputs()->decltype(insn::outputs()) override
         { decltype(outputs()) res(params.size()); for (auto &it: params) res.push_back(&it); return res; }
   private: // implementation helpers
      template<typename Pos> RSN_INLINE explicit insn_entry(Pos pos, decltype(params) &&params) noexcept
         : insn{pos}, params(std::move(params)) {}
   # if RSN_USE_DEBUG
   public:
      void dump() const noexcept override { log << params << (params.empty() ? "" : " := ") << "entry"; }
   # endif
   };

   class insn_ret final: public insn {
   public: // public data members
      std::vector<smart_ptr<operand>> results;
   public: // construction/destruction
      RSN_INLINE static auto make(bblock *owner, decltype(results) results)
         { return new insn_ret{owner, std::move(results)}; }
      RSN_INLINE static auto make(insn *next, decltype(results) results)
         { return new insn_ret{next, std::move(results)}; }
      RSN_NOINLINE insn_ret *clone(bblock *owner) const override
         { return make(owner, results); }
      RSN_NOINLINE insn_ret *clone(insn *next) const override
         { return make(next, results); }
   public: // querying arguments and performing transformations
      RSN_NOINLINE auto inputs()->decltype(insn::inputs()) override
         { decltype(inputs()) res(results.size()); for (auto &it: results) res.push_back(&it); return res; }
   private: // implementation helpers
      template<typename Pos> RSN_INLINE explicit insn_ret(Pos pos, decltype(results) &&results) noexcept
         : insn{pos}, results(std::move(results)) {}
   # if RSN_USE_DEBUG
   public:
      void dump() const noexcept override { log << "ret" << (results.empty() ? "" : " ") << results; }
   # endif
   };

   class insn_call final: public insn {
   public: // public data members
      std::vector<smart_ptr<vreg>> results;
      smart_ptr<operand> proc; std::vector<smart_ptr<operand>> params;
   public: // construction/destruction
      RSN_INLINE static auto make(bblock *owner, decltype(results) results, decltype(proc) proc, decltype(params) params)
         { return new insn_call{owner, std::move(results), std::move(proc), std::move(params)}; }
      RSN_INLINE static auto make(insn *next, decltype(results) results, decltype(proc) proc, decltype(params) params)
         { return new insn_call{next, std::move(results), std::move(proc), std::move(params)}; }
      RSN_NOINLINE insn_call *clone(bblock *owner) const override
         { return make(owner, results, proc, params); }
      RSN_NOINLINE insn_call *clone(insn *next) const override
         { return make(next, results, proc, params); }
   public: // querying arguments and performing transformations
      RSN_NOINLINE auto outputs()->decltype(insn::outputs()) override
         { decltype(outputs()) res(results.size()); for (auto &it: results) res.push_back(&it); return res; }
      RSN_NOINLINE auto inputs()->decltype(insn::inputs()) override
         { decltype(inputs()) res(1 + params.size()); res.push_back(&proc); for (auto &it: params) res.push_back(&it); return res; }
   private: // implementation helpers
      template<typename Pos> RSN_INLINE explicit insn_call(Pos pos, decltype(results) &&results, decltype(proc) &&proc, decltype(params) &&params) noexcept
         : insn{pos}, results(std::move(results)), proc(std::move(proc)), params(std::move(params)) {}
   # if RSN_USE_DEBUG
   public:
      void dump() const noexcept override { log << results << (results.empty() ? "" : " := ") << "call " << proc << " (" << params << ')'; }
   # endif
   };

   class insn_mov final: public insn {
   public: // public data members
      smart_ptr<vreg> dest; smart_ptr<operand> src;
   public: // construction/destruction
      RSN_INLINE static auto make(bblock *owner, decltype(dest) dest, decltype(src) src)
         { return new insn_mov{owner, std::move(dest), std::move(src)}; }
      RSN_INLINE static auto make(insn *next, decltype(dest) dest, decltype(src) src)
         { return new insn_mov{next, std::move(dest), std::move(src)}; }
      insn_mov *clone(bblock *owner) const override
         { return make(owner, dest, src); }
      insn_mov *clone(insn *next) const override
         { return make(next, dest, src); }
   public: // querying arguments and performing transformations
      RSN_INLINE auto outputs()->decltype(insn::outputs()) override
         { return {&dest}; }
      RSN_INLINE auto inputs()->decltype(insn::inputs()) override
         { return {&src}; }
   private: // implementation helpers
      template<typename Pos> RSN_INLINE explicit insn_mov(Pos pos, decltype(dest) &&dest, decltype(src) &&src) noexcept
         : insn{pos}, dest(std::move(dest)), src(std::move(src)) {}
   # if RSN_USE_DEBUG
   public:
      void dump() const noexcept override { log << dest << " := mov " << src; }
   # endif
   };

   class insn_load final: public insn {
   public: // public data members
      smart_ptr<vreg> dest; smart_ptr<operand> src;
   public: // construction/destruction
      RSN_INLINE static auto make(bblock *owner, decltype(dest) dest, decltype(src) src)
         { return new insn_load{owner, std::move(dest), std::move(src)}; }
      RSN_INLINE static auto make(insn *next, decltype(dest) dest, decltype(src) src)
         { return new insn_load{next, std::move(dest), std::move(src)}; }
      insn_load *clone(bblock *owner) const override
         { return make(owner, dest, src); }
      insn_load *clone(insn *next) const override
         { return make(next, dest, src); }
   public: // querying arguments and performing transformations
      RSN_INLINE auto outputs()->decltype(insn::outputs()) override
         { return {&dest}; }
      RSN_INLINE auto inputs()->decltype(insn::inputs()) override
         { return {&src}; }
   private: // implementation helpers
      template<typename Pos> RSN_INLINE explicit insn_load(Pos pos, decltype(dest) &&dest, decltype(src) &&src) noexcept
         : insn{pos}, dest(std::move(dest)), src(std::move(src)) {}
   # if RSN_USE_DEBUG
   public:
      void dump() const noexcept override { log << dest << " := load " << src; }
   # endif
   };

   class insn_store final: public insn {
   public: // public data members
      smart_ptr<operand> src, dest;
   public: // construction/destruction
      RSN_INLINE static auto make(bblock *owner, decltype(dest) dest, decltype(src) src)
         { return new insn_store{owner, std::move(src), std::move(dest)}; }
      RSN_INLINE static auto make(insn *next, decltype(dest) dest, decltype(src) src)
         { return new insn_store{next, std::move(src), std::move(dest)}; }
      insn_store *clone(bblock *owner) const override
         { return make(owner, src, dest); }
      insn_store *clone(insn *next) const override
         { return make(next, src, dest); }
   public: // querying arguments and performing transformations
      RSN_INLINE auto inputs()->decltype(insn::inputs()) override
         { return {&src, &dest}; }
   private: // implementation helpers
      template<typename Pos> RSN_INLINE explicit insn_store(Pos pos, decltype(src) &&src, decltype(dest) &&dest) noexcept
         : insn{pos}, src(std::move(src)), dest(std::move(dest)) {}
   # if RSN_USE_DEBUG
   public:
      void dump() const noexcept override { log << "store " << src << " -> " << dest; }
   # endif
   };

   class insn_binop final: public insn {
   public: // public data members
      const enum {
         _add, _sub, _umul, _udiv, _urem, _smul, _sdiv, _srem,
         _cmpeq, _cmpne, _ucmplt, _ucmple, _scmplt, _scmple,
         _and, _or, _xor, _shl, _ushr, _sshr,
      } op;
   public:
      smart_ptr<vreg> dest; smart_ptr<operand> lhs, rhs;
   public: // construction/destruction
      RSN_INLINE static auto make(bblock *owner, decltype(op) op, decltype(dest) dest, decltype(lhs) lhs, decltype(rhs) rhs)
         { return new insn_binop{owner, op, std::move(dest), std::move(lhs), std::move(rhs)}; }
      RSN_INLINE static auto make(insn *next, decltype(op) op, decltype(dest) dest, decltype(lhs) lhs, decltype(rhs) rhs)
         { return new insn_binop{next, op, std::move(dest), std::move(lhs), std::move(rhs)}; }
   # define RSN_M1(OP) \
      RSN_INLINE static auto make##OP(bblock *owner, decltype(dest) dest, decltype(lhs) lhs, decltype(rhs) rhs) \
         { return new insn_binop{owner, OP, std::move(dest), std::move(lhs), std::move(rhs)}; } \
      RSN_INLINE static auto make##OP(insn *next, decltype(dest) dest, decltype(lhs) lhs, decltype(rhs) rhs) \
         { return new insn_binop{next, OP, std::move(dest), std::move(lhs), std::move(rhs)}; } \
   // end # define RSN_M1(OP)
      RSN_M1(_add) RSN_M1(_sub) RSN_M1(_umul) RSN_M1(_udiv) RSN_M1(_urem) RSN_M1(_smul) RSN_M1(_sdiv) RSN_M1(_srem)
      RSN_M1(_cmpeq) RSN_M1(_cmpne) RSN_M1(_ucmplt) RSN_M1(_ucmple) RSN_M1(_scmplt) RSN_M1(_scmple)
      RSN_M1(_and) RSN_M1(_or) RSN_M1(_xor) RSN_M1(_shl) RSN_M1(_ushr) RSN_M1(_sshr)
   # undef RSN_M1
      insn_binop *clone(bblock *owner) const override
         { return make(owner, op, dest, lhs, rhs); }
      insn_binop *clone(insn *next) const override
         { return make(next, op, dest, lhs, rhs); }
   public: // querying arguments and performing transformations
      RSN_INLINE auto outputs()->decltype(insn::outputs()) override
         { return {&dest}; }
      RSN_INLINE auto inputs()->decltype(insn::inputs()) override
         { return {&lhs, &rhs}; }
   public:
      bool try_to_fold() override;
   private: // implementation helpers
      template<typename Pos> RSN_INLINE explicit insn_binop(Pos pos, decltype(op) op, decltype(dest) &&dest, decltype(lhs) &&lhs, decltype(rhs) &&rhs) noexcept
         : insn{pos}, op(op), dest(std::move(dest)), lhs(std::move(lhs)), rhs(std::move(rhs)) {}
   # if RSN_USE_DEBUG
   public:
      void dump() const noexcept override {
         static constexpr const char *mnemo[]{
            "add", "sub", "umul", "udiv", "urem", "smul", "sdiv", "srem",
            "cmpeq", "cmpne", "ucmplt", "ucmple", "scmplt", "scmple",
            "and", "or", "xor", "shl", "ushr", "sshr",
         };
         log << dest << " := " << mnemo[op] << ' ' << lhs << ", " << rhs;
      }
   # endif
   };

   class insn_jmp final: public insn {
   public: // public data members
      bblock *dest;
   public: // construction/destruction
      RSN_INLINE static auto make(bblock *owner, decltype(dest) dest)
         { return new insn_jmp{owner, std::move(dest)}; }
      RSN_INLINE static auto make(insn *next, decltype(dest) dest)
         { return new insn_jmp{next, std::move(dest)}; }
      insn_jmp *clone(bblock *owner) const override
         { return make(owner, dest); }
      insn_jmp *clone(insn *next) const override
         { return make(next, dest); }
   public: // querying arguments and performing transformations
      RSN_INLINE auto targets()->decltype(insn::targets()) override
         { return {&dest}; }
   private: // implementation helpers
      template<typename Pos> RSN_INLINE explicit insn_jmp(Pos pos, decltype(dest) &&dest) noexcept
         : insn{pos}, dest(std::move(dest)) {}
   # if RSN_USE_DEBUG
   public:
      void dump() const noexcept override { log << "jmp to " << dest; }
   # endif
   };

   class insn_cond_jmp final: public insn {
   public: // public data members
      smart_ptr<operand> cond; bblock *dest1, *dest2;
   public: // construction/destruction
      RSN_INLINE static auto make(bblock *owner, decltype(cond) cond, decltype(dest1) dest1, decltype(dest2) dest2)
         { return new insn_cond_jmp{owner, std::move(cond), std::move(dest1), std::move(dest2)}; }
      RSN_INLINE static auto make(insn *next, decltype(cond) cond, decltype(dest1) dest1, decltype(dest2) dest2)
         { return new insn_cond_jmp{next, std::move(cond), std::move(dest1), std::move(dest2)}; }
      insn_cond_jmp *clone(bblock *owner) const override
         { return make(owner, cond, dest1, dest2); }
      insn_cond_jmp *clone(insn *next) const override
         { return make(next, cond, dest1, dest2); }
   public: // querying arguments and performing transformations
      RSN_INLINE auto inputs()->decltype(insn::inputs()) override
         { return {&cond}; }
      RSN_INLINE auto targets()->decltype(insn::targets()) override
         { return {&dest1, &dest2}; }
   public:
      bool try_to_fold() override;
   private: // implementation helpers
      template<typename Pos> RSN_INLINE explicit insn_cond_jmp(Pos pos, decltype(cond) &&cond, decltype(dest1) &&dest1, decltype(dest2) &&dest2) noexcept
         : insn{pos}, cond(std::move(cond)), dest1(std::move(dest1)), dest2(std::move(dest2)) {}
   # if RSN_USE_DEBUG
   public:
      void dump() const noexcept override { log << "jmp " << cond << " to " << dest1 << ", " << dest2; }
   # endif
   };

   class insn_switch_jmp final: public insn {
   public: // public data members
      smart_ptr<operand> index; std::vector<bblock *> dests;
   public: // construction/destruction
      RSN_INLINE static auto make(bblock *owner, decltype(index) index, decltype(dests) dests)
         { return new insn_switch_jmp{owner, std::move(index), std::move(dests)}; }
      RSN_INLINE static auto make(insn *next, decltype(index) index, decltype(dests) dests)
         { return new insn_switch_jmp{next, std::move(index), std::move(dests)}; }
      RSN_NOINLINE insn_switch_jmp *clone(bblock *owner) const override
         { return make(owner, index, dests); }
      RSN_NOINLINE insn_switch_jmp *clone(insn *next) const override
         { return make(next, index, dests); }
   public: // querying arguments and performing transformations
      RSN_INLINE auto inputs()->decltype(insn::inputs()) override
         { return {&index}; }
      RSN_NOINLINE auto targets()->decltype(insn::targets()) override
         { decltype(targets()) res(dests.size()); for (auto &it: dests) res.push_back(&it); return res; }
   public:
      bool try_to_fold() override;
   private: // implementation helpers
      template<typename Pos> RSN_INLINE explicit insn_switch_jmp(Pos pos, decltype(index) &&index, decltype(dests) &&dests) noexcept
         : insn{pos}, index(std::move(index)), dests(std::move(dests)) {}
   # if RSN_USE_DEBUG
   public:
      void dump() const noexcept override { log << "jmp " << index << " to (" << dests << ')'; }
   # endif
   };

   class insn_undefined final: public insn {
   public: // construction/destruction
      RSN_INLINE static auto make(bblock *owner)
         { return new insn_undefined{owner}; }
      RSN_INLINE static auto make(insn *next)
         { return new insn_undefined{next}; }
      insn_undefined *clone(bblock *owner) const override
         { return make(owner); }
      insn_undefined *clone(insn *next) const override
         { return make(next); }
   private: // implementation helpers
      template<typename Pos> RSN_INLINE explicit insn_undefined(Pos pos) noexcept
         : insn{pos} {}
   # if RSN_USE_DEBUG
   public:
      void dump() const noexcept override { log << "undefined"; }
   # endif
   };

} // namespace rsn::opt

# endif // # ifndef RSN_INCLUDED_IR
