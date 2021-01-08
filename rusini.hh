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

   /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

   class smart_rc;

   template<typename Obj>
   class smart_ptr { // simple, intrussive, and MT-unsafe analog of std::shared_ptr
   public: // Standard operations
      smart_ptr() = default;
      RSN_INLINE smart_ptr(const smart_ptr &rhs) noexcept: rep(rhs.rep) { retain(); }
      RSN_INLINE smart_ptr(smart_ptr &&rhs) noexcept: rep(rhs.rep) { rhs.rep = {}; }
      RSN_INLINE ~smart_ptr() { release(); }
      RSN_INLINE smart_ptr &operator=(const smart_ptr &rhs) noexcept { rhs.retain(), release(), rep = rhs.rep; return *this; }
      RSN_INLINE smart_ptr &operator=(smart_ptr &&rhs) noexcept { swap(*this, rhs); return *this; }
      RSN_INLINE void swap(smart_ptr &rhs) noexcept { using std::swap; swap(rep, rhs.rep); }
   public: // Miscellaneous operations
      template<typename ...Args> RSN_INLINE RSN_NODISCARD static auto make(Args &&...args) { return smart_ptr{new Obj(std::forward<Args>(args)...)}; }
      template<typename Rhs> RSN_INLINE smart_ptr(const smart_ptr<Rhs> &rhs) noexcept: rep(rhs.rep) { retain(); }
      template<typename Rhs> RSN_INLINE smart_ptr(smart_ptr<Rhs> &&rhs) noexcept: rep(rhs.rep) { rhs.rep = {}; }
      template<typename Rhs> RSN_INLINE smart_ptr &operator=(const smart_ptr<Rhs> &rhs) noexcept { rhs.retain(), release(), rep = rhs.rep; return *this; }
      template<typename Rhs> RSN_INLINE smart_ptr &operator=(smart_ptr<Rhs> &&rhs) noexcept { rep = rhs.rep, rhs.rep = {}; return *this; }
      RSN_INLINE operator Obj *() const noexcept { return rep; }
      RSN_INLINE Obj &operator*() const noexcept { return *rep; }
      RSN_INLINE Obj *operator->() const noexcept { return rep; }
   private: // Internal representation
      Obj *rep{};
      template<typename> friend class smart_ptr;
   private: // Implementation helpers
      RSN_INLINE explicit smart_ptr(Obj *rep) noexcept: rep(rep) {}
      RSN_INLINE void retain() const noexcept { if (RSN_LIKELY(rep)) ++rep->smart_rc::rc; }
      RSN_INLINE void release() const noexcept { if (RSN_LIKELY(rep) && RSN_UNLIKELY(!--rep->smart_rc::rc)) delete rep; }
      template<typename Dest, typename Src> friend std::enable_if_t<std::is_base_of_v<smart_rc, Src>, smart_ptr<Dest>> as_smart(const smart_ptr<Src> &) noexcept;
   };
   // Non-member functions
   template<typename Obj> RSN_INLINE inline void swap(smart_ptr<Obj> &lhs, smart_ptr<Obj> &rhs) noexcept { lhs.swap(rhs); }

   class smart_rc {
      long rc = 1;
      template<typename> friend class smart_ptr;
   protected:
      smart_rc() = default;
      ~smart_rc() = default;
   public:
      smart_rc(const smart_rc &) = delete;
      smart_rc &operator=(const smart_rc &) = delete;
   };

   /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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
      RSN_INLINE explicit small_vec(std::size_t res = 0):
         _pbuf(reinterpret_cast<decltype(_pbuf)>(RSN_LIKELY(res <= MinRes) ? _buf.data() : new std::aligned_union_t<0, value_type>[res])) {
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
            delete[] reinterpret_cast<std::aligned_union_t<0, value_type> *>(const_cast<std::remove_cv_t<Obj> *>(_pbuf));
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
   public:
      void reset(std::size_t res = 0) {
         this->~small_vec(), new (this) small_vec(res);
      }
   private: // internal representation
      pointer _pbuf;
      size_type _size = 0;
      std::array<std::aligned_union_t<0, value_type>, MinRes> _buf;
   };

   /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

   template<typename Begin, typename End = Begin>
   class slice_ref { // non-owning "reference" to a range of container elements (generalized analog of llvm::ArrayRef)
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
      template<typename Rhs> RSN_INLINE slice_ref(Rhs &rhs): _begin(adl_begin(rhs)), _end(adl_end(rhs)) {}
      template<typename Rhs> slice_ref(Rhs &&) = delete;
      template<typename Rhs> RSN_INLINE slice_ref &operator=(Rhs &rhs) { _begin = adl_begin(rhs), _end = adl_end(rhs); return *this; }
      template<typename Rhs> slice_ref &operator=(Rhs &&) = delete;
   public: // container-like access/iteration
      RSN_INLINE bool empty() const noexcept(noexcept(_end == _begin))
         { return _end == _begin; }
      RSN_INLINE size_type size() const noexcept(noexcept(_end - _begin))
         { return _end - _begin; }
      RSN_INLINE size_type size_ex() const noexcept(noexcept(std::distance(_begin, _end)))
         { return std::distance(_begin, _end); }
   public:
      RSN_INLINE auto begin() const noexcept { return _begin; }
      RSN_INLINE auto end() const noexcept { return _end; }
      RSN_INLINE auto rbegin() const noexcept { return std::reverse_iterator(_end); }
      RSN_INLINE auto rend() const noexcept { return std::reverse_iterator(_begin); }
      RSN_INLINE auto cbegin() const noexcept { return begin(); }
      RSN_INLINE auto cend() const noexcept { return end(); }
      RSN_INLINE auto crbegin() const noexcept { return rbegin(); }
      RSN_INLINE auto crend() const noexcept { return rend(); }
   public: // convenience operations
      RSN_INLINE reference head() const noexcept(noexcept(*_begin))
         { return *_begin; }
      RSN_INLINE reference tail() const noexcept(noexcept(*std::prev(_end)))
         { return *std::prev(_end); }
      RSN_INLINE auto drop_head() const noexcept(noexcept(drop_head_ex(1)))
         { return drop_head_ex(1); }
      RSN_INLINE auto drop_tail() const noexcept(noexcept(drop_tail_ex(1)))
         { return drop_tail_ex(1); }
      RSN_INLINE auto drop_head(difference_type size) const noexcept(noexcept(lib::slice_ref{_begin + size, _end}))
         { return lib::slice_ref{_begin + size, _end}; }
      RSN_INLINE auto drop_tail(difference_type size) const noexcept(noexcept(lib::slice_ref{_begin, _end - size}))
         { return lib::slice_ref{_begin, _end - size}; }
      RSN_INLINE auto drop_head_ex(difference_type size) const noexcept(noexcept(lib::slice_ref{std::next(_begin, size), _end}))
         { return lib::slice_ref{std::next(_begin, size), _end}; }
      RSN_INLINE auto drop_tail_ex(difference_type size) const noexcept(noexcept(lib::slice_ref{_begin, std::prev(_end, size)}))
         { return lib::slice_ref{_begin, std::prev(_end, size)}; }
      RSN_INLINE auto reverse() const noexcept(noexcept(lib::slice_ref{rbegin(), rend()}))
         { return lib::slice_ref{rbegin(), rend()}; }
   private: // internal representation
      Begin _begin; End _end;
      template<typename, typename> friend class slice_ref;
   private: // implementation helpers
      template<typename Cont> RSN_INLINE static auto adl_begin(Cont &cont) { using std::begin; return begin(cont); }
      template<typename Cont> RSN_INLINE static auto adl_end(Cont &cont) { using std::end; return end(cont); }
   };
   // non-member functions
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
   // construction helper
   template<typename Cont> RSN_INLINE inline auto make_slice_ref(Cont &cont)
      { using std::begin, std::end; return slice_ref{begin(cont), end(cont)}; }

}

# endif // # ifndef RSN_INCLUDED_RUSINI
