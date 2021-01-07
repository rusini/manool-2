
# include <cstdio>
# include <set>
# include <utility>
# include <vector>

# include "rusini.hh"

struct it {
   it() = default;
   it(const it &) { std::puts("it(const it &)"); }
   it(it &&) { std::puts("it(it &)"); }
};

struct it2 {
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

void ff(rsn::lib::slice_ref<std::vector<int>::iterator>) {}

int main() {
   std::vector<int> a1{11, 12, 13, 21, 22, 33};
   const std::vector<int> a2{11, 12, 13, 21, 22, 33};
   rsn::lib::slice_ref<std::vector<int>::iterator> s1;
   rsn::lib::slice_ref<std::vector<int>::const_iterator> s2;
   const rsn::lib::slice_ref<std::vector<int>::iterator> s3 = s1;
   const rsn::lib::slice_ref<std::vector<int>::const_iterator> s4 = s1;

   //const rsn::lib::slice_ref<std::vector<int>::iterator> s1;
   rsn::lib::slice_ref<std::vector<int>::const_iterator> ss = s3;

   int x[]{123, 456, 789, 1, 2, 3};
   //std::set<int> x{123, 456, 789, 1, 2, 3};
   for (auto n: rsn::lib::make_slice_ref(x).drop_head().drop_tail(2).reverse()) std::printf("%d\n", n);

   return {};
}
