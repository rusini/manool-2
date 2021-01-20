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

   // Inheritance hierarchy
   class operand;           // data operand of "infinite" width (absolute, relocatable, virtual register, etc.)... excluding jump targets
      class imm_val;        // absolute or relocatable immediate constant
         class abs_imm;     // 64-bit absolute constant value
         class rel_imm;     // relocatable constant value
            class ext_rel;  // extern (externally defined) relocatable value
            class proc;     // procedure, AKA function, subroutine, etc. (translation unit)
            class data;     // static initialized-data block
      class vreg;           // virtual register
   class bblock;            // basic block (also used to specify a jump target)
   class insn;              // IR instruction

   namespace aux {
      class node: lib::noncopyable { // IR node base class
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
            template<typename Node> const auto &operator<<(lib::range_ref<Node **> nodes) const noexcept
               { bool flag{}; for (auto &it: nodes) { if (flag) std::fputs(", ", stderr); it->dump_ref(), flag = true; } return *this; }
            template<typename Node> const auto &operator<<(lib::range_ref<lib::smart_ptr<Node> *> nodes) const noexcept
               { bool flag{}; for (auto &it: nodes) { if (flag) std::fputs(", ", stderr); it->dump_ref(), flag = true; } return *this; }
         } log{};
      private:
         static thread_local inline unsigned node_count;
      # endif // # if RSN_USE_DEBUG
      };
   } // namespace aux

   // Iteration Helpers ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

   template<typename Obj> RSN_NOINLINE auto all(Obj *ptr)->std::enable_if_t<std::is_base_of_v<aux::node, Obj>, lib::small_vec<decltype(ptr->head())>>
      { decltype(all(ptr)) res(ptr->count()); for (auto _ptr = ptr->head(); _ptr; _ptr = _ptr->next()) res.push_back(_ptr); return res; }
   template<typename Obj> RSN_NOINLINE auto rev(Obj *ptr)->std::enable_if_t<std::is_base_of_v<aux::node, Obj>, lib::small_vec<decltype(ptr->tail())>>
      { decltype(all(ptr)) res(ptr->count()); for (auto _ptr = ptr->tail(); _ptr; _ptr = _ptr->prev()) res.push_back(_ptr); return res; }

   template<typename Obj> RSN_NOINLINE auto all_after(Obj *ptr)->std::enable_if_t<std::is_base_of_v<aux::node, Obj>, lib::small_vec<decltype(ptr->next())>>
      { decltype(all_after(ptr)) res(ptr->owner()->count()); for (auto _ptr = ptr->next(); _ptr; _ptr = _ptr->next()) res.push_back(_ptr); return res; }
   template<typename Obj> RSN_NOINLINE auto rev_after(Obj *ptr)->std::enable_if_t<std::is_base_of_v<aux::node, Obj>, lib::small_vec<decltype(ptr->prev())>>
      { decltype(rev_after(ptr)) res(ptr->owner()->count()); for (auto _ptr = ptr->prev(); _ptr; _ptr = _ptr->prev()) res.push_back(_ptr); return res; }

   template<typename Obj> RSN_NOINLINE auto all_from(Obj *ptr)->std::enable_if_t<std::is_base_of_v<aux::node, Obj>, lib::small_vec<decltype(ptr->next())>>
      { decltype(all_from(ptr)) res(ptr->owner()->count()); decltype(ptr->next()) _ptr = ptr; do res.push_back(_ptr), _ptr = _ptr->next(); while (_ptr); return res; }
   template<typename Obj> RSN_NOINLINE auto rev_from(Obj *ptr)->std::enable_if_t<std::is_base_of_v<aux::node, Obj>, lib::small_vec<decltype(ptr->prev())>>
      { decltype(rev_from(ptr)) res(ptr->owner()->count()); decltype(ptr->prev()) _ptr = ptr; do res.push_back(_ptr), _ptr = _ptr->prev(); while (_ptr); return res; }

   template<typename Obj> RSN_NOINLINE auto all(const lib::smart_ptr<Obj> &ptr)->std::enable_if_t<std::is_base_of_v<aux::node, Obj>, decltype(all(&*ptr))>
      { return all(&*ptr); }
   template<typename Obj> RSN_NOINLINE auto rev(const lib::smart_ptr<Obj> &ptr)->std::enable_if_t<std::is_base_of_v<aux::node, Obj>, decltype(rev(&*ptr))>
      { return rev(&*ptr); }

   template<typename Obj> RSN_NOINLINE auto all_after(const lib::smart_ptr<Obj> &ptr)->std::enable_if_t<std::is_base_of_v<aux::node, Obj>, decltype(all_after(&*ptr))>
      { return all_after(&*ptr); }
   template<typename Obj> RSN_NOINLINE auto rev_after(const lib::smart_ptr<Obj> &ptr)->std::enable_if_t<std::is_base_of_v<aux::node, Obj>, decltype(rev_after(&*ptr))>
      { return rev_after(&*ptr); }

   template<typename Obj> RSN_NOINLINE auto all_from(const lib::smart_ptr<Obj> &ptr)->std::enable_if_t<std::is_base_of_v<aux::node, Obj>, decltype(all_from(&*ptr))>
      { return all_from(&*ptr); }
   template<typename Obj> RSN_NOINLINE auto rev_from(const lib::smart_ptr<Obj> &ptr)->std::enable_if_t<std::is_base_of_v<aux::node, Obj>, decltype(rev_from(&*ptr))>
      { return rev_from(&*ptr); }

   /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

   class operand: protected aux::node, lib::smart_rc { // data operand of "infinite" width (absolute, relocatable, virtual register, etc.)... excluding jump targets
      long rc = 1;
      operand() = default;
      virtual ~operand() = 0;
      template<typename> friend class lib::smart_ptr;
      friend imm_val; // descendant
      friend vreg;    // ditto
   # if RSN_USE_DEBUG
   public:
      virtual void dump() const noexcept = 0;
   private:
      virtual void dump_ref() const noexcept = 0;
      friend struct log;
   # endif // # if RSN_USE_DEBUG
   };
   RSN_INLINE inline operand::~operand() = default;

   class imm_val: public operand { // absolute or relocatable immediate constant
      imm_val() = default;
      ~imm_val() override = 0;
      template<typename> friend class lib::smart_ptr;
      friend abs_imm; // descendant
      friend rel_imm; // ditto
   # if RSN_USE_DEBUG
   private:
      friend struct log;
   # endif // # if RSN_USE_DEBUG
   };
   RSN_INLINE inline imm_val::~imm_val() = default;

   class abs_imm final: public imm_val { // 64-bit absolute constant value
   public: // public data members
      const unsigned long long value;
   public: // construction/destruction
      RSN_INLINE RSN_NODISCARD static auto make(decltype(value) value) { return lib::smart_ptr<abs_imm>::make(std::move(value)); }
   private: // implementation helpers
      RSN_INLINE explicit abs_imm(decltype(value) &&value) noexcept: value(std::move(value)) {}
      ~abs_imm() override = default;
      template<typename> friend class lib::smart_ptr;
   # if RSN_USE_DEBUG
   public:
      void dump() const noexcept override { std::fprintf(stderr, "N%u = abs #%lld=0x%llX\n\n", sn, (long long)value, value); }
   private:
      void dump_ref() const noexcept override { std::fprintf(stderr, "N%u#%lld=0x%llX", sn, (long long)value, value); }
      friend struct log;
   # endif // # if RSN_USE_DEBUG
   };

   class rel_imm: public imm_val { // relocatable constant value
   public: // public data members
      const std::pair<unsigned long long, unsigned long long> id;
   private: // implementation helpers
      RSN_INLINE rel_imm(decltype(id) &&id): id(std::move(id)) {}
      ~rel_imm() override = 0;
      template<typename> friend class lib::smart_ptr;
      friend ext_rel; // descendant
      friend proc;    // ditto
      friend data;    // ditto
   # if RSN_USE_DEBUG
   private:
      friend struct log;
   # endif // # if RSN_USE_DEBUG
   };
   RSN_INLINE inline rel_imm::~rel_imm() = default;

   class ext_rel final: public rel_imm { // extern (externally defined) relocatable value
   public: // construction/destruction
      RSN_INLINE RSN_NODISCARD static auto make(decltype(id) id) { return lib::smart_ptr<ext_rel>::make(std::move(id)); }
   private: // implementation helpers
      RSN_INLINE explicit ext_rel(decltype(id) &&id) noexcept: rel_imm{std::move(id)} {}
      ~ext_rel() override = default;
      template<typename> friend class lib::smart_ptr;
   # if RSN_USE_DEBUG
   public:
      void dump() const noexcept override { std::fprintf(stderr, "X%u = extern $0x%016llX%016llX\n\n", sn, id.second, id.first); }
   private:
      void dump_ref() const noexcept override { std::fprintf(stderr, "X%u$0x%016llX%016llX", sn, id.second, id.first); }
      friend struct log;
   # endif // # if RSN_USE_DEBUG
   };

   class proc final: public rel_imm, // procedure, AKA function, subroutine, etc. (translation unit)
      public lib::collection_mixin<bblock> {
   public: // construction/destruction
      RSN_INLINE RSN_NODISCARD static auto make(decltype(id) id) { return lib::smart_ptr<proc>::make(std::move(id)); }
   private: // implementation helpers
      RSN_INLINE explicit proc(decltype(id) &&id): rel_imm{std::move(id)} {}
      ~proc() override;
      template<typename> friend class lib::smart_ptr;
   # ifdef RSN_USE_DEBUG
   public:
      void dump() const noexcept override;
   private:
      void dump_ref() const noexcept override { std::fprintf(stderr, "P%u$0x%016llX%016llX", sn, id.second, id.first); }
      friend struct log;
   # endif // # if RSN_USE_DEBUG
   public: // embedded temporary data
      struct {
      # define RSN_OPT_TEMP_PROC
      # include "opt-temp-tcc"
      # undef RSN_OPT_TEMP_PROC
      } temp;
   };

   class data final: public rel_imm { // static initialized-data block
   public: // public data members
      const std::vector<lib::smart_ptr<imm_val>> values;
   public: // construction/destruction
      RSN_INLINE RSN_NODISCARD static auto make(decltype(id) id, decltype(values) values) { return lib::smart_ptr<data>::make(std::move(id), std::move(values)); }
   private: // implementation helpers
      RSN_INLINE explicit data(decltype(id) &&id, decltype(values) &&values): rel_imm{std::move(id)}, values(std::move(values)) {}
      ~data() override = default;
      template<typename> friend class lib::smart_ptr;
   # if RSN_USE_DEBUG
   public:
      void dump() const noexcept override {
         std::fprintf(stderr, "D%u = data $0x%016llX%016llX as\n", sn, id.second, id.first);
         for (auto &&it: values) log << "  " << it << '\n';
         std::fprintf(stderr, "end data D%u\n\n", sn);
      }
   private:
      void dump_ref() const noexcept override { std::fprintf(stderr, "D%u$0x%016llX%016llX", sn, id.second, id.first); }
      friend struct log;
   # endif // # if RSN_USE_DEBUG
   };

   class vreg final: public operand { // virtual register
   public: // construction/destruction
      RSN_INLINE RSN_NODISCARD static auto make() { return lib::smart_ptr<vreg>::make(); }
   private: // implementation helpers
      RSN_INLINE explicit vreg(proc *owner) noexcept: _owner(owner) {}
      ~vreg() override = default;
      template<typename> friend class lib::smart_ptr;
   # if RSN_USE_DEBUG
   public:
      void dump() const noexcept override { std::fprintf(stderr, "R%u = vreg ", sn), log << '(' << owner() << ')', std::fputs("\n\n", stderr); }
   private:
      void dump_ref() const noexcept override { std::fprintf(stderr, "R%u", sn); }
      friend struct log;
   # endif // # if RSN_USE_DEBUG
   public: // embedded temporary data
      struct {
      # define RSN_OPT_TEMP_VREG
      # include "opt-temp-tcc"
      # undef RSN_OPT_TEMP_VREG
      } temp;
   };

   class bblock final: protected aux::node, // basic block (also used to specify a jump target)
      public lib::collection_item_mixin<bblock>, public lib::collection_mixin<insn> {
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
   RSN_INLINE inline proc::~proc() = default;

   class insn: protected aux::node, // IR instruction
      public lib::collection_item_mixin<insn> {
   protected: // constructors/destructors
      RSN_INLINE explicit insn(bblock *owner) noexcept: collection_item_mixin(owner) {} // attach to the specified owner basic block at the end
      RSN_INLINE explicit insn(insn *next) noexcept: collection_item_mixin(next) {}     // attach to the owner basic block before the specified sibling instruction
   protected:
      virtual ~insn() = default;
      friend collection_item_mixin;
   public: // copy-construction
      virtual insn *clone(bblock *owner) const = 0; // make a copy and attach it to the specified new owner basic block at the end
      virtual insn *clone(insn *next) const = 0;    // make a copy and attach it to the new owner basic block before the specified sibling instruction
   public: // querying contents and instruction simplification
      RSN_INLINE auto outputs() noexcept->lib::range_ref<lib::smart_ptr<vreg> *>                { return _outputs; }
      RSN_INLINE auto outputs() const noexcept->lib::range_ref<const lib::smart_ptr<vreg> *>    { return _outputs; }
      RSN_INLINE auto inputs () noexcept->lib::range_ref<lib::smart_ptr<operand> *>             { return _inputs; }
      RSN_INLINE auto inputs () const noexcept->lib::range_ref<const lib::smart_ptr<operand> *> { return _inputs; }
      RSN_INLINE auto targets() noexcept->lib::range_ref<bblock **>                             { return _targets; }
      RSN_INLINE auto targets() const noexcept->lib::range_ref<bblock *const *>                 { return _targets; }
   public:
      RSN_INLINE virtual bool simplify() { return false; } // constant folding, algebraic simplification (e.g. x + 0 -> x), and canonicalization (e.g. 0 == x -> x == 0)
   protected: // internal representation
      lib::range_ref<lib::smart_ptr<vreg> *> _outputs;
      lib::range_ref<lib::smart_ptr<operand> *> _inputs;
      lib::range_ref<bblock **> _targets;
   # if RSN_USE_DEBUG
   public:
      virtual void dump() const noexcept = 0;
   # endif // # if RSN_USE_DEBUG
   public: // embedded temporary data
      struct {
      # define RSN_OPT_TEMP_INSN
      # include "opt-temp-tcc"
      # undef RSN_OPT_TEMP_INSN
      } temp;
   };
   RSN_INLINE inline bblock::~bblock() = default;

} // namespace rsn::opt

# endif // # ifndef RSN_INCLUDED_IR0
