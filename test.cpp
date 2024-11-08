#include "tiny_test.hpp"
#include "deque.h"

#include <algorithm>
#include <tuple>
#include <type_traits>
#include <vector>
#include <iterator>
#include <random>

using testing::make_test;
using testing::PrettyTest;
using testing::SimpleTest;
using testing::TestGroup;
using groups_t = std::vector<TestGroup>;


struct Fragile {
    // will throw after *durability* assignments
    Fragile(int durability, int data): durability(durability), data(data) {}
    ~Fragile() = default;
    
    // for std::swap
    Fragile(Fragile&& other): Fragile() {
        *this = other;
    }

    Fragile(const Fragile& other): Fragile() {
        *this = other;
    }

    Fragile& operator=(const Fragile& other) {
        durability = other.durability - 1;
        data = other.data;
        if (durability <= 0) {
            throw 2;
        }
        return *this;
    }

    int durability;
    int data;
private:
    Fragile() {

    }
};

struct Explosive {
    struct Safeguard {};

    inline static bool exploded = false;

    Explosive(): should_explode(true) {
        throw 1;
    }

    Explosive(Safeguard): should_explode(false) {

    }

    Explosive(const Explosive&): should_explode(true) {
        throw 2;
    }

    //TODO: is this ok..?
    Explosive& operator=(const Explosive&) {return *this;}

    ~Explosive() {
        exploded |= should_explode;
    }

private:
    const bool should_explode;
};

struct DefaultConstructible {
    DefaultConstructible() : data(default_data) { }

    int data = default_data;
    inline static const int default_data = 117;
};

struct NotDefaultConstructible {
    NotDefaultConstructible() = delete;
    NotDefaultConstructible(int input): data(input) {}
    int data;

    auto operator<=>(const NotDefaultConstructible&) const = default;
};

struct CountedException : public std::exception { };

template<int ThrowCounter>
struct Counted {
    inline static int counter = 0;

    Counted() {
        ++counter;
        if (counter == ThrowCounter) {
            --counter;
            throw CountedException();
        }
    }

    Counted(const Counted&): Counted() { }

    ~Counted() {
        --counter;
    }
};

template<typename iter, typename T>
struct CheckIter{
    using traits = std::iterator_traits<iter>;

    static_assert(std::is_same_v<std::remove_cv_t<typename traits::value_type>, std::remove_cv_t<T>>);
    static_assert(std::is_same_v<typename traits::pointer, T*>);
    static_assert(std::is_same_v<typename traits::reference, T&>);
    static_assert(std::is_same_v<typename traits::iterator_category, std::random_access_iterator_tag>);

    static_assert(std::is_same_v<decltype(std::declval<iter>()++), iter>);
    static_assert(std::is_same_v<decltype(++std::declval<iter>()), iter&>);
    static_assert(std::is_same_v<decltype(std::declval<iter>() + 5), iter>);
    static_assert(std::is_same_v<decltype(std::declval<iter>() += 5), iter&>);

    static_assert(std::is_same_v<decltype(std::declval<iter>() - std::declval<iter>()), typename traits::difference_type>);
    static_assert(std::is_same_v<decltype(*std::declval<iter>()), T&>);
    
    static_assert(std::is_same_v<decltype(std::declval<iter>() < std::declval<iter>()), bool>);
    static_assert(std::is_same_v<decltype(std::declval<iter>() <= std::declval<iter>()), bool>);
    static_assert(std::is_same_v<decltype(std::declval<iter>() > std::declval<iter>()), bool>);
    static_assert(std::is_same_v<decltype(std::declval<iter>() >= std::declval<iter>()), bool>);
    static_assert(std::is_same_v<decltype(std::declval<iter>() == std::declval<iter>()), bool>);
    static_assert(std::is_same_v<decltype(std::declval<iter>() != std::declval<iter>()), bool>);
};

TestGroup create_constructor_tests() {
    return { "constructors",
        make_test<PrettyTest>("default", [](auto& test){
            Deque<int> defaulted;
            test.check((defaulted.size() == 0));
            Deque<NotDefaultConstructible> without_default;
            test.check((without_default.size() == 0));
        }),

        make_test<PrettyTest>("copy empty deque", [](auto& test){
            Deque<NotDefaultConstructible> without_default;
            Deque<NotDefaultConstructible> copy = without_default;
            test.equals(copy.size(), size_t(0));
        }),

        make_test<PrettyTest>("copy non empty deque", [](auto& test) {
            const size_t size = 5;
            const int value = 10;
            Deque<NotDefaultConstructible> without_default(size, NotDefaultConstructible(value));
            Deque<NotDefaultConstructible> copy = without_default;
            test.equals(copy.size(), size);
            test.equals(without_default.size(), size);
            test.check(std::equal(copy.begin(), copy.end(), without_default.begin()));
        }),

        make_test<PrettyTest>("with size", [](auto& test){
            size_t size = 17;
            int value = 14;
            Deque<int> simple(size);
            test.check((simple.size() == size) && std::all_of(simple.begin(), simple.end(), [](int item){ return item == 0; }));
            Deque<NotDefaultConstructible> less_simple(size, value);
            test.check((less_simple.size() == size) && std::all_of(less_simple.begin(), less_simple.end(), [&](const auto& item){ 
                        return item.data == value; 
            }));
            Deque<DefaultConstructible> default_constructor(size);
            test.check(std::all_of(default_constructor.begin(), default_constructor.end(), [](const auto& item) { 
                        return item.data == DefaultConstructible::default_data;
            }));
        }),

        make_test<PrettyTest>("assignment", [](auto& test){
            Deque<int> first(10, 10);
            Deque<int> second(9, 9);
            first = second;
            test.check((first.size() == second.size()) && (first.size() == 9) && std::equal(first.begin(), first.end(), second.begin()));
        }),

        make_test<SimpleTest>("static asserts", []{
            using T1 = int;
            using T2 = NotDefaultConstructible;

            static_assert(std::is_default_constructible_v<Deque<T1>>, "should have default constructor");
            static_assert(std::is_default_constructible_v<Deque<T2>>, "should have default constructor");
            static_assert(std::is_copy_constructible_v<Deque<T1> >, "should have copy constructor");
            static_assert(std::is_copy_constructible_v<Deque<T2> >, "should have copy constructor");
            static_assert(std::is_constructible_v<Deque<T1>, size_t>, "should have constructor from size_t");
            static_assert(std::is_constructible_v<Deque<T2>, size_t>, "should have constructor from size_t");
            static_assert(std::is_constructible_v<Deque<T1>, size_t, const T1&>, "should have constructor from size_t and const T&");
            static_assert(std::is_constructible_v<Deque<T2>, size_t, const T2&>, "should have constructor from size_t and const T&");

            static_assert(std::is_copy_assignable_v<Deque<T1>>, "should have assignment operator");
            static_assert(std::is_copy_assignable_v<Deque<T2>>, "should have assignment operator");

            return true;       
        })
    };
}

TestGroup create_access_tests() {
    return { "access",
        make_test<PrettyTest>("operator[]", [](auto& test){
            Deque<size_t> defaulted(1300, 43);
            test.check((defaulted[0] == defaulted[1280]) && (defaulted[0] == 43));
            test.check((defaulted.at(0) == defaulted[1280]) && (defaulted.at(0) == 43));
            int caught = 0;

            try {
                defaulted.at(size_t(-1));
            } catch (std::out_of_range& e) {
                ++caught;
            }

            try {
                defaulted.at(1300);
            } catch (std::out_of_range& e) {
                ++caught;
            }

            test.equals(caught, 2);
        }),
        make_test<SimpleTest>("static asserts", []{
            Deque<size_t> defaulted;
            const Deque<size_t> constant;
            static_assert(std::is_same_v<decltype(defaulted[0]), size_t&>);
            static_assert(std::is_same_v<decltype(defaulted.at(0)), size_t&>);
            static_assert(std::is_same_v<decltype(constant[0]), const size_t&>);
            static_assert(std::is_same_v<decltype(constant.at(0)), const size_t&>);

            //static_assert(noexcept(defaulted[0]), "operator[] should not throw");
            static_assert(!noexcept(defaulted.at(0)), "at() can throw");

            return true;        
        })
    };
}

TestGroup create_iterator_tests() {
    return { "iterators",
        make_test<SimpleTest>("static asserts", []{
            CheckIter<Deque<int>::iterator, int> iter;
            std::ignore = iter;
            CheckIter<decltype(std::declval<Deque<int>>().rbegin()), int> reverse_iter;
            std::ignore = reverse_iter;
            CheckIter<decltype(std::declval<Deque<int>>().cbegin()), const int> const_iter;
            std::ignore = const_iter;

            static_assert(std::is_convertible_v<
                    decltype(std::declval<Deque<int>>().begin()), 
                    decltype(std::declval<Deque<int>>().cbegin()) 
                    >, "should be able to construct const iterator from non const iterator");
            static_assert(!std::is_convertible_v<
                    decltype(std::declval<Deque<int>>().cbegin()), 
                    decltype(std::declval<Deque<int>>().begin()) 
                    >, "should NOT be able to construct iterator from const iterator");

            return true;
        }),
        make_test<PrettyTest>("arithmetic", [](auto& test){
            Deque<int> empty;
            test.equals((empty.end() - empty.begin()), 0);
            test.equals(empty.begin() + 0, empty.end());
            test.equals(empty.end() - 0, empty.begin());
            auto iter = empty.begin();
            test.equals(iter++, empty.begin());
            Deque<int> one(1);
            auto iter2 = one.end();
            test.equals(--iter2, one.begin());

            test.equals(empty.rend() - empty.rbegin(), 0);
            test.equals(empty.rbegin() + 0, empty.rend());
            test.equals(empty.rend() - 0, empty.rbegin());
            auto r_iter = empty.rbegin();
            test.check((r_iter++ == empty.rbegin()));

            test.check((empty.cend() - empty.cbegin()) == 0);
            test.check((empty.cbegin() + 0 == empty.cend()) && (empty.cend() - 0 == empty.cbegin()));
            auto c_iter = empty.cbegin();
            test.check((c_iter++ == empty.cbegin()));

            Deque<int> d(1000, 3);
            test.equals(size_t(d.end() - d.begin()), d.size());
            test.equals(d.begin() + int(d.size()), d.end());
            test.equals(d.end() - int(d.size()), d.begin());
        }),
        make_test<PrettyTest>("comparison", [](auto& test){
            Deque<int> d(1000, 3);

            test.check(d.end() > d.begin());
            test.check(d.cend() > d.cbegin());
            test.check(d.rend() > d.rbegin());
        }),
        make_test<PrettyTest>("algos", [](auto& test){
            Deque<int> d(1000, 3);

            std::iota(d.begin(), d.end(), 13);
            std::mt19937 g(31415);
            std::shuffle(d.begin(), d.end(), g);
            std::sort(d.rbegin(), d.rbegin() + 500);
            std::reverse(d.begin(), d.end());
            auto sorted_border = std::is_sorted_until(d.begin(), d.end());
            //std::copy(d.begin(), d.end(), std::ostream_iterator<int>(std::cout, " "));
            //std::cout << std::endl;
            test.equals(sorted_border - d.begin(), 500);
        })
    };
}

TestGroup create_modification_tests() {
    return { "Modification",
        make_test<PrettyTest>("push and pop", [](auto& test){
            // insert or erase to either end of the deque 
            // should NOT invalidate any references or pointers
            // to the elements other than elements deleted.
            // HOWEVER _inserts_ may invalidate any of the existing 
            // iterators to the deque, unlike erases, that should NOT
            // invalidate any iterators except for the deleted elements
            // and, possibly, .end()
            Deque<NotDefaultConstructible> d(10000, { 1 });
            auto start_size = d.size();
            
            auto middle_iterator = d.begin() + int(start_size) / 2;
            auto middle_pointer = &(*(middle_iterator)); // 5000
            auto& middle_element = *middle_pointer;
            auto begin = &(*d.begin());

            auto inner_pointer = &(*(d.begin() + int(start_size) / 2 + 2000)); // 7000
            
            // remove 400 elements 
            for (size_t i = 0; i < 400; ++i) {
                d.pop_back();
            }
            
            // begin, middle and inner pointers are still valid
            test.equals(begin->data, 1);
            test.equals(middle_pointer->data, 1);
            test.equals(middle_element.data, 1);
            test.equals(inner_pointer->data, 1);
            // middle iterator is still valid, as only erases
            // to the back were performed
            test.equals(middle_iterator->data, 1);

            auto end = &*d.rbegin();
            
            // 800 elemets removed in total
            for (size_t i = 0; i < 400; ++i) {
                d.pop_front();
            }

            // still only erases to the ends were performed
            test.equals(end->data, 1);
            test.equals(middle_pointer->data, 1);
            test.equals(middle_element.data, 1);
            test.equals(inner_pointer->data, 1);
            test.equals(middle_iterator->data, 1);
            
            // removed 9980 items in total
            for (size_t i = 0; i < 4590; ++i) {
                d.pop_front();
                d.pop_back();
            }

            test.equals(d.size(), size_t(20));
            test.check(std::all_of(d.begin(), d.end(), [](const auto& item) { return item.data == 1; } ));

            begin = &*d.begin();
            end = &*d.rbegin();

            for (size_t i = 0; i < 5500; ++i) {
                d.push_back({2});
                d.push_front({2});
            }

            test.equals((*begin).data, 1);
            test.equals((*end).data, 1);
            test.equals(d.begin()->data, 2);
            test.equals(d.size(), size_t(5500 * 2 + 20));
            test.equals(std::count(d.begin(), d.end(), NotDefaultConstructible{1}), 20);
            test.equals(std::count(d.begin(), d.end(), NotDefaultConstructible{2}), 11000);
        }),

        make_test<PrettyTest>("insert and erase", [](auto& test){
            Deque<NotDefaultConstructible> d(10000, { 1 });
            auto start_size = d.size();
            
            d.insert(d.begin() + int(start_size) / 2, NotDefaultConstructible{2});
            test.equals(d.size(), start_size + 1);
            d.erase(d.begin() + int(start_size) / 2 - 1);
            test.equals(d.size(), start_size);

            test.equals(size_t(std::count(d.begin(), d.end(), NotDefaultConstructible{1})), start_size - 1);
            test.equals(std::count(d.begin(), d.end(), NotDefaultConstructible{2}), 1);

            Deque<NotDefaultConstructible> copy;
            std::copy(d.cbegin(), d.cend(), std::inserter(copy, copy.begin()));
            
            test.equals(d.size(), copy.size());
            test.check(std::equal(d.begin(), d.end(), copy.begin()));
        }),
        make_test<PrettyTest>("exceptions", [](auto& test) {
            try {
                Deque<Counted<17>> d(100);
            } catch (CountedException& e) {
                // should have caught same exception as thrown by Counted
                test.equals(Counted<17>::counter, 0);
            }

            try {
                Deque<Explosive> d(100);
            } catch (...) {
                
            }

            // no objects should have been created
            Deque<Explosive> d;

            test.equals(Explosive::exploded, false);

            try {
                Deque<Explosive> d;
                auto safe = Explosive(Explosive::Safeguard{});
                d.push_back(safe);
            } catch (...) {

            }

            // Destructor should not be called for an object
            // with no finihshed constructor
            // the only destructor called - safe explosive with the safeguard
            test.equals(Explosive::exploded, false);
        }),

        make_test<PrettyTest>("strong guarantee", [](auto& test) {
            // If an exception is thrown when inserting a single 
            // element at either end, insert has no effect (strong exception guarantee).
            auto check_guarantee = [&test](auto& deque, const auto& is_intact) {
                try {
                    deque.insert(deque.begin(), Fragile(0, 1));
                } catch (...) {
                    test.check(is_intact(deque));
                }
                try {
                    deque.push_front(Fragile(0, 1));
                } catch (...) {
                    test.check(is_intact(deque));
                }
                try {
                    deque.insert(deque.end(), Fragile(0, 2));
                } catch (...) {
                    test.check(is_intact(deque));
                }
                try {
                    deque.push_back(Fragile(0, 2));
                } catch (...) {
                    test.check(is_intact(deque));
                }
            };
            Deque<Fragile> empty;
            check_guarantee(empty, [](const auto& deque) {return deque.size() == 0;});

            const size_t size = 20'000;
            const size_t initial_data = 100;
            Deque<Fragile> filled(size, Fragile(size, initial_data));
            auto is_intact = [&](const auto& d) {
                return d.size() == size && std::all_of(
                    d.begin(), d.end(), 
                    [initial_data](const auto& item) {return item.data == initial_data;} 
               );
            };
            check_guarantee(filled, is_intact);
        })
    };
}


int main() {
    groups_t groups {};
    groups.push_back(create_constructor_tests());
    groups.push_back(create_access_tests());
    groups.push_back(create_iterator_tests());
    groups.push_back(create_modification_tests());

    bool res = true;
    for (auto& g : groups) {
        res &= g.run();
    }

    return res ? 0 : 1;
}

