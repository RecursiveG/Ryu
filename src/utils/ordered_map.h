#pragma once

#include <algorithm>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ryu {
template <typename K, typename V>
class ordered_map_iterator;
template <typename K, typename V>
class ordered_map_const_iterator;

template <typename K, typename V>
class ordered_map {
  public:
    using iterator = ordered_map_iterator<K, V>;
    using const_iterator = ordered_map_const_iterator<K, V>;

    void insert(K key, V val);
    bool contains(const K& key) const;
    std::optional<V> erase(const K& key);
    const V& operator[](const K& key) const;
    V& operator[](const K& key);
    std::pair<const K, V>& at(size_t position);
    size_t size() const;
    bool empty() const;
    void clear();

    iterator begin();
    iterator end();

    const_iterator begin() const;
    const_iterator end() const;

  private:
    std::unordered_map<K, V> map_;
    std::vector<K> order_;
};

template <typename K, typename V>
class ordered_map_iterator {
  public:
    ordered_map_iterator(std::unordered_map<K, V>& map, const std::vector<K>& order, size_t size,
                         size_t position = 0);

    bool operator!=(const ordered_map_iterator<K, V>& another) const;
    ordered_map_iterator<K, V>& operator++();
    std::pair<const K, V>& operator*() const;

  private:
    std::unordered_map<K, V>& map_;
    const std::vector<K>& order_;
    size_t size_;
    size_t position_;
};

template <typename K, typename V>
class ordered_map_const_iterator {
  public:
    ordered_map_const_iterator(const std::unordered_map<K, V>& map, const std::vector<K>& order, size_t size,
                         size_t position = 0);

    bool operator!=(const ordered_map_const_iterator<K, V>& another) const;
    ordered_map_const_iterator<K, V>& operator++();
    const std::pair<const K, V>& operator*() const;

  private:
    const std::unordered_map<K, V>& map_;
    const std::vector<K>& order_;
    size_t size_;
    size_t position_;
};

//
//
//
template <typename K, typename V>
void ordered_map<K, V>::insert(K key, V val) {
    if (!contains(key)) {
        map_.insert(std::make_pair(key, std::move(val)));
        order_.push_back(key);
    } else {
        map_[key] = std::move(val);
    }
}

template <typename K, typename V>
bool ordered_map<K, V>::contains(const K& key) const {
    return map_.find(key) != map_.end();
}

template <typename K, typename V>
std::optional<V> ordered_map<K, V>::erase(const K& key) {
    auto iter = map_.find(key);
    if (iter == map_.end()) return {};
    V val = std::move(iter->second);
    map_.erase(iter);
    order_.erase(std::find(order_.begin(), order_.end(), key));
    return val;
}

template <typename K, typename V>
const V& ordered_map<K, V>::operator[](const K& key) const {
    auto iter = map_.find(key);
    if (iter == map_.end()) {
        throw std::out_of_range("key is not in this ordered_map");
    }
    return iter->second;
}

template <typename K, typename V>
V& ordered_map<K, V>::operator[](const K& key) {
    auto iter = map_.find(key);
    if (iter == map_.end()) {
        throw std::out_of_range("key is not in this ordered_map");
    }
    return iter->second;
}

template <typename K, typename V>
std::pair<const K, V>& ordered_map<K, V>::at(size_t position) {
    if (position >= order_.size()) {
        throw std::out_of_range("position out of range in ordered_map");
    }
    return *map_.find(order_[position]);
}

template <typename K, typename V>
size_t ordered_map<K, V>::size() const {
    return map_.size();
}

template <typename K, typename V>
bool ordered_map<K, V>::empty() const {
    return map_.empty();
}

template <typename K, typename V>
void ordered_map<K, V>::clear() {
    map_.clear();
    order_.clear();
}

template <typename K, typename V>
typename ordered_map<K, V>::iterator ordered_map<K, V>::begin() {
    return ordered_map_iterator(map_, order_, order_.size(), 0);
}

template <typename K, typename V>
typename ordered_map<K, V>::iterator ordered_map<K, V>::end() {
    return ordered_map_iterator(map_, order_, order_.size(), order_.size());
}

template <typename K, typename V>
typename ordered_map<K, V>::const_iterator ordered_map<K, V>::begin() const {
    return ordered_map_const_iterator(map_, order_, order_.size(), 0);
}

template <typename K, typename V>
typename ordered_map<K, V>::const_iterator ordered_map<K, V>::end() const {
    return ordered_map_const_iterator(map_, order_, order_.size(), order_.size());
}

//
//
//
template <typename K, typename V>
ordered_map_iterator<K, V>::ordered_map_iterator(std::unordered_map<K, V>& map,
                                                 const std::vector<K>& order, size_t size,
                                                 size_t position)
    : map_(map), order_(order), size_(size), position_(position) {}

template <typename K, typename V>
bool ordered_map_iterator<K, V>::operator!=(const ordered_map_iterator<K, V>& another) const {
    return &map_ != &another.map_ || &order_ != &another.order_ || size_ != another.size_ ||
           position_ != another.position_;
}

template <typename K, typename V>
ordered_map_iterator<K, V>& ordered_map_iterator<K, V>::operator++() {
    if (++position_ > size_) position_ = size_;
    return *this;
}

template <typename K, typename V>
std::pair<const K, V>& ordered_map_iterator<K, V>::operator*() const {
    if (position_ >= size_) {
        throw std::out_of_range("ordered_map_iterator reached end");
    }
    return *map_.find(order_[position_]);
}

//
//
//
template <typename K, typename V>
ordered_map_const_iterator<K, V>::ordered_map_const_iterator(const std::unordered_map<K, V>& map,
                                                 const std::vector<K>& order, size_t size,
                                                 size_t position)
    : map_(map), order_(order), size_(size), position_(position) {}

template <typename K, typename V>
bool ordered_map_const_iterator<K, V>::operator!=(const ordered_map_const_iterator<K, V>& another) const {
    return &map_ != &another.map_ || &order_ != &another.order_ || size_ != another.size_ ||
           position_ != another.position_;
}

template <typename K, typename V>
ordered_map_const_iterator<K, V>& ordered_map_const_iterator<K, V>::operator++() {
    if (++position_ > size_) position_ = size_;
    return *this;
}

template <typename K, typename V>
const std::pair<const K, V>& ordered_map_const_iterator<K, V>::operator*() const {
    if (position_ >= size_) {
        throw std::out_of_range("ordered_map_iterator reached end");
    }
    return *map_.find(order_[position_]);
}

}  // namespace ryu