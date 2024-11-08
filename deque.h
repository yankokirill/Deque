#ifndef PROJECT_DEQUE_H
#define PROJECT_DEQUE_H
#include <deque>
#include <memory>
#include <iterator>

namespace {
    const size_t chunk_size = 1 << 5;
    const ptrdiff_t ptr_chunk_size = 1 << 5;
}

template <typename T>
struct BaseDeque {
    struct ChunkArray {
    private:
        ChunkArray(size_t elems_count, size_t chunks_count) :  begin(new T*[chunks_count + 1]),
                                                               end(begin + chunks_count),
                                                               cur_begin(begin),
                                                               cur_end(begin + elems_count / chunk_size) {}

    public:
        T** begin;
        T** end;
        T** cur_begin;
        T** cur_end;

        ChunkArray(const ChunkArray&) = delete;
        ChunkArray& operator=(ChunkArray) = delete;

        explicit ChunkArray(size_t elems_count) : ChunkArray(elems_count, (elems_count + chunk_size - 1) / chunk_size) {
            size_t sz = (elems_count + chunk_size - 1) / chunk_size;
            size_t ind = 0;
            try {
                for (; ind < sz; ++ind) {
                    begin[ind] = reinterpret_cast<T*>(new int8_t[chunk_size * sizeof(T)]);
                }
                ++ind;
                *end = reinterpret_cast<T*>(new int8_t[sizeof(T)]);
            } catch (...) {
                for (size_t j = 0; j < ind; ++j) {
                    delete[] reinterpret_cast<int8_t*>(begin[j]);
                }
                delete[] begin;
                throw;
            }
        }

        void swap(ChunkArray& tmp) {
            std::swap(begin, tmp.begin);
            std::swap(end, tmp.end);
            std::swap(cur_begin, tmp.cur_begin);
            std::swap(cur_end, tmp.cur_end);
        }

        void shift() {
            //NOLINTNEXTLINE(readability-magic-numbers)
            T** new_begin = begin + (end - begin) / 3;
            T** it = new_begin;
            for (; cur_begin < cur_end; ++it) {
                std::swap(*cur_begin, *it);
                ++cur_begin;
            }
            if (cur_end != end) {
                std::swap(*cur_end, *it);
            }
            cur_begin = new_begin;
            cur_end = it;
        }

        void reallocate() {
            size_t old_size = static_cast<size_t>(end - begin) + 1;
            //NOLINTNEXTLINE(readability-magic-numbers)
            T** new_arr = new T*[3 * old_size + 1]{};
            std::copy(begin, end, new_arr + old_size);
            //NOLINTNEXTLINE(readability-magic-numbers)
            new_arr[3 * old_size] = *end;
            delete[] begin;

            auto diff = cur_end - cur_begin;
            cur_begin = new_arr + old_size + (cur_begin - begin);
            cur_end = cur_begin + diff;

            begin = new_arr;
            //NOLINTNEXTLINE(readability-magic-numbers)
            end = begin + 3 * old_size;
        }

        void update() {
            //NOLINTNEXTLINE(readability-magic-numbers)
            if (3 * (cur_end - cur_begin + 1) < end - begin + 1) {
                shift();
            } else {
                reallocate();
            }
        }

        ~ChunkArray() {
            for (auto it = begin; it <= end; ++it) {
                delete[] reinterpret_cast<int8_t*>(*it);
            }
            delete[] begin;
        }
    };

    ChunkArray arr;
    size_t m_size;
    T* m_begin;
    T* m_end;

    explicit BaseDeque(size_t n) : arr(n),
                                   m_size(n),
                                   m_begin(*arr.cur_begin),
                                   m_end(arr.cur_begin[n / chunk_size] + n % chunk_size) {}
};

template <typename T>
class Deque : BaseDeque<T> {
public:
    using value_type = T;

private:
    using BaseDeque<T>::arr;
    using BaseDeque<T>::m_begin;
    using BaseDeque<T>::m_end;
    using BaseDeque<T>::m_size;

    void next_end() {
        ++m_end;
        ++m_size;
        if (m_end == *arr.cur_end + chunk_size) {
            ++arr.cur_end;
            if (!*(arr.cur_end)) {
                *(arr.cur_end) = reinterpret_cast<T*>(new int8_t[chunk_size * sizeof(T)]);
            }
            m_end = *arr.cur_end;
        }
    }

    void update() {
        arr.update();
        if (m_end == *arr.end) {
            if (!*arr.cur_end) {
                *arr.cur_end = reinterpret_cast<T*>(new int8_t[chunk_size * sizeof(T)]);
            }
            m_end = *arr.cur_end;
            if (m_begin == *arr.end) {
                m_begin = *arr.cur_end;
            }
        }
    }

public:
    Deque() : BaseDeque<T>(0) {}

    explicit Deque(size_t n) : BaseDeque<T>(n) {
        static_assert(std::is_default_constructible<T>::value);
        auto it = begin();
        try {
            for (it = begin(); it != end(); ++it) {
                new(&*it) T();
            }
        } catch (...) {
            for (--it; it >= begin(); --it) {
                it->~T();
            }
            throw;
        }
    }

    Deque(const Deque& copy) : BaseDeque<T>(copy.size()) {
        std::uninitialized_copy(copy.begin(), copy.end(), begin());
    }

    Deque(size_t n, const T& val) : BaseDeque<T>(n) {
        std::uninitialized_fill(begin(), end(), val);
    }

    void swap(Deque& tmp) {
        arr.swap(tmp.arr);
        std::swap(m_begin, tmp.m_begin);
        std::swap(m_end, tmp.m_end);
        std::swap(m_size, tmp.m_size);
    }

    Deque<T>& operator=(Deque<T> copy) {
        swap(copy);
        return *this;
    }

    size_t size() const {
        return m_size;
    }
    bool empty() const {
        return m_size == 0;
    }

    void push_front(const T& val) {
        if (m_begin != *arr.cur_begin) {
            auto ptr = m_begin - 1;
            new(ptr) T(val);
            --m_begin;
            ++m_size;
            return;
        }

        if (arr.cur_begin == arr.begin) {
            update();
        }

        if (!*(arr.cur_begin - 1)) {
            *(arr.cur_begin - 1) = reinterpret_cast<T*>(new int8_t[chunk_size * sizeof(T)]);
        }

        new(*(arr.cur_begin - 1) + chunk_size - 1) T(val);
        --arr.cur_begin;
        m_begin = *arr.cur_begin + chunk_size - 1;
        ++m_size;
    }

    void push_back(const T& val) {
        if (arr.cur_end == arr.end) {
            update();
        }

        new(m_end) T(val);
        next_end();
    }

    void pop_front() {
        m_begin->~T();
        ++m_begin;
        --m_size;
        if (m_begin == *arr.cur_begin + chunk_size) {
            ++arr.cur_begin;
            m_begin = *arr.cur_begin;
        }
    }

    void pop_back() {
        if (m_end == *arr.cur_end) {
            --arr.cur_end;
            m_end = *arr.cur_end + chunk_size;
        }
        --m_end;
        --m_size;
        m_end->~T();
    }

    T& operator[](size_t ind) {
        ind += static_cast<size_t>(m_begin - *arr.cur_begin);
        return arr.cur_begin[ind / chunk_size][ind % chunk_size];
    }

    const T& operator[](size_t ind) const {
        ind += static_cast<size_t>(m_begin - *arr.cur_begin);
        return arr.cur_begin[ind / chunk_size][ind % chunk_size];
    }

    T& at(size_t ind) {
        ind += static_cast<size_t>(m_begin - *arr.cur_begin);
        auto it = arr.cur_begin + ind / chunk_size;
        if (it < arr.end - 1 || (it == arr.end - 1 && *it + ind % chunk_size < m_end)) {
            return arr.cur_begin[ind / chunk_size][ind % chunk_size];
        }
        throw std::out_of_range("Deque index out of range");
    }

    const T& at(size_t ind) const {
        ind += static_cast<size_t>(m_begin - *arr.cur_begin);
        auto it = arr.cur_begin + ind / chunk_size;
        if (it < arr.end - 1 || (it == arr.end - 1 && *it + ind % chunk_size < m_end)) {
            return arr.cur_begin[ind / chunk_size][ind % chunk_size];
        }
        throw std::out_of_range("Deque index out of range");
    }

    T& front() {
        return *m_begin;
    }

    const T& front() const {
        return *m_begin;
    }

    T& back() {
        return *rbegin();
    }

    const T& back() const {
        return *crbegin();
    }


    template <typename Value>
    struct BaseIterator {
    public:
        using difference_type = ptrdiff_t;
        using value_type = Value;
        using pointer = value_type*;
        using reference = value_type&;
        using const_reference = const value_type&;
        using iterator_category = std::random_access_iterator_tag;

    private:
        pointer item;
        T** cur_arr;

        T** first;
        T** last;

    public:
        BaseIterator(pointer item, T** cur_arr, T** first, T** last) : item(item),
                                                                       cur_arr(cur_arr),
                                                                       first(first),
                                                                       last(last) {}

        operator BaseIterator<const Value>() const {
            return BaseIterator<const Value>(item, cur_arr, first, last);
        }

        reference operator*() const {
            return *item;
        }

        pointer operator->() const {
            return item;
        }

        BaseIterator& operator++() {
            ++item;
            if (cur_arr < last && item == *cur_arr + ptr_chunk_size) {
                ++cur_arr;
                item = *cur_arr;
            }
            return *this;
        }

        BaseIterator operator++(int) {
            auto copy = *this;
            ++*this;
            return copy;
        }

        BaseIterator& operator--() {
            if (cur_arr > first && item == *cur_arr) {
                --cur_arr;
                item = *cur_arr + ptr_chunk_size;
            }
            --item;
            return *this;
        }

        BaseIterator operator--(int) {
            auto copy = *this;
            --*this;
            return copy;
        }

    private:
        template <size_t N>
        void sum(difference_type diff, T** border) {
            diff += item - (*cur_arr + N);
            cur_arr += diff / ptr_chunk_size;
            if (first <= cur_arr && cur_arr <= last) {
                item = (*cur_arr + N) + diff % ptr_chunk_size;
            } else {
                item = *border + diff % ptr_chunk_size + (cur_arr - border) * ptr_chunk_size;
                cur_arr = border;
            }
        }

    public:
        BaseIterator& operator+=(difference_type diff) {
            if (diff >= 0) {
                sum<0>(diff, last);
            } else {
                sum<chunk_size - 1>(diff, first);
            }
            return *this;
        }

        BaseIterator& operator-=(difference_type diff) {
            return *this += -diff;
        }

        BaseIterator operator+(difference_type diff) const {
            BaseIterator copy = *this;
            copy += diff;
            return copy;
        }

        BaseIterator operator-(difference_type diff) const {
            return *this + (-diff);
        }

        difference_type operator-(const BaseIterator& it) const {
            return ptr_chunk_size * (cur_arr - it.cur_arr) + (item - *cur_arr) - (it.item - *it.cur_arr);
        }

        reference operator[](difference_type diff) {
            return *(*this + diff);
        }
        const_reference operator[](difference_type diff) const {
            return *(*this + diff);
        }

        bool operator==(const BaseIterator& other) const {
            return item == other.item;
        }
        bool operator!=(const BaseIterator& other) const {
            return item != other.item;
        }

        auto operator<=>(const BaseIterator& other) const {
            return cur_arr == other.cur_arr ? item <=> other.item : cur_arr <=> other.cur_arr;
        }
    };

    using iterator = BaseIterator<T>;
    using const_iterator = BaseIterator<const T>;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    iterator begin() {
        return iterator(m_begin, arr.cur_begin, arr.cur_begin, arr.cur_end);
    }
    iterator end() {
        return iterator(m_end, arr.cur_end, arr.cur_begin, arr.cur_end);
    }

    const_iterator begin() const {
        return iterator(m_begin, arr.cur_begin, arr.cur_begin, arr.cur_end);
    }
    const_iterator end() const {
        return iterator(m_end, arr.cur_end, arr.cur_begin, arr.cur_end);
    }

    const_iterator cbegin() const {
        return const_iterator(m_begin, arr.cur_begin, arr.cur_begin, arr.cur_end);
    }
    const_iterator cend() const {
        return const_iterator(m_end, arr.cur_end, arr.cur_begin, arr.cur_end);
    }

    reverse_iterator rbegin() {
        return reverse_iterator(end());
    }
    reverse_iterator rend() {
        return reverse_iterator(begin());
    }

    const_reverse_iterator rbegin() const {
        return const_reverse_iterator(end());
    }
    const_reverse_iterator rend() const {
        return const_reverse_iterator(begin());
    }

    const_reverse_iterator crbegin() const {
        return const_reverse_iterator(cend());
    }
    const_reverse_iterator crend() const {
        return const_reverse_iterator(cbegin());
    }


    iterator insert(iterator iter, const T& val) {
        if (iter == begin()) {
            push_front(val);
            return begin();
        }
        if (iter == end()) {
            push_back(val);
            return --end();
        }

        auto ind = iter - begin();
        if (arr.cur_end == arr.end) {
            update();
        }

        new(m_end) T(back());

        auto ans = begin() + ind;
        for (auto it = --end(); it != ans; --it) {
            *it = *(it - 1);
        }
        *ans = val;
        next_end();
        return ans;
    }

    iterator erase(iterator iter) {
        if (iter == begin()) {
            pop_front();
            return begin();
        }
        for (auto it = iter; it + 1 != end(); ++it) {
            *it = *(it + 1);
        }
        pop_back();
        return iter;
    }

    ~Deque() {
        std::destroy(begin(), end());
    }
};


#endif //PROJECT_DEQUE_H
