// rusini.hh

/*    Copyright (C) 2020, 2021 Alexey Protasov (AKA Alex or rusini)

   This is free software: you can redistribute it and/or modify it under the terms of the version 3 of the GNU General Public License
   as published by the Free Software Foundation (and only version 3).

   This software is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along with this software.  If not, see <https://www.gnu.org/licenses/>.  */


# ifndef RSN_INCLUDED_RUSINI
# define RSN_INCLUDED_RUSINI

# include <iterator>    // begin, const_reverse_iterator, end, reverse_iterator
# include <type_traits> // enable_if_t, is_base_of_v, is_convertible_v, remove_cv_t
# include <utility>     // forward, move

# include "rusini0.hh"

namespace rsn::lib {

   namespace aux { // ADL for begin/end
      using std::begin, std::end;
      template<typename Cont> RSN_INLINE static auto _begin(Cont &cont) noexcept(noexcept(begin(cont))) { return begin(cont); }
      template<typename Cont> RSN_INLINE static auto _end(Cont &cont) noexcept(noexcept(end(cont))) { return end(cont); }
   }

   // Utilities for Raw and RC-ing Pointers ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

   template<typename = void> class noncopyable { // CRTP to avoid wasting space under multiple inheritance
   protected:
      noncopyable() = default;
      ~noncopyable() = default;
   public:
      noncopyable(const noncopyable &) = delete;
      noncopyable &operator=(const noncopyable &) = delete;
   };
   // Downcast for raw pointers to non-copyables ("main" variety)
   template<typename Dest, typename Src> RSN_INLINE inline std::enable_if_t<std::is_base_of_v<noncopyable<>, Src>, bool> is(Src *src) noexcept
      { static_assert(std::is_convertible_v<Dest *, const Src *>); return src->template type_check<std::remove_cv_t<Dest>>(); }
   template<typename Dest, typename Src> RSN_INLINE inline std::enable_if_t<std::is_base_of_v<noncopyable<>, Src>, Dest *> as(Src *src) noexcept
      { static_assert(std::is_convertible_v<Dest *, const Src *>); return static_cast<Dest *>(src); }

   class smart_rc_mixin: noncopyable<smart_rc_mixin> {
   protected:
      struct smart_tag {};
      smart_rc_mixin() = default;
      ~smart_rc_mixin() = default;
   private:
      long rc = 1;
      template<typename> friend class smart_ptr;
   };

   template<typename Obj> class smart_ptr { // simple, intrussive, and MT-unsafe analog of std::shared_ptr
   public: // standard operations
      smart_ptr() = default;
      RSN_INLINE smart_ptr(const smart_ptr &rhs) noexcept: rep(rhs) { retain(); }
      RSN_INLINE smart_ptr(smart_ptr &&rhs) noexcept: rep(rhs) { rhs.rep = {}; }
      RSN_INLINE ~smart_ptr() { release(); }
      RSN_INLINE smart_ptr &operator=(const smart_ptr &rhs) noexcept { rhs.retain(), release(), rep = rhs; return *this; }
      RSN_INLINE smart_ptr &operator=(smart_ptr &&rhs) noexcept { swap(rhs); return *this; }
      RSN_INLINE void swap(smart_ptr &rhs) noexcept { using std::swap; swap(rep, rhs.rep); }
   public: // miscellaneous operations
      template<typename ...Args> RSN_INLINE RSN_NODISCARD static auto make(Args &&...args)
         { return smart_ptr(new Obj(smart_rc_mixin::smart_tag{}, std::forward<Args>(args)...), 0); }
   public:
      template<typename Rhs, typename std::enable_if_t<std::is_convertible_v<Rhs *, Obj *>, int> = 0>
         RSN_INLINE smart_ptr(const smart_ptr<Rhs> &rhs) noexcept: rep(rhs) { retain(); }
      template<typename Rhs, typename std::enable_if_t<std::is_convertible_v<Rhs *, Obj *>, int> = 0>
         RSN_INLINE smart_ptr(smart_ptr<Rhs> &&rhs) noexcept: rep(rhs) { rhs.rep = {}; }
   public:
      RSN_INLINE smart_ptr(Obj *rhs) noexcept: rep(rhs) { retain(); }
      RSN_INLINE operator Obj *() const & noexcept { return rep; }
      operator Obj *() const && = delete;
      RSN_INLINE Obj &operator*() const noexcept { return *rep; }
      RSN_INLINE Obj *operator->() const noexcept { return rep; }
   private: // internal representation
      Obj *rep = {};
      template<typename> friend class smart_ptr;
   private: // implementation helpers
      RSN_INLINE explicit smart_ptr(Obj *rep, int) noexcept: rep(rep) {}
      RSN_INLINE void retain() const noexcept  { if (RSN_LIKELY(rep)) ++static_cast<smart_rc_mixin *>(rep)->rc; }
      RSN_INLINE void release() const noexcept { if (RSN_LIKELY(rep) && RSN_UNLIKELY(!--static_cast<smart_rc_mixin *>(rep)->rc)) delete rep; }
      template<typename Dest, typename Src> friend std::enable_if_t<std::is_base_of_v<noncopyable<>, Src>, smart_ptr<Dest>> as_smart(smart_ptr<Src> &&) noexcept;
   };
   // Non-member operations
   template<typename Obj> RSN_INLINE inline void swap(smart_ptr<Obj> &lhs, smart_ptr<Obj> &rhs) noexcept { lhs.swap(rhs); }

   // Downcast for smart_ptr
   template<typename Dest, typename Src> RSN_INLINE inline std::enable_if_t<std::is_base_of_v<noncopyable<>, Src>, bool> is(const smart_ptr<Src> &src) noexcept
      { return is<Dest>((Src *)src); }
   template<typename Dest, typename Src> RSN_INLINE inline std::enable_if_t<std::is_base_of_v<noncopyable<>, Src>, Dest *> as(const smart_ptr<Src> &src) noexcept
      { return as<Dest>((Src *)src); }
   template<typename Dest, typename Src> RSN_INLINE RSN_NODISCARD inline std::enable_if_t<std::is_base_of_v<noncopyable<>, Src>, smart_ptr<Dest>>
      as_smart(const smart_ptr<Src> &src) noexcept { return as<Dest>(src); }
   template<typename Dest, typename Src> RSN_INLINE RSN_NODISCARD inline std::enable_if_t<std::is_base_of_v<noncopyable<>, Src>, smart_ptr<Dest>>
      as_smart(smart_ptr<Src> &&src) noexcept { smart_ptr res(as<Dest>(src), 0); src.rep = {}; return res; }
   template<typename Dest, typename Src> RSN_INLINE RSN_NODISCARD inline std::enable_if_t<std::is_base_of_v<noncopyable<>, Src>, smart_ptr<Dest>>
      as_smart(Src *src) noexcept { return as<Dest>(src); }

   // Collections //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

   template<typename Obj, typename Owner> class collection_item_mixin;

   template<typename Owner, typename Obj>
   class collection_mixin: noncopyable<collection_mixin<Owner, Obj>> { // an object that owns a varying number of other objects (pointing to the owner)
   protected: // constructors/destructors
      collection_mixin() = default;
      ~collection_mixin() { while (rear()) obj_mixin(rear())->eliminate(); }
   public: // item access
      RSN_INLINE Obj *head() noexcept { return _head; }
      RSN_INLINE const Obj *head() const noexcept { return _head; }
      RSN_INLINE Obj *rear() noexcept { return _rear; }
      RSN_INLINE const Obj *rear() const noexcept { return _rear; }
   private: // internal representation
      Obj *_head{}, *_rear{};
      friend collection_item_mixin<Obj, Owner>;
   private: // implementation helpers
      RSN_INLINE static collection_item_mixin<Obj, Owner> *obj_mixin(Obj *obj) noexcept { return obj; }
   };

   template<typename Obj, typename Owner>
   class collection_item_mixin: noncopyable<collection_item_mixin<Obj, Owner>> { // objects owned by a collection (which are organized in a linked list)
   protected: // constructors/destructors
      RSN_INLINE explicit collection_item_mixin(Owner *owner) noexcept: _owner(owner) { attach(); }                    // attach to the specified owner at the end
      RSN_INLINE explicit collection_item_mixin(Obj *next) noexcept: _owner(obj_mixin(next)->_owner) { attach(next); } // attach to the owner before the spec. sibling
      RSN_INLINE ~collection_item_mixin() { detach(); }
   public: // item access
      RSN_INLINE Obj *next() noexcept { return _next; }
      RSN_INLINE const Obj *next() const noexcept { return _next; }
      RSN_INLINE Obj *prev() noexcept { return _prev; }
      RSN_INLINE const Obj *prev() const noexcept { return _prev; }
      RSN_INLINE Owner *owner() noexcept { return _owner; }
      RSN_INLINE const Owner *owner() const noexcept { return _owner; }
   public: // destruction/relocation
      RSN_INLINE void eliminate() noexcept { delete static_cast<Obj *>(this); } // detach from the owner procedure (in the destructor) and destroy/free
   public:
      RSN_INLINE void reattach() noexcept { detach(); attach(); }                                                // re-attach to the end of the current owner
      RSN_INLINE void reattach(Owner *owner) noexcept { detach(); _owner = owner, attach(); }                    // re-attach to the specified new owner at the end
      RSN_INLINE void reattach(Obj *next) noexcept { detach(); _owner = obj_mixin(next)->_owner, attach(next); } // re-attach to the new owner before the spec. sibling
   private: // internal representation
      Obj *_next, *_prev;
      Owner *_owner;
   private: // implementation helpers
      RSN_INLINE static collection_item_mixin *obj_mixin(Obj *obj) noexcept { return obj; }
      RSN_INLINE static collection_mixin<Owner, Obj> *owner_mixin(Owner *owner) noexcept { return owner; }
   private:
      RSN_INLINE void detach() noexcept {
         (RSN_LIKELY(_prev) ? obj_mixin(_prev)->_next : owner_mixin(_owner)->_head) = _next;
         (RSN_LIKELY(_next) ? obj_mixin(_next)->_prev : owner_mixin(_owner)->_rear) = _prev;
      }
      RSN_INLINE void attach() noexcept {
         owner_mixin(_owner)->_rear = (RSN_LIKELY(_prev = owner_mixin(_owner)->_rear) ? obj_mixin(_prev)->_next : owner_mixin(_owner)->_head) = static_cast<Obj *>(this);
         _next = {};
      }
      RSN_INLINE void attach(Obj *next) noexcept {
         obj_mixin(next)->_prev = (RSN_LIKELY(_prev = obj_mixin(next)->_prev) ? obj_mixin(_prev)->_next : owner_mixin(_owner)->_head) = static_cast<Obj *>(this);
         _next = next;
      }
   };

   // Utilities for "stable" iteration over collection items

   template<typename Owner, typename Obj> RSN_INLINE inline auto all(collection_mixin<Owner, Obj> *owner)       { return all(owner->head(), {}); }
   template<typename Owner, typename Obj> RSN_INLINE inline auto all(const collection_mixin<Owner, Obj> *owner) { return all(owner->head(), {}); }
   template<typename Owner, typename Obj> RSN_INLINE inline auto rev(collection_mixin<Owner, Obj> *owner)       { return rev({}, owner->rear()); }
   template<typename Owner, typename Obj> RSN_INLINE inline auto rev(const collection_mixin<Owner, Obj> *owner) { return rev({}, owner->rear()); }

   template<typename Obj, typename Owner> RSN_NOINLINE auto all(collection_item_mixin<Obj, Owner> *begin, decltype(nullptr)) {
      typename std::vector<Obj *>::size_type size = 0; for (auto it = begin; it; it = it->next()) ++size;
      std::vector<Obj *> res(size); decltype(size) sn = 0; // redundant value-initialization is actually faster than extra branching in push_back
      for (auto it = begin; it; it = it->next(), ++sn) res[sn] = static_cast<Obj *>(it);
      return res;
   }
   template<typename Obj, typename Owner> RSN_NOINLINE auto all(const collection_item_mixin<Obj, Owner> *begin, decltype(nullptr)) {
      typename std::vector<const Obj *>::size_type size = 0; for (auto it = begin; it; it = it->next()) ++size;
      std::vector<const Obj *> res(size); decltype(size) sn = 0; // ditto
      for (auto it = begin; it; it = it->next(), ++sn) res[sn] = static_cast<const Obj *>(it);
      return res;
   }
   template<typename Obj, typename Owner, typename End> RSN_NOINLINE auto all(collection_item_mixin<Obj, Owner> *begin, End *end) {
      typename std::vector<Obj *>::size_type size = 0; for (auto it = begin; it != end; it = it->next()) ++size;
      std::vector<Obj *> res(size); decltype(size) sn = 0; // ditto
      for (auto it = begin; it != end; it = it->next(), ++sn) res[sn] = static_cast<Obj *>(it);
      return res;
   }
   template<typename Obj, typename Owner, typename End> RSN_NOINLINE auto all(const collection_item_mixin<Obj, Owner> *begin, End *end) {
      typename std::vector<const Obj *>::size_type size = 0; for (auto it = begin; it != end; it = it->next()) ++size;
      std::vector<const Obj *> res(size); decltype(size) sn = 0; // ditto
      for (auto it = begin; it != end; it = it->next(), ++sn) res[sn] = static_cast<const Obj *>(it);
      return res;
   }

   template<typename Obj, typename Owner> RSN_NOINLINE auto rev(decltype(nullptr), collection_item_mixin<Obj, Owner> *begin) {
      typename std::vector<Obj *>::size_type size = 0; for (auto it = begin; it; it = it->prev()) ++size;
      std::vector<Obj *> res(size); decltype(size) sn = 0; // ditto
      for (auto it = begin; it; it = it->prev(), ++sn) res[sn] = static_cast<Obj *>(it);
      return res;
   }
   template<typename Obj, typename Owner> RSN_NOINLINE auto rev(decltype(nullptr), const collection_item_mixin<Obj, Owner> *begin) {
      typename std::vector<const Obj *>::size_type size = 0; for (auto it = begin; it; it = it->prev()) ++size;
      std::vector<const Obj *> res(size); decltype(size) sn = 0; // ditto
      for (auto it = begin; it; it = it->prev(), ++sn) res[sn] = static_cast<const Obj *>(it);
      return res;
   }
   template<typename Obj, typename Owner, typename End> RSN_NOINLINE auto rev(End *end, collection_item_mixin<Obj, Owner> *begin) {
      typename std::vector<Obj *>::size_type size = 0; for (auto it = begin; it != end; it = it->prev()) ++size;
      std::vector<Obj *> res(size); decltype(size) sn = 0; // ditto
      for (auto it = begin; it != end; it = it->prev(), ++sn) res[sn] = static_cast<Obj *>(it);
      return res;
   }
   template<typename Obj, typename Owner, typename End> RSN_NOINLINE auto rev(End *end, const collection_item_mixin<Obj, Owner> *begin) {
      typename std::vector<const Obj *>::size_type size = 0; for (auto it = begin; it != end; it = it->prev()) ++size;
      std::vector<const Obj *> res(size); decltype(size) sn = 0; // ditto
      for (auto it = begin; it != end; it = it->prev(), ++sn) res[sn] = static_cast<const Obj *>(it);
      return res;
   }

   // Non-owning References to a Range of Values ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

   template<typename Begin, typename End = Begin> class range_ref { // generalized analog of llvm::ArrayRef
   public: // type aliases to imitate std::containers
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
      RSN_INLINE reference operator[](size_type sn) const noexcept(noexcept(_begin[sn]))
         { return _begin[sn]; }
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
