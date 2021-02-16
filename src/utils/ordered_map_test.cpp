#include "ordered_map.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <utility>

using std::string;
using std::literals::string_literals::operator""s;
using ryu::ordered_map;

TEST(OrderedMapTest, SetRead) {
    ordered_map<string, string> m;
    m.insert("key", "val");
    EXPECT_EQ(m.size(), 1);
    EXPECT_FALSE(m.empty());
    EXPECT_EQ(m["key"], "val");
    EXPECT_EQ(m.at(0).first, "key");
    EXPECT_EQ(m.at(0).second, "val");
}

TEST(OrderedMapTest, SetReadPtr) {
    ordered_map<string, std::unique_ptr<int>> m;
    auto ptr = std::make_unique<int>();
    int* val = ptr.get();

    m.insert("key", std::move(ptr));
    EXPECT_EQ(m.size(), 1);
    EXPECT_FALSE(m.empty());
    EXPECT_EQ(m["key"].get(), val);
    EXPECT_EQ(m.at(0).first, "key");
    EXPECT_EQ(m.at(0).second.get(), val);
}

TEST(OrderedMapTest, Clear) {
    ordered_map<string, string> m;
    m.insert("key", "val");
    m.clear();
    EXPECT_EQ(m.size(), 0);
    EXPECT_TRUE(m.empty());
}

TEST(OrderedMapTest, Assign) {
    ordered_map<string, string> m;
    m.insert("key", "val");
    EXPECT_EQ(m["key"], "val");
    EXPECT_EQ(m.at(0).second, "val");
    m.insert("key", "val2");
    EXPECT_EQ(m["key"], "val2");
    EXPECT_EQ(m.at(0).second, "val2");
}

TEST(OrderedMapTest, AssignPtr) {
    ordered_map<string, std::unique_ptr<int>> m;
    auto p1 = std::make_unique<int>();
    auto p2 = std::make_unique<int>();
    int* v1 = p1.get();
    int* v2 = p2.get();

    m.insert("key", std::move(p1));
    EXPECT_EQ(m["key"].get(), v1);
    EXPECT_EQ(m.at(0).second.get(), v1);
    m.insert("key", std::move(p2));
    EXPECT_EQ(m["key"].get(), v2);
    EXPECT_EQ(m.at(0).second.get(), v2);
}

TEST(OrderedMapTest, SubscriptAssignPtr) {
    ordered_map<string, std::unique_ptr<int>> m;
    auto p1 = std::make_unique<int>();
    auto p2 = std::make_unique<int>();
    int* v1 = p1.get();
    int* v2 = p2.get();

    m.insert("key", std::move(p1));
    EXPECT_EQ(m["key"].get(), v1);
    EXPECT_EQ(m.at(0).second.get(), v1);
    
    auto p1_moved = std::move(m["key"]);
    m["key"] = std::move(p2);

    EXPECT_EQ(m["key"].get(), v2);
    EXPECT_EQ(m.at(0).second.get(), v2);
    EXPECT_EQ(p1_moved.get(), v1);
}

TEST(OrderedMapTest, Erase) {
    ordered_map<string, string> m;
    m.insert("key", "val");
    m.insert("key2", "val2");
    m.insert("key3", "val3");

    EXPECT_EQ(m.size(), 3);
    EXPECT_EQ(m.at(0).first, "key");
    EXPECT_EQ(m.at(0).second, "val");
    EXPECT_EQ(m.at(1).first, "key2");
    EXPECT_EQ(m.at(1).second, "val2");
    EXPECT_EQ(m.at(2).first, "key3");
    EXPECT_EQ(m.at(2).second, "val3");

    EXPECT_EQ(m.erase("key2"), "val2");

    EXPECT_EQ(m.size(), 2);
    EXPECT_EQ(m.at(0).first, "key");
    EXPECT_EQ(m.at(0).second, "val");
    EXPECT_EQ(m.at(1).first, "key3");
    EXPECT_EQ(m.at(1).second, "val3");
}

TEST(OrderedMapTest, ErasePtr) {
    ordered_map<string, std::unique_ptr<int>> m;
    auto p1 = std::make_unique<int>();
    auto p2 = std::make_unique<int>();
    auto p3 = std::make_unique<int>();
    int* v1 = p1.get();
    int* v2 = p2.get();
    int* v3 = p3.get();

    m.insert("key", std::move(p1));
    m.insert("key2", std::move(p2));
    m.insert("key3", std::move(p3));

    EXPECT_EQ(m.size(), 3);
    EXPECT_EQ(m.at(0).first, "key");
    EXPECT_EQ(m.at(0).second.get(), v1);
    EXPECT_EQ(m.at(1).first, "key2");
    EXPECT_EQ(m.at(1).second.get(), v2);
    EXPECT_EQ(m.at(2).first, "key3");
    EXPECT_EQ(m.at(2).second.get(), v3);

    EXPECT_EQ(m.erase("key2")->get(), v2);

    EXPECT_EQ(m.size(), 2);
    EXPECT_EQ(m.at(0).first, "key");
    EXPECT_EQ(m.at(0).second.get(), v1);
    EXPECT_EQ(m.at(1).first, "key3");
    EXPECT_EQ(m.at(1).second.get(), v3);
}

TEST(OrderedMapTest, Iterator) {
    ordered_map<string, string> m;
    m.insert("key", "val");
    m.insert("key2", "val2");
    EXPECT_EQ(m.size(), 2);
    EXPECT_EQ(m.at(0).first, "key");
    EXPECT_EQ(m.at(0).second, "val");
    EXPECT_EQ(m.at(1).first, "key2");
    EXPECT_EQ(m.at(1).second, "val2");
    int i = 0;
    for (const auto& [k, v] : m) {
        if (i == 0) {
            EXPECT_EQ(k, "key");
            EXPECT_EQ(v, "val");
        } else if (i == 1) {
            EXPECT_EQ(k, "key2");
            EXPECT_EQ(v, "val2");
        }
        i++;
    }
}

TEST(OrderedMapTest, IteratorPtr) {
    ordered_map<string, std::unique_ptr<int>> m;
    auto p1 = std::make_unique<int>();
    auto p2 = std::make_unique<int>();
    int* v1 = p1.get();
    int* v2 = p2.get();

    m.insert("key", std::move(p1));
    m.insert("key2", std::move(p2));
    EXPECT_EQ(m.size(), 2);
    EXPECT_EQ(m.at(0).first, "key");
    EXPECT_EQ(m.at(0).second.get(), v1);
    EXPECT_EQ(m.at(1).first, "key2");
    EXPECT_EQ(m.at(1).second.get(), v2);
    int i = 0;
    for (const auto& [k, v] : m) {
        if (i == 0) {
            EXPECT_EQ(k, "key");
            EXPECT_EQ(v.get(), v1);
        } else if (i == 1) {
            EXPECT_EQ(k, "key2");
            EXPECT_EQ(v.get(), v2);
        }
        i++;
    }
}

TEST(OrderedMapTest, IteratorEdit) {
    ordered_map<string, string> m;
    m.insert("key", "val");
    EXPECT_EQ(m.size(), 1);
    EXPECT_EQ(m.at(0).first, "key");
    EXPECT_EQ(m.at(0).second, "val");
    for (auto& p : m) {
        p.second = "val_edit";
    }
    EXPECT_EQ(m.at(0).first, "key");
    EXPECT_EQ(m.at(0).second, "val_edit");
}

TEST(OrderedMapTest, IteratorEditPtr) {
    ordered_map<string, std::unique_ptr<int>> m;
    auto p1 = std::make_unique<int>();
    auto p2 = std::make_unique<int>();
    int* v1 = p1.get();
    int* v2 = p2.get();
    m.insert("key", std::move(p1));
    EXPECT_EQ(m.size(), 1);
    EXPECT_EQ(m.at(0).first, "key");
    EXPECT_EQ(m.at(0).second.get(), v1);

    for (auto& p : m) {
        p.second = std::move(p2);
    }
    EXPECT_EQ(m.at(0).first, "key");
    EXPECT_EQ(m.at(0).second.get(), v2);
}