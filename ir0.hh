// ir0.hh

/*    Copyright (C) 2020, 2021 Alexey Protasov (AKA Alex or rusini)

   This is free software: you can redistribute it and/or modify it under the terms of the version 3 of the GNU General Public License
   as published by the Free Software Foundation (and only version 3).

   This software is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along with this software.  If not, see <https://www.gnu.org/licenses/>.  */


# ifndef RSN_INCLUDED_IR0
# define RSN_INCLUDED_IR0

# include <utility> // pair
# include <vector>

# if RSN_USE_DEBUG
   # include <cstdio> // fprintf, fputc, fputs, stderr
# endif

# include "rusini.hh"

namespace rsn::opt {

   // Basic Declarations ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

   using lib::is, lib::as;

   namespace aux {
      class node: lib::noncopyable<> { // IR node base class
         node() = default;
         ~node() = default;
         friend operand;
         friend bblock;
         friend insn;
      public:
         node(const node &) = delete;
         node &operator=(const node &) = delete;
      # if RSN_USE_DEBUG
      protected:
         const unsigned sn = ++node_count;
      protected:
         static constexpr struct {
            const auto &operator<<(const char *str) const noexcept
               { std::fputs(str, stderr); return *this; }
            const auto &operator<<(char chr) const noexcept
               { std::fputc(chr, stderr); return *this; }
            template<typename Node> const auto &operator<<(Node *node) const noexcept
               { node->dump_ref(); return *this; }
            template<typename Node> const auto &operator<<(const lib::smart_ptr<Node> &node) const noexcept
               { node->dump_ref(); return *this; }
            template<typename Begin, typename End = Begin> const auto &operator<<(lib::range_ref<Begin, End> nodes) const noexcept
               { bool flag{}; for (auto &it: nodes) { if (flag) std::fputs(", ", stderr); it->dump_ref(), flag = true; } return *this; }
         } log{};
      private:
         static RSN_IF_WITH_MT(thread_local) inline unsigned node_count;
      # endif // # if RSN_USE_DEBUG
      };
   } // namespace aux

   // Data Operands ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

   /* Inheritance Hierarchy
      operand         -- data operand of "infinite" width (abs, rel, reg, etc.)... excluding jump targets
       | imm          -- absolute or relocatable immediate constant
       |  | abs       -- 64-bit absolute constant value
       |  + rel_base  -- base relocatable (w/o addendum); extern (externally defined) when not subclassed
       |  |  + proc   -- procedure, AKA function, subroutine, etc. (translation unit)
       |  |  + data   -- static initialized-data block
       |  + rel_disp  -- displaced relocatable (w/ nonzero addendum)
       + reg          -- generalized (virtual or real) register
   */

   class operand: protected aux::node, lib::smart_rc { // data operand of "infinite" width (abs, rel, vreg, etc.)... excluding jump targets
      RSN_INLINE operand(decltype(kind) kind) noexcept: kind(kind) {}
      virtual ~operand() = 0;
      template<typename> friend class lib::smart_ptr;
      friend imm; // descendant
      friend reg; // ditto
   public: // fast (and trivial) RTTI
      RSN_INLINE bool is_imm() const noexcept      { return kind >= _imm; }
      RSN_INLINE bool is_abs() const noexcept      { return kind == _abs; }
      RSN_INLINE bool is_rel_base() const noexcept { return kind >= _rel_base; }
      RSN_INLINE bool is_proc() const noexcept     { return kind == _proc; }
      RSN_INLINE bool is_data() const noexcept     { return kind == _data; }
      RSN_INLINE bool is_rel_disp() const noexcept { return kind == _rel_disp; }
      RSN_INLINE bool is_reg() const noexcept      { return kind == _reg; }
   public:
      RSN_INLINE auto as_imm() noexcept;
      RSN_INLINE auto as_abs() noexcept;
      RSN_INLINE auto as_rel_base() noexcept;
      RSN_INLINE auto as_proc() noexcept;
      RSN_INLINE auto as_data() noexcept;
      RSN_INLINE auto as_rel_disp() noexcept;
      RSN_INLINE auto as_reg() noexcept;
   public:
      RSN_INLINE auto as_smart_imm() noexcept;
      RSN_INLINE auto as_smart_abs() noexcept;
      RSN_INLINE auto as_smart_rel_base() noexcept;
      RSN_INLINE auto as_smart_proc() noexcept;
      RSN_INLINE auto as_smart_data() noexcept;
      RSN_INLINE auto as_smart_rel_disp() noexcept;
      RSN_INLINE auto as_smart_reg() noexcept;
   public: // miscellaneous
      virtual bool equals(operand *) const noexcept = 0; // "provably denotes the same value at some point"
   private: // internal representation
      const enum: unsigned char { _imm = 1, _abs = 1, _rel_base = 3, _proc, _data, _rel_disp = 2, _reg = 0 } kind;
   # if RSN_USE_DEBUG
   public: // debugging
      virtual void dump() const noexcept = 0;
   private:
      virtual void dump_ref() const noexcept = 0;
      friend decltype(log);
   # endif // # if RSN_USE_DEBUG
   };
   RSN_INLINE inline operand::~operand() = default;

   class imm: public operand { // absolute or relocatable immediate constant
      RSN_INLINE imm(decltype(kind) kind = _imm) noexcept: operand{kind} {}
      ~imm() override = 0;
      template<typename> friend class lib::smart_ptr;
      friend abs;      // descendant
      friend rel_base; // ditto
      friend rel_disp; // ditto
   };
   RSN_INLINE inline imm::~imm() = default;

   class abs final: public imm { // 64-bit absolute constant value
   public: // public data members
      const unsigned long long val;
   public: // construction/destruction
      RSN_INLINE RSN_NODISCARD static auto make(decltype(val) val) { return lib::smart_ptr<abs_imm>::make(std::move(val)); }
   public: // miscellaneous
      RSN_INLINE bool equals(operand *rhs) const noexcept override { return RSN_UNLIKELY(rhs->is_abs()) && rhs->as_abs()->val == val; }
   private: // implementation helpers
      RSN_INLINE explicit abs(decltype(val) &&val, decltype(kind) kind = _abs) noexcept: imm{kind}, val(std::move(val)) {}
      ~abs() override = default;
      template<typename> friend class lib::smart_ptr;
   # if RSN_USE_DEBUG
   public: // debugging
      void dump() const noexcept override { std::fprintf(stderr, "N%u = abs #%lld[#0x%llX]\n\n", sn, (long long)val, val); }
   private:
      void dump_ref() const noexcept override { std::fprintf(stderr, "N%u#%lld[#0x%llX]", sn, (long long)val, val); }
      friend decltype(log);
   # endif // # if RSN_USE_DEBUG
   };

   class rel_base: public imm { // base relocatable (w/o addendum)
   public: // public data members
      const std::pair<unsigned long long, unsigned long long> id; // link-time symbol (content hash)
   public: // construction/destruction
      RSN_INLINE RSN_NODISCARD static auto make(decltype(id) id) { return lib::smart_ptr<rel_base>::make(std::move(id)); }
   public: // miscellaneous
      RSN_INLINE bool equals(operand *rhs) const noexcept final { return RSN_UNLIKELY(rhs->is_rel_base()) && rhs->as_rel_base()->id == id; }
   private: // implementation helpers
      RSN_INLINE rel_base(decltype(id) &&id, decltype(kind) kind = _rel_base): imm{kind}, id(std::move(id)) {}
      ~rel_base() override = default;
      template<typename> friend class lib::smart_ptr;
      friend proc; // descendant
      friend data; // ditto
   # if RSN_USE_DEBUG
   public: // debugging
      void dump() const noexcept override { std::fprintf(stderr, "X%u = extern $0x%016llX%016llX\n\n", sn, id.second, id.first); }
   private:
      void dump_ref() const noexcept override { std::fprintf(stderr, "X%u$0x%016llX%016llX", sn, id.second, id.first); }
      friend decltype(log);
   # endif // # if RSN_USE_DEBUG
   };

   class proc final: public rel_base, // procedure, AKA function, subroutine, etc. (translation unit)
      public lib::collection_mixin<proc, bblock> {
   public: // construction/destruction
      RSN_INLINE RSN_NODISCARD static auto make(decltype(id) id) { return lib::smart_ptr<proc>::make(std::move(id)); }
   private: // implementation helpers
      RSN_INLINE explicit proc(decltype(id) &&id, decltype(kind) kind = _proc): rel_base{std::move(id), kind} {}
      ~proc() override;
      template<typename> friend class lib::smart_ptr;
   # ifdef RSN_USE_DEBUG
   public: // debugging
      void dump() const noexcept override;
   private:
      void dump_ref() const noexcept override { std::fprintf(stderr, "P%u$0x%016llX%016llX", sn, id.second, id.first); }
      friend decltype(log);
   # endif // # if RSN_USE_DEBUG
   public: // embedded temporary data
      struct {
      # define RSN_OPT_TEMP_PROC
         # include "opt-temp.tcc"
      # undef RSN_OPT_TEMP_PROC
      } temp;
   };

   class data final: public rel_base { // static initialized-data block
   public: // public data members
      const std::vector<lib::smart_ptr<imm_val>> values;
   public: // construction/destruction
      RSN_INLINE RSN_NODISCARD static auto make(decltype(id) id, decltype(values) values) { return lib::smart_ptr<data>::make(std::move(id), std::move(values)); }
   private: // implementation helpers
      RSN_INLINE explicit data(decltype(id) &&id, decltype(values) &&values, decltype(kind) kind = _data): rel_base{std::move(id), kind}, values(std::move(values)) {}
      ~data() override = default;
      template<typename> friend class lib::smart_ptr;
   # if RSN_USE_DEBUG
   public: // debugging
      void dump() const noexcept override {
         std::fprintf(stderr, "D%u = data $0x%016llX%016llX as\n", sn, id.second, id.first);
         for (auto &&it: values) log << "  " << it << '\n';
         std::fprintf(stderr, "end data D%u\n\n", sn);
      }
   private:
      void dump_ref() const noexcept override { std::fprintf(stderr, "D%u$0x%016llX%016llX", sn, id.second, id.first); }
      friend decltype(log);
   # endif // # if RSN_USE_DEBUG
   };

   class rel_disp final: public imm { // displaced relocatable (w/ nonzero addendum)
   public: // public data members
      const lib::smart_ptr<rel_base> base; // base relocatable w/o addendum
      const unsigned long long add;        // the addendum
   public: // construction/destruction
      RSN_INLINE RSN_NODISCARD static auto make(decltype(base) base, decltype(add) add) { return lib::smart_ptr<rel_disp>::make(std::move(base), std::move(add)); }
   public: // miscellaneous
      RSN_INLINE bool equals(const operand *rhs) const noexcept override
         { return RSN_UNLIKELY(is<disp_rel>(rhs)) && RSN_UNLIKELY(as<const disp_rel>(rhs)->base->equals(base)) && as<const disp_rel>(rhs)->offset == offset; }
   private: // implementation helpers
      RSN_INLINE explicit rel_disp(decltype(base) base, decltype(offset) offset) noexcept: imm{_rel_disp}, base(std::move(base)), add(std::move(add)) {}
      ~rel_disp() override = default;
      template<typename> friend class lib::smart_ptr;
   # if RSN_USE_DEBUG
   public: // debugging
      void dump() const noexcept override { std::fprintf(stderr, "A%u = add ", sn), log << base, std::fprintf(stderr, "%lld[+0xllX]\n\n", (long long)offset, offset); }
   private:
      void dump_ref() const noexcept override { std::fprintf(stderr, "A%u", sn), log << base, std::fprintf(stderr, "+%llu=0xllX", offset, offset); }
      friend decltype(log);
   # endif // # if RSN_USE_DEBUG
   };

   class reg final: public operand { // generalized (virtual or real) register
   public: // construction/destruction
      RSN_INLINE RSN_NODISCARD static auto make() { return lib::smart_ptr<reg>::make(); }
   private: // implementation helpers
      RSN_INLINE explicit reg(const char *name = {}) noexcept RSN_IF_USE_DEBUG(: name(name)) {}
      ~reg() override = default;
      template<typename> friend class lib::smart_ptr;
   public:
      RSN_INLINE bool equals(const operand *rhs) const noexcept final { return rhs == this; }
   # if RSN_USE_DEBUG
   public: // debugging
      void dump() const noexcept override { if (!name) std::fprintf(stderr, "R%u = reg\n\n", sn); else std::fprintf(stderr, "R%s = reg\n\n", name); }
   private:
      const char *const name;
      void dump_ref() const noexcept override { if (!name) std::fprintf(stderr, "R%u", sn); else std::fprintf(stderr, "R%s", name); }
      friend decltype(log);
   # endif // # if RSN_USE_DEBUG
   public: // embedded temporary data
      struct {
      # define RSN_OPT_TEMP_VREG
         # include "opt-temp.tcc"
      # undef RSN_OPT_TEMP_VREG
      } temp;
   };

   RSN_INLINE inline auto operand::as_abs() noexcept      { return lib::as<imm>(this); }
   RSN_INLINE inline auto operand::as_abs() noexcept      { return lib::as<abs>(this); }
   RSN_INLINE inline auto operand::as_rel_base() noexcept { return lib::as<rel_base>(this); }
   RSN_INLINE inline auto operand::as_proc() noexcept     { return lib::as<proc>(this); }
   RSN_INLINE inline auto operand::as_data() noexcept     { return lib::as<data>(this); }
   RSN_INLINE inline auto operand::as_rel_disp() noexcept { return lib::as<rel_disp>(this); }
   RSN_INLINE inline auto operand::as_reg() noexcept      { return lib::as<reg>(this); }

   RSN_INLINE inline auto operand::as_smart_imm() noexcept      { return lib::as_smart<imm>(this); }
   RSN_INLINE inline auto operand::as_smart_abs() noexcept      { return lib::as_smart<abs>(this); }
   RSN_INLINE inline auto operand::as_smart_rel_base() noexcept { return lib::as_smart<rel_base>(this); }
   RSN_INLINE inline auto operand::as_smart_proc() noexcept     { return lib::as_smart<proc>(this); }
   RSN_INLINE inline auto operand::as_smart_data() noexcept     { return lib::as_smart<data>(this); }
   RSN_INLINE inline auto operand::as_smart_rel_disp() noexcept { return lib::as_smart<rel_disp>(this); }
   RSN_INLINE inline auto operand::as_smart_reg() noexcept      { return lib::as_smart<reg>(this); }

   // Basic Blocks and Instructions ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

   class bblock final: aux::node, // basic block (also used to specify a jump target)
      public lib::collection_item_mixin<bblock, proc>, public lib::collection_mixin<bblock, class insn> {
   public: // construction
      static auto make(proc *owner) { return new bblock(owner); } // construct and attach to the specified owner procedure at the end
      static auto make(bblock *next) { return new bblock(next); } // construct and attach to the owner procedure before the specified sibling basic block
   private: // implementation helpers
      RSN_INLINE explicit bblock(proc *owner) noexcept: collection_item_mixin(owner) {}
      RSN_INLINE explicit bblock(bblock *next) noexcept: collection_item_mixin(next) {}
   private:
      ~bblock();
      friend collection_item_mixin;
   # if RSN_USE_DEBUG
   public: // debugging
      void dump() const noexcept;
   private:
      void dump_ref() const noexcept { std::fprintf(stderr, "L%u", sn); }
      friend decltype(log);
   # endif // # if RSN_USE_DEBUG
   public: // embedded temporary data
      struct {
      # define RSN_OPT_TEMP_BBLOCK
         # include "opt-temp.tcc"
      # undef RSN_OPT_TEMP_BBLOCK
      } temp;
   };
   RSN_NOINLINE inline proc::~proc() = default;

   class insn: protected aux::node, // IR instruction
      public lib::collection_item_mixin<insn, bblock> {
   protected: // constructors/destructors
      RSN_INLINE explicit insn(bblock *owner) noexcept: collection_item_mixin(owner) {} // attach to the specified owner basic block at the end
      RSN_INLINE explicit insn(insn *next) noexcept: collection_item_mixin(next) {}     // attach to the owner basic block before the specified sibling instruction
   protected:
      RSN_INLINE virtual ~insn() = default;
      friend collection_item_mixin;
   public: // copy-construction
      virtual insn *clone(bblock *owner) const = 0; // make a copy and attach it to the specified new owner basic block at the end
      virtual insn *clone(insn *next) const = 0;    // make a copy and attach it to the new owner basic block before the specified sibling instruction
   public: // querying contents
      RSN_INLINE auto outputs() noexcept->lib::range_ref<lib::smart_ptr<vreg> *>               { return _outputs; }
      RSN_INLINE auto outputs() const noexcept->lib::range_ref<const lib::smart_ptr<vreg> *>   { return _outputs; }
      RSN_INLINE auto inputs() noexcept->lib::range_ref<lib::smart_ptr<operand> *>             { return _inputs;  }
      RSN_INLINE auto inputs() const noexcept->lib::range_ref<const lib::smart_ptr<operand> *> { return _inputs;  }
      RSN_INLINE auto targets() noexcept->lib::range_ref<bblock **>                            { return _targets; }
      RSN_INLINE auto targets() const noexcept->lib::range_ref<bblock *const *>                { return _targets; }
   public: // miscellaneous
      RSN_INLINE virtual bool simplify() { return false; } // constant folding, algebraic simplification, and canonicalization
   protected: // internal representation
      lib::range_ref<lib::smart_ptr<vreg> *> _outputs{nullptr, nullptr};   // the compiler is to eliminate redundant stores in initialization
      lib::range_ref<lib::smart_ptr<operand> *> _inputs{nullptr, nullptr}; // ditto
      lib::range_ref<bblock **> _targets{nullptr, nullptr};                // ditto
   # if RSN_USE_DEBUG
   public: // debugging
      virtual void dump() const noexcept = 0;
   # endif
   public: // embedded temporary data
      struct {
      # define RSN_OPT_TEMP_INSN
         # include "opt-temp.tcc"
      # undef RSN_OPT_TEMP_INSN
      } temp;
   };
   RSN_NOINLINE inline bblock::~bblock() = default;

} // namespace rsn::opt

# endif // # ifndef RSN_INCLUDED_IR0
