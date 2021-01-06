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

   template<typename Iter>
   class slice_ref { // non-owning "reference" to a range of container elements (generalized analog of llvm::ArrayRef)
   public: // typedefs to imitate standard containers
      typedef Iter                                                     iterator, const_iterator;
      typedef typename std::iterator_traits<iterator>::value_type      value_type;
      typedef typename std::iterator_traits<iterator>::pointer         pointer, const_pointer;
      typedef typename std::iterator_traits<iterator>::reference       reference, const_reference;
      typedef typename std::iterator_traits<iterator>::difference_type difference_type;
      typedef std::make_unsigned_t<difference_type>                    size_type;
   public: // standard operations
      slice_ref() = default;
      /* implicitly defaulted copy and move constructors and assignment operators */
      RSN_INLINE void swap(slice_ref &rhs) noexcept { using std::swap; swap(_begin, rhs._begin), swap(_end, rhs._end); }
      template<typename Lhs, typename Rhs> friend bool operator==(const slice_ref<Lhs> &, const slice_ref<Rhs> &) noexcept;
      template<typename Lhs, typename Rhs> friend bool operator!=(const slice_ref<Lhs> &, const slice_ref<Rhs> &) noexcept;
   public: // miscellaneous constructors and assignment operators
      RSN_INLINE slice_ref(iterator _begin, iterator _end) noexcept: _begin(std::move(_begin)), _end(std::move(_end)) {}
      template<typename Begin, typename End> RSN_INLINE slice_ref(Begin _begin, End _end) noexcept: _begin(std::move(_begin)), _end(std::move(_end)) {}
   public:
      template<typename Rhs> RSN_INLINE slice_ref(Rhs &rhs) noexcept: slice_ref{adl_begin(rhs), adl_end(rhs)} {}
      template<typename Rhs> slice_ref(Rhs &&) = delete;
      template<typename Rhs> RSN_INLINE slice_ref &operator=(Rhs &rhs) noexcept { _begin = adl_begin(rhs), _end = adl_end(rhs); return *this; }
      template<typename Rhs> slice_ref &operator=(Rhs &&) = delete;
   public:
      template<typename Rhs> RSN_INLINE slice_ref(slice_ref<Rhs> &rhs) noexcept
         : _begin(rhs._begin), _end(rhs._end) {}
      template<typename Rhs> RSN_INLINE slice_ref(const slice_ref<Rhs> &rhs) noexcept
         : _begin(rhs._begin), _end(rhs._end) {}
      template<typename Rhs> RSN_INLINE slice_ref(slice_ref<Rhs> &&rhs) noexcept
         : _begin(std::move(rhs._begin)), _end(std::move(rhs._end)) {}
      template<typename Rhs> RSN_INLINE slice_ref(const slice_ref<Rhs> &&rhs) noexcept
         : _begin(rhs._begin), _end(rhs._end) {}
      template<typename Rhs> RSN_INLINE slice_ref &operator=(slice_ref<Rhs> &rhs) noexcept
         { _begin = rhs._begin, _end = rhs._end; return *this; }
      template<typename Rhs> RSN_INLINE slice_ref &operator=(const slice_ref<Rhs> &rhs) noexcept
         { _begin = rhs._begin, _end = rhs._end; return *this; }
      template<typename Rhs> RSN_INLINE slice_ref &operator=(slice_ref<Rhs> &&rhs) noexcept
         { _begin = std::move(rhs._begin), _end = std::move(rhs._end); return *this; }
      template<typename Rhs> RSN_INLINE slice_ref &operator=(const slice_ref<Rhs> &&rhs) noexcept
         { _begin = rhs._begin, _end = rhs._end; return *this; }
   public: // container-like access/iteration
      RSN_INLINE bool empty() const noexcept { return end() == begin(); }
      RSN_INLINE size_type size() const noexcept { return end() - begin(); }
      RSN_INLINE size_type size_ex() const noexcept { return std::distance(begin(), end()); }
   public:
      RSN_INLINE auto begin() const noexcept { return _begin; }
      RSN_INLINE auto end() const noexcept { return _end; }
      RSN_INLINE auto cbegin() const noexcept { return begin(); }
      RSN_INLINE auto cend() const noexcept { return end(); }
   public: // convenience operations
      RSN_INLINE auto &head() const noexcept { return *begin(); }
      RSN_INLINE auto &tail() const noexcept { return *std::prev(end()); }
      RSN_INLINE slice_ref drop_head() const noexcept { return drop_head_ex(1); }
      RSN_INLINE slice_ref drop_tail() const noexcept { return drop_tail_ex(1); }
      RSN_INLINE slice_ref drop_head(difference_type count) const noexcept { return {begin() + count, end()}; }
      RSN_INLINE slice_ref drop_tail(difference_type count) const noexcept { return {begin(), end() - count}; }
      RSN_INLINE slice_ref drop_head_ex(difference_type count) const noexcept { return {std::next(begin(), count), end()}; }
      RSN_INLINE slice_ref drop_tail_ex(difference_type count) const noexcept { return {begin(), std::prev(end(), count)}; }
   private: // internal representation
      iterator _begin, _end;
      template<typename> friend class slice_ref;
   private: // implementation helpers
      template<typename Cont> RSN_INLINE static auto adl_begin(Cont &&cont) { using std::begin; return begin(std::forward<Cont>(cont)); }
      template<typename Cont> RSN_INLINE static auto adl_end(Cont &&cont) { using std::end; return end(std::forward<Cont>(cont)); }
   };
   // non-member functions
   template<typename Iter> RSN_INLINE inline void swap(slice_ref<Iter> &lhs, slice_ref<Iter> &rhs)
      { lhs.swap(rhs); }
   template<typename Lhs, typename Rhs> RSN_INLINE inline bool operator==(const slice_ref<Lhs> &lhs, const slice_ref<Rhs> &rhs) noexcept
      { return lhs.begin == rhs.begin && lhs.end == rhs.end; }
   template<typename Lhs, typename Rhs> RSN_INLINE inline bool operator!=(const slice_ref<Lhs> &lhs, const slice_ref<Rhs> &rhs) noexcept
      { return lhs.begin != rhs.begin || lhs.end != rhs.end; }
   // construction helper
   template<typename Cont> RSN_INLINE inline auto make_slice_ref(Cont &cont) noexcept
      { using std::begin, std::end; return slice_ref{begin(cont), end(cont)}; }

}

# endif // # ifndef RSN_INCLUDED_RUSINI
