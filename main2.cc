
# include <cstdio>
# include <set>
# include <map>
# include <list>
# include <utility>
# include <vector>

# include "rusini.hh"

int main() {
   rsn::lib::small_vec a{1, 2, 3, 4, 5};
   rsn::lib::small_vec b{10, 20, 30, 40, 50};
   {  auto t = std::move(a);
      a = std::move(b);
      b = std::move(t);
   }
   for (auto &&n: a) std::printf("%d\n", n);
   for (auto &&n: b) std::printf("%d\n", n);

   return {};
}
