// ir0.hh

/*    Copyright (C) 2020 Alexey Protasov (AKA Alex or rusini)

   This is free software: you can redistribute it and/or modify it under the terms of the version 3 of the GNU General Public License
   as published by the Free Software Foundation (and only version 3).

   This software is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along with MANOOL.  If not, see <https://www.gnu.org/licenses/>.  */


# ifndef RSN_INCLUDED_IR0
# define RSN_INCLUDED_IR0

# include <type_traits> // enable_if_t, is_base_of_v
# include <utility>     // pair
# include <vector>

# if RSN_USE_DEBUG
   # include <cstdio> // fprintf, fputc, fputs, stderr
# endif

# include "rusini.hh"

namespace rsn::opt {

   using lib::is, lib::as;

   // Inheritance hierarchy
   class operand;             // data operand of "infinite" width (abs, rel, vreg, etc.)... excluding jump targets
      class imm_val;          // absolute or relocatable immediate constant
         class abs_imm;       // 64-bit absolute constant value
         class rel_imm;       // relocatable constant value
            class base_rel;   // base relocatable (w/ zero addendum)
               class ext_rel; // extern (externally defined) relocatable value
               class proc;    // procedure, AKA function, subroutine, etc. (translation unit)
               class data;    // static initialized-data block
            class disp_rel;   // displaced relocatable (w/ nonzero addendum)
      class vreg;             // virtual register
   class bblock;              // basic block (also used to specify a jump target)
   class insn;                // IR instruction

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

   /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

   class operand: protected aux::node, lib::smart_rc { // data operand of "infinite" width (abs, rel, vreg, etc.)... excluding jump targets
      operand() = default;
      virtual ~operand() = 0;
      template<typename> friend class lib::smart_ptr;
      friend imm_val; // descendant
      friend vreg;    // ditto
   public: // miscellaneous
      virtual bool equals(const operand *) const noexcept = 0; // "provably denotes the same value at some point"
   # if RSN_USE_DEBUG
   public: // debugging
      virtual void dump() const noexcept = 0;
   private:
      virtual void dump_ref() const noexcept = 0;
      friend decltype(log);
   # endif // # if RSN_USE_DEBUG
   };
   RSN_INLINE inline operand::~operand() = default;

   class imm_val: public operand { // absolute or relocatable immediate constant
      imm_val() = default;
      ~imm_val() override = 0;
      template<typename> friend class lib::smart_ptr;
      friend abs_imm; // descendant
      friend rel_imm; // ditto
   };
   RSN_INLINE inline imm_val::~imm_val() = default;

   class abs_imm final: public imm_val { // 64-bit absolute constant value
   public: // public data members
      const unsigned long long value;
   public: // construction/destruction
      RSN_INLINE RSN_NODISCARD static auto make(decltype(value) value) { return lib::smart_ptr<abs_imm>::make(std::move(value)); }
   public: // miscellaneous
      RSN_INLINE bool equals(const operand *rhs) const noexcept override { return RSN_UNLIKELY(is<abs_imm>(rhs)) && as<const abs_imm>(rhs)->value == value; }
   private: // implementation helpers
      RSN_INLINE explicit abs_imm(decltype(value) &&value) noexcept: value(std::move(value)) {}
      ~abs_imm() override = default;
      template<typename> friend class lib::smart_ptr;
   # if RSN_USE_DEBUG
   public: // debugging
      void dump() const noexcept override { std::fprintf(stderr, "N%u = abs #%lld=0x%llX\n\n", sn, (long long)value, value); }
   private:
      void dump_ref() const noexcept override { std::fprintf(stderr, "N%u#%lld=0x%llX", sn, (long long)value, value); }
      friend decltype(log);
   # endif // # if RSN_USE_DEBUG
   };

   class rel_imm: public imm_val { // relocatable constant value
      rel_imm() = default;
      ~rel_imm() override = 0;
      template<typename> friend class lib::smart_ptr;
      friend base_rel; // descendant
      friend disp_rel; // ditto
   };
   RSN_INLINE inline rel_imm::~rel_imm() = default;

   class base_rel: public rel_imm { // base relocatable (w/ zero addendum)
   public: // public data members
      const std::pair<unsigned long long, unsigned long long> id; // link-time symbol (content hash)
   public: // miscellaneous
      RSN_INLINE bool equals(const operand *rhs) const noexcept final { return RSN_UNLIKELY(is<base_rel>(rhs)) && as<const base_rel>(rhs)->id == id; }
   private: // implementation helpers
      RSN_INLINE base_rel(decltype(id) &&id): id(std::move(id)) {}
      ~base_rel() override = 0;
      template<typename> friend class lib::smart_ptr;
      friend ext_rel; // descendant
      friend proc;    // ditto
      friend data;    // ditto
   };
   RSN_INLINE inline base_rel::~base_rel() = default;

   class ext_rel final: public base_rel { // extern (externally defined) relocatable value
   public: // construction/destruction
      RSN_INLINE RSN_NODISCARD static auto make(decltype(id) id) { return lib::smart_ptr<ext_rel>::make(std::move(id)); }
   private: // implementation helpers
      RSN_INLINE explicit ext_rel(decltype(id) &&id) noexcept: base_rel{std::move(id)} {}
      ~ext_rel() override = default;
      template<typename> friend class lib::smart_ptr;
   # if RSN_USE_DEBUG
   public: // debugging
      void dump() const noexcept override { std::fprintf(stderr, "X%u = extern $0x%016llX%016llX\n\n", sn, id.second, id.first); }
   private:
      void dump_ref() const noexcept override { std::fprintf(stderr, "X%u$0x%016llX%016llX", sn, id.second, id.first); }
      friend decltype(log);
   # endif // # if RSN_USE_DEBUG
   };

   class proc final: public base_rel, // procedure, AKA function, subroutine, etc. (translation unit)
      public lib::collection_mixin<proc, bblock> {
   public: // construction/destruction
      RSN_INLINE RSN_NODISCARD static auto make(decltype(id) id) { return lib::smart_ptr<proc>::make(std::move(id)); }
   private: // implementation helpers
      RSN_INLINE explicit proc(decltype(id) &&id): base_rel{std::move(id)} {}
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

   class data final: public base_rel { // static initialized-data block
   public: // public data members
      const std::vector<lib::smart_ptr<imm_val>> values;
   public: // construction/destruction
      RSN_INLINE RSN_NODISCARD static auto make(decltype(id) id, decltype(values) values) { return lib::smart_ptr<data>::make(std::move(id), std::move(values)); }
   private: // implementation helpers
      RSN_INLINE explicit data(decltype(id) &&id, decltype(values) &&values): base_rel{std::move(id)}, values(std::move(values)) {}
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

   class disp_rel final: public rel_imm { // displaced relocatable (w/ nonzero addendum)
   public: // public data members
      const lib::smart_ptr<base_rel> base;
      const unsigned long long offset;
   public: // construction/destruction
      RSN_INLINE RSN_NODISCARD static auto make(decltype(base) base, decltype(offset) offset)
         { return !offset ? lib::smart_ptr<rel_imm>(std::move(base)) : lib::smart_ptr<rel_imm>(lib::smart_ptr<disp_rel>::make(std::move(base), std::move(offset))); }
   public: // miscellaneous
      RSN_INLINE bool equals(const operand *rhs) const noexcept override
         { return RSN_UNLIKELY(is<disp_rel>(rhs)) && RSN_UNLIKELY(as<const disp_rel>(rhs)->base->equals(base)) && as<const disp_rel>(rhs)->offset == offset; }
   private: // implementation helpers
      RSN_INLINE explicit disp_rel(decltype(base) base, decltype(offset) offset) noexcept: base(std::move(base)), offset(std::move(offset)) {}
      ~disp_rel() override = default;
      template<typename> friend class lib::smart_ptr;
   # if RSN_USE_DEBUG
   public: // debugging
      void dump() const noexcept override { std::fprintf(stderr, "A%u = add ", sn), log << base, std::fprintf(stderr, "%lld[+0xllX]\n\n", (long long)offset, offset); }
   private:
      void dump_ref() const noexcept override { std::fprintf(stderr, "A%u", sn), log << base, std::fprintf(stderr, "+%llu=0xllX", offset, offset); }
      friend decltype(log);
   # endif // # if RSN_USE_DEBUG
   };

   class vreg final: public operand { // virtual register
   public: // construction/destruction
      RSN_INLINE RSN_NODISCARD static auto make() { return lib::smart_ptr<vreg>::make(); }
   private: // implementation helpers
      RSN_INLINE explicit vreg() noexcept {}
      ~vreg() override = default;
      template<typename> friend class lib::smart_ptr;
   public:
      RSN_INLINE bool equals(const operand *rhs) const noexcept final { return RSN_UNLIKELY(rhs == this); }
   # if RSN_USE_DEBUG
   public: // debugging
      void dump() const noexcept override { std::fprintf(stderr, "R%u = vreg\n\n", sn); }
   private:
      void dump_ref() const noexcept override { std::fprintf(stderr, "R%u", sn); }
      friend decltype(log);
   # endif // # if RSN_USE_DEBUG
   public: // embedded temporary data
      struct {
      # define RSN_OPT_TEMP_VREG
         # include "opt-temp.tcc"
      # undef RSN_OPT_TEMP_VREG
      } temp;
   };

   class bblock final: aux::node, // basic block (also used to specify a jump target)
      public lib::collection_item_mixin<bblock, proc>, public lib::collection_mixin<bblock, insn> {
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
