#pragma once

#include <functional>

namespace ct {
template <typename... Ts>
class Variant;

namespace detail {
template <typename R, typename Visitor, typename SpecialVariant>
constexpr R special_index_visit(Visitor&&, SpecialVariant&&);
} // namespace detail

template <typename... Types>
constexpr void swap(Variant<Types...>& lhs, Variant<Types...>& rhs) noexcept(noexcept(lhs.swap(rhs))) {
  lhs.swap(rhs);
}

template <typename T>
static constexpr size_t variant_size = variant_npos;

template <typename... Ts>
static constexpr size_t variant_size<Variant<Ts...>> = sizeof...(Ts);

template <typename... Ts>
static constexpr size_t variant_size<const Variant<Ts...>> = sizeof...(Ts);

template <size_t N, typename T>
struct variant_alternative_impl;

template <size_t N, typename... Ts>
struct variant_alternative_impl<N, Variant<Ts...>> {
  using type = detail::get_i_t<N, Ts...>;
};

template <size_t N, typename... Ts>
struct variant_alternative_impl<N, const Variant<Ts...>> {
  using type = const detail::get_i_t<N, Ts...>;
};

template <size_t N, typename T>
using variant_alternative = typename variant_alternative_impl<N, std::remove_reference_t<T>>::type;

namespace detail {

template <size_t N, typename Variant>
constexpr decltype(auto) get_impl(Variant&& variant) {
  if (N != variant.index() || variant.valueless_by_exception()) {
    throw BadVariantAccess();
  }
  return variant.storage.template get<N>();
}

template <size_t N, typename Variant>
constexpr decltype(auto) get_if_impl(Variant&& variant) {
  if (N == variant.index() && !variant.valueless_by_exception()) {
    return std::addressof(get<N>(variant));
  }
  return static_cast<decltype(std::addressof(get<N>(variant)))>(nullptr);
}

} // namespace detail

template <size_t N, typename... Types>
constexpr variant_alternative<N, Variant<Types...>>& get(Variant<Types...>& variant) {
  return detail::get_impl<N>(variant);
}

template <size_t N, typename... Types>
constexpr variant_alternative<N, Variant<Types...>>&& get(Variant<Types...>&& variant) {
  return std::move(get<N>(variant));
}

template <size_t N, typename... Types>
constexpr const variant_alternative<N, Variant<Types...>>& get(const Variant<Types...>& variant) {
  return detail::get_impl<N>(variant);
}

template <size_t N, typename... Types>
constexpr const variant_alternative<N, Variant<Types...>>&& get(const Variant<Types...>&& variant) {
  return std::move(get<N>(variant));
}

template <typename T, typename... Types>
  requires (!detail::has_duplicate<T, Types...>)
constexpr T& get(Variant<Types...>& variant) {
  return get<detail::get_index<T, Types...>>(variant);
}

template <typename T, typename... Types>
  requires (!detail::has_duplicate<T, Types...>)
constexpr T&& get(Variant<Types...>&& variant) {
  return std::move(get<detail::get_index<T, Types...>>(variant));
}

template <typename T, typename... Types>
  requires (!detail::has_duplicate<T, Types...>)
constexpr const T& get(const Variant<Types...>& variant) {
  return get<detail::get_index<T, Types...>>(variant);
}

template <typename T, typename... Types>
  requires (!detail::has_duplicate<T, Types...>)
constexpr const T&& get(const Variant<Types...>&& variant) {
  return std::move(get<detail::get_index<T, Types...>>(variant));
}

template <size_t N, typename... Types>
constexpr std::add_pointer_t<variant_alternative<N, Variant<Types...>>>
get_if(Variant<Types...>* variant_ptr) noexcept {
  return variant_ptr ? detail::get_if_impl<N>(*variant_ptr) : nullptr;
}

template <size_t N, class... Types>
constexpr std::add_pointer_t<const variant_alternative<N, Variant<Types...>>>
get_if(const Variant<Types...>* variant_ptr) noexcept {
  return variant_ptr ? detail::get_if_impl<N>(*variant_ptr) : nullptr;
}

template <typename T, class... Types>
  requires (!detail::has_duplicate<T, Types...>)
constexpr std::add_pointer_t<T> get_if(Variant<Types...>* variant_ptr) noexcept {
  return variant_ptr ? detail::get_if_impl<detail::get_index<T, Types...>>(*variant_ptr) : nullptr;
}

template <typename T, class... Types>
  requires (!detail::has_duplicate<T, Types...>)
constexpr std::add_pointer_t<const T> get_if(const Variant<Types...>* variant_ptr) noexcept {
  return variant_ptr ? detail::get_if_impl<detail::get_index<T, Types...>>(*variant_ptr) : nullptr;
}

template <typename T, typename... Types>
constexpr bool holds_alternative(const Variant<Types...>& variant) noexcept {
  return detail::is_in<T, Types...> && detail::get_index<T, Types...> == variant.index();
}

namespace detail {

template <typename... Types>
constexpr Variant<Types...>&& implicit_to_variant(Variant<Types...>&& v) {
  return v;
}

template <typename... Types>
constexpr Variant<Types...>& implicit_to_variant(Variant<Types...>& v) {
  return v;
}

template <typename... Types>
constexpr const Variant<Types...>& implicit_to_variant(const Variant<Types...>& v) {
  return v;
}

template <typename... Types>
constexpr const Variant<Types...>&& implicit_to_variant(const Variant<Types...>&& v) {
  return v;
}

template <typename R, typename Visitor, typename... Variants>
struct FuncTable {
  using Indexes = std::array<size_t, sizeof...(Variants)>;

  using F = R (*)(Visitor&&, Variants&&...);

  template <typename F, size_t... Sizes>
  struct TableImpl;

  template <typename F>
  struct TableImpl<F> {
    F func;

    constexpr F get(const Indexes&, size_t) const {
      return func;
    }

    template <size_t... Indexes>
    constexpr void set() {
      func = [](Visitor&& visitor, Variants&&... variants) -> R {
        return std::invoke(std::forward<Visitor>(visitor), ct::get<Indexes>(std::forward<Variants>(variants))...);
      };
    }
  };

  template <typename F, size_t FirstSize, size_t... RestSizes>
  struct TableImpl<F, FirstSize, RestSizes...> {
    std::array<TableImpl<F, RestSizes...>, FirstSize> sub_tables;

    constexpr F get(const Indexes& indexes, size_t n) const {
      return sub_tables[indexes[n]].get(indexes, n + 1);
    }

    template <size_t... CurrentIndexes>
    constexpr void set() {
      [this]<size_t... I>(std::index_sequence<I...>) {
        ((sub_tables[I].template set<CurrentIndexes..., I>()), ...);
      }(std::make_index_sequence<FirstSize>{});
    }
  };

  constexpr FuncTable() {
    table.template set<>();
  }

  constexpr F get(const Indexes& indices) const {
    return table.get(indices, 0);
  }

  using TableType = TableImpl<F, variant_size<std::remove_reference_t<Variants>>...>;
  TableType table;
};

template <typename R, typename Visitor, typename Variant>
struct SpecialIndexTable {
  static constexpr size_t size = variant_size<std::remove_reference_t<Variant>>;
  using F = R (*)(Visitor&&, Variant&&);
  using Table = std::array<F, size>;

  constexpr SpecialIndexTable() {
    [this]<size_t... I>(std::index_sequence<I...>) {
      (set<I>(table), ...);
    }(std::make_index_sequence<size>{});
  }

  template <size_t I>
  static constexpr void set(Table& table) {
    table[I] = [](Visitor&& visitor, Variant&& variant) -> R {
      return std::forward<Visitor>(visitor).template operator()<I>(get<I>(std::forward<Variant>(variant)));
    };
  }

  Table table;
};

template <typename R, typename Visitor, typename Variant>
constexpr R special_index_visit(Visitor&& visitor, Variant&& variant) {
  using TableType = SpecialIndexTable<R, Visitor&&, Variant&&>;
  constexpr TableType table;

  auto func = table.table[variant.index()];
  return func(std::forward<Visitor>(visitor), std::forward<Variant>(variant));
}
} // namespace detail

template <typename R, typename Visitor, typename... Variants>
constexpr R visit_impl(Visitor&& visitor, Variants&&... variants) {
  using TableType = detail::FuncTable<R, Visitor, Variants...>;
  constexpr TableType table;

  auto func = table.get({variants.index()...});
  return func(std::forward<Visitor>(visitor), std::forward<Variants>(variants)...);
}

template <typename Visitor, typename... Variants>
using visit_result_t =
    std::invoke_result_t<Visitor, decltype(get<0>(detail::implicit_to_variant(std::declval<Variants>())))...>;

template <typename R, typename Visitor, typename... Variants>
constexpr R visit(Visitor&& visitor, Variants&&... variants) {
  if ((variants.valueless_by_exception() || ...)) {
    throw BadVariantAccess();
  }
  if constexpr (sizeof...(Variants) == 0) {
    return std::forward<Visitor>(visitor)();
  } else {
    return visit_impl<R>(
        std::forward<Visitor>(visitor),
        detail::implicit_to_variant(std::forward<Variants>(variants))...
    );
  }
}

template <typename Visitor, typename... Variants>
constexpr decltype(auto) visit(Visitor&& visitor, Variants&&... variants) {
  if ((variants.valueless_by_exception() || ...)) {
    throw BadVariantAccess();
  }
  if constexpr (sizeof...(Variants) == 0) {
    return std::forward<Visitor>(visitor)();
  } else {
    using R = visit_result_t<Visitor, Variants...>;
    return visit_impl<R>(
        std::forward<Visitor>(visitor),
        detail::implicit_to_variant(std::forward<Variants>(variants))...
    );
  }
}

namespace detail {
enum class OpFlags : uint32_t {
  BothValueless = 1 << 0,
  RhsValueless = 1 << 1,
  LhsValueless = 1 << 2,
  RhsIndexBigger = 1 << 3,
  LhsIndexBigger = 1 << 4,
};

constexpr uint32_t to_u(OpFlags flag) {
  return static_cast<uint32_t>(flag);
}

enum class Op {
  Equal = to_u(OpFlags::BothValueless),
  NotEqual = to_u(OpFlags::RhsValueless) | to_u(OpFlags::LhsValueless) | to_u(OpFlags::LhsIndexBigger) |
             to_u(OpFlags::RhsIndexBigger),
  Less = to_u(OpFlags::LhsValueless) | to_u(OpFlags::RhsIndexBigger),
  EqualLess = Equal | Less,
  Greater = to_u(OpFlags::RhsValueless) | to_u(OpFlags::LhsIndexBigger),
  EqualGreater = Equal | Greater
};

constexpr uint32_t to_u(Op op) {
  return static_cast<uint32_t>(op);
}

template <Op op>
constexpr bool both_valueless() {
  return to_u(op) & to_u(OpFlags::BothValueless);
}

template <Op op>
constexpr bool one_valueless(bool is_lhs) {
  return to_u(op) & (is_lhs ? to_u(OpFlags::LhsValueless) : to_u(OpFlags::RhsValueless));
}

template <Op op>
constexpr bool different_indexes(bool is_lhs_bigger) {
  return to_u(op) & (is_lhs_bigger ? to_u(OpFlags::LhsIndexBigger) : to_u(OpFlags::RhsIndexBigger));
}

template <Op op, typename T>
constexpr bool cmp(const T& lhs, const T& rhs) {
  switch (op) {
  case Op::Equal:
    return lhs == rhs;
  case Op::NotEqual:
    return lhs != rhs;
  case Op::Less:
    return lhs < rhs;
  case Op::EqualLess:
    return lhs <= rhs;
  case Op::Greater:
    return lhs > rhs;
  case Op::EqualGreater:
    return lhs >= rhs;
  }
  std::unreachable();
}

template <Op op, typename... Types>
constexpr bool cmp_impl(const Variant<Types...>& lhs, const Variant<Types...>& rhs) {
  if (lhs.valueless_by_exception() || rhs.valueless_by_exception()) {
    if (lhs.valueless_by_exception() && rhs.valueless_by_exception()) {
      return both_valueless<op>();
    }
    return one_valueless<op>(lhs.valueless_by_exception());
  }
  if (lhs.index() != rhs.index()) {
    return different_indexes<op>(lhs.index() > rhs.index());
  }
  return ct::visit<bool>(
      []<typename TL, typename TR>(TL&& l, TR&& r) {
        if constexpr (std::is_same_v<TL, TR>) {
          return cmp<op, TL>(l, r);
        } else {
          return false;
        }
      },
      lhs,
      rhs
  );
}
} // namespace detail

template <typename... Types>
constexpr bool operator==(const Variant<Types...>& lhs, const Variant<Types...>& rhs) {
  return detail::cmp_impl<detail::Op::Equal>(lhs, rhs);
}

template <typename... Types>
constexpr bool operator!=(const Variant<Types...>& lhs, const Variant<Types...>& rhs) {
  return detail::cmp_impl<detail::Op::NotEqual>(lhs, rhs);
}

template <typename... Types>
constexpr bool operator<(const Variant<Types...>& lhs, const Variant<Types...>& rhs) {
  return detail::cmp_impl<detail::Op::Less>(lhs, rhs);
}

template <typename... Types>
constexpr bool operator>(const Variant<Types...>& lhs, const Variant<Types...>& rhs) {
  return detail::cmp_impl<detail::Op::Greater>(lhs, rhs);
}

template <typename... Types>
constexpr bool operator<=(const Variant<Types...>& lhs, const Variant<Types...>& rhs) {
  return detail::cmp_impl<detail::Op::EqualLess>(lhs, rhs);
}

template <typename... Types>
constexpr bool operator>=(const Variant<Types...>& lhs, const Variant<Types...>& rhs) {
  return detail::cmp_impl<detail::Op::EqualGreater>(lhs, rhs);
}

template <typename... Types>
constexpr std::common_comparison_category_t<std::compare_three_way_result_t<Types>...>
operator<=>(const Variant<Types...>& lhs, const Variant<Types...>& rhs) {
  if (lhs.valueless_by_exception() || rhs.valueless_by_exception()) {
    if (lhs.valueless_by_exception() && rhs.valueless_by_exception()) {
      return std::strong_ordering::equal;
    }
    return lhs.valueless_by_exception() ? std::strong_ordering::less : std::strong_ordering::greater;
  }

  if (lhs.index() != rhs.index()) {
    return lhs.index() <=> rhs.index();
  }
  return ct::visit<std::common_comparison_category_t<std::compare_three_way_result_t<Types>...>>(
      []<typename TL, typename TR>(TL&& l, TR&& r) {
        if constexpr (std::is_same_v<TL, TR>) {
          return l <=> r;
        } else {
          return std::strong_ordering::equal;
        }
      },
      lhs,
      rhs
  );
}

} // namespace ct
