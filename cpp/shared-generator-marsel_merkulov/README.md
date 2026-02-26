# Shared Generator

## Генераторы

Представим, что нам необходимо каким-либо образом сгенерировать последовательность значений и, пройдясь по ней, выполнить какое-то действие для каждого элемента.
Наивно можно сгенерировать все значения и собрать их в список (e.g. `std::vector` в C++, `list` в Python), затем обрабатывать его значения.

К сожалению, если набор значений достаточно большой, на ненужную для обработчика материализацию будет тратиться неоправданно много вычислительного ресурса.
Было бы удобно генерировать значения on-demand.
То есть когда мы готовы обработать очередное значение, тогда его и получать.
В частности, тогда не нужно держать в памяти весь набор данных.
Более того, это позволяет генерировать бесконечные последовательности.

Это описание как раз напоминает механизм корутин: on-demand мы передаем исполнение корутине, генерирующей значение, а после того как она "отдаст" очередное значение, мы продолжим исполнение там, где значение запрашивалось.

Для этого в различных языках есть сущность "генератор".
В C++23 появился `std::generator`, предназначенный как раз для этого.

```c++
std::generator<int> producer() {
    for (int i = 42; i < 69; ++i) {
        co_yield i;
    }
}

void consumer() {
    for (int x : producer()) {
        std::cout << "consuming value: " << x << std::endl;
    }
}
```

## Shared Generator

В примере выше видно, что мы разделяем генерацию и обработку данных на сущности *producer* и *consumer*.
Но можно расширить этот подход, поддержав несколько консьюмеров &mdash; как раз это вам и предстоит сделать.

Нужно поддержать возможность *доставать* значения из генератора из разных потоков
(семантически: генерируем значения, параллельно обрабатываем их в разных потоках).

Каждый поток имеет свою **копию** генератора, при этом генераторы имеют общее состояние:

```c++
// каждая копия генератора содержит общее состояние,
// через которое достигается синхронизация
SharedGenerator g = generator();
int x, y;

std::jthread t1([g, &x] mutable {
    x = g.next();
});
std::jthread t2([g, &y] mutable {
    y = g.next();
});
```

Общее состояние описывает контекст для всех порождённых этим генератором копий.
Из этого следует, что это общее состояние должно существовать, пока не уничтожены все ссылающиеся на него генераторы.

Обратите внимание на потокобезопасность совершаемых операций: операции `next`, `copy constructor`, `destructor`, вызываемые из разных потоков, не должны приводить к неопределенному поведению.

### Интерфейс

Стандартный генератор &mdash; это [`range`](https://en.cppreference.com/w/cpp/ranges/range.html), что позволяет использовать его в range-based цикле, а также в алгоритмах стандартной библиотеки.

Тем не менее, если вы посмотрите на [предлагаемый интерфейс](src/shared-generator.h) `SharedGenerator`, вы увидите, что он не оперирует итераторами, и вместо этого предоставляет лишь метод `std::optional<T> next()`.

> **Упражение.**
> Сформулируйте требования к потенциальным итераторам для `SharedGenerator` и определите, какие операции итератора должны делать `resume` корутины.
> Основываясь на сделанных выводах, объясните предложенный дизайн API генератора.

## (Бонусная версия) Поддержка вложенных геренаторов

Генератор потенциально может вызывать другие генераторы, чтобы сгенерировать свои значения (такая операция поддержана во многих языках с генераторами, e.g. `yield from` в Python, `yield*` в JavaScript):

```python
def generator():
    for i in range(4):
        yield i

def generator_wrapper():
    yield from generator()

for i in generator_wrapper():
    process_value(i)
```

```c++
#include <generator>
#include <iostream>

std::generator<int> fib(int a = 0, int b = 1) {
    co_yield a;
    co_yield std::ranges::elements_of(fib(b, a + b));
}

int main() {
    std::ranges::copy(
        fib() | std::views::take(10),
        std::ostream_iterator<int>(std::cout, " ")
    );
}
```

Классический пример, когда такое бывает очень полезно &mdash; обход дерева:

```c++
std::generator<T> BinaryTree<T>::traverse() {
    if (this->left) {
        co_yield std::ranges::elements_of(this->left.traverse());
    }
    co_yield this->value;
    if (this->left) {
        co_yield std::ranges::elements_of(this->right.traverse());
    }
}
```

Описанное выше поведение поддерживается в `std::generator`, но для нашего генератора это пока никак не определено.
Как вы можете догадаться, это потребует соответствующей перегрузки `promise_type::yield_value`.

Можно заметить, что исполнение генераторов в таком случае имеет структуру, похожую на вызов синхронных функций.
Как вы, вероятно, знаете, подобные структуры хорошо представляются с помощью [стека](https://en.wikipedia.org/wiki/Call_stack).

### Эффективность

Рассмотрим три сущности: `Consumer` будет вызывать генератор `OuterGenerator`, который в свою очередь будет делать *yield-from* другого генератора &mdash; `InnerGenerator`.

При наивном подходе схема вызовов будет выглядеть так:

![](https://mermaid.ink/img/pako:eNp1kU9LxDAQxb9KmZNCd2lqmrY5eNkF8SBePEkvYTP9A22yzCbqWvrdTbusUtQ5zTze-03CjHCwGkFC3dv3Q6vIRS_7ykShdtac_IAUbTYR4dze3Ib-Pnr2DukBDZJyli7mtTZHzh32evFfQf85V_BHY37B19oP_E31Hv98EsTQUKdBOvIYQ1g-qHmEcSZW4FocsAIZWo218r2roDJTiB2VebV2uCbJ-qYFWav-FCZ_1MrhvlMNqeFbJTQaaWe9cSBZzrKFAnKED5BpJrYsTUWWMC54wXkM56CKfCtYkXBWlikr8myK4XNZm2x5Hiok7tKyFIUINNRd-NXT5VDLvaYvO7KLGg?type=png)

Однако, в отличие от вызова синхронных функций (и наивного решения), возврат к внешнему генератору может происходить при `co_yield` значения в самом глубоком генераторе (то есть за `O(1)`).

Тогда при первом `next()` должно случиться такое исполнение:

![](https://mermaid.ink/img/pako:eNplkT1rwzAQhv-KuakFx1i2I9kasiRQOpQunYoWEZ0_wJaCIrVNjf97JYdQQk7L-57uudPHDEejEDi0o_k-9tK65OMgdBJib_TZT2iTzSaxGOXTc9C75N07tC-o0Upn7LX4PveAvGr9gNznInIZcFTJlxw9rtTtBELHBSl0dlDAnfWYQshPMlqYYz8BrscJBfAgFbbSj06A0EvATlJ_GjPdSGt81wNv5XgOzp-UdHgYZGflfwlqhXZvvHbACWF0bQJ8hp_oi4wUBWVlXlaUsoalcAFO64ySOq9I0xSkZtslhd91ap5VLESxzcN-TXNWpoBqCJd-u779-gXLH-TXfB8?type=png)

При втором вызове &mdash; `OuterGenerator` не нужен совсем:

![](https://mermaid.ink/img/pako:eNqNksFLwzAUxv-V8E4K3Wi6pOsieNlAPIgHPUkvYXlby9KkpInbHPvfTVsmThF8p_c-vt_3QpITrK1CELDRdr-upPPkdVUaEmtpTRcadGQyuSfPwaN7QINOeutGw7U22B6N-WX7lkMc9u3N7Z_ma61HjjVqRd6lDjhQl7jSjISuze7FHzWSlHTe2R0K76TpWunQ-GSUJvta-Uqk7eHuJ0X_RUECW1criK6ACcT9jexHOPV5JfgKGyxBxFbhRgbtSyjNOWKtNG_WNhfS2bCtQGyk7uIUWiU9rmq5dbL5UuMRFLqlDcaDoHy2GFJAnOAAIuP5lGZZzlPKclYwlsAxqvl8mtMiZXSxyGgx5-cEPoa16ZTNY2VsVmQpn3HGE0BVx9t9Gl9--ADnT08EqZo?type=png)

Также не хочется сломать другие механизмы языка, в частности необходимо поддержать исключения и их отлавливание на любом этапе. Для этого придётся искать catch-блоки в каждом "фрейме", то есть прокидывание исключения, в отличие от `co_yield`, происходит за `O(k)`, где `k` &mdash; глубина вызовов.

Стоит также отдельно подумать о том, как вложенные генераторы поведут себя с учётом семантики разделённого владения корутиной, присущей `SharedGenerator`.

Тесты для бонусной версии задания не предоставляются.
Допишите необходимые тесты сами, а также укажите ограничения своего решения в описании Merge Request.
