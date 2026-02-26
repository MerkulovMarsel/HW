# Variant

Интерфейс, все свойства и гарантии должны соответствовать [std::variant](https://en.cppreference.com/w/cpp/utility/variant).

`std::visit` достаточно реализовать в виде свободной функции (метод `visit` из C++26 &mdash; по желанию). Специализацию `std::hash` писать не надо.

По аналогии с заданием `Optional`, требуется:
- по возможности сохранять тривиальности для [special member functions](https://en.cppreference.com/w/cpp/language/member_functions#Special_member_functions);
- поддерживать использование в [constant expressions](https://en.cppreference.com/w/cpp/language/constant_expression.html).

Также стоит уделить внимание правильности расстановки `noexcept`.
