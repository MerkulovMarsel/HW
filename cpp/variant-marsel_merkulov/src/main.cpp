#include "variant.h"

#include <numbers>

// #define F
#ifdef F

struct ConstexprNonTrivialDestructor {
  constexpr ~ConstexprNonTrivialDestructor() {}

  int x;
};

// static_assert([] {
//   using A = ConstexprNonTrivialDestructor;
//   using V = ct::Variant<int, const int, const A, A>;
//
//   {
//     V a1(ct::in_place_index<1>, 42);
//     V b1 = a1;
//     if (b1.index() != a1.index()) {
//       return false;
//     }
//   }
//
//   {
//     V a2(ct::in_place_index<2>);
//     V b2 = a2;
//     if (b2.index() != a2.index()) {
//       return false;
//     }
//   }
//
//   return true;
// }());

int main() {
  using A = ConstexprNonTrivialDestructor;
  using V = ct::Variant<int, const int, const A, A>;
  constexpr V a2(ct::in_place_index<2>);
  constexpr V b2 = a2;
}
#endif
