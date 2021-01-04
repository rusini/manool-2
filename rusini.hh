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

   namespace aux {
      using std::begin, std::end;
      template<typename Obj> using iterator_begin = decltype(begin(*static_cast<Obj *>(nullptr)));
      template<typename Obj> using iterator_end = decltype(end(*static_cast<Obj *>(nullptr)));
   }

   template<typename Obj>
   class slice_ref { // non-owning "reference" to a range of container elements (generalized analog of llvm::ArrayRef)
   public: // expose typedef traits and check begin/end consistency
      typedef aux::iterator_begin<Obj>                                 iterator, const_iterator;
      typedef typename std::iterator_traits<iterator>::value_type      value_type;
      typedef typename std::iterator_traits<iterator>::pointer         pointer, const_pointer;
      typedef typename std::iterator_traits<iterator>::reference       reference, const_reference;
      typedef typename std::iterator_traits<iterator>::difference_type difference_type;
      typedef std::make_unsigned_t<difference_type>                    size_type;
      static_assert (std::is_same_v<aux::iterator_end<Obj>, iterator>);
   public: // standard operations
      slice_ref() = default;
      /* implicitly defaulted copy and move constructors and assignment operators */
      RSN_INLINE void swap(slice_ref &rhs) noexcept { using std::swap; swap(_begin, rhs._begin), swap(_end, rhs._end); }
      template<typename Lhs, typename Rhs> friend bool operator==(const slice_ref<Lhs> &, const slice_ref<Rhs> &) noexcept;
      template<typename Lhs, typename Rhs> friend bool operator!=(const slice_ref<Lhs> &, const slice_ref<Rhs> &) noexcept;
   public: // miscellaneous constructors and assignment operators
      RSN_INLINE slice_ref(Obj &rhs) noexcept: _begin(std::begin(rhs)), _end(std::end(rhs)) {}
      RSN_INLINE slice_ref(iterator _begin, iterator _end) noexcept: _begin(std::move(_begin)), _end(std::move(_end)) {}
      template<typename Begin, typename End> RSN_INLINE slice_ref(Begin _begin, End _end) noexcept: _begin(std::move(_begin)), _end(std::move(_end)) {}
   public:
      template<typename Rhs> RSN_INLINE slice_ref(Rhs &rhs) noexcept: _begin(std::begin(rhs)), _end(std::end(rhs)) {}
      template<typename Rhs> slice_ref(Rhs &&) = delete;
      template<typename Rhs> RSN_INLINE slice_ref &operator=(Rhs &rhs) noexcept { _begin = std::begin(rhs), _end = std::end(rhs); return *this; }
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
   public: // container-like operations
      RSN_INLINE bool empty() const noexcept { return end() == begin(); }
      RSN_INLINE size_type size() const noexcept { return end() - begin(); }
      RSN_INLINE size_type size_ex() const noexcept { return std::distance(begin(), end()); }
   public:
      RSN_INLINE auto &begin() noexcept       { return _begin; }
      RSN_INLINE auto &begin() const noexcept { return _begin; }
      RSN_INLINE auto &end() noexcept       { return _end; }
      RSN_INLINE auto &end() const noexcept { return _end; }
      RSN_INLINE auto &cbegin() noexcept       { return begin(); }
      RSN_INLINE auto &cbegin() const noexcept { return begin(); }
      RSN_INLINE auto &cend() noexcept       { return end(); }
      RSN_INLINE auto &cend() const noexcept { return end(); }
   public: // convenience operations
      RSN_INLINE value_type &head() const noexcept { return *begin(); }
      RSN_INLINE value_type &tail() const noexcept { return *std::prev(end()); }
      RSN_INLINE slice_ref skip_head() const noexcept { return {std::next(begin()), end()}; }
      RSN_INLINE slice_ref skip_tail() const noexcept { return {begin(), std::prev(end())}; }
      RSN_INLINE slice_ref skip_head(difference_type count) const noexcept { return {begin() + count, end()}; }
      RSN_INLINE slice_ref skip_tail(difference_type count) const noexcept { return {begin(), end() - count}; }
      RSN_INLINE slice_ref skip_head_ex(difference_type count) const noexcept { return {std::next(begin(), count), end()}; }
      RSN_INLINE slice_ref skip_tail_ex(difference_type count) const noexcept { return {begin(), std::prev(end(), count)}; }
   private: // internal representation
      iterator _begin, _end;
      template<typename> friend class slice_ref;
   };
   template<typename Obj>
   RSN_INLINE inline void swap(slice_ref<Obj> &lhs, slice_ref<Obj> &rhs)
      { lhs.swap(rhs); }
   template<typename Lhs, typename Rhs> RSN_INLINE inline bool operator==(const slice_ref<Lhs> &lhs, const slice_ref<Rhs> &rhs) noexcept
      { return lhs.begin == rhs.begin && lhs.end == rhs.end; }
   template<typename Lhs, typename Rhs> RSN_INLINE inline bool operator!=(const slice_ref<Lhs> &lhs, const slice_ref<Rhs> &rhs) noexcept
      { return lhs.begin != rhs.begin || lhs.end != rhs.end; }

}

# endif // # ifndef RSN_INCLUDED_RUSINI
