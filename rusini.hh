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

# include "rusini0.hh"

namespace rsn::lib {

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
