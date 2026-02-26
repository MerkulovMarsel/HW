# AnyIterator

Ваша задача написать `AnyIterator`, поддерживающий `std::forward_iterator_tag`, `std::bidirectional_iterator_tag` и `std::random_access_iterator_tag` в качестве параметра.

Это type-erasure обёртка над итератором.
Интерфейс, который нужно реализовать, находится в [any-iterator.h](src/any-iterator.h).

При написании этого задания необходимо применить small-object оптимизацию (SOO).
