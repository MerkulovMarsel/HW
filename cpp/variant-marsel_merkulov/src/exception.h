#pragma once
#include <exception>

namespace ct {
class BadVariantAccess : public std::exception {
  constexpr const char* what() const noexcept override {
    return "bad_variant_access";
  }
};
} // namespace ct
