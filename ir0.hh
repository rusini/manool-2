// ir0.hh

/*    Copyright (C) 2020 Alexey Protasov (AKA Alex or rusini)

   This is free software: you can redistribute it and/or modify it under the terms of the version 3 of the GNU General Public License
   as published by the Free Software Foundation (and only version 3).

   This software is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along with MANOOL.  If not, see <https://www.gnu.org/licenses/>.  */


# ifndef RSN_INCLUDED_IR0
# define RSN_INCLUDED_IR0

# include <array>
# include <cstdlib>     // ptrdiff_t, size_t
# include <cstring>     // memcpy
# include <initializer_list>
# include <iterator>    // const_reverse_iterator, reverse_iterator
# include <type_traits> // aligned_storage_t, enable_if_t, is_base_of_v, is_trivially_copyable_v, remove_cv_t
# include <utility>     // forward, move, pair
# include <vector>
# include <unordered_set>

# if RSN_USE_DEBUG
   # include <cstdio> // fprintf, fputc, fputs, stderr
# endif

# include "rusini0.hh"

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

   template<typename> class smart_ptr;

   namespace aux {
      class node { // IR node base class
         node() = default;
         ~node() = default;
         friend operand;
         friend bblock;
         friend insn;
      public:
         node(const node &) = delete;
         node &operator=(const node &) = delete;
      # if RSN_USE_DEBUG
      public:
         static thread_local inline unsigned next_sn; // the serial number (S/N) to assign to the subsequent created node
      protected:
         const unsigned sn = next_sn++;
      protected:
         static constexpr struct log {
            const log &operator<<(const char *str) const noexcept
               { std::fputs(str, stderr); return *this; }
            const log &operator<<(char chr) const noexcept
               { std::fputc(chr, stderr); return *this; }
            template<typename Node> const log &operator<<(Node *node) const noexcept
               { node->dump_ref(); return *this; }
            template<typename Node> const log &operator<<(const smart_ptr<Node> &node) const noexcept
               { node->dump_ref(); return *this; }
            template<typename Node> const log &operator<<(const std::vector<Node *> &nodes) const noexcept
               { bool flag = false; for (auto &&it: nodes) { if (flag) std::fputs(", ", stderr); flag = true; it->dump_ref(); } return *this; }
            template<typename Node> const log &operator<<(const std::vector<smart_ptr<Node>> &nodes) const noexcept
               { bool flag = false; for (auto &&it: nodes) { if (flag) std::fputs(", ", stderr); flag = true; it->dump_ref(); } return *this; }
         } log{};
      # endif // # if RSN_USE_DEBUG
      };
   } // namespace aux

   // Downcasts for IR nodes
   template<typename Dest, typename Src> RSN_INLINE inline std::enable_if_t<std::is_base_of_v<aux::node, Src>, bool> is(Src *ptr) noexcept
      { return dynamic_cast<Dest *>(ptr); }
   template<typename Dest, typename Src> RSN_INLINE inline std::enable_if_t<std::is_base_of_v<aux::node, Src>, Dest *> as(Src *ptr) noexcept
      { return static_cast<Dest *>(ptr); }
   template<typename Dest, typename Src> RSN_INLINE inline std::enable_if_t<std::is_base_of_v<aux::node, Src>, bool> is(const smart_ptr<Src> &ptr) noexcept
      { return is<Dest>(&*ptr); }
   template<typename Dest, typename Src> RSN_INLINE inline std::enable_if_t<std::is_base_of_v<aux::node, Src>, Dest *> as(const smart_ptr<Src> &ptr) noexcept
      { return as<Dest>(&*ptr); }

   template<typename Obj>
   class smart_ptr { // simple, intrussive, and MT-unsafe analog of std::shared_ptr for operands
   public: // standard operations
      smart_ptr() = default;
      RSN_INLINE smart_ptr(const smart_ptr &rhs) noexcept: rep(rhs.rep) { retain(); }
      RSN_INLINE smart_ptr(smart_ptr &&rhs) noexcept: rep(rhs.rep) { rhs.rep = {}; }
      RSN_INLINE ~smart_ptr() { release(); }
      RSN_INLINE smart_ptr &operator=(const smart_ptr &rhs) noexcept { rhs.retain(), release(), rep = rhs.rep; return *this; }
      RSN_INLINE smart_ptr &operator=(smart_ptr &&rhs) noexcept { swap(*this, rhs); return *this; }
      RSN_INLINE friend void swap(smart_ptr &lhs, smart_ptr &rhs) noexcept { std::swap(lhs.rep, rhs.rep); }
   public: // miscellaneous operations
      template<typename ...Args> RSN_INLINE RSN_NODISCARD static auto make(Args &&...args) { return smart_ptr{new Obj(std::forward<Args>(args)...)}; }
      template<typename Rhs> RSN_INLINE smart_ptr(const smart_ptr<Rhs> &rhs) noexcept: rep(rhs.rep) { retain(); }
      template<typename Rhs> RSN_INLINE smart_ptr(smart_ptr<Rhs> &&rhs) noexcept: rep(rhs.rep) { rhs.rep = {}; }
      template<typename Rhs> RSN_INLINE smart_ptr &operator=(const smart_ptr<Rhs> &rhs) noexcept { rhs.retain(), release(), rep = rhs.rep; return *this; }
      template<typename Rhs> RSN_INLINE smart_ptr &operator=(smart_ptr<Rhs> &&rhs) noexcept { rep = rhs.rep, rhs.rep = {}; return *this; }
      RSN_INLINE operator Obj *() const noexcept { return rep; }
      RSN_INLINE Obj &operator*() const noexcept { return *rep; }
      RSN_INLINE Obj *operator->() const noexcept { return rep; }
   private: // internal representation
      Obj *rep{};
      template<typename> friend class smart_ptr;
   private: // implementation helpers
      RSN_INLINE explicit smart_ptr(Obj *rep) noexcept: rep(rep) {}
      RSN_INLINE void retain() const noexcept { if (RSN_LIKELY(rep)) ++rep->operand::rc; }
      RSN_INLINE void release() const noexcept { if (RSN_LIKELY(rep) && RSN_UNLIKELY(!--rep->operand::rc)) delete rep; }
      template<typename Dest, typename Src> friend std::enable_if_t<std::is_base_of_v<aux::node, Src>, smart_ptr<Dest>> as_smart(const smart_ptr<Src> &) noexcept;
   };
   template<typename Obj> void swap(smart_ptr<Obj> &, smart_ptr<Obj> &) noexcept;

   template<typename Dest, typename Src> RSN_INLINE RSN_NODISCARD inline std::enable_if_t<std::is_base_of_v<aux::node, Src>, smart_ptr<Dest>>
      as_smart(const smart_ptr<Src> &ptr) noexcept { return ptr.retain(), smart_ptr{as<Dest>(ptr)}; }

   template<typename Obj, std::size_t MinRes = (128 - sizeof(Obj *) - sizeof(std::size_t)) / sizeof(Obj)>
   class small_vec { // lightweight analog of llvm::SmallVector
   public: // typedefs to imitate standard containers
      typedef Obj              value_type;
      typedef value_type       *pointer;
      typedef const value_type *const_pointer;
      typedef value_type       &reference;
      typedef const value_type &const_reference;
      typedef std::size_t      size_type;
      typedef std::ptrdiff_t   difference_type;
      typedef pointer          iterator;
      typedef const_pointer    const_iterator;
      typedef std::reverse_iterator<iterator> reverse_iterator;
      typedef std::reverse_iterator<const_iterator> const_reverse_iterator;
   public: // construction/destruction
      RSN_INLINE explicit small_vec(std::size_t res):
         _pbuf(reinterpret_cast<decltype(_pbuf)>(RSN_LIKELY(res <= MinRes) ? _buf.data() : new std::aligned_storage_t<sizeof(value_type)>[res])) {
      }
      small_vec(std::initializer_list<value_type> init): small_vec(init.size()) {
         for (auto &&val: init) push_back(val);
      }
      RSN_INLINE small_vec(small_vec &&rhs) noexcept: _size(rhs._size) {
         rhs._size = 0;
         if (RSN_UNLIKELY(rhs._pbuf != reinterpret_cast<decltype(rhs._pbuf)>(rhs._buf.data())))
            _pbuf = rhs._pbuf, rhs._pbuf = reinterpret_cast<decltype(rhs._pbuf)>(rhs._buf.data());
         else {
            _pbuf = reinterpret_cast<decltype(_pbuf)>(_buf.data());
            if constexpr (std::is_trivially_copyable_v<value_type>)
               std::memcpy(&_buf, &rhs._buf, sizeof _buf);
            else for (std::size_t sn = 0; sn < _size; ++sn)
               new (const_cast<std::remove_cv_t<Obj> *>(_pbuf) + sn) const value_type(std::move(rhs._pbuf[sn])), rhs._pbuf[sn].~value_type();
         }
      }
      ~small_vec() {
         if constexpr (!std::is_trivially_copyable_v<value_type>)
            for (std::size_t sn = _size; sn;) _pbuf[--sn].~value_type();
         if (RSN_UNLIKELY(_pbuf != reinterpret_cast<decltype(_pbuf)>(_buf.data())))
            delete[] reinterpret_cast<std::aligned_storage<sizeof(value_type)> *>(const_cast<std::remove_cv_t<Obj> *>(_pbuf));
      }
   public:
      small_vec &operator=(small_vec &&) = delete;
   public: // incremental building
      void push_back(const value_type &obj) noexcept       { new (const_cast<std::remove_cv_t<Obj> *>(_pbuf) + _size++) const value_type(obj); }
      RSN_INLINE void push_back(value_type &&obj) noexcept { new (const_cast<std::remove_cv_t<Obj> *>(_pbuf) + _size++) const value_type(std::move(obj)); }
   public: // access/iteration
      RSN_INLINE bool empty() const noexcept { return !size(); }
      RSN_INLINE size_type size() const noexcept { return _size; }
      RSN_INLINE pointer data() noexcept             { return _pbuf; }
      RSN_INLINE const_pointer data() const noexcept { return _pbuf; }
   public:
      RSN_INLINE iterator       begin() noexcept        { return _pbuf; }
      RSN_INLINE const_iterator begin() const noexcept  { return _pbuf; }
      RSN_INLINE iterator       end() noexcept          { return _pbuf + _size; }
      RSN_INLINE const_iterator end() const noexcept    { return _pbuf + _size; }
      RSN_INLINE const_iterator cbegin() const noexcept { return begin(); }
      RSN_INLINE const_iterator cend() const noexcept   { return end(); }
      RSN_INLINE auto rbegin() noexcept        { return reverse_iterator(end()); }
      RSN_INLINE auto rbegin() const noexcept  { return const_reverse_iterator(end()); }
      RSN_INLINE auto rend() noexcept          { return reverse_iterator(begin()); }
      RSN_INLINE auto rend() const noexcept    { return const_reverse_iterator(begin()); }
      RSN_INLINE auto crbegin() const noexcept { return rbegin(); }
      RSN_INLINE auto crend() const noexcept   { return rend(); }
   private: // internal representation
      pointer _pbuf;
      size_type _size = 0;
      std::array<std::aligned_storage_t<sizeof(value_type)>, MinRes> _buf;
   };

   // Iteration Helpers ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

   template<typename Obj> RSN_NOINLINE auto all(Obj *ptr)->std::enable_if_t<std::is_base_of_v<aux::node, Obj>, small_vec<decltype(ptr->head())>>
      { decltype(all(ptr)) res(ptr->count()); for (auto _ptr = ptr->head(); _ptr; _ptr = _ptr->next()) res.push_back(_ptr); return res; }
   template<typename Obj> RSN_NOINLINE auto rev(Obj *ptr)->std::enable_if_t<std::is_base_of_v<aux::node, Obj>, small_vec<decltype(ptr->tail())>>
      { decltype(all(ptr)) res(ptr->count()); for (auto _ptr = ptr->tail(); _ptr; _ptr = _ptr->prev()) res.push_back(_ptr); return res; }

   template<typename Obj> RSN_NOINLINE auto all_after(Obj *ptr)->std::enable_if_t<std::is_base_of_v<aux::node, Obj>, small_vec<decltype(ptr->next())>>
      { decltype(all_after(ptr)) res(ptr->owner()->count()); for (auto _ptr = ptr->next(); _ptr; _ptr = _ptr->next()) res.push_back(_ptr); return res; }
   template<typename Obj> RSN_NOINLINE auto rev_after(Obj *ptr)->std::enable_if_t<std::is_base_of_v<aux::node, Obj>, small_vec<decltype(ptr->prev())>>
      { decltype(rev_after(ptr)) res(ptr->owner()->count()); for (auto _ptr = ptr->prev(); _ptr; _ptr = _ptr->prev()) res.push_back(_ptr); return res; }

   template<typename Obj> RSN_NOINLINE auto all_from(Obj *ptr)->std::enable_if_t<std::is_base_of_v<aux::node, Obj>, small_vec<decltype(ptr->next())>>
      { decltype(all_from(ptr)) res(ptr->owner()->count()); decltype(ptr->next()) _ptr = ptr; do res.push_back(_ptr), _ptr = _ptr->next(); while (_ptr); return res; }
   template<typename Obj> RSN_NOINLINE auto rev_from(Obj *ptr)->std::enable_if_t<std::is_base_of_v<aux::node, Obj>, small_vec<decltype(ptr->prev())>>
      { decltype(rev_from(ptr)) res(ptr->owner()->count()); decltype(ptr->prev()) _ptr = ptr; do res.push_back(_ptr), _ptr = _ptr->prev(); while (_ptr); return res; }

   template<typename Obj> RSN_NOINLINE auto all(const smart_ptr<Obj> &ptr)->std::enable_if_t<std::is_base_of_v<aux::node, Obj>, decltype(all(&*ptr))>
      { return all(&*ptr); }
   template<typename Obj> RSN_NOINLINE auto rev(const smart_ptr<Obj> &ptr)->std::enable_if_t<std::is_base_of_v<aux::node, Obj>, decltype(rev(&*ptr))>
      { return rev(&*ptr); }

   template<typename Obj> RSN_NOINLINE auto all_after(const smart_ptr<Obj> &ptr)->std::enable_if_t<std::is_base_of_v<aux::node, Obj>, decltype(all_after(&*ptr))>
      { return all_after(&*ptr); }
   template<typename Obj> RSN_NOINLINE auto rev_after(const smart_ptr<Obj> &ptr)->std::enable_if_t<std::is_base_of_v<aux::node, Obj>, decltype(rev_after(&*ptr))>
      { return rev_after(&*ptr); }

   template<typename Obj> RSN_NOINLINE auto all_from(const smart_ptr<Obj> &ptr)->std::enable_if_t<std::is_base_of_v<aux::node, Obj>, decltype(all_from(&*ptr))>
      { return all_from(&*ptr); }
   template<typename Obj> RSN_NOINLINE auto rev_from(const smart_ptr<Obj> &ptr)->std::enable_if_t<std::is_base_of_v<aux::node, Obj>, decltype(rev_from(&*ptr))>
      { return rev_from(&*ptr); }

   /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

   class operand: protected aux::node { // data operand of "infinite" width (absolute, relocatable, virtual register, etc.)... excluding jump targets
      long rc = 1;
      operand() = default;
      virtual ~operand() = 0;
      template<typename> friend class smart_ptr;
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
      template<typename> friend class smart_ptr;
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
      RSN_INLINE RSN_NODISCARD static auto make(decltype(value) value) { return smart_ptr<abs_imm>::make(std::move(value)); }
   private: // implementation helpers
      RSN_INLINE explicit abs_imm(decltype(value) &&value) noexcept: value(std::move(value)) {}
      ~abs_imm() override = default;
      template<typename> friend class smart_ptr;
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
      template<typename> friend class smart_ptr;
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
      RSN_INLINE RSN_NODISCARD static auto make(decltype(id) id) { return smart_ptr<ext_rel>::make(std::move(id)); }
   private: // implementation helpers
      RSN_INLINE explicit ext_rel(decltype(id) &&id) noexcept: rel_imm{std::move(id)} {}
      ~ext_rel() override = default;
      template<typename> friend class smart_ptr;
   # if RSN_USE_DEBUG
   public:
      void dump() const noexcept override { std::fprintf(stderr, "X%u = extern $0x%016llX%016llX\n\n", sn, id.second, id.first); }
   private:
      void dump_ref() const noexcept override { std::fprintf(stderr, "X%u$0x%016llX%016llX", sn, id.second, id.first); }
      friend struct log;
   # endif // # if RSN_USE_DEBUG
   };

   class proc final: public rel_imm { // procedure, AKA function, subroutine, etc. (translation unit)
   public: // construction/destruction
      RSN_INLINE RSN_NODISCARD static auto make(decltype(id) id) { return smart_ptr<proc>::make(std::move(id)); }
   public: // querying contents
      RSN_INLINE auto head () const noexcept { return _head; }
      RSN_INLINE auto tail () const noexcept { return _tail; }
      RSN_INLINE auto count() const noexcept { return _count; }
   private: // internal representation
      bblock *_head{}, *_tail{}; long _count{}; friend bblock;
   private: // implementation helpers
      RSN_INLINE explicit proc(decltype(id) &&id): rel_imm{std::move(id)} {}
      ~proc() override;
      template<typename> friend class smart_ptr;
   # ifdef RSN_USE_DEBUG
   public:
      void dump() const noexcept override;
   private:
      void dump_ref() const noexcept override { std::fprintf(stderr, "P%u$0x%016llX%016llX", sn, id.second, id.first); }
      friend struct log;
   # endif // # if RSN_USE_DEBUG
   };

   class data final: public rel_imm { // static initialized-data block
   public: // public data members
      const std::vector<smart_ptr<imm_val>> values;
   public: // construction/destruction
      RSN_INLINE RSN_NODISCARD static auto make(decltype(id) id, decltype(values) values) { return smart_ptr<data>::make(std::move(id), std::move(values)); }
   private: // implementation helpers
      RSN_INLINE explicit data(decltype(id) &&id, decltype(values) &&values): rel_imm{std::move(id)}, values(std::move(values)) {}
      ~data() override = default;
      template<typename> friend class smart_ptr;
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
      RSN_INLINE RSN_NODISCARD static auto make(proc *owner) { return smart_ptr<vreg>::make(owner); }
   public: // querying owner
      RSN_INLINE auto owner() const noexcept { return _owner; }
   private: // internal representation
      proc *const _owner;
   private: // implementation helpers
      RSN_INLINE explicit vreg(proc *owner) noexcept: _owner(owner) {}
      ~vreg() override = default;
      template<typename> friend class smart_ptr;
   public: // embedded temporary data
      struct {
         smart_ptr<vreg> vr;
      } temp;
   # if RSN_USE_DEBUG
   public:
      void dump() const noexcept override { std::fprintf(stderr, "R%u = vreg ", sn), log << '(' << owner() << ')', std::fputs("\n\n", stderr); }
   private:
      void dump_ref() const noexcept override { std::fprintf(stderr, "R%u", sn); }
      friend struct log;
   # endif // # if RSN_USE_DEBUG
   };

   class bblock final: protected aux::node { // basic block (also used to specify a jump target)
   public: // construction/destruction
      static auto make(proc *owner) { return new bblock{owner}; } // attach to the specified owner procedure at the end
      static auto make(bblock *next) { return new bblock{next}; } // attach to the owner procedure before the specified sibling basic block
      RSN_NOINLINE void remove() noexcept { delete this; }        // remove from the owner procedure and dispose
   public: // querying owner/sibling and relocation
      RSN_INLINE auto owner() const noexcept { return _owner; }
      RSN_INLINE auto next () const noexcept { return _next; }
      RSN_INLINE auto prev () const noexcept { return _prev; }
   public:
      RSN_INLINE void reattach() noexcept { // move to the end of the owner procedure
         (RSN_LIKELY(_prev) ? _prev->_next : _owner->_head) = _next, (RSN_LIKELY(_next) ? _next->_prev : _owner->_tail) = _prev;
         _next = {}, _prev = _owner->_tail; _owner->_tail = (RSN_LIKELY(_prev) ? _prev->_next : _owner->_head) = this;
      }
   public: // querying contents
      RSN_INLINE auto head () const noexcept { return _head; }
      RSN_INLINE auto tail () const noexcept { return _tail; }
      RSN_INLINE auto count() const noexcept { return _count; }
   private: // internal representation
      proc *const _owner; bblock *_next, *_prev;
      insn *_head{}, *_tail{}; long _count{}; friend insn;
   private: // implementation helpers
      RSN_INLINE explicit bblock(proc *owner) noexcept: _owner(owner), _next{}, _prev(owner->_tail) // attach to the specified owner at the end
         { _owner->_tail = (RSN_LIKELY(_prev) ? _prev->_next : _owner->_head) = this; ++_owner->_count; }
      RSN_INLINE explicit bblock(bblock *next) noexcept: _owner(next->_owner), _next(next), _prev(next->_prev) // attach to the owner before the specified basic block
         { _next->_prev = (RSN_LIKELY(_prev) ? _prev->_next : _owner->_head) = this; ++_owner->_count; }
   private:
      ~bblock();
   public: // embedded temporary data
      struct {
         bblock *bb;
         std::unordered_set<bblock *> preds;
      } temp;
   # if RSN_USE_DEBUG
   public:
      void dump() const noexcept;
   private:
      void dump_ref() const noexcept { std::fprintf(stderr, "L%u", sn); }
      friend struct log;
   # endif // # if RSN_USE_DEBUG
   };
   RSN_INLINE inline proc::~proc() {
      while (head()) head()->remove();
   }

   class insn: protected aux::node { // IR instruction
   public: // construction/destruction
      virtual insn *clone(bblock *owner) const = 0; // attach the copy to the specified new owner basic block at the end
      virtual insn *clone(insn *next) const = 0;    // attach the copy to the new owner basic block before the specified sibling instruction
      void remove() noexcept { delete this; }       // remove from the owner basic block and dispose
   public: // querying owner/sibling and relocation
      RSN_INLINE auto owner() const noexcept { return _owner; }
      RSN_INLINE auto next () const noexcept { return _next; }
      RSN_INLINE auto prev () const noexcept { return _prev; }
   public:
      RSN_INLINE void reattach(bblock *owner) noexcept { // move to the end of the specified new owner basic block
         (RSN_LIKELY(_prev) ? _prev->_next : _owner->_head) = _next, (RSN_LIKELY(_next) ? _next->_prev : _owner->_tail) = _prev; --_owner->_count;
         _owner = owner, _next = {}, _prev = _owner->_tail; _owner->_tail = (RSN_LIKELY(_prev) ? _prev->_next : _owner->_head) = this; ++_owner->_count;
      }
   public: // querying arguments and performing transformations
      RSN_INLINE virtual auto outputs()->small_vec<smart_ptr<vreg> *> { return {}; }
      RSN_INLINE virtual auto inputs ()->small_vec<smart_ptr<operand> *> { return {}; }
      RSN_INLINE virtual auto targets()->small_vec<bblock **> { return {}; }
      RSN_INLINE virtual bool try_to_fold() { return false; }
   private: // internal representation
      bblock *_owner; insn *_next, *_prev;
   protected: // implementation helpers
      RSN_INLINE explicit insn(bblock *owner) noexcept: _owner(owner), _next{}, _prev(owner->_tail) // attach to the specified owner at the end
         { _owner->_tail = (RSN_LIKELY(_prev) ? _prev->_next : _owner->_head) = this, ++_owner->_count; }
      RSN_INLINE explicit insn(insn *next) noexcept: _owner(next->_owner), _next(next), _prev(next->_prev) // attach to the owner before the specified instruction
         { _next->_prev = (RSN_LIKELY(_prev) ? _prev->_next : _owner->_head) = this, ++_owner->_count; }
      RSN_INLINE virtual ~insn()
         { (RSN_LIKELY(_prev) ? _prev->_next : _owner->_head) = _next, (RSN_LIKELY(_next) ? _next->_prev : _owner->_tail) = _prev; --_owner->_count; }
   public: // embedded temporary data
      struct {
         bool visited{};
      } temp;
   # if RSN_USE_DEBUG
   public:
      virtual void dump() const noexcept = 0;
   # endif // # if RSN_USE_DEBUG
   };
   RSN_INLINE inline bblock::~bblock() {
      (RSN_LIKELY(_prev) ? _prev->_next : _owner->_head) = _next, (RSN_LIKELY(_next) ? _next->_prev : _owner->_tail) = _prev; --_owner->_count;
      while (head()) head()->remove();
   }

} // namespace rsn::opt

# endif // # ifndef RSN_INCLUDED_IR0
