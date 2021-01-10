// rusini.hh

/*    Copyright (C) 2020 Alexey Protasov (AKA Alex or rusini)

   This is free software: you can redistribute it and/or modify it under the terms of the version 3 of the GNU General Public License
   as published by the Free Software Foundation (and only version 3).

   This software is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along with MANOOL.  If not, see <https://www.gnu.org/licenses/>.  */


# ifndef RSN_INCLUDED_RUSINI
# define RSN_INCLUDED_RUSINI

# include <iterator>
# include <type_traits>
# include <utility>
# include <vector>
# include <array>
# include <cstring>

# include "rusini0.hh"

namespace rsn::lib {

   // Utilities for Raw and Smart Pointers /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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

   template<typename Obj>
   class smart_ptr { // simple, intrussive, and MT-unsafe analog of std::shared_ptr
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

   // Buffers Optimized for Small Sizes ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

   template<typename Obj, std::size_t MinRes = /*two cache-lines*/ (128 - sizeof(Obj *) - sizeof(std::size_t)) / sizeof(Obj)>
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
      RSN_INLINE explicit small_vec(size_type res = 0):
         _pbuf(reinterpret_cast<decltype(_pbuf)>(RSN_LIKELY(res <= MinRes) ? _buf.data() : new std::aligned_union_t<0, value_type>[res])) {
      }
      small_vec(std::initializer_list<value_type> rhs): small_vec(rhs.size()) {
         for (auto &&obj: rhs) push_back(obj);
      }
      RSN_INLINE small_vec(small_vec &&rhs) noexcept: _size(rhs._size) {
         rhs._size = 0;
         if (RSN_UNLIKELY(rhs._pbuf != reinterpret_cast<decltype(rhs._pbuf)>(rhs._buf.data())))
            _pbuf = rhs._pbuf, rhs._pbuf = reinterpret_cast<decltype(rhs._pbuf)>(rhs._buf.data());
         else {
            _pbuf = reinterpret_cast<decltype(_pbuf)>(_buf.data());
            if constexpr (std::is_trivially_copyable_v<value_type>)
               std::memcpy(&_buf, &rhs._buf, sizeof _buf);
            else for (decltype(_size) sn = 0; sn < _size; ++sn)
               new (const_cast<std::remove_cv_t<Obj> *>(_pbuf) + sn) const value_type(std::move(rhs._pbuf[sn])), rhs._pbuf[sn].~value_type();
         }
      }
      ~small_vec() {
         if constexpr (!std::is_trivially_copyable_v<value_type>)
            for (auto sn = _size; sn;) _pbuf[--sn].~value_type();
         if (RSN_UNLIKELY(_pbuf != reinterpret_cast<decltype(_pbuf)>(_buf.data())))
            delete[] reinterpret_cast<std::aligned_union_t<0, value_type> *>(const_cast<std::remove_cv_t<Obj> *>(_pbuf));
      }
      void reset(size_type res = 0) {
         this->~small_vec(), new(this) small_vec(res);
      }
   public:
      small_vec &operator=(small_vec &&) = delete;
   public: // incremental building
      void push_back(const value_type &obj)                { new (const_cast<std::remove_cv_t<Obj> *>(_pbuf) + _size++) value_type(obj); }
      RSN_INLINE void push_back(value_type &&obj) noexcept { new (const_cast<std::remove_cv_t<Obj> *>(_pbuf) + _size++) value_type(std::move(obj)); }
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
      std::array<std::aligned_union_t<0, value_type>, MinRes> _buf;
   };

   /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

   namespace aux {
      using std::begin, std::end, std::cbegin, std::cend;
      template<typename Cont> RSN_INLINE static auto _begin(Cont &cont) noexcept(begin(cont)) { return begin(cont); }
      template<typename Cont> RSN_INLINE static auto _end(Cont &cont) noexcept(end(cont)) { return end(cont); }
      template<typename Cont> RSN_INLINE static auto _cbegin(Cont &cont) noexcept(cbegin(cont)) { return cbegin(cont); }
      template<typename Cont> RSN_INLINE static auto _cend(Cont &cont) noexcept(cend(cont)) { return cend(cont); }
      using std::rbegin, std::rend, std::crbegin, std::crend;
      template<typename Cont> RSN_INLINE static auto _rbegin(Cont &cont) noexcept(rbegin(cont)) { return rbegin(cont); }
      template<typename Cont> RSN_INLINE static auto _rend(Cont &cont) noexcept(rend(cont)) { return rend(cont); }
      template<typename Cont> RSN_INLINE static auto _crbegin(Cont &cont) noexcept(crbegin(cont)) { return crbegin(cont); }
      template<typename Cont> RSN_INLINE static auto _crend(Cont &cont) noexcept(crend(cont)) { return crend(cont); }
   }

   template<typename Begin, typename End = Begin>
   class slice_ref { // non-owning view that refers to a range of container elements (generalized analog of llvm::ArrayRef)
   public: // typedefs to imitate standard containers
      typedef Begin                                                    iterator, const_iterator;
      typedef typename std::iterator_traits<iterator>::value_type      value_type;
      typedef typename std::iterator_traits<iterator>::pointer         pointer, const_pointer;
      typedef typename std::iterator_traits<iterator>::reference       reference, const_reference;
      typedef typename std::iterator_traits<iterator>::difference_type difference_type;
      typedef std::make_unsigned_t<difference_type>                    size_type;
   public: // standard operations
      slice_ref() = default; // + implicitly defaulted copy and move constructors and assignment operators
      RSN_INLINE void swap(slice_ref &rhs) noexcept { using std::swap; swap(_begin, rhs._begin), swap(_end, rhs._end); }
   public: // miscellaneous constructors and assignment operators
      RSN_INLINE slice_ref(Begin _begin, End _end) noexcept
         : _begin(std::move(_begin)), _end(std::move(_end)) {}
      template<typename RhsBegin, typename RhsEnd = RhsBegin> RSN_INLINE slice_ref(RhsBegin _begin, RhsEnd _end) noexcept
         : _begin(std::move(_begin)), _end(std::move(_end)) {}
   public:
      template<typename RhsBegin, typename RhsEnd = RhsBegin> RSN_INLINE slice_ref(slice_ref<RhsBegin, RhsEnd> &rhs) noexcept
         : _begin(rhs._begin), _end(rhs._end) {}
      template<typename RhsBegin, typename RhsEnd = RhsBegin> RSN_INLINE slice_ref(const slice_ref<RhsBegin, RhsEnd> &rhs) noexcept
         : _begin(rhs._begin), _end(rhs._end) {}
      template<typename RhsBegin, typename RhsEnd = RhsBegin> RSN_INLINE slice_ref(slice_ref<RhsBegin, RhsEnd> &&rhs) noexcept
         : _begin(std::move(rhs._begin)), _end(std::move(rhs._end)) {}
      template<typename RhsBegin, typename RhsEnd = RhsBegin> RSN_INLINE slice_ref(const slice_ref<RhsBegin, RhsEnd> &&rhs) noexcept
         : _begin(std::move(rhs._begin)), _end(std::move(rhs._end)) {}
      template<typename RhsBegin, typename RhsEnd = RhsBegin> RSN_INLINE slice_ref &operator=(slice_ref<RhsBegin, RhsEnd> &rhs) noexcept
         { _begin = rhs._begin, _end = rhs._end; return *this; }
      template<typename RhsBegin, typename RhsEnd = RhsBegin> RSN_INLINE slice_ref &operator=(const slice_ref<RhsBegin, RhsEnd> &rhs) noexcept
         { _begin = rhs._begin, _end = rhs._end; return *this; }
      template<typename RhsBegin, typename RhsEnd = RhsBegin> RSN_INLINE slice_ref &operator=(slice_ref<RhsBegin, RhsEnd> &&rhs) noexcept
         { _begin = std::move(rhs._begin), _end = std::move(rhs._end); return *this; }
      template<typename RhsBegin, typename RhsEnd = RhsBegin> RSN_INLINE slice_ref &operator=(const slice_ref<RhsBegin, RhsEnd> &&rhs) noexcept
         { _begin = std::move(rhs._begin), _end = std::move(rhs._end); return *this; }
   public:
      template<typename Rhs> slice_ref(Rhs &&) = delete;
      template<typename Rhs> RSN_INLINE slice_ref(Rhs &rhs) noexcept(noexcept(aux::_begin(rhs), aux::_end(rhs)))
         : _begin(aux::_begin(rhs)), _end(aux::_end(rhs)) {}
      template<typename Rhs> slice_ref &operator=(Rhs &&) = delete;
      template<typename Rhs> RSN_INLINE slice_ref &operator=(Rhs &rhs) noexcept(noexcept(aux::_begin(rhs), aux::_end(rhs)))
         { _begin = _begin(rhs), _end = _end(rhs); return *this; }
   public: // container-like access/iteration
      RSN_INLINE bool empty() const noexcept(noexcept(_end == _begin)) // gcc bug #52869 prior to gcc-9
         { return _end == _begin; }
      RSN_INLINE size_type size() const noexcept(noexcept(end() - begin()))
         { return end() - begin(); }
      RSN_INLINE size_type size_ex() const noexcept(noexcept(std::distance(begin(), end())))
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
      RSN_INLINE reference head() const noexcept(noexcept(*_begin))
         { return *_begin; }
      RSN_INLINE reference tail() const noexcept(noexcept(*std::prev(end())))
         { return *std::prev(end()); }
      RSN_INLINE auto drop_head() const noexcept(noexcept(drop_head_ex(1)))
         { return drop_head_ex(1); }
      RSN_INLINE auto drop_tail() const noexcept(noexcept(drop_tail_ex(1)))
         { return drop_tail_ex(1); }
      RSN_INLINE auto drop_head(difference_type size) const noexcept(noexcept(lib::slice_ref{begin() + size, end()}))
         { return lib::slice_ref{begin() + size, end()}; }
      RSN_INLINE auto drop_tail(difference_type size) const noexcept(noexcept(lib::slice_ref{begin(), end() - size}))
         { return lib::slice_ref{begin(), end() - size}; }
      RSN_INLINE auto drop_head_ex(difference_type size) const noexcept(noexcept(lib::slice_ref{std::next(begin(), size), end()}))
         { return lib::slice_ref{std::next(begin(), size), end()}; }
      RSN_INLINE auto drop_tail_ex(difference_type size) const noexcept(noexcept(lib::slice_ref{begin(), std::prev(end(), size)}))
         { return lib::slice_ref{begin(), std::prev(end(), size)}; }
      RSN_INLINE auto reverse() const noexcept(noexcept(lib::slice_ref{rbegin(), rend()}))
         { return lib::slice_ref{rbegin(), rend()}; }
   private: // internal representation
      Begin _begin; End _end;
      template<typename, typename> friend class slice_ref;
   };
   // Non-member functions
   template<typename Begin, typename End = Begin>
   RSN_INLINE inline void swap(slice_ref<Begin, End> &lhs, slice_ref<Begin, End> &rhs) noexcept
      { lhs.swap(rhs); }
   template<typename LhsBegin, typename RhsBegin, typename LhsEnd = LhsBegin, typename RhsEnd = RhsBegin>
   RSN_INLINE inline bool operator==(const slice_ref<LhsBegin, LhsEnd> &lhs, const slice_ref<RhsBegin, RhsEnd> &rhs)
   noexcept(noexcept(lhs.begin() == rhs.begin() && lhs.end() == rhs.end()))
      { return lhs.begin() == rhs.begin() && lhs.end() == rhs.end(); }
   template<typename LhsBegin, typename RhsBegin, typename LhsEnd = LhsBegin, typename RhsEnd = RhsBegin>
   RSN_INLINE inline bool operator!=(const slice_ref<LhsBegin, LhsEnd> &lhs, const slice_ref<RhsBegin, RhsEnd> &rhs)
   noexcept(noexcept(lhs.begin() != rhs.begin() || lhs.end() != rhs.end()))
      { return lhs.begin() != rhs.begin() || lhs.end() != rhs.end(); }
   // Construction helper
   template<typename Cont> RSN_INLINE inline auto make_slice_ref(Cont &cont)
      { using std::begin, std::end; return slice_ref{begin(cont), end(cont)}; }

}

# endif // # ifndef RSN_INCLUDED_RUSINI
