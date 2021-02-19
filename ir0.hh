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

# include "rusini.hh"

namespace rsn::opt {

   // Basic Declarations ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

   class operand;
   class bblock;
   class insn;

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
      operand         -- data operand (abs, rel, vreg)... excluding jump targets
       +-imm          -- 64-bit (abs or rel) immediate constant value
       |  +-abs       -- absolute constant value
       |  +-rel_base  -- base relocatable (w/o addendum); externally defined ("extern") when not subclassed
       |  |  +-proc   -- (address of) procedure, AKA function or subroutine (translation unit)
       |  |  +-data   -- address of static data block
       |  +-rel_disp  -- displaced relocatable (w/ nonzero addendum)
       +-vreg         -- virtual register of "infinite" width
   */

   class operand: protected aux::node, // data operand (abs, rel, vreg)... excluding jump targets
      protected lib::smart_rc_mixin {
      template<typename> friend class lib::smart_ptr;
   protected:
      const enum: unsigned char { _imm = 1, _abs = 1, _rel_base = 3, _proc, _data, _rel_disp = 2, _reg = 0 } kind;
   private: // implementation helpers
      RSN_INLINE explicit operand(decltype(kind) kind) noexcept: kind(kind) {}
      virtual ~operand() = 0;
      friend class imm;  // descendant
      friend class vreg; // ditto
   private: // fast (and trivial) RTTI
      template<typename> bool type_check() const noexcept = delete;
      template<typename, typename Src> friend std::enable_if_t<std::is_base_of_v<noncopyable<>, Src>, bool> lib::is(Src *) noexcept;
   # if RSN_USE_DEBUG
   public: // debugging
      virtual void dump() const noexcept = 0;
   private:
      virtual void dump_ref() const noexcept = 0;
      friend decltype(log);
   # endif // # if RSN_USE_DEBUG
   };
   RSN_INLINE inline operand::~operand() = default;

   class imm: public operand { // 64-bit (abs or rel) immediate constant value
      RSN_INLINE explicit imm(decltype(kind) kind) noexcept: operand{kind} {}
      ~imm() override = 0;
      template<typename> friend class lib::smart_ptr;
      friend class abs;      // descendant
      friend class rel_base; // ditto
      friend class rel_disp; // ditto
   };
   RSN_INLINE inline imm::~imm() = default;
   template<> RSN_INLINE inline bool operand::type_check<imm>() const noexcept { return kind >= _imm; }

   class abs final: public imm { // absolute constant value
   public: // public data members
      const unsigned long long val;
   public: // construction/destruction
      RSN_INLINE RSN_NODISCARD static auto make(decltype(val) val) { return lib::smart_ptr<abs>::make(std::move(val)); }
   private: // implementation helpers
      RSN_INLINE explicit abs(decltype(val) &&val) noexcept: imm{_abs}, val(std::move(val)) {}
      RSN_INLINE explicit abs(smart_tag, decltype(val) &&val) noexcept: abs{std::move(val)} {}
      ~abs() override = default;
      template<typename> friend class lib::smart_ptr;
   # if RSN_USE_DEBUG
   public: // debugging
      void dump() const noexcept override { std::fprintf(stderr, "N%u = abs #%lld[0x%llX]\n\n", node::sn, (long long)val, val); }
   private:
      void dump_ref() const noexcept override { std::fprintf(stderr, "N%u#%lld[0x%llX]", node::sn, (long long)val, val); }
      friend decltype(log);
   # endif // # if RSN_USE_DEBUG
   };
   template<> RSN_INLINE inline bool operand::type_check<abs>() const noexcept { return kind == _abs; }

   class rel_base: public imm { // base relocatable (w/o addendum); externally defined ("extern") when not subclassed
   public: // public data members
      const std::pair<unsigned long long, unsigned long long> id; // link-time symbol (content hash)
   public: // construction/destruction
      RSN_INLINE RSN_NODISCARD static auto make(decltype(id) id) { return lib::smart_ptr<rel_base>::make(std::move(id)); }
   private: // implementation helpers
      RSN_INLINE explicit rel_base(decltype(kind) kind, decltype(id) &&id) noexcept: imm{kind}, id(std::move(id)) {}
      RSN_INLINE explicit rel_base(smart_tag, decltype(id) &&id) noexcept: rel_base{_rel_base, std::move(id)} {}
      ~rel_base() override = default;
      template<typename> friend class lib::smart_ptr;
      friend class proc; // descendant
      friend class data; // ditto
   # if RSN_USE_DEBUG
   public: // debugging
      void dump() const noexcept override
         { std::fprintf(stderr, "X%u = extern $0x%08X[$0x%016llX%016llX]\n\n", node::sn, (unsigned)id.first, id.second, id.first); }
   private:
      void dump_ref() const noexcept override { std::fprintf(stderr, "X%u$0x%08X[...]", node::sn, (unsigned)id.first); }
      friend decltype(log);
   # endif // # if RSN_USE_DEBUG
   };
   template<> RSN_INLINE inline bool operand::type_check<rel_base>() const noexcept { return kind >= _rel_base; }

   class proc final: public rel_base, // (address of) procedure, AKA function or subroutine (translation unit)
      public lib::collection_mixin<proc, bblock> {
   public: // construction/destruction
      RSN_INLINE RSN_NODISCARD static auto make(decltype(id) id) { return lib::smart_ptr<proc>::make(std::move(id)); }
   private: // implementation helpers
      RSN_INLINE explicit proc(decltype(id) &&id) noexcept: rel_base{_proc, std::move(id)} {}
      RSN_INLINE explicit proc(smart_tag, decltype(id) &&id) noexcept: proc{std::move(id)} {}
      ~proc() override;
      template<typename> friend class lib::smart_ptr;
   # ifdef RSN_USE_DEBUG
   public: // debugging
      void dump() const noexcept override;
   private:
      void dump_ref() const noexcept override { std::fprintf(stderr, "P%u$0x%08X[...]", node::sn, (unsigned)id.first); }
      friend decltype(log);
   # endif // # if RSN_USE_DEBUG
   };
   template<> RSN_INLINE inline bool operand::type_check<proc>() const noexcept { return kind == _proc; }

   class data final: public rel_base { // address of static data block
   public: // public data members
      const std::vector<lib::smart_ptr<imm>> values;
   public: // construction/destruction
      RSN_INLINE RSN_NODISCARD static auto make(decltype(id) id, decltype(values) values) { return lib::smart_ptr<data>::make(std::move(id), std::move(values)); }
   private: // implementation helpers
      RSN_INLINE explicit data(decltype(id) &&id, decltype(values) &&values) noexcept: rel_base{_data, std::move(id)}, values(std::move(values)) {}
      RSN_INLINE explicit data(smart_tag, decltype(id) &&id, decltype(values) &&values) noexcept: data{std::move(id), std::move(values)} {}
      ~data() override = default;
      template<typename> friend class lib::smart_ptr;
   # if RSN_USE_DEBUG
   public: // debugging
      void dump() const noexcept override {
         std::fprintf(stderr, "D%u = data D%u$0x%08X[...] as\n", node::sn, (unsigned)id.first, id.second, id.first);
         for (auto &&it: values) log << "    " << it << '\n';
         std::fprintf(stderr, "end data D%u\n\n", node::sn);
      }
   private:
      void dump_ref() const noexcept override { std::fprintf(stderr, "D%u$0x%08X[...]", node::sn, (unsigned)id.first); }
      friend decltype(log);
   # endif // # if RSN_USE_DEBUG
   };
   template<> RSN_INLINE inline bool operand::type_check<data>() const noexcept { return kind == _data; }

   class rel_disp final: public imm { // displaced relocatable (w/ nonzero addendum)
   public: // public data members
      const lib::smart_ptr<rel_base> base; // base relocatable w/o addendum
      const unsigned long long add;        // the addendum
   public: // construction/destruction
      RSN_INLINE RSN_NODISCARD static auto make(decltype(base) base, decltype(add) add) { return lib::smart_ptr<rel_disp>::make(std::move(base), std::move(add)); }
   private: // implementation helpers
      RSN_INLINE explicit rel_disp(decltype(base) base, decltype(add) add) noexcept: imm{_rel_disp}, base(std::move(base)), add(std::move(add)) {}
      RSN_INLINE explicit rel_disp(smart_tag, decltype(base) base, decltype(add) add) noexcept: rel_disp{std::move(base), std::move(add)} {}
      ~rel_disp() override = default;
      template<typename> friend class lib::smart_ptr;
   # if RSN_USE_DEBUG
   public: // debugging
      void dump() const noexcept override { std::fprintf(stderr, "A%u = rel +", node::sn), log << base, std::fprintf(stderr, "%+lld[0x%llX]\n\n", (long long)add, add); }
   private:
      void dump_ref() const noexcept override { std::fprintf(stderr, "A%u+", node::sn), log << base, std::fprintf(stderr, "%+lld[0x%llX]", add, add); }
      friend decltype(log);
   # endif // # if RSN_USE_DEBUG
   };
   template<> RSN_INLINE inline bool operand::type_check<rel_disp>() const noexcept { return kind == _rel_disp; }

   class vreg final: public operand { // virtual register of "infinite" width
   public: // construction/destruction
      RSN_INLINE RSN_NODISCARD static auto make() { return lib::smart_ptr<vreg>::make(); }
   public: // miscellaneous
      std::size_t sn;
   private: // implementation helpers
      RSN_INLINE explicit vreg() noexcept: operand{_reg} {}
      RSN_INLINE explicit vreg(smart_tag) noexcept: vreg{} {}
      ~vreg() override = default;
      template<typename> friend class lib::smart_ptr;
   # if RSN_USE_DEBUG
   public: // debugging
      void dump() const noexcept override { std::fprintf(stderr, "R%u = vreg\n\n", node::sn); }
   private:
      void dump_ref() const noexcept override { std::fprintf(stderr, "R%u", node::sn); }
      friend decltype(log);
   # endif // # if RSN_USE_DEBUG
   };
   template<> RSN_INLINE inline bool operand::type_check<vreg>() const noexcept { return kind == _reg; }

   // Basic Blocks and Instructions ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

   class bblock final: aux::node, // basic block (also used to specify a jump target)
      public lib::collection_item_mixin<bblock, proc>, public lib::collection_mixin<bblock, insn> {
   public: // construction
      static auto make(proc *owner) { return new bblock(owner); } // construct and attach to the specified owner procedure at the end
      static auto make(bblock *next) { return new bblock(next); } // construct and attach to the owner procedure before the specified sibling basic block
   public: // miscellaneous
      std::size_t sn;
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
      void dump_ref() const noexcept { std::fprintf(stderr, "L%u", node::sn); }
      friend decltype(log);
   # endif // # if RSN_USE_DEBUG
   };
   RSN_NOINLINE inline proc::~proc() = default;

   class insn: protected aux::node, // IR instruction
      public lib::collection_item_mixin<insn, bblock> {
   protected:
      enum kind: int;
   protected: // constructors/destructors
      RSN_INLINE explicit insn(enum kind kind, bblock *owner) noexcept // attach to the specified owner basic block at the end
         : collection_item_mixin(owner), kind(kind) {}
      RSN_INLINE explicit insn(enum kind kind, insn *next) noexcept // attach to the owner basic block before the specified sibling instruction
         : collection_item_mixin(next), kind(kind) {}
   protected:
      RSN_INLINE virtual ~insn() = default;
      friend collection_item_mixin;
   public: // copy-construction
      virtual insn *clone(bblock *owner) const = 0; // make a copy and attach it to the specified new owner basic block at the end
      virtual insn *clone(insn *next) const = 0;    // make a copy and attach it to the new owner basic block before the specified sibling instruction
   private: // fast (and trivial) RTTI
      template<typename> bool type_check() const noexcept = delete;
      template<typename, typename Src> friend std::enable_if_t<std::is_base_of_v<noncopyable<>, Src>, bool> lib::is(Src *) noexcept;
   public: // querying contents
      RSN_INLINE auto inputs() noexcept->lib::range_ref<lib::smart_ptr<operand> *>             { return _inputs;  }
      RSN_INLINE auto inputs() const noexcept->lib::range_ref<const lib::smart_ptr<operand> *> { return _inputs;  }
      RSN_INLINE auto outputs() noexcept->lib::range_ref<lib::smart_ptr<vreg> *>               { return _outputs; }
      RSN_INLINE auto outputs() const noexcept->lib::range_ref<const lib::smart_ptr<vreg> *>   { return _outputs; }
      RSN_INLINE auto targets() noexcept->lib::range_ref<bblock **>                            { return _targets; }
      RSN_INLINE auto targets() const noexcept->lib::range_ref<bblock *const *>                { return _targets; }
   public: // miscellaneous
      RSN_INLINE virtual bool simplify() { return false; } // constant folding, algebraic simplification, and canonicalization
   private: // internal representation
      const enum kind kind;
   protected:
      lib::range_ref<lib::smart_ptr<operand> *> _inputs{nullptr, nullptr}; // the optimizer is to eliminate
      lib::range_ref<lib::smart_ptr<vreg> *> _outputs{nullptr, nullptr};   //  redundant stores
      lib::range_ref<bblock **> _targets{nullptr, nullptr};                //  in initialization
   # if RSN_USE_DEBUG
   public: // debugging
      virtual void dump() const noexcept = 0;
   # endif
   };
   RSN_NOINLINE inline bblock::~bblock() = default;

   class pure_insn: public insn { using insn::insn; };
   template<> RSN_INLINE inline bool insn::type_check<pure_insn>() const noexcept { return kind > 0; }

   class impure_insn: public insn { using insn::insn; };
   template<> RSN_INLINE inline bool insn::type_check<impure_insn>() const noexcept { return kind < 0; }

} // namespace rsn::opt

# endif // # ifndef RSN_INCLUDED_IR0
