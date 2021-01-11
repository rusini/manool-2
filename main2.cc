
# include <cstdio>
# include <set>
# include <map>
# include <list>
# include <utility>
# include <vector>

# include "rusini.hh"

namespace lib {
   class smart_rc {
   public:
      long rc = 1;
   protected:
      smart_rc() = default;
      ~smart_rc() = default;
   };
}
class n: rsn::lib::smart_rc {
   n() = default;
   ~n() = default;
   template<typename> friend class rsn::lib::smart_ptr;
public: // construction/destruction
   RSN_INLINE RSN_NODISCARD static auto make() { return rsn::lib::smart_ptr<n>::make(); }
};

struct it: std::iterator<std::random_access_iterator_tag, int> {
   it() = default;
   it(const it &) { std::puts("it(const it &)"); }
   it(it &&) { std::puts("it(it &)"); }
};

struct it2: std::iterator<std::random_access_iterator_tag, int> {
   it2() = default;
   it2(const it2 &) { std::puts("it(const it2 &)"); }
   it2(it2 &&) { std::puts("it(it2 &)"); }
   it2(const it &) { std::puts("it2(const it &)"); }
   it2(it &&) { std::puts("it2(it &&)"); }
};

std::vector<int> f1() { return {1, 2, 3}; }
const std::vector<int> f2() { return {4, 5, 6}; }
rsn::lib::slice_ref<std::vector<int>::iterator> f3() { return {}; }
rsn::lib::slice_ref<std::vector<int>::const_iterator> f4() { return {}; }
const rsn::lib::slice_ref<std::vector<int>::iterator> f5() { return {}; }
const rsn::lib::slice_ref<std::vector<int>::const_iterator> f6() { return {}; }

std::vector<int> v1;
const std::vector<int> v2;
rsn::lib::slice_ref<std::vector<int>::iterator> v3;
rsn::lib::slice_ref<std::vector<int>::const_iterator> v4;
const rsn::lib::slice_ref<std::vector<int>::iterator> v5;
const rsn::lib::slice_ref<std::vector<int>::const_iterator> v6;

auto g() { return rsn::lib::slice_ref(f6()); }

int main() {

   std::vector<long> v{1, 2, 3};
   rsn::lib::small_vec s{10, 20, 30};
   for (auto &&n: s) std::printf("%d\n", n);

   /*std::vector<int> a1{11, 12, 13, 21, 22, 33};
   const std::vector<int> a2{11, 12, 13, 21, 22, 33};
   rsn::lib::slice_ref<std::vector<int>::iterator> s1;
   rsn::lib::slice_ref<std::vector<int>::const_iterator> s2;
   const rsn::lib::slice_ref<std::vector<int>::iterator> s3 = s1;
   const rsn::lib::slice_ref<std::vector<int>::const_iterator> s4 = s1;

   //const rsn::lib::slice_ref<std::vector<int>::iterator> s1;
   rsn::lib::slice_ref<std::vector<int>::const_iterator> ss = s3;

   rsn::lib::small_vec<int> x{123, 456, 789, 1, 2, 3};
   for (auto n: rsn::lib::slice_ref(x).drop_head().drop_tail(2).reverse()) std::printf("%d\n", n);

   x.clear(100);
   x.push_back(1);
   x.push_back(2);
   x.push_back(3);
   *x.begin() = 11;
   for (auto n: rsn::lib::slice_ref(x).reverse()) std::printf("%d\n", n);

   auto pn = n::make();

   rsn::lib::small_vec<const int> y(10);
   y.push_back(1);
   //*y.begin() = 11;

   int aaa[10];
   auto sss = rsn::lib::slice_ref(aaa, aaa + 10);*/

   /*rsn::lib::slice_ref<it> s1;
   rsn::lib::slice_ref<it2> s2 = std::move(s1);
   rsn::lib::slice_ref<const char *> s3{"", ""};*/

   return {};
}
