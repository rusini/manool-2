
# include <cstdio>
# include <set>
# include <map>
# include <list>
# include <utility>
# include <vector>

# include "rusini.hh"

std::vector<int> f() { return {}; }

int main() {
   const std::vector<int> v;
   rsn::lib::slice_ref r = v;
   //*r.begin() = 1;

   return {};
}
