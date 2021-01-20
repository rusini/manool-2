// rusini.hh

/*    Copyright (C) 2020 Alexey Protasov (AKA Alex or rusini)

   This is free software: you can redistribute it and/or modify it under the terms of the version 3 of the GNU General Public License
   as published by the Free Software Foundation (and only version 3).

   This software is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along with MANOOL.  If not, see <https://www.gnu.org/licenses/>.  */


# ifndef RSN_INCLUDED_RUSINI
# define RSN_INCLUDED_RUSINI

# include <algorithm>   // equal, lexicographical_compare
# include <array>
# include <cstdlib>     // ptrdiff_t, size_t
# include <cstring>     // memcpy
# include <initializer_list>
# include <iterator>    // begin, const_reverse_iterator, end, size, reverse_iterator
# include <type_traits> // aligned_union_t, enable_if_t, is_base_of_v, is_trivially_copyable_v, remove_cv_t
# include <utility>     // forward, move

# include "rusini0.hh"

namespace rsn::lib {

   namespace aux { // ADL for begin/end/size
      using std::begin, std::end, std::size;
      template<typename Cont> RSN_INLINE static auto _begin(Cont &cont) noexcept(noexcept(begin(cont))) { return begin(cont); }
      template<typename Cont> RSN_INLINE static auto _end(Cont &cont) noexcept(noexcept(end(cont))) { return end(cont); }
      template<typename Cont> RSN_INLINE static auto _size(Cont &cont) noexcept(noexcept(size(cont))) { return size(cont); }
   }

   // Utilities for Raw and RC-ing Pointers ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

   class noncopyable {
   public:
      noncopyable(const noncopyable &) = delete;
      noncopyable &operator=(const noncopyable &) = delete;
   protected:
      noncopyable() = default;
      ~noncopyable() = default;
   };
   // Downcast for raw pointers to non-copyables
   template<typename Dest, typename Src> RSN_INLINE inline std::enable_if_t<std::is_base_of_v<noncopyable, Src>, bool> is(Src *src) noexcept
      { return dynamic_cast<Dest *>(src); }
   template<typename Dest, typename Src> RSN_INLINE inline std::enable_if_t<std::is_base_of_v<noncopyable, Src>, Dest *> as(Src *src) noexcept
      { return static_cast<Dest *>(src); }

   class smart_rc;

   template<typename Obj> class smart_ptr { // simple, intrussive, and MT-unsafe analog of std::shared_ptr
      static_assert(std::is_base_of_v<smart_rc, Obj>);
   public: // standard operations
      smart_ptr() = default;
      RSN_INLINE smart_ptr(const smart_ptr &rhs) noexcept: rep(rhs) { retain(); }
      RSN_INLINE smart_ptr(smart_ptr &&rhs) noexcept: rep(rhs) { rhs.rep = {}; }
      RSN_INLINE ~smart_ptr() { release(); }
      RSN_INLINE smart_ptr &operator=(const smart_ptr &rhs) noexcept { rhs.retain(), release(), rep = rhs; return *this; }
      RSN_INLINE smart_ptr &operator=(smart_ptr &&rhs) noexcept { swap(rhs); return *this; }
      RSN_INLINE void swap(smart_ptr &rhs) noexcept { using std::swap; swap(rep, rhs.rep); }
   public: // miscellaneous operations
      template<typename ...Args> RSN_INLINE RSN_NODISCARD static auto make(Args &&...args) { return smart_ptr{new Obj(std::forward<Args>(args)...)}; }
      template<typename Rhs> RSN_INLINE smart_ptr(const smart_ptr<Rhs> &rhs) noexcept: rep(rhs) { retain(); }
      template<typename Rhs> RSN_INLINE smart_ptr(smart_ptr<Rhs> &&rhs) noexcept: rep(rhs) { rhs.rep = {}; }
      template<typename Rhs> RSN_INLINE smart_ptr &operator=(const smart_ptr<Rhs> &rhs) noexcept { rhs.retain(), release(), rep = rhs; return *this; }
      template<typename Rhs> RSN_INLINE smart_ptr &operator=(smart_ptr<Rhs> &&rhs) noexcept { rep = rhs, rhs.rep = {}; return *this; }
      RSN_INLINE operator Obj *() const noexcept { return rep; }
      RSN_INLINE Obj &operator*() const noexcept { return *rep; }
      RSN_INLINE Obj *operator->() const noexcept { return rep; }
   private: // internal representation
      Obj *rep{};
      template<typename> friend class smart_ptr;
   private: // implementation helpers
      RSN_INLINE explicit smart_ptr(Obj *rep) noexcept: rep(rep) {}
      RSN_INLINE void retain() const noexcept { if (RSN_LIKELY(rep)) ++rep->lib::smart_rc::rc; }
      RSN_INLINE void release() const noexcept { if (RSN_LIKELY(rep) && RSN_UNLIKELY(!--rep->lib::smart_rc::rc)) delete rep; }
      template<typename Dest, typename Src> friend std::enable_if_t<std::is_base_of_v<smart_rc, Src>, smart_ptr<Dest>> as_smart(const smart_ptr<Src> &) noexcept;
   };
   // Non-member functions
   template<typename Obj> RSN_INLINE inline void swap(smart_ptr<Obj> &lhs, smart_ptr<Obj> &rhs) noexcept
      { lhs.swap(rhs); }
   // Downcast utilities
   template<typename Dest, typename Src> RSN_INLINE inline std::enable_if_t<std::is_base_of_v<smart_rc, Src>, bool> is(const smart_ptr<Src> &src) noexcept
      { return is<Dest>(&*src); }
   template<typename Dest, typename Src> RSN_INLINE inline std::enable_if_t<std::is_base_of_v<smart_rc, Src>, Dest *> as(const smart_ptr<Src> &src) noexcept
      { return as<Dest>(&*src); }
   template<typename Dest, typename Src> RSN_INLINE RSN_NODISCARD inline std::enable_if_t<std::is_base_of_v<smart_rc, Src>, smart_ptr<Dest>>
      as_smart(const smart_ptr<Src> &src) noexcept { return src.retain(), smart_ptr{as<Dest>(src)}; }

   class smart_rc: noncopyable {
   protected:
      smart_rc() = default;
      ~smart_rc() = default;
   private:
      long rc = 1;
      template<typename> friend class smart_ptr;
   };

   // Collections //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

   template<typename> class collection_item_mixin;

   template<typename Obj> class collection_mixin { // an object that owns a varying number of other objects of a specific (static) type
      static_assert(std::is_base_of_v<collection_item_mixin<Obj>, Obj>); /* Obj must be actually *the only* descendant class of collection_item_mixin<Obj> */
   protected: // constructors/destructors
      collection_mixin() = default;
      ~collection_mixin() { while (rear()) rear()->collection_item_mixin<Obj>::remove(); }
   public: // item access
      RSN_INLINE auto head() noexcept       { return static_cast<Obj *>(_head); }
      RSN_INLINE auto head() const noexcept { return static_cast<const Obj *>(_head); }
      RSN_INLINE auto rear() noexcept       { return static_cast<Obj *>(_rear); }
      RSN_INLINE auto rear() const noexcept { return static_cast<const Obj *>(_rear); }
   private: // internal representation
      collection_item_mixin<Obj> *_head{}, *_rear{};
      friend collection_item_mixin<Obj>;
   };

   template<typename Obj> class collection_item_mixin: noncopyable { // collection items organized in a linked list
      static_assert(std::is_base_of_v<collection_item_mixin, Obj>); /* Obj must be actually *the only* descendant class of collection_item_mixin<Obj> */
   protected: // constructors/destructors
      RSN_INLINE collection_item_mixin(collection_mixin<Obj> *owner) noexcept { attach(this, this, owner); }
      RSN_INLINE collection_item_mixin(collection_item_mixin *next) noexcept { attach(this, this, next); }
      RSN_INLINE ~collection_item_mixin() { detach(this, this); }
   public: // item access
      RSN_INLINE auto next() noexcept       { return static_cast<Obj *>(_next); }
      RSN_INLINE auto next() const noexcept { return static_cast<const Obj *>(_next); }
      RSN_INLINE auto prev() noexcept       { return static_cast<Obj *>(_prev); }
      RSN_INLINE auto prev() const noexcept { return static_cast<const Obj *>(_prev); }
   public: // destruction/relocation
      void remove() noexcept { delete static_cast<Obj *>(this); }
      RSN_INLINE void reattach(collection_mixin<Obj> *owner) noexcept { reattach(this, this, owner); }
      RSN_INLINE void reattach(collection_item_mixin *next) noexcept { reattach(this, this, next); }
   public:
      RSN_INLINE static void reattach(collection_item_mixin *head, collection_item_mixin *rear, collection_mixin<Obj> *owner) noexcept
         { detach(head, rear), attach(head, rear, owner); }
      RSN_INLINE static void reattach(collection_item_mixin *head, collection_item_mixin *rear, Obj *next) noexcept
         { detach(head, rear), attach(head, rear, next); }
   private: // internal representation
      collection_item_mixin *_next, *_prev;
      collection_mixin<Obj> *_owner; // must be valid only when !_next || !_prev
   private: // implementation helpers
      RSN_INLINE static void attach(collection_item_mixin *head, collection_item_mixin *rear, collection_mixin<Obj> *owner) noexcept {
         if (RSN_LIKELY(head->_prev = owner->_rear)) owner->_rear->_next = head; else (owner->_head = head)->_owner = owner;
         ((rear->_owner = owner)->_rear = rear)->_next = {};
      }
      RSN_INLINE static void attach(collection_item_mixin *head, collection_item_mixin *rear, llist_obj_mixin *next) noexcept {
         if (RSN_LIKELY(head->_prev = next->_prev)) next->_prev->_next = head; else (next->_owner->_head = head)->_owner = next->_owner;
         (next->_prev = rear)->_next = next;
      }
      RSN_INLINE static void detach(collection_item_mixin *head, collection_item_mixin *rear) noexcept {
         if (RSN_LIKELY(head->_prev))
            if (RSN_UNLIKELY(rear->_next))
               (head->_prev->_next = rear->_next)->_prev = head->_prev;
            else
               ((head->_prev->_owner = rear->_owner)->_rear = head->_prev)->_next = {};
         else
            if (RSN_UNLIKELY(rear->_next))
               ((rear->_next->_owner = head->_owner)->_head = rear->_next)->_prev = {};
            else
               head->_owner->_rear = head->_owner->_head = {};
      }
   };

   // Temporary Vectors Optimized for Small Sizes //////////////////////////////////////////////////////////////////////////////////////////////////////////////

   template<typename Obj, std::size_t MinRes = /*up to 2 cache-lines*/ (128 - sizeof(Obj *) - sizeof(std::size_t)) / sizeof(Obj)>
   class small_vec { // lightweight analog of llvm::SmallVector
      static_assert(std::is_same_v<Obj, std::remove_cv_t<Obj>>); // no const/volatile like for std::containers, which makes sense
   public: // typedefs to imitate std::containers
      typedef Obj              value_type;
      typedef value_type       *pointer, &reference;
      typedef const value_type *const_pointer, &const_reference;
      typedef std::size_t      size_type;
      typedef std::ptrdiff_t   difference_type;
      typedef pointer          iterator;
      typedef const_pointer    const_iterator;
      typedef std::reverse_iterator<iterator> reverse_iterator;
      typedef std::reverse_iterator<const_iterator> const_reverse_iterator;
   public: // standard operations
      RSN_INLINE explicit small_vec() noexcept
         : _begin(reinterpret_cast<decltype(_begin)>(buf.data())), _end(_begin) {
      }
      RSN_INLINE small_vec(const small_vec &rhs)
         : small_vec((size_type)aux::_size(rhs), rhs) {
      }
      RSN_INLINE small_vec(small_vec &&rhs) noexcept {
         if (RSN_UNLIKELY(rhs._begin != reinterpret_cast<decltype(rhs._begin)>(rhs.buf.data())))
            _begin = rhs._begin, _end = rhs._end, rhs._begin = reinterpret_cast<decltype(rhs._begin)>(rhs.buf.data());
         else {
            _end = (_begin = reinterpret_cast<decltype(_begin)>(buf.data())) + rhs.size();
            if constexpr (std::is_trivially_copyable_v<value_type>)
               std::memcpy(&buf, &rhs.buf, sizeof buf <= 64 ? sizeof buf : (const unsigned char *)_end - (const unsigned char *)_begin);
            else {
               for (auto dest = _begin, end = _end, src = rhs._begin; dest != end;) new(dest++) value_type(std::move(*src++));
               for (auto begin = rhs._begin; rhs._end != begin;) (--rhs._end)->~value_type();
            }
         }
         rhs._end = rhs._begin;
      }
      RSN_INLINE ~small_vec() {
         if constexpr (!std::is_trivially_copyable_v<value_type>)
            for (auto begin = _begin; _end != begin;) (--_end)->~value_type();
         if (RSN_UNLIKELY(_begin != reinterpret_cast<decltype(_begin)>(buf.data())))
            delete[] reinterpret_cast<std::aligned_union_t<0, value_type> *>(_begin);
      }
      RSN_INLINE small_vec &operator=(const small_vec &rhs) {
         clear(rhs); return *this;
      }
      RSN_INLINE small_vec &operator=(small_vec &&rhs) noexcept {
         clear(std::move(rhs)); return *this;
      }
      RSN_INLINE void swap(small_vec &rhs) noexcept {
         std::swap(*this, rhs); // the standard version of swap is just fine
      }
   public: // miscellaneous construction operations
      RSN_INLINE explicit small_vec(size_type res)
         : _begin(reinterpret_cast<decltype(_begin)>(RSN_LIKELY(res <= MinRes) ? buf.data() : new std::aligned_union_t<0, value_type>[res])), _end(_begin) {}
      small_vec(size_type res, std::initializer_list<value_type> rhs)
         : small_vec(res) { for (auto &&obj: rhs) push_back(obj); }
      template<typename Rhs> small_vec(size_type res, Rhs &rhs)
         : small_vec(res) { for (auto &&obj: rhs) emplace_back(obj); }
      template<typename Rhs> small_vec(size_type res, Rhs &&rhs)
         : small_vec(res) { for (auto &&obj: rhs) emplace_back(std::move(obj)); }
      RSN_INLINE small_vec(std::initializer_list<value_type> rhs, size_type res = 0)
         : small_vec(rhs.size() + res, rhs) {}
      template<typename Rhs> RSN_INLINE small_vec(Rhs &rhs, std::enable_if_t<!std::is_integral_v<Rhs> && !std::is_enum_v<Rhs>, size_type> res = 0)
         : small_vec((size_type)aux::_size(rhs) + res, rhs) {}
      template<typename Rhs> RSN_INLINE small_vec(Rhs &&rhs, std::enable_if_t<!std::is_integral_v<Rhs> && !std::is_enum_v<Rhs>, size_type> res = 0)
         : small_vec((size_type)aux::_size(rhs) + res, std::move(rhs)) {}
   public:
      RSN_INLINE void clear() noexcept {
         if constexpr (!std::is_trivially_copyable_v<value_type>)
            for (auto begin = _begin; _end != begin;) (--_end)->~value_type();
      }
   public:
      RSN_INLINE void clear(size_type res)
         { this->~small_vec(); try { new(this) small_vec(res); } catch (...) { new(this) small_vec; throw; } }
      RSN_INLINE void clear(size_type res, std::initializer_list<value_type> rhs)
         { this->~small_vec(); try { new(this) small_vec(res, rhs); } catch (...) { new(this) small_vec; throw; } }
      template<typename Rhs> RSN_INLINE void clear(size_type res, Rhs &rhs)
         { this->~small_vec(); try { new(this) small_vec(res, rhs); } catch (...) { new(this) small_vec; throw; } }
      template<typename Rhs> RSN_INLINE void clear(size_type res, Rhs &&rhs)
         { this->~small_vec(); try { new(this) small_vec(res, std::move(rhs)); } catch (...) { new(this) small_vec; throw; } }
      RSN_INLINE void clear(std::initializer_list<value_type> rhs, size_type res = 0)
         { this->~small_vec(); try { new(this) small_vec(rhs, res); } catch (...) { new(this) small_vec; throw; } }
      template<typename Rhs> RSN_INLINE void clear(Rhs &rhs, std::enable_if_t<!std::is_integral_v<Rhs> && !std::is_enum_v<Rhs>, size_type> res = 0)
         { this->~small_vec(); try { new(this) small_vec(rhs, res); } catch (...) { new(this) small_vec; throw; } }
      template<typename Rhs> RSN_INLINE void clear(Rhs &&rhs, std::enable_if_t<!std::is_integral_v<Rhs> && !std::is_enum_v<Rhs>, size_type> res = 0)
         { this->~small_vec(); try { new(this) small_vec(std::move(rhs), res); } catch (...) { new(this) small_vec; throw; } }
   public: // iterators
      RSN_INLINE iterator       begin() noexcept       { return _begin; }
      RSN_INLINE const_iterator begin() const noexcept { return _begin; }
      RSN_INLINE iterator       end() noexcept       { return _end; }
      RSN_INLINE const_iterator end() const noexcept { return _end; }
      RSN_INLINE auto rbegin() noexcept       { return reverse_iterator{end()}; }
      RSN_INLINE auto rbegin() const noexcept { return const_reverse_iterator{end()}; }
      RSN_INLINE auto rend() noexcept       { return reverse_iterator{begin()}; }
      RSN_INLINE auto rend() const noexcept { return const_reverse_iterator{begin()}; }
      RSN_INLINE auto cbegin() const noexcept  { return begin(); }
      RSN_INLINE auto cend() const noexcept    { return end(); }
      RSN_INLINE auto crbegin() const noexcept { return rbegin(); }
      RSN_INLINE auto crend() const noexcept   { return rend(); }
   public: // capacity
      RSN_INLINE bool empty() const noexcept { return end() == begin(); }
      RSN_INLINE size_type size() const noexcept { return end() - begin(); }
      RSN_INLINE size_type max_size() const noexcept { return std::numeric_limits<std::size_t>::max() / sizeof(Obj); }
      size_type capacity() const noexcept = delete; // not applicable in case of small_vec
      void reserve(size_type) = delete;             // ditto
      void shrink_to_fit() = delete;                // ditto
   public: // element and data access
      RSN_INLINE auto &operator[](size_type sn) noexcept       { return begin()[sn]; }
      RSN_INLINE auto &operator[](size_type sn) const noexcept { return begin()[sn]; }
      RSN_INLINE auto &at(size_type sn)       { return check_range(sn), (*this)[sn]; }
      RSN_INLINE auto &at(size_type sn) const { return check_range(sn), (*this)[sn]; }
      RSN_INLINE auto front() noexcept       { return *begin(); }
      RSN_INLINE auto front() const noexcept { return *begin(); }
      RSN_INLINE auto back() noexcept       { return *std::prev(end()); }
      RSN_INLINE auto back() const noexcept { return *std::prev(end()); }
      RSN_INLINE auto data() noexcept       { return begin(); }
      RSN_INLINE auto data() const noexcept { return begin(); }
   private:
      RSN_INLINE void check_range(size_type sn) const
         { if (RSN_UNLIKELY(sn >= size())) throw std::out_of_range{"::rsn::lib::small_vec::at(std::size_t)"}; }
   public: // modifiers
      template<typename ...Rhs> RSN_INLINE reference emplace_back(Rhs &&...rhs) noexcept(noexcept(new(nullptr) value_type(std::forward<Rhs>(rhs)...)))
         { return new(_end) value_type(std::forward<Rhs>(rhs)...), *_end++; }
      RSN_INLINE void push_back(const value_type &rhs) noexcept(noexcept(emplace_back(rhs)))
         { emplace_back(rhs); }
      RSN_INLINE void push_back(value_type &&rhs) noexcept
         { emplace_back(std::move(rhs)); }
      RSN_INLINE void pop_back() noexcept
         { (--_end)->~value_type(); }
   private: // internal representation
      pointer _begin, _end;
      std::array<std::aligned_union_t<0, value_type>, MinRes> buf;
   };
   // Deduction guides
   template<typename Rhs> small_vec(Rhs &&) -> small_vec<std::decay_t<decltype(*aux::_begin(std::declval<Rhs>()))>>;
   // Non-member functions
   template<typename Obj, std::size_t MinRes> RSN_INLINE inline void swap(small_vec<Obj, MinRes> &lhs, small_vec<Obj, MinRes> &rhs) noexcept // for completeness
      { lhs.swap(rhs); }
   template<typename Obj, std::size_t MinRes> RSN_NOINLINE inline bool operator==(const small_vec<Obj, MinRes> &lhs, const small_vec<Obj, MinRes> &rhs)
      { return lhs.size() == rhs.size() && std::equal(lhs.begin(), lhs.end(), rhs.begin()); }
   template<typename Obj, std::size_t MinRes> RSN_NOINLINE inline bool operator< (const small_vec<Obj, MinRes> &lhs, const small_vec<Obj, MinRes> &rhs)
      { return std::lexicographical_compare(lhs.begin(), lhs.end(), rhs.begin(), rhs.end()); }
   template<typename Obj, std::size_t MinRes> RSN_NOINLINE inline bool operator!=(const small_vec<Obj, MinRes> &lhs, const small_vec<Obj, MinRes> &rhs)
      { return std::rel_ops::operator!=(lhs, rhs); }
   template<typename Obj, std::size_t MinRes> RSN_NOINLINE inline bool operator> (const small_vec<Obj, MinRes> &lhs, const small_vec<Obj, MinRes> &rhs)
      { return std::rel_ops::operator> (lhs, rhs); }
   template<typename Obj, std::size_t MinRes> RSN_NOINLINE inline bool operator<=(const small_vec<Obj, MinRes> &lhs, const small_vec<Obj, MinRes> &rhs)
      { return std::rel_ops::operator<=(lhs, rhs); }
   template<typename Obj, std::size_t MinRes> RSN_NOINLINE inline bool operator>=(const small_vec<Obj, MinRes> &lhs, const small_vec<Obj, MinRes> &rhs)
      { return std::rel_ops::operator>=(lhs, rhs); }

   // Non-owning References to a Range of Values ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

   template<typename Begin, typename End = Begin> class range_ref { // generalized analog of llvm::ArrayRef
   public: // typedefs to imitate standard containers
      typedef Begin                                                    iterator, const_iterator;
      typedef typename std::iterator_traits<iterator>::value_type      value_type;
      typedef typename std::iterator_traits<iterator>::pointer         pointer, const_pointer;
      typedef typename std::iterator_traits<iterator>::reference       reference, const_reference;
      typedef typename std::iterator_traits<iterator>::difference_type difference_type;
      typedef std::make_unsigned_t<difference_type>                    size_type;
   public: // standard operations
      range_ref() = default; // + implicitly defaulted copy and move constructors and assignment operators
      RSN_INLINE void swap(range_ref &rhs) noexcept { using std::swap; swap(_begin, rhs._begin), swap(_end, rhs._end); }
   public: // miscellaneous constructors and assignment operators
      template<typename RhsBegin, typename RhsEnd = RhsBegin> RSN_INLINE range_ref(range_ref<RhsBegin, RhsEnd> &rhs) noexcept
         : _begin(rhs._begin), _end(rhs._end) {}
      template<typename RhsBegin, typename RhsEnd = RhsBegin> RSN_INLINE range_ref(const range_ref<RhsBegin, RhsEnd> &rhs) noexcept
         : _begin(rhs._begin), _end(rhs._end) {}
      template<typename RhsBegin, typename RhsEnd = RhsBegin> RSN_INLINE range_ref(range_ref<RhsBegin, RhsEnd> &&rhs) noexcept
         : _begin(std::move(rhs._begin)), _end(std::move(rhs._end)) {}
      template<typename RhsBegin, typename RhsEnd = RhsBegin> RSN_INLINE range_ref(const range_ref<RhsBegin, RhsEnd> &&rhs) noexcept
         : _begin(std::move(rhs._begin)), _end(std::move(rhs._end)) {}
      template<typename RhsBegin, typename RhsEnd = RhsBegin> RSN_INLINE range_ref &operator=(range_ref<RhsBegin, RhsEnd> &rhs) noexcept
         { _begin = rhs._begin, _end = rhs._end; return *this; }
      template<typename RhsBegin, typename RhsEnd = RhsBegin> RSN_INLINE range_ref &operator=(const range_ref<RhsBegin, RhsEnd> &rhs) noexcept
         { _begin = rhs._begin, _end = rhs._end; return *this; }
      template<typename RhsBegin, typename RhsEnd = RhsBegin> RSN_INLINE range_ref &operator=(range_ref<RhsBegin, RhsEnd> &&rhs) noexcept
         { _begin = std::move(rhs._begin), _end = std::move(rhs._end); return *this; }
      template<typename RhsBegin, typename RhsEnd = RhsBegin> RSN_INLINE range_ref &operator=(const range_ref<RhsBegin, RhsEnd> &&rhs) noexcept
         { _begin = std::move(rhs._begin), _end = std::move(rhs._end); return *this; }
   public:
      template<typename Rhs> RSN_INLINE range_ref(Rhs &rhs) noexcept(noexcept(aux::_begin(rhs), aux::_end(rhs)))
         : _begin(aux::_begin(rhs)), _end(aux::_end(rhs)) {}
      template<typename Rhs> RSN_INLINE range_ref &operator=(Rhs &rhs) noexcept(noexcept(aux::_begin(rhs), aux::_end(rhs)))
         { _begin = aux::_begin(rhs), _end = aux::_end(rhs); return *this; }
   public:
      template<typename Rhs> range_ref(Rhs &&) = delete;
      template<typename Rhs> range_ref &operator=(Rhs &&) = delete;
   public:
      template<typename RhsBegin, typename RhsEnd = RhsBegin> RSN_INLINE range_ref(RhsBegin &&_begin, RhsEnd &&_end) noexcept
         : _begin(std::forward<RhsBegin>(_begin)), _end(std::forward<RhsEnd>(_end)) {}
   public: // container-like access/iteration
      RSN_INLINE bool empty() const noexcept(noexcept(_end == _begin)) // gcc bug #52869 prior to gcc-9
         { return _end == _begin; }
      RSN_INLINE size_type size() const noexcept(noexcept(_end - _begin))
         { return _end - _begin; }
      RSN_INLINE size_type size_ex() const noexcept(noexcept(std::distance(begin(), end()))) // potentially ex(pensive)
         { return std::distance(begin(), end()); }
   public:
      RSN_INLINE auto begin() const noexcept { return _begin; }
      RSN_INLINE auto end() const noexcept { return _end; }
      RSN_INLINE auto rbegin() const noexcept { return std::reverse_iterator(end()); }
      RSN_INLINE auto rend() const noexcept { return std::reverse_iterator(begin()); }
      RSN_INLINE auto cbegin() const noexcept { return begin(); }
      RSN_INLINE auto cend() const noexcept { return end(); }
      RSN_INLINE auto crbegin() const noexcept { return rbegin(); }
      RSN_INLINE auto crend() const noexcept { return rend(); }
   public: // convenience operations
      RSN_INLINE reference first() const noexcept(noexcept(*_begin))
         { return *_begin; }
      RSN_INLINE reference last() const noexcept(noexcept(*std::prev(end())))
         { return *std::prev(end()); }
      RSN_INLINE auto drop_first() const noexcept(noexcept(drop_first_ex()))
         { return drop_first_ex(); }
      RSN_INLINE auto drop_last() const noexcept(noexcept(drop_last_ex()))
         { return drop_last_ex(); }
      RSN_INLINE range_ref drop_first(size_type size) const noexcept(noexcept(_begin + (difference_type)1))
         { return {_begin + (difference_type)size, end()}; }
      RSN_INLINE range_ref drop_last(size_type size) const noexcept(noexcept(_end - (difference_type)1))
         { return {begin(), _end - (difference_type)size}; }
      RSN_INLINE range_ref drop_first_ex(size_type size = 1) const noexcept(noexcept(std::next(begin())))
         { return {std::next(begin(), size), end()}; }
      RSN_INLINE range_ref drop_last_ex(size_type size = 1) const noexcept(noexcept(std::prev(end())))
         { return {begin(), std::prev(end(), size)}; }
      RSN_INLINE auto reverse() const noexcept
         { return lib::range_ref{rbegin(), rend()}; }
   private: // internal representation
      Begin _begin; End _end;
      template<typename, typename> friend class range_ref;
   };
   // Deduction guides
   template<typename Rhs> range_ref(Rhs &&) -> range_ref<decltype(aux::_begin(std::declval<Rhs &>())), decltype(aux::_end(std::declval<Rhs &>()))>;
   template<typename Begin, typename End> range_ref(Begin, End) -> range_ref<Begin, End>;
   // Non-member functions
   template<typename Begin, typename End = Begin>
   RSN_INLINE inline void swap(range_ref<Begin, End> &lhs, range_ref<Begin, End> &rhs) noexcept
      { lhs.swap(rhs); }
   template<typename LhsBegin, typename RhsBegin, typename LhsEnd = LhsBegin, typename RhsEnd = RhsBegin>
   RSN_INLINE inline bool operator==(const range_ref<LhsBegin, LhsEnd> &lhs, const range_ref<RhsBegin, RhsEnd> &rhs)
   noexcept(noexcept(lhs.begin() == rhs.begin() && lhs.end() == rhs.end()))
      { return lhs.begin() == rhs.begin() && lhs.end() == rhs.end(); }
   template<typename LhsBegin, typename RhsBegin, typename LhsEnd = LhsBegin, typename RhsEnd = RhsBegin>
   RSN_INLINE inline bool operator!=(const range_ref<LhsBegin, LhsEnd> &lhs, const range_ref<RhsBegin, RhsEnd> &rhs)
   noexcept(noexcept(lhs.begin() != rhs.begin() || lhs.end() != rhs.end()))
      { return lhs.begin() != rhs.begin() || lhs.end() != rhs.end(); }

}

# endif // # ifndef RSN_INCLUDED_RUSINI
