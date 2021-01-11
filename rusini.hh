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

   // Temporary Vectors Optimized for Small Sizes //////////////////////////////////////////////////////////////////////////////////////////////////////////////

   /* The interface of small_vec is modelled after std::vector, with the following differences:
      * The capacity is not tracked; it cannot be queried and cannot be altered without clearing elements.
      * The capacity can be conveniently reserved on construction (instead of specifying an initial size).
   */

   template<typename Obj, std::size_t MinRes = /*up to 2 cache-lines*/ (128 - sizeof(Obj *) - sizeof(std::size_t)) / sizeof(Obj)>
   class small_vec { // lightweight analog of llvm::SmallVector
   public: // typedefs to imitate standard containers
      typedef Obj              value_type; // Obj = const T is allowed (compared to std::vector, std::list, and std::set) <-- TODO: maybe generally a BAD idea, since it affects push_back etc efficiency!
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
      RSN_INLINE explicit small_vec() noexcept
         : _begin(reinterpret_cast<decltype(_begin)>(buf.data())), _end(_begin) {}
      RSN_INLINE explicit small_vec(size_type res)
         : _begin(reinterpret_cast<decltype(_begin)>(RSN_LIKELY(res <= MinRes) ? buf.data() : new std::aligned_union_t<0, value_type>[res])), _end(_begin) {}
      small_vec(std::initializer_list<value_type> rhs, size_type res = 0)
         : small_vec(rhs.size() + res) { for (auto &&obj: rhs) push_back(obj); }
      small_vec(size_type res, std::initializer_list<value_type> rhs)
         : small_vec(res) { for (auto &&obj: rhs) push_back(obj); }
      RSN_INLINE void clear() noexcept {
         if constexpr (!std::is_trivially_copyable_v<value_type>)
            for (auto it = _end; it-- != _begin;) it->~value_type();
         _end = _begin;
      }
      RSN_INLINE void clear(size_type res)
         { this->~small_vec(), new(this) small_vec(res); }
      void clear(std::initializer_list<value_type> rhs, size_type res = 0)
         { this->~small_vec(), new(this) small_vec(rhs, res); }
      void clear(size_type res, std::initializer_list<value_type> rhs)
         { this->~small_vec(), new(this) small_vec(res, rhs); }
   public:
      RSN_INLINE small_vec(small_vec &&rhs)
      noexcept(noexcept(new(nullptr) value_type(std::declval<value_type &&>()))) {
         if (RSN_UNLIKELY(rhs._begin != reinterpret_cast<decltype(rhs._begin)>(rhs.buf.data())))
            _begin = rhs._begin, _end = rhs._end, rhs._begin = reinterpret_cast<decltype(rhs._begin)>(rhs.buf.data());
         else {
            _end = (_begin = reinterpret_cast<decltype(_begin)>(buf.data())) + rhs.size();
            if constexpr (std::is_trivially_copyable_v<value_type>)
               std::memcpy(&buf, &rhs.buf, sizeof buf);
            else for (auto dest = _begin, src = rhs._begin; dest != _end; ++dest, ++src)
               new(const_cast<std::remove_cv_t<value_type> *>(dest)) value_type(std::move(*src)), src->~value_type();
         }
         rhs._end = rhs._begin;
      }
      RSN_INLINE ~small_vec() {
         if constexpr (!std::is_trivially_copyable_v<value_type>)
            for (auto it = _end; it-- != _begin;) it->~value_type();
         if (RSN_UNLIKELY(_begin != reinterpret_cast<decltype(_begin)>(buf.data())))
            delete[] reinterpret_cast<std::aligned_union_t<0, value_type> *>(const_cast<std::remove_cv_t<Obj> *>(_begin));
      }
   public: // incremental building
      RSN_INLINE void push_back(const value_type &rhs) noexcept(noexcept(new(nullptr) value_type(rhs)))
         { new(const_cast<std::remove_cv_t<value_type> *>(_end++)) value_type(rhs); }
      RSN_INLINE void push_back(value_type &&rhs) noexcept(noexcept(new(nullptr) value_type(std::move(rhs))))
         { new(const_cast<std::remove_cv_t<value_type> *>(_end++)) value_type(std::move(rhs)); }
      template<typename ...Rhs> RSN_INLINE reference emplace_back(Rhs &&...rhs) noexcept(noexcept(new(nullptr) value_type(std::forward<Rhs>(rhs)...)))
         { return *new(const_cast<std::remove_cv_t<value_type> *>(_end++)) value_type(std::forward<Rhs>(rhs)...); }
      RSN_INLINE void pop_back() noexcept
         { (--_end)->~value_type(); }
   public: // access/iteration
      RSN_INLINE bool empty() const noexcept { return end() == begin(); }
      RSN_INLINE size_type size() const noexcept { return end() - begin(); }
      size_type capacity() const = delete; // unaccessible in case of small_vec
      void reserve() = delete;
      RSN_INLINE auto data() noexcept       { return begin(); }
      RSN_INLINE auto data() const noexcept { return begin(); }
      RSN_INLINE auto &operator[](size_type sn) noexcept       { return data()[sn]; }
      RSN_INLINE auto &operator[](size_type sn) const noexcept { return data()[sn]; }
   public:
      RSN_INLINE iterator       begin() noexcept       { return _begin; }
      RSN_INLINE const_iterator begin() const noexcept { return _begin; }
      RSN_INLINE iterator       end() noexcept       { return _end; }
      RSN_INLINE const_iterator end() const noexcept { return _end; }
      RSN_INLINE auto rbegin() noexcept       { return reverse_iterator(end()); }
      RSN_INLINE auto rbegin() const noexcept { return const_reverse_iterator(end()); }
      RSN_INLINE auto rend() noexcept       { return reverse_iterator(begin()); }
      RSN_INLINE auto rend() const noexcept { return const_reverse_iterator(begin()); }
      RSN_INLINE auto cbegin() const noexcept  { return begin(); }
      RSN_INLINE auto cend() const noexcept    { return end(); }
      RSN_INLINE auto crbegin() const noexcept { return rbegin(); }
      RSN_INLINE auto crend() const noexcept   { return rend(); }
   private: // internal representation
      pointer _begin, _end;
      std::array<std::aligned_union_t<0, value_type>, MinRes> buf;
   };

   // Non-owning References to a Range of Values ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

   namespace aux { // ADL for begin/end
      using std::begin, std::end;
      template<typename Cont> RSN_INLINE static auto _begin(Cont &cont) noexcept(noexcept(begin(cont))) { return begin(cont); }
      template<typename Cont> RSN_INLINE static auto _end(Cont &cont) noexcept(noexcept(end(cont))) { return end(cont); }
   }

   template<typename Begin, typename End = Begin>
   class slice_ref { // generalized analog of llvm::ArrayRef
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
      template<typename Rhs> RSN_INLINE slice_ref(Rhs &rhs) noexcept(noexcept(aux::_begin(rhs), aux::_end(rhs)))
         : _begin(aux::_begin(rhs)), _end(aux::_end(rhs)) {}
      template<typename Rhs> RSN_INLINE slice_ref &operator=(Rhs &rhs) noexcept(noexcept(aux::_begin(rhs), aux::_end(rhs)))
         { _begin = aux::_begin(rhs), _end = aux::_end(rhs); return *this; }
   public:
      template<typename Rhs> slice_ref(Rhs &&) = delete;
      template<typename Rhs> slice_ref &operator=(Rhs &&) = delete;
   public:
      template<typename RhsBegin, typename RhsEnd = RhsBegin> RSN_INLINE slice_ref(RhsBegin &&_begin, RhsEnd &&_end) noexcept
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
      RSN_INLINE auto rbegin() const noexcept(noexcept(std::reverse_iterator(end()))) { return std::reverse_iterator(end()); }
      RSN_INLINE auto rend() const noexcept(noexcept(std::reverse_iterator(begin()))) { return std::reverse_iterator(begin()); }
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
      RSN_INLINE slice_ref drop_head(size_type size) const noexcept(noexcept(_begin + (difference_type)size))
         { return {_begin + (difference_type)size, end()}; }
      RSN_INLINE slice_ref drop_tail(size_type size) const noexcept(noexcept(_end - (difference_type)size))
         { return {begin(), _end - (difference_type)size}; }
      RSN_INLINE slice_ref drop_head_ex(size_type size) const noexcept(noexcept(std::next(begin(), size)))
         { return {std::next(begin(), size), end()}; }
      RSN_INLINE slice_ref drop_tail_ex(size_type size) const noexcept(noexcept(std::prev(end(), size)))
         { return {begin(), std::prev(end(), size)}; }
      RSN_INLINE auto reverse() const noexcept(noexcept(lib::slice_ref{rbegin(), rend()}))
         { return lib::slice_ref{rbegin(), rend()}; }
   private: // internal representation
      Begin _begin; End _end;
      template<typename, typename> friend class slice_ref;
   };
   // Deduction guides
   template<typename Rhs> slice_ref(Rhs &&) -> slice_ref<decltype(aux::_begin(std::declval<Rhs &>())), decltype(aux::_end(std::declval<Rhs &>()))>;
   template<typename Begin, typename End> slice_ref(Begin, End) -> slice_ref<Begin, End>;
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

}

# endif // # ifndef RSN_INCLUDED_RUSINI
