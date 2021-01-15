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
   public: // construction/destruction
      RSN_INLINE static auto make( bblock *owner,
         std::vector<lib::smart_ptr<vreg>> params )
         { return new insn_entry(owner, std::move(params)); }
      RSN_INLINE static auto make( insn *next,
         std::vector<lib::smart_ptr<vreg>> params )
         { return new insn_entry(next,  std::move(params)); }
   public:
      RSN_NOINLINE insn_entry *clone(bblock *owner) const override { return new insn_entry(owner, _outputs); }
      RSN_NOINLINE insn_entry *clone(insn *next) const override { return new insn_entry(next, _outputs); }
   public: // data operands and jump targets
      auto params() noexcept       { return lib::range_ref{&*_outputs.begin(), &*_outputs.end()}; }
      auto params() const noexcept { return lib::range_ref{&*_outputs.begin(), &*_outputs.end()}; }
   public: // internal representation
      std::vector<lib::smart_ptr<vreg>> _outputs;
   private: // implementation helpers
      template<typename Loc> RSN_INLINE explicit insn_entry( Loc loc,
         std::vector<lib::smart_ptr<vreg>> &&params ) noexcept
         : insn(loc), _outputs(std::move(params)) {
         _outputs.shrink_to_fit();
         init(_outputs);
      }
      template<typename Loc> RSN_INLINE explicit insn_entry( Loc loc,
         const declype(_outputs) &outputs )
         : insn(loc), _outputs(outputs) {
         init(_outputs);
      }
   # if RSN_USE_DEBUG
   public:
      void dump() const noexcept override { log << params() << (params().empty() ? "" : " := ") << "entry"; }
   # endif
   };

   class insn_ret final: public insn {
   public: // construction/destruction
      RSN_INLINE static auto make( bblock *owner,
         lib::smart_ptr<operand> results )
         { return new insn_ret(owner, std::move(results)); }
      RSN_INLINE static auto make( insn *next,
         lib::smart_ptr<operand> results )
         { return new insn_ret(next,  std::move(results)); }
   public:
      RSN_NOINLINE insn_ret *clone(bblock *owner) const override { return new insn_ret(owner, _inputs); }
      RSN_NOINLINE insn_ret *clone(insn *next) const override { return new insn_ret(next, _inputs); }
   public: // data operands and jump targets
      auto results() noexcept       { return lib::range_ref{&*_inputs.begin(), &*_inputs.end()}; }
      auto results() const noexcept { return lib::range_ref{&*_inputs.begin(), &*_inputs.end()}; }
   public: // internal representation
      std::vector<lib::smart_ptr<operand>> _inputs;
   private: // implementation helpers
      template<typename Loc> RSN_INLINE explicit insn_ret( Loc loc,
         std::vector<lib::smart_ptr<vreg>> &&results ) noexcept
         : insn(loc), _inputs(std::move(results)) {
         _inputs.shrink_to_fit();
         init(_inputs);
      }
      template<typename Loc> RSN_INLINE explicit insn_ret( Loc loc,
         const declype(_inputs) &inputs )
         : insn(loc), _inputs(inputs) {
         init(_inputs);
      }
   # if RSN_USE_DEBUG
   public:
      void dump() const noexcept override { log << "ret" << (results().empty() ? "" : " ") << results(); }
   # endif
   };

   class insn_call final: public insn {
   public: // construction/destruction
      RSN_INLINE static auto make( bblock *owner,
         std::vector<lib::smart_ptr<vreg>> results, lib::smart_ptr<operand> dest, std::vector<lib::smart_ptr<operand>> params )
         { return new insn_call(owner, std::move(results), std::move(dest), std::move(params)); }
      RSN_INLINE static auto make( insn *next,
         std::vector<lib::smart_ptr<vreg>> results, lib::smart_ptr<operand> dest, std::vector<lib::smart_ptr<operand>> params )
         { return new insn_call(next,  std::move(results), std::move(dest), std::move(params)); }
   public:
      RSN_NOINLINE insn_call *clone(bblock *owner) const override { return new insn_call(owner, _outputs, _inputs); }
      RSN_NOINLINE insn_call *clone(insn *next) const override { return new insn_call(next, _outputs, _inputs); }
   public: // data operands and jump targets
      auto  results() noexcept       { return lib::range_ref{&*_outputs.begin(), &*_outputs.end()}; }
      auto  results() const noexcept { return lib::range_ref{&*_outputs.begin(), &*_outputs.end()}; }
      auto &dest() noexcept          { return lib::range_ref{&*_inputs.begin(),  &*_inputs.end()}.last(); }
      auto &dest() const noexcept    { return lib::range_ref{&*_inputs.begin(),  &*_inputs.end()}.last(); }
      auto  params() noexcept        { return lib::range_ref{&*_inputs.begin(),  &*_inputs.end()}.drop_last(); }
      auto  params() const noexcept  { return lib::range_ref{&*_inputs.begin(),  &*_inputs.end()}.drop_last(); }
   public: // miscellaneous
      bool try_to_fold() override;
   public: // internal representation
      std::vector<lib::smart_ptr<vreg>> _outputs;
      std::vector<lib::smart_ptr<operand>> _inputs;
   private: // implementation helpers
      template<typename Pos> RSN_INLINE explicit insn_call( Loc loc,
         std::vector<lib::smart_ptr<vreg>> results &&results, lib::smart_ptr<operand> &&dest, std::vector<lib::smart_ptr<operand>> &&params ) noexcept
         : insn(loc), _outputs(std::move(results)), _inputs(std::move(params)) {
         _inputs.push_back(std::move(dest));
         _outputs.shrink_to_fit(), _inputs.shrink_to_fit();
         init(_outputs, _inputs);
      }
      template<typename Loc> RSN_INLINE explicit insn_call( Loc loc,
         const declype(_outputs) &outputs, const declype(_inputs) &inputs )
         : insn(loc), _outputs(outputs), _inputs(inputs) {
         init(_outputs, _inputs);
      }
   # if RSN_USE_DEBUG
   public:
      void dump() const noexcept override { log << results() << (results().empty() ? "" : " := ") << "call " << dest() << " ( " << params() << " )"; }
   # endif
   };

   class insn_mov final: public insn {
   public: // construction/destruction
      RSN_INLINE static auto make( bblock *owner,
         lib::smart_ptr<vreg> dest, lib::smart_ptr<operand> src )
         { return new insn_mov(owner, std::move(dest), std::move(src)); }
      RSN_INLINE static auto make( insn *next,
         lib::smart_ptr<vreg> dest, lib::smart_ptr<operand> src )
         { return new insn_mov(next,  std::move(dest), std::move(src)); }
   public:
      insn_mov *clone(bblock *owner) const override { return new insn_mov(owner, _outputs, _inputs); }
      insn_mov *clone(insn *next) const override { return new insn_mov(next, _outputs, _inputs); }
   public: // data operands and jump targets
      auto &dest() noexcept       { return _outputs[0]; }
      auto &dest() const noexcept { return _outputs[0]; }
      auto &src() noexcept        { return _inputs [0]; }
      auto &src() const noexcept  { return _inputs [0]; }
   public: // internal representation
      std::array<lib::smart_ptr<vreg>, 1> _outputs;
      std::array<lib::smart_ptr<operand>, 1> _inputs;
   private: // implementation helpers
      template<typename Loc> RSN_INLINE explicit insn_mov( Loc loc,
         lib::smart_ptr<vreg> &&dest, lib::smart_ptr<operand> &&src ) noexcept
         : insn(loc), _outputs{std::move(dest)}, _inputs{std::move(src)} {
         init(_outputs, _inputs);
      }
      template<typename Loc> RSN_INLINE explicit insn_mov( Loc loc,
         const declype(_outputs) &outputs, const declype(_inputs) &inputs ) noexcept
         : insn(loc), _outputs(outputs), _inputs(inputs) {
         init(_outputs, _inputs);
      }
   # if RSN_USE_DEBUG
   public:
      void dump() const noexcept override { log << dest() << " := mov " << src(); }
   # endif
   };

   class insn_load final: public insn {
   public: // construction/destruction
      RSN_INLINE static auto make( bblock *owner,
         lib::smart_ptr<vreg> dest, lib::smart_ptr<operand> src )
         { return new insn_load(owner, std::move(dest), std::move(src)); }
      RSN_INLINE static auto make( insn *next,
         lib::smart_ptr<vreg> dest, lib::smart_ptr<operand> src )
         { return new insn_load(next,  std::move(dest), std::move(src)); }
   public:
      insn_load *clone(bblock *owner) const override { return new insn_load(owner, _outputs, _inputs); }
      insn_load *clone(insn *next) const override { return new insn_load(next, _outputs, _inputs); }
   public: // data operands and jump targets
      auto &dest() noexcept       { return _outputs[0]; }
      auto &dest() const noexcept { return _outputs[0]; }
      auto &src() noexcept        { return _inputs [0]; }
      auto &src() const noexcept  { return _inputs [0]; }
   public: // internal representation
      std::array<lib::smart_ptr<vreg>, 1> _outputs;
      std::array<lib::smart_ptr<operand>, 1> _inputs;
   private: // implementation helpers
      template<typename Loc> RSN_INLINE explicit insn_load( Loc loc,
         lib::smart_ptr<vreg> &&dest, lib::smart_ptr<operand> &&src ) noexcept
         : insn(loc), _outputs{std::move(dest)}, _inputs{std::move(src)} {
         init(_outputs, _inputs);
      }
      template<typename Loc> RSN_INLINE explicit insn_load( Loc loc,
         const declype(_outputs) &outputs, const declype(_inputs) &inputs ) noexcept
         : insn(loc), _outputs(outputs), _inputs(inputs) {
         init(_outputs, _inputs);
      }
   # if RSN_USE_DEBUG
   public:
      void dump() const noexcept override { log << dest() << " := load @" << src(); }
   # endif
   };

   class insn_store final: public insn {
   public: // construction/destruction
      RSN_INLINE static auto make( bblock *owner,
         lib::smart_ptr<operand> src, lib::smart_ptr<operand> dest )
         { return new insn_store(owner, std::move(src), std::move(dest)); }
      RSN_INLINE static auto make( insn *next,
         lib::smart_ptr<operand> src, lib::smart_ptr<operand> dest )
         { return new insn_store(next,  std::move(src), std::move(dest)); }
   public:
      insn_store *clone(bblock *owner) const override { return new insn_store(owner, _inputs); }
      insn_store *clone(insn *next) const override { return new insn_store(next, _inputs); }
   public: // data operands and jump targets
      auto &src() noexcept        { return _inputs[0]; }
      auto &src() const noexcept  { return _inputs[0]; }
      auto &dest() noexcept       { return _inputs[1]; }
      auto &dest() const noexcept { return _inputs[1]; }
   public: // internal representation
      std::array<lib::smart_ptr<operand>, 2> _inputs;
   private: // implementation helpers
      template<typename Loc> RSN_INLINE explicit insn_store( Loc loc,
         lib::smart_ptr<operand> &&src, lib::smart_ptr<operand> &&dest ) noexcept
         : insn(loc), _inputs{std::move(src), std::move(dest)} {
         init(_inputs);
      }
      template<typename Loc> RSN_INLINE explicit insn_store( Loc loc,
         const declype(_inputs) &inputs ) noexcept
         : insn(loc), _inputs(inputs) {
         init(_inputs);
      }
   # if RSN_USE_DEBUG
   public:
      void dump() const noexcept override { log << "store " << src() << " -> @" << dest(); }
   # endif
   };

   class insn_binop final: public insn {
   public: // public data members
      const enum { _add, _sub, _umul, _udiv, _urem, _smul, _sdiv, _srem, _and, _or, _xor, _shl, _ushr, _sshr } op;
   public: // construction/destruction
      RSN_INLINE static auto make( bblock *owner, decltype(op) op,
         lib::smart_ptr<vreg> dest, lib::smart_ptr<operand> lhs, lib::smart_ptr<operand> rhs )
         { return new insn_binop(owner, op, std::move(dest), std::move(lhs), std::move(rhs)); }
      RSN_INLINE static auto make( insn *next, decltype(op) op,
         lib::smart_ptr<vreg> dest, lib::smart_ptr<operand> lhs, lib::smart_ptr<operand> rhs )
         { return new insn_binop(next, op,  std::move(dest), std::move(lhs), std::move(rhs)); }
   # define RSN_M1(OP) \
      RSN_INLINE static auto make##OP(bblock *owner, \
         lib::smart_ptr<vreg> dest, lib::smart_ptr<operand> lhs, lib::smart_ptr<operand> rhs ) \
         { return new insn_binop(owner, OP, std::move(dest), std::move(lhs), std::move(rhs)); } \
      RSN_INLINE static auto make##OP(insn *next, \
         lib::smart_ptr<vreg> dest, lib::smart_ptr<operand> lhs, lib::smart_ptr<operand> rhs ) \
         { return new insn_binop(next, OP,  std::move(dest), std::move(lhs), std::move(rhs)); } \
   // end # define RSN_M1(OP)
      RSN_M1(_add) RSN_M1(_sub) RSN_M1(_umul) RSN_M1(_udiv) RSN_M1(_urem) RSN_M1(_smul) RSN_M1(_sdiv) RSN_M1(_srem)
      RSN_M1(_and) RSN_M1(_or) RSN_M1(_xor) RSN_M1(_shl) RSN_M1(_ushr) RSN_M1(_sshr)
   # undef RSN_M1
   public:
      insn_binop *clone(bblock *owner) const override { return new insn_binop(owner, op, _outputs, _inputs); }
      insn_binop *clone(insn *next) const override { return new insn_binop(next, op, _outputs, _inputs); }
   public: // data operands and jump targets
      auto &dest() noexcept       { return _outputs[0]; }
      auto &dest() const noexcept { return _outputs[0]; }
      auto &lhs() noexcept        { return _inputs [0]; }
      auto &lhs() const noexcept  { return _inputs [0]; }
      auto &rhs() noexcept        { return _inputs [1]; }
      auto &rhs() const noexcept  { return _inputs [1]; }
   public: // miscellaneous
      bool try_to_fold() override;
   public: // internal representation
      std::array<lib::smart_ptr<vreg>, 1> _outputs;
      std::array<lib::smart_ptr<operand>, 2> _inputs;
   private: // implementation helpers
      template<typename Loc> RSN_INLINE explicit insn_binop( Loc loc, decltype(op) op,
         lib::smart_ptr<vreg> &&dest, lib::smart_ptr<operand> &&lhs, lib::smart_ptr<operand> &&rhs ) noexcept
         : insn(loc), op(op), _outputs{std::move(dest)}, _inputs{std::move(lhs)), rhs(std::move(rhs)} {
         init(_outputs, _inputs);
      }
      template<typename Loc> RSN_INLINE explicit insn_binop( Loc loc, decltype(op) op,
         const declype(_outputs) &outputs, const declype(_inputs) &inputs ) noexcept
         : insn(loc), op(op), _outputs(outputs), _inputs(inputs) {
         init(_outputs, _inputs);
      }
   # if RSN_USE_DEBUG
   public:
      void dump() const noexcept override {
         static constexpr const char *mnemo[]
            {"add", "sub", "umul", "udiv", "urem", "smul", "sdiv", "srem", "and", "or", "xor", "shl", "ushr", "sshr"};
         log << dest() << " := " << mnemo[op] << ' ' << lhs() << ", " << rhs();
      }
   # endif
   };

   class insn_jmp final: public insn {
   public: // construction/destruction
      RSN_INLINE static auto make( bblock *owner,
         bblock *&dest )
         { return new insn_jmp(owner, std::move(dest)); }
      RSN_INLINE static auto make( insn *next,
         bblock *&dest )
         { return new insn_jmp(next,  std::move(dest)); }
   public:
      insn_jmp *clone(bblock *owner) const override { return new insn_jmp(owner, _targets); }
      insn_jmp *clone(insn *next) const override { return new insn_jmp(next, _targets); }
   public: // data operands and jump targets
      auto &dest() noexcept       { return _targets[0]; }
      auto &dest() const noexcept { return _targets[0]; }
   public: // internal representation
      std::array<bblock *, 1> _targets;
   private: // implementation helpers
      template<typename Loc> RSN_INLINE explicit insn_jmp( Loc loc,
         bblock *&&dest ) noexcept
         : insn(loc), _targets{std::move(dest)} {
         init(_targets);
      }
      template<typename Loc> RSN_INLINE explicit insn_jmp( Loc loc,
         const declype(_targets) &targets ) noexcept
         : insn(loc), _targets(targets) {
         init(_targets);
      }
   # if RSN_USE_DEBUG
   public:
      void dump() const noexcept override { log << "jmp to " << dest(); }
   # endif
   };

   class insn_br final: public insn {
   public: // public data members
      const enum { _beq, _bne, _ublt, _uble, _sblt, _sble } op;
   public: // construction/destruction
      RSN_INLINE static auto make( bblock *owner, decltype(op) op,
         lib::smart_ptr<operand> lhs, lib::smart_ptr<operand> rhs, bblock *dest1, bblock *dest2 )
         { return new insn_br(owner, op, std::move(lhs), std::move(rhs), std::move(dest1), std::move(dest2)); }
      RSN_INLINE static auto make(insn *next, decltype(op) op,
         lib::smart_ptr<operand> lhs, lib::smart_ptr<operand> rhs, bblock *dest1, bblock *dest2 )
         { return new insn_br(next, op,  std::move(lhs), std::move(rhs), std::move(dest1), std::move(dest2)); }
   # define RSN_M1(OP) \
      RSN_INLINE static auto make##OP(bblock *owner, \
         lib::smart_ptr<operand> lhs, lib::smart_ptr<operand> rhs, bblock *dest1, bblock *dest2 ) \
         { return new insn_br(owner, OP, std::move(lhs), std::move(rhs), std::move(dest1), std::move(dest2)); } \
      RSN_INLINE static auto make##OP(insn *next, \
         lib::smart_ptr<operand> lhs, lib::smart_ptr<operand> rhs, bblock *dest1, bblock *dest2 ) \
         { return new insn_br(next, OP,  std::move(lhs), std::move(rhs), std::move(dest1), std::move(dest2)); } \
   // end # define RSN_M1(OP)
      RSN_M1(_beq) RSN_M1(_bne) RSN_M1(_ublt) RSN_M1(_uble) RSN_M1(_sblt) RSN_M1(_sble)
   # undef RSN_M1
   public:
      insn_br *clone(bblock *owner) const override { return new insn_br(owner, op, _inputs, _targets); }
      insn_br *clone(insn *next) const override { return new insn_br(next, op, _inputs, _targets); }
   public: // data operands and jump targets
      auto &lhs() noexcept         { return _inputs [0]; }
      auto &lhs() const noexcept   { return _inputs [0]; }
      auto &rhs() noexcept         { return _inputs [1]; }
      auto &rhs() const noexcept   { return _inputs [1]; }
      auto &dest1() noexcept       { return _targets[0]; }
      auto &dest1() const noexcept { return _targets[0]; }
      auto &dest2() noexcept       { return _targets[1]; }
      auto &dest2() const noexcept { return _targets[1]; }
   public: // miscellaneous
      bool try_to_fold() override;
   public: // internal representation
      std::array<lib::smart_ptr<operand>, 2> _inputs;
      std::array<bblock *, 2> _targets;
   private: // implementation helpers
      template<typename Loc> RSN_INLINE explicit insn_br( Loc loc, decltype(op) op,
         lib::smart_ptr<operand> &&lhs, lib::smart_ptr<operand> &&rhs, bblock *&&dest1, bblock *&&dest2 )
         : insn(loc), op(op), _inputs{std::move(lhs), std::move(rhs)}, _targets{std::move(dest1)), rhs(std::move(dest2)} {
         init(_inputs, _targets);
      }
      template<typename Loc> RSN_INLINE explicit insn_br( Loc loc, decltype(op) op,
         const declype(_inputs) &inputs, const declype(_targets) &targets ) noexcept
         : insn(loc), op(op), _inputs(inputs), _targets(targets) {
         init(_inputs, _targets);
      }
   # if RSN_USE_DEBUG
   public:
      void dump() const noexcept override {
         static constexpr const char *mnemo[]
            {"beq", "bne", "ublt", "uble", "sblt", "sble"};
         log << mnemo[op] << ' ' << lhs() << ", " << rhs() << " to " << dest1() << ", " << dest2();
      }
   # endif
   };

   class insn_switch_br final: public insn {
   public: // construction/destruction
      RSN_INLINE static auto make( bblock *owner,
         lib::smart_ptr<operand> index, std::vector<bblock *> dests )
         { return new insn_switch_br(owner, std::move(index), std::move(dests)); }
      RSN_INLINE static auto make(insn *next,
         lib::smart_ptr<operand> index, std::vector<bblock *> dests )
         { return new insn_switch_br(next,  std::move(index), std::move(dests)); }
   public:
      insn_switch_br *clone(bblock *owner) const override { return new insn_switch_br(owner, _inputs, _targets); }
      insn_switch_br *clone(insn *next) const override { return new insn_switch_br(next, _inputs, _targets); }
   public: // data operands and jump targets
      auto &index() noexcept       { return _inputs[0]; }
      auto &index() const noexcept { return _inputs[0]; }
      auto  dests() noexcept       { return lib::range_ref{&*_targets.begin(), &*_targets.end()}; }
      auto  dests() const noexcept { return lib::range_ref{&*_targets.begin(), &*_targets.end()}; }
   public: // miscellaneous
      bool try_to_fold() override;
   public: // internal representation
      std::array<lib::smart_ptr<operand>, 1> _inputs;
      std::vector<bblock *> _targets;
   private: // implementation helpers
      template<typename Loc> RSN_INLINE explicit insn_switch_br( Loc loc,
         lib::smart_ptr<operand> &&index, std::vector<bblock *> &&dests )
         : insn(loc), _inputs{std::move(index)}, _targets(std::move(dests)) {
         init(_inputs, _targets);
      }
      template<typename Loc> RSN_INLINE explicit insn_switch_br( Loc loc,
         const declype(_inputs) &inputs, const declype(_targets) &targets ) noexcept
         : insn(loc), _inputs(inputs), _targets(targets) {
         init(_inputs, _targets);
      }
   # if RSN_USE_DEBUG
   public:
      void dump() const noexcept override { log << "jmp " << index << " to ( " << dests << " )"; }
   # endif
   };

   class insn_undefined final: public insn {
   public: // construction/destruction
      RSN_INLINE static auto make(bblock *owner)
         { return new insn_undefined(owner); }
      RSN_INLINE static auto make(insn *next)
         { return new insn_undefined(next); }
   public:
      insn_undefined *clone(bblock *owner) const override { return new insn_undefined(owner); }
      insn_undefined *clone(insn *next) const override { return new insn_undefined(next); }
   private: // implementation helpers
      template<typename Loc> RSN_INLINE explicit insn_undefined(Loc loc) noexcept
         : insn(loc) {
         init();
      }
      template<typename Loc> RSN_INLINE explicit insn_undefined(Loc loc) noexcept
         : insn(loc) {
         init();
      }
   # if RSN_USE_DEBUG
   public:
      void dump() const noexcept override { log << "undefined"; }
   # endif
   };

} // namespace rsn::opt

# endif // # ifndef RSN_INCLUDED_IR
