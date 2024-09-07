/*
 * Copyright (c) 2024 David Leeds <davidesleeds@gmail.com>
 *
 * jstore is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include <jstore.hpp>

#include <filesystem>
#include <iostream>

#include <catch2/catch_test_macros.hpp>

using namespace std;
using json = nlohmann::json;

namespace test {

/*
 * Type with user-defined JSON serialization
 */
template <typename T>
struct complex {
    T r;
    T i;

    bool operator==(const complex &) const = default;
};

template <typename T>
void to_json(json &j, const complex<T> &v)
{
    j[0] = v.r;
    j[1] = v.i;
}

template <typename T>
void from_json(const json &j, complex<T> &v)
{
    j.at(0).get_to(v.r);
    j.at(1).get_to(v.i);
}


/*
 * Visitable struct
 */
struct visitable {
    bool b              = true;
    string s            = "string";
    int i               = 99;
    json j              = json::parse(R"({ "word": "foo", "list": [ 1, 2, 3] })");
    map<string, int> m  = { { "x", 11 }, { "y", 22 } };

    bool operator==(const visitable &) const = default;
};

} /* namespace test */

VISITABLE_STRUCT(test::visitable, b, s, i, j, m);


/*
 * Error callback.
 */
static void on_error(string_view msg)
{
    cerr << "ut_jstore [error] " << msg << '\n';
}


TEST_CASE("jstore::for_each", "[jstore]")
{
    SECTION("non-container")
    {
        size_t leaf_count = 0;
        size_t nonleaf_count = 0;
        size_t all_count = 0;

        int n = 42;

        /* Leaf */
        jstore::for_each<jstore::LEAF>(n, [&](auto &path, auto &value) {
            REQUIRE(path == "");
            REQUIRE(value == 42);

            value = 43;
            ++leaf_count;
        });

        REQUIRE(leaf_count == 1);
        REQUIRE(n == 43);

        /* Non-leaf */
        jstore::for_each<jstore::NON_LEAF>(n, [&](auto &path, auto &value) {
            ++nonleaf_count;
        });

        REQUIRE(nonleaf_count == 0);

        /* All */
        jstore::for_each<jstore::ALL>(n, [&](auto &path, auto &value) {
            REQUIRE(path == "");
            REQUIRE(value == 43);

            value = 44;
            ++all_count;
        });

        REQUIRE(all_count == 1);
        REQUIRE(n == 44);
    }

    SECTION("array-type container")
    {
        size_t leaf_count = 0;
        size_t nonleaf_count = 0;
        size_t all_count = 0;

        vector<string> v = { "a", "b", "c" };

        /* Leaf */
        jstore::for_each<jstore::LEAF>(v, [&](auto &path, auto &value) {
            REQUIRE(path == to_string(leaf_count));
            REQUIRE(value == v[leaf_count]);

            ++leaf_count;
        });

        REQUIRE(leaf_count == 3);

        /* Non-leaf */
        jstore::for_each<jstore::NON_LEAF>(v, [&](auto &path, auto &value) {
            REQUIRE(path == "");
            REQUIRE(is_same_v<vector<string>, decay_t<decltype(value)>>);

            ++nonleaf_count;
        });

        REQUIRE(nonleaf_count == 1);

        /* All */
        leaf_count = 0;
        jstore::for_each<jstore::ALL>(v, [&](auto &path, auto &value) {
            using node_type = decay_t<decltype(value)>;

            if constexpr (jstore::traits::is_container_v<node_type>) {
                REQUIRE(path == "");
                STATIC_REQUIRE(is_same_v<vector<string>, decay_t<decltype(value)>>);
            } else {
                REQUIRE(path == to_string(leaf_count));
                REQUIRE(value == v[leaf_count]);
                ++leaf_count;
            }

            ++all_count;
        });

        REQUIRE(all_count == 4);
    }

    SECTION("object-type container")
    {
        size_t leaf_count = 0;
        size_t nonleaf_count = 0;
        size_t all_count = 0;

        const map<double, string> m = { { 0.1 , "a" }, { 0.2, "b" }, { 0.3, "c" } };

        /* Leaf */
        jstore::for_each<jstore::LEAF>(m, [&](auto &path, auto &value) {
            double key = ((double)leaf_count + 1) / 10;

            REQUIRE(path == fmt::format("{}", key));
            REQUIRE(value == m.at(key));

            ++leaf_count;
        });

        REQUIRE(leaf_count == 3);

        /* Non-leaf */
        jstore::for_each<jstore::NON_LEAF>(m, [&](auto &path, auto &value) {
            REQUIRE(path == "");
            STATIC_REQUIRE(is_same_v<map<double, string>, decay_t<decltype(value)>>);

            ++nonleaf_count;
        });

        REQUIRE(nonleaf_count == 1);

        /* All */
        leaf_count = 0;
        jstore::for_each<jstore::ALL>(m, [&](auto &path, auto &value) {
            using node_type = decay_t<decltype(value)>;

            if constexpr (jstore::traits::is_container_v<node_type>) {
                REQUIRE(path == "");
                STATIC_REQUIRE(is_same_v<map<double, string>, decay_t<decltype(value)>>);
            } else {
                double key = ((double)leaf_count + 1) / 10;

                REQUIRE(path == fmt::format("{}", key));
                REQUIRE(value == m.at(key));

                ++leaf_count;
            }

            ++all_count;
        });

        REQUIRE(all_count == 4);
    }

    SECTION("visitable_struct")
    {
        size_t leaf_count = 0;
        size_t nonleaf_count = 0;
        size_t all_count = 0;

        const test::visitable v;
        vector<string> paths = {
                "b",
                "s",
                "i",
                "j",
                "m/x",
                "m/y"
        };

        /* Leaf */
        jstore::for_each<jstore::LEAF>(v, [&](auto &path, auto &value) {
            REQUIRE(path == paths[leaf_count]);

            ++leaf_count;
        });

        REQUIRE(leaf_count == 6);

        /* Non-leaf */
        jstore::for_each<jstore::NON_LEAF>(v, [&](auto &path, auto &value) {
            ++nonleaf_count;
        });

        REQUIRE(nonleaf_count == 2);

        /* All */
        leaf_count = 0;
        jstore::for_each<jstore::ALL>(v, [&](auto &path, auto &value) {
            using node_type = decay_t<decltype(value)>;

            if constexpr (jstore::traits::is_container_v<node_type>) {

            } else {
                REQUIRE(path == paths[leaf_count]);

                ++leaf_count;
            }

            ++all_count;
        });

        REQUIRE(all_count == 8);
    }

} /* for_each */


TEST_CASE("jstore::visit_path", "[jstore]")
{
    SECTION("non-container")
    {
        size_t call_count = 0;
        int n = 42;

        SECTION("valid path (root)")
        {
            bool result = jstore::visit_path(n, "", [&](auto &value) {
                REQUIRE(addressof(value) == addressof(n));

                value = 43;
                ++call_count;
            }, false, on_error);

            REQUIRE(result == true);
            REQUIRE(call_count == 1);
            REQUIRE(n == 43);
        }

        SECTION("invalid path 1")
        {
            bool result = jstore::visit_path(n, " ", [&](auto &value) {
                ++call_count;
            }, false, on_error);

            REQUIRE(result == false);
            REQUIRE(call_count == 0);
        }

        SECTION("invalid path 2")
        {
            bool result = jstore::visit_path(n, "/", [&](auto &value) {
                ++call_count;
            }, false, on_error);

            REQUIRE(result == false);
            REQUIRE(call_count == 0);
        }

        SECTION("invalid path 3")
        {
            bool result = jstore::visit_path(n, "nonexistent", [&](auto &value) {
                ++call_count;
            }, false, on_error);

            REQUIRE(result == false);
            REQUIRE(call_count == 0);
        }

        SECTION("invalid path 4")
        {
            bool result = jstore::visit_path(n, "nonexistent/sub", [&](auto &value) {
                ++call_count;
            }, false, on_error);

            REQUIRE(result == false);
            REQUIRE(call_count == 0);
        }

        SECTION("invalid path 5")
        {
            bool result = jstore::visit_path(n, "/nonexistent", [&](auto &value) {
                ++call_count;
            }, false, on_error);

            REQUIRE(result == false);
            REQUIRE(call_count == 0);
        }

        SECTION("invalid path 6")
        {
            bool result = jstore::visit_path(n, "nonexistent/", [&](auto &value) {
                ++call_count;
            }, false, on_error);

            REQUIRE(result == false);
            REQUIRE(call_count == 0);
        }
    }

    SECTION("array-type container")
    {
        size_t call_count = 0;

        vector<string> v = { "a", "b", "c" };

        SECTION("valid path (root)")
        {
            bool result = jstore::visit_path(v, "", [&](auto &value) {
                if constexpr (jstore::traits::container<decltype(value)>) {
                    REQUIRE(addressof(value) == addressof(v));

                    value.push_back("d");
                } else {
                    FAIL("callback for leaf");
                }

                ++call_count;
            }, false, on_error);

            REQUIRE(result == true);
            REQUIRE(call_count == 1);
            REQUIRE(v.at(3) == "d");
        }

        SECTION("valid path (element[0])")
        {
            bool result = jstore::visit_path(v, "0", [&](auto &value) {
                if constexpr (jstore::traits::not_container<decltype(value)>) {
                    REQUIRE(addressof(value) == addressof(v[0]));

                    value = "aa";
                } else {
                    FAIL("callback for root");
                }

                ++call_count;
            }, false, on_error);

            REQUIRE(result == true);
            REQUIRE(call_count == 1);
            REQUIRE(v[0] == "aa");
        }

        SECTION("valid path (element[2])")
        {
            bool result = jstore::visit_path(v, "2", [&](auto &value) {
                if constexpr (jstore::traits::not_container<decltype(value)>) {
                    REQUIRE(addressof(value) == addressof(v[2]));

                    value = "cc";
                } else {
                    FAIL("callback for root");
                }

                ++call_count;
            }, false, on_error);

            REQUIRE(result == true);
            REQUIRE(call_count == 1);
            REQUIRE(v[2] == "cc");
        }

        SECTION("valid path (not populated)")
        {
            bool result = jstore::visit_path(v, "3", [&](auto &value) {
                ++call_count;
            }, false, on_error);

            REQUIRE(result == false);
            REQUIRE(call_count == 0);
        }

        SECTION("valid path (not populated, insert_keys true)")
        {
            bool result = jstore::visit_path(v, "3", [&](auto &value) {
                ++call_count;
            }, true, on_error);

            /* Cannot insert array elements */
            REQUIRE(result == false);
            REQUIRE(call_count == 0);
        }

        SECTION("invalid path 1")
        {
            bool result = jstore::visit_path(v, " ", [&](auto &value) {
                ++call_count;
            }, false, on_error);

            REQUIRE(result == false);
            REQUIRE(call_count == 0);
        }

        SECTION("invalid path 2")
        {
            bool result = jstore::visit_path(v, "0 ", [&](auto &value) {
                ++call_count;
            }, false, on_error);

            REQUIRE(result == false);
            REQUIRE(call_count == 0);
        }

        SECTION("invalid path 3")
        {
            bool result = jstore::visit_path(v, " 0", [&](auto &value) {
                ++call_count;
            }, false, on_error);

            REQUIRE(result == false);
            REQUIRE(call_count == 0);
        }

        SECTION("invalid path 4")
        {
            bool result = jstore::visit_path(v, "/", [&](auto &value) {
                ++call_count;
            }, false, on_error);

            REQUIRE(result == false);
            REQUIRE(call_count == 0);
        }

        SECTION("invalid path 5")
        {
            bool result = jstore::visit_path(v, "nonexistent", [&](auto &value) {
                ++call_count;
            }, false, on_error);

            REQUIRE(result == false);
            REQUIRE(call_count == 0);
        }

        SECTION("invalid path 6")
        {
            bool result = jstore::visit_path(v, "nonexistent/sub", [&](auto &value) {
                ++call_count;
            }, false, on_error);

            REQUIRE(result == false);
            REQUIRE(call_count == 0);
        }

        SECTION("invalid path 7")
        {
            bool result = jstore::visit_path(v, "/nonexistent", [&](auto &value) {
                ++call_count;
            }, false, on_error);

            REQUIRE(result == false);
            REQUIRE(call_count == 0);
        }

        SECTION("invalid path 8")
        {
            bool result = jstore::visit_path(v, "nonexistent/", [&](auto &value) {
                ++call_count;
            }, false, on_error);

            REQUIRE(result == false);
            REQUIRE(call_count == 0);
        }
    }

    SECTION("object-type container (string key)")
    {
        size_t call_count = 0;

        map<string, int> m = { { "a", 1 }, { "b", 2 }, { "c", 3 } };

        SECTION("valid path (root)")
        {
            bool result = jstore::visit_path(m, "", [&](auto &value) {
                if constexpr (jstore::traits::container<decltype(value)>) {
                    REQUIRE(addressof(value) == addressof(m));

                    value["d"] = 4;
                } else {
                    FAIL("callback for leaf");
                }

                ++call_count;
            }, false, on_error);

            REQUIRE(result == true);
            REQUIRE(call_count == 1);
            REQUIRE(m.at("d") == 4);
        }

        SECTION("valid path (element[a])")
        {
            bool result = jstore::visit_path(m, "a", [&](auto &value) {
                if constexpr (jstore::traits::not_container<decltype(value)>) {
                    REQUIRE(addressof(value) == addressof(m.at("a")));

                    value = 11;
                } else {
                    FAIL("callback for root");
                }

                ++call_count;
            }, false, on_error);

            REQUIRE(result == true);
            REQUIRE(call_count == 1);
            REQUIRE(m.at("a") == 11);
        }

        SECTION("valid path (element[c])")
        {
            bool result = jstore::visit_path(m, "c", [&](auto &value) {
                if constexpr (jstore::traits::not_container<decltype(value)>) {
                    REQUIRE(addressof(value) == addressof(m.at("c")));

                    value = 33;
                } else {
                    FAIL("callback for root");
                }

                ++call_count;
            }, false, on_error);

            REQUIRE(result == true);
            REQUIRE(call_count == 1);
            REQUIRE(m.at("c") == 33);
        }

        SECTION("valid path (not populated)")
        {
            bool result = jstore::visit_path(m, "d", [&](auto &value) {
                ++call_count;
            }, false, on_error);

            REQUIRE(result == false);
            REQUIRE(call_count == 0);
        }

        SECTION("valid path (not populated, insert_keys true)")
        {
            bool result = jstore::visit_path(m, "d", [&](auto &value) {
                if constexpr (jstore::traits::not_container<decltype(value)>) {
                    REQUIRE(addressof(value) == addressof(m.at("d")));

                    value = 44;
                } else {
                    FAIL("callback for root");
                }

                ++call_count;
            }, true, on_error);

            REQUIRE(result == true);
            REQUIRE(call_count == 1);
            REQUIRE(m.at("d") == 44);
        }

        SECTION("invalid path 1")
        {
            bool result = jstore::visit_path(m, "/", [&](auto &value) {
                ++call_count;
            }, false, on_error);

            REQUIRE(result == false);
            REQUIRE(call_count == 0);
        }

        SECTION("invalid path 2")
        {
            bool result = jstore::visit_path(m, "/nonexistent", [&](auto &value) {
                ++call_count;
            }, false, on_error);

            REQUIRE(result == false);
            REQUIRE(call_count == 0);
        }

        SECTION("invalid path 3")
        {
            bool result = jstore::visit_path(m, "a/nonexistent", [&](auto &value) {
                ++call_count;
            }, false, on_error);

            REQUIRE(result == false);
            REQUIRE(call_count == 0);
        }
    }

    SECTION("object-type container (non-string key)")
    {
        size_t call_count = 0;

        map<int, int> m = { { 1, 1 }, { 2, 2 }, { 3, 3 } };

        SECTION("valid path (element[1])")
        {
            bool result = jstore::visit_path(m, "1", [&](auto &value) {
                if constexpr (jstore::traits::not_container<decltype(value)>) {
                    REQUIRE(addressof(value) == addressof(m.at(1)));

                    value = 11;
                } else {
                    FAIL("callback for root");
                }

                ++call_count;
            }, false, on_error);

            REQUIRE(result == true);
            REQUIRE(call_count == 1);
            REQUIRE(m.at(1) == 11);
        }

        SECTION("valid path (element[3])")
        {
            bool result = jstore::visit_path(m, "3", [&](auto &value) {
                if constexpr (jstore::traits::not_container<decltype(value)>) {
                    REQUIRE(addressof(value) == addressof(m.at(3)));

                    value = 33;
                } else {
                    FAIL("callback for root");
                }

                ++call_count;
            }, false, on_error);

            REQUIRE(result == true);
            REQUIRE(call_count == 1);
            REQUIRE(m.at(3) == 33);
        }

        SECTION("valid path (element[4] not populated, insert_keys true)")
        {
            bool result = jstore::visit_path(m, "4", [&](auto &value) {
                if constexpr (jstore::traits::not_container<decltype(value)>) {
                    REQUIRE(addressof(value) == addressof(m.at(4)));

                    value = 44;
                } else {
                    FAIL("callback for root");
                }

                ++call_count;
            }, true, on_error);

            REQUIRE(result == true);
            REQUIRE(call_count == 1);
            REQUIRE(m.at(4) == 44);
        }

        SECTION("invalid key")
        {
            bool result = jstore::visit_path(m, "Invalid", [&](auto &value) {
                ++call_count;
            }, false, on_error);

            REQUIRE(result == false);
            REQUIRE(call_count == 0);
        }
    }

    SECTION("visitable_struct")
    {
        size_t call_count = 0;

        test::visitable v;

        SECTION("valid path (root)")
        {
            bool result = jstore::visit_path(v, "", [&](auto &value) {
                if constexpr (jstore::traits::visitable<decltype(value)>) {
                    REQUIRE(addressof(value) == addressof(v));

                    value.i = 42;
                } else {
                    FAIL("callback for leaf");
                }

                ++call_count;
            }, false, on_error);

            REQUIRE(result == true);
            REQUIRE(call_count == 1);
            REQUIRE(v.i == 42);
        }

        SECTION("valid path (element[s])")
        {
            bool result = jstore::visit_path(v, "s", [&](auto &value) {
                using value_type = decay_t<decltype(value)>;

                if constexpr (is_same_v<value_type, string>) {
                    REQUIRE(addressof(value) == addressof(v.s));

                    value = "foo";
                } else {
                    FAIL("callback for wrong element");
                }

                ++call_count;
            }, false, on_error);

            REQUIRE(result == true);
            REQUIRE(call_count == 1);
            REQUIRE(v.s == "foo");
        }

        SECTION("valid path (element[m/x])")
        {
            bool result = jstore::visit_path(v, "m/x", [&](auto &value) {
                using value_type = decay_t<decltype(value)>;

                if constexpr (is_same_v<value_type, int>) {
                    REQUIRE(addressof(value) == addressof(v.m.at("x")));

                    value = 100;
                } else {
                    FAIL("callback for wrong element");
                }

                ++call_count;
            }, false, on_error);

            REQUIRE(result == true);
            REQUIRE(call_count == 1);
            REQUIRE(v.m.at("x") == 100);
        }

        SECTION("invalid path 1")
        {
            bool result = jstore::visit_path(v, "/", [&](auto &value) {
                ++call_count;
            }, false, on_error);

            REQUIRE(result == false);
            REQUIRE(call_count == 0);
        }

        SECTION("invalid path 2")
        {
            bool result = jstore::visit_path(v, "nonexistent", [&](auto &value) {
                ++call_count;
            }, false, on_error);

            REQUIRE(result == false);
            REQUIRE(call_count == 0);
        }

        SECTION("invalid path 3")
        {
            bool result = jstore::visit_path(v, "nonexistent/sub", [&](auto &value) {
                ++call_count;
            }, false, on_error);

            REQUIRE(result == false);
            REQUIRE(call_count == 0);
        }

        SECTION("invalid path 4")
        {
            bool result = jstore::visit_path(v, "/nonexistent", [&](auto &value) {
                ++call_count;
            }, false, on_error);

            REQUIRE(result == false);
            REQUIRE(call_count == 0);
        }

        SECTION("invalid path 5")
        {
            bool result = jstore::visit_path(v, "nonexistent/", [&](auto &value) {
                ++call_count;
            }, false, on_error);

            REQUIRE(result == false);
            REQUIRE(call_count == 0);
        }
    }

    SECTION("map of visitable_struct")
    {
        size_t call_count = 0;

        map<unsigned, test::visitable> m{
            { 1, test::visitable{} },
            { 2, test::visitable{} }
        };

        SECTION("valid path (element[2/m/y])")
        {
            bool result = jstore::visit_path(m, "2/m/y", [&](auto &value) {
                using value_type = decay_t<decltype(value)>;

                if constexpr (is_same_v<value_type, int>) {
                    REQUIRE(addressof(value) == addressof(m.at(2).m.at("y")));
                } else {
                    FAIL("callback for wrong element");
                }

                ++call_count;
            }, false, on_error);

            REQUIRE(result == true);
            REQUIRE(call_count == 1);
        }
    }

} /* visit_path */


TEST_CASE("jstore::serialize", "[jstore]")
{
    /*
     * Discrete value types
     */

    SECTION("boolean")
    {
        const bool b = true;
        json j;

        REQUIRE(jstore::serialize(j, b, false, on_error));
        REQUIRE(j == json::parse("true"));
    }

    SECTION("string")
    {
        const string s = "string";
        json j;

        REQUIRE(jstore::serialize(j, s, false, on_error));
        REQUIRE(j == json::parse("\"string\""));
    }

    SECTION("char pointer")
    {
        const char *s = "char pointer";
        json j;

        REQUIRE(jstore::serialize(j, s, false, on_error));
        REQUIRE(j == json::parse("\"char pointer\""));
    }

    SECTION("char array")
    {
        const char s[100] = "char array";
        json j;

        REQUIRE(jstore::serialize(j, s, false, on_error));
        REQUIRE(j == json::parse("\"char array\""));
    }

    SECTION("int8")
    {
        const int8_t n = -8;
        json j;

        REQUIRE(jstore::serialize(j, n, false, on_error));
        REQUIRE(j == json::parse("-8"));
    }

    SECTION("int16")
    {
        const int16_t n = -1600;
        json j;

        REQUIRE(jstore::serialize(j, n, false, on_error));
        REQUIRE(j == json::parse("-1600"));
    }

    SECTION("int32")
    {
        const int32_t n = -320000;
        json j;

        REQUIRE(jstore::serialize(j, n, false, on_error));
        REQUIRE(j == json::parse("-320000"));
    }

    SECTION("int64")
    {
        const int64_t n = -64000000000000;
        json j;

        REQUIRE(jstore::serialize(j, n, false, on_error));
        REQUIRE(j == json::parse("-64000000000000"));
    }

    SECTION("uint8")
    {
        const uint8_t n = 8;
        json j;

        REQUIRE(jstore::serialize(j, n, false, on_error));
        REQUIRE(j == json::parse("8"));
    }

    SECTION("uint16")
    {
        const uint16_t n = 1600;
        json j;

        REQUIRE(jstore::serialize(j, n, false, on_error));
        REQUIRE(j == json::parse("1600"));
    }

    SECTION("uint32")
    {
        const uint32_t n = 320000;
        json j;

        REQUIRE(jstore::serialize(j, n, false, on_error));
        REQUIRE(j == json::parse("320000"));
    }

    SECTION("uint64")
    {
        const uint64_t n = 64000000000000;
        json j;

        REQUIRE(jstore::serialize(j, n, false, on_error));
        REQUIRE(j == json::parse("64000000000000"));
    }

    SECTION("json")
    {
        const json obj = json::parse("{\"key\": 99}");
        json j;

        REQUIRE(jstore::serialize(j, obj, false, on_error));
        REQUIRE(j == obj);
    }

    SECTION("custom")
    {
        const test::complex<double> c = { 123.4, 9.99 };
        json j;

        REQUIRE(jstore::serialize(j, c, false, on_error));
        REQUIRE(j == json::parse("[123.4, 9.99]"));
    }


    /*
     * Array type containers
     */

    SECTION("array")
    {
        const array c { "1", "2", "3" };
        json j;

        REQUIRE(jstore::serialize(j, c, false, on_error));
        REQUIRE(j == json::parse(R"(["1","2","3"])"));
    }

    SECTION("vector")
    {
        const vector<string> c { "1", "2", "3" };
        json j;

        REQUIRE(jstore::serialize(j, c, false, on_error));
        REQUIRE(j == json::parse(R"(["1","2","3"])"));
    }

    SECTION("list")
    {
        const list<string> c { "1", "2", "3" };
        json j;

        REQUIRE(jstore::serialize(j, c, false, on_error));
        REQUIRE(j == json::parse(R"(["1","2","3"])"));
    }

    SECTION("deque")
    {
        const deque<string> c { "1", "2", "3" };
        json j;

        REQUIRE(jstore::serialize(j, c, false, on_error));
        REQUIRE(j == json::parse(R"(["1","2","3"])"));
    }

    SECTION("set")
    {
        const set<string> c { "1", "2", "3" };
        json j;

        REQUIRE(jstore::serialize(j, c, false, on_error));
        REQUIRE(j == json::parse(R"(["1","2","3"])"));
    }

    SECTION("unordered_set")
    {
        const unordered_set<string> c { "1", "2", "3" };
        json j;

        REQUIRE(jstore::serialize(j, c, false, on_error));
        sort(j.begin(), j.end());
        REQUIRE(j[0] == "1");
        REQUIRE(j[1] == "2");
        REQUIRE(j[2] == "3");
    }

    SECTION("empty array type")
    {
        const vector<string> c {};
        json j;

        REQUIRE_FALSE(jstore::serialize(j, c, false, on_error)); /* serialize() returns false for empty output */
        REQUIRE(j == json::parse(R"([])"));
    }


    /*
     * Object type containers
     */

    SECTION("map")
    {
        const map<string, int> c1 { { "1", 1 }, { "2", 2 }, { "3", 3 } };
        const map<int, string> c2 { { 1, "1" }, { 2, "2" }, { 3, "3" } };
        json j1, j2;

        REQUIRE(jstore::serialize(j1, c1, false, on_error));
        REQUIRE(j1 == json::parse(R"({"1":1,"2":2,"3":3})"));

        REQUIRE(jstore::serialize(j2, c2, false, on_error));
        REQUIRE(j2 == json::parse(R"([[1,"1"],[2,"2"],[3,"3"]])"));
    }

    SECTION("unordered_map")
    {
        const unordered_map<string, int> c1 { { "1", 1 }, { "2", 2 }, { "3", 3 } };
        const unordered_map<int, string> c2 { { 1, "1" }, { 2, "2" }, { 3, "3" } };
        json j1, j2;

        REQUIRE(jstore::serialize(j1, c1, false, on_error));
        REQUIRE(j1 == json::parse(R"({"1":1,"2":2,"3":3})"));

        REQUIRE(jstore::serialize(j2, c2, false, on_error));
        sort(j2.begin(), j2.end(), [](const auto &a, const auto &b) { return a.at(0) < b.at(0); });
        REQUIRE(j2 == json::parse(R"([[1,"1"],[2,"2"],[3,"3"]])"));
    }

    SECTION("empty object type")
    {
        const map<string, int> c {};
        json j;

        REQUIRE_FALSE(jstore::serialize(j, c, false, on_error)); /* serialize() returns false for empty output */
        REQUIRE(j == json::parse(R"({})"));

    }


    /*
     * Visitable struct (requires visit_struct library)
     */

    SECTION("visitable_struct")
    {
        test::visitable v;
        json j;

        SECTION("include defaults")
        {
            REQUIRE(jstore::serialize(j, v, false, on_error));
            REQUIRE(j == json::parse(R"({"b":true,"s":"string","i":99,"j":{"word":"foo","list":[1,2,3] },"m":{"x":11,"y":22}})"));

            v.i = 42;
            REQUIRE(jstore::serialize(j, v, false, on_error));
            REQUIRE(j == json::parse(R"({"b":true,"s":"string","i":42,"j":{"word":"foo","list":[1,2,3] },"m":{"x":11,"y":22}})"));

            v.m.erase("x");
            v.m["z"] = 33;
            REQUIRE(jstore::serialize(j, v, false, on_error));
            REQUIRE(j == json::parse(R"({"b":true,"s":"string","i":42,"j":{"word":"foo","list":[1,2,3] },"m":{"y":22,"z":33}})"));
        }

        SECTION("omit defaults") {
            REQUIRE_FALSE(jstore::serialize(j, v, true, on_error)); /* serialize() returns false for empty output */
            REQUIRE(j.is_object());
            REQUIRE(j.empty());

            v.i = 42;
            REQUIRE(jstore::serialize(j, v, true, on_error));
            REQUIRE(j == json::parse(R"({"i":42})"));

            v.m.erase("x");
            v.m["z"] = 33;
            REQUIRE(jstore::serialize(j, v, true, on_error));
            REQUIRE(j == json::parse(R"({"i":42,"m":{"y":22,"z":33}})"));
        }
    }

} /* serialize */


TEST_CASE("jstore::deserialize", "[jstore]")
{
    /*
     * Discrete value types
     */

    SECTION("boolean")
    {
        bool b {};
        const json j = json::parse("true");

        REQUIRE(jstore::deserialize(j, b, on_error));
        REQUIRE(b);
    }

    SECTION("string")
    {
        string s;
        const json j = json::parse("\"string\"");

        REQUIRE(jstore::deserialize(j, s, on_error));
        REQUIRE(s == "string");
    }

    SECTION("int8")
    {
        int8_t n {};
        const json j = json::parse("-8");

        REQUIRE(jstore::deserialize(j, n, on_error));
        REQUIRE(n == -8);
    }

    SECTION("int16")
    {
        int16_t n {};
        const json j = json::parse("-1600");

        REQUIRE(jstore::deserialize(j, n, on_error));
        REQUIRE(n == -1600);
    }

    SECTION("int32")
    {
        int32_t n {};
        const json j = json::parse("-320000");

        REQUIRE(jstore::deserialize(j, n, on_error));
        REQUIRE(n == -320000);
    }

    SECTION("int64")
    {
        int64_t n {};
        const json j = json::parse("-64000000000000");

        REQUIRE(jstore::deserialize(j, n, on_error));
        REQUIRE(n == -64000000000000);
    }

    SECTION("uint8")
    {
        uint8_t n {};
        const json j = json::parse("8");

        REQUIRE(jstore::deserialize(j, n, on_error));
        REQUIRE(n == 8);
    }

    SECTION("uint16")
    {
        uint16_t n {};
        const json j = json::parse("1600");

        REQUIRE(jstore::deserialize(j, n, on_error));
        REQUIRE(n == 1600);
    }

    SECTION("uint32")
    {
        uint32_t n {};
        const json j = json::parse("320000");

        REQUIRE(jstore::deserialize(j, n, on_error));
        REQUIRE(n == 320000);
    }

    SECTION("uint64")
    {
        uint64_t n {};
        const json j = json::parse("64000000000000");

        REQUIRE(jstore::deserialize(j, n, on_error));
        REQUIRE(n == 64000000000000);
    }

    SECTION("json")
    {
        const json obj = json::parse("{\"key\": 99}");
        json j;

        REQUIRE(jstore::deserialize(obj, j, on_error));
        REQUIRE(j == obj);
    }

    SECTION("custom")
    {
        test::complex<double> c;
        const test::complex<double> res { 123.4, 9.99 };
        const json j = json::parse("[123.4, 9.99]");

        REQUIRE(jstore::deserialize(j, c, on_error));
        REQUIRE(c == res);
    }


    /*
     * Array type containers
     */

    SECTION("array")
    {
        array<string, 3> c;
        const json j = json::parse(R"(["1","2","3"])");

        REQUIRE(c[0] == "");
        REQUIRE(c[1] == "");
        REQUIRE(c[2] == "");

        REQUIRE(jstore::deserialize(j, c, on_error));
        REQUIRE(c[0] == "1");
        REQUIRE(c[1] == "2");
        REQUIRE(c[2] == "3");
    }

    SECTION("vector")
    {
        vector<string> c;
        const json j = json::parse(R"(["1","2","3"])");

        REQUIRE(jstore::deserialize(j, c, on_error));
        REQUIRE(c == vector<string>{ "1", "2", "3" });
    }

    SECTION("list")
    {
        list<string> c;
        const json j = json::parse(R"(["1","2","3"])");

        REQUIRE(jstore::deserialize(j, c, on_error));
        REQUIRE(c == list<string>{ "1", "2", "3" });
    }

    SECTION("deque")
    {
        deque<string> c;
        const json j = json::parse(R"(["1","2","3"])");

        REQUIRE(jstore::deserialize(j, c, on_error));
        REQUIRE(c == deque<string>{ "1", "2", "3" });
    }

    SECTION("set")
    {
        set<string> c;
        const json j = json::parse(R"(["1","2","3"])");

        REQUIRE(jstore::deserialize(j, c, on_error));
        REQUIRE(c == set<string>{ "1", "2", "3" });
    }

    SECTION("unordered_set")
    {
        unordered_set<string> c;
        const json j = json::parse(R"(["1","2","3"])");

        REQUIRE(jstore::deserialize(j, c, on_error));
        REQUIRE(c == unordered_set<string>{ "1", "2", "3" });
    }

    SECTION("empty array type")
    {
        vector<string> c { "1", "2", "3" }; /* Pre-populate with content to overwrite */
        const json j = json::parse(R"([])");

        REQUIRE(jstore::deserialize(j, c, on_error));
        REQUIRE(c.empty());
    }


    /*
     * Object type containers
     */

    SECTION("map")
    {
        map<string, int> c1;
        map<int, string> c2;
        const json j1 = json::parse(R"({"1":1,"2":2,"3":3})");
        const json j2 = json::parse(R"([[1,"1"],[2,"2"],[3,"3"]])");

        REQUIRE(jstore::deserialize(j1, c1, on_error));
        REQUIRE(c1 == map<string, int>{ { "1", 1 }, { "2", 2 }, { "3", 3 } });

        REQUIRE(jstore::deserialize(j2, c2, on_error));
        REQUIRE(c2 == map<int, string>{ { 1, "1" }, { 2, "2" }, { 3, "3" } });
    }

    SECTION("unordered_map ")
    {
        unordered_map<string, int> c1;
        unordered_map<int, string> c2;
        const json j1 = json::parse(R"({"1":1,"2":2,"3":3})");
        const json j2 = json::parse(R"([[1,"1"],[2,"2"],[3,"3"]])");

        REQUIRE(jstore::deserialize(j1, c1, on_error));
        REQUIRE(c1 == unordered_map<string, int>{ { "1", 1 }, { "2", 2 }, { "3", 3 } });

        REQUIRE(jstore::deserialize(j2, c2, on_error));
        REQUIRE(c2 == unordered_map<int, string>{ { 1, "1" }, { 2, "2" }, { 3, "3" } });
    }

    SECTION("empty object type")
    {
        map<string, int> c { { "1", 1 }, { "2", 2 }, { "3", 3 } }; /* Pre-populate with content to overwrite */
        const json j = json::parse(R"({})");

        REQUIRE(jstore::deserialize(j, c, on_error));
        REQUIRE(c.empty());
    }


    /*
     * Visitable struct (requires visit_struct library)
     */

    SECTION("visitable_struct")
    {
        test::visitable v;
        const json j = json::parse(R"({"b":false,"s":"string2","j":[],"m":{"z":33}})");

        REQUIRE(jstore::deserialize(j, v, on_error));
        REQUIRE(v.b == false);
        REQUIRE(v.s == "string2");
        REQUIRE(v.i == 99); // Default value
        REQUIRE(v.j == json::array());
        REQUIRE(v.m == map<string, int>{ { "z", 33 } });
    }

} /* deserialize */


TEST_CASE("jstore::load", "[jstore]")
{
    const filesystem::path file = "/tmp/test/jstore/data.json";

    filesystem::remove_all(file);
    filesystem::create_directories(file.parent_path());

    SECTION("no file")
    {
        jstore::tree<map<string, test::visitable>> conf(file, on_error);

        REQUIRE(conf.root().empty());
    }

    SECTION("empty file")
    {
        jstore::tree<map<string, test::visitable>> conf(file, on_error);

        ofstream f(file);
        f.close();
        REQUIRE(filesystem::exists(file));

        REQUIRE(conf.root().empty());
        REQUIRE_THROWS(conf.load());
        REQUIRE(conf.root().empty());
    }

    SECTION("corrupt file (parse failure)")
    {
        jstore::tree<map<string, test::visitable>> conf(file, on_error);

        ofstream f(file);
        f << "{{ 1234";
        f.flush();
        f.close();
        REQUIRE(filesystem::exists(file));

        /* Exception on parse failure */
        REQUIRE_THROWS(conf.load());
        REQUIRE(conf.root().empty());
    }

    SECTION("corrupt file (tree mismatch)")
    {
        jstore::tree<map<string, test::visitable>> conf(file, on_error);

        ofstream f(file);
        f << R"([ "abc", "def", 99 ])";
        f.flush();
        f.close();
        REQUIRE(filesystem::exists(file));

        /* Warn and continue for unexpected structure */
        REQUIRE_NOTHROW(conf.load());
        REQUIRE(conf->empty());
    }

    SECTION("map entries, sparse values")
    {
        jstore::tree<map<string, test::visitable>> conf(file, on_error);

        ofstream f(file);
        f << R"({ "profile1": null, "profile2": {}, "profile3": { "b": true }, "profile4": { "i": 9876, "j": [9,9,9] } })";
        f.flush();
        f.close();
        REQUIRE(filesystem::exists(file));

        /* Expect map entries to be populated, even without content */
        REQUIRE_NOTHROW(conf.load());
        REQUIRE(conf->size() == 4);

        REQUIRE(conf->at("profile1") == test::visitable{});

        REQUIRE(conf->at("profile2") == test::visitable{});

        REQUIRE(conf->at("profile3") == test::visitable{});

        REQUIRE(conf->at("profile4").i == 9876);
        REQUIRE(conf->at("profile4").j == json::parse(R"([9,9,9])"));
    }

    SECTION("vector entries, sparse values")
    {
        jstore::tree<vector<test::visitable>> conf(file, on_error);

        ofstream f(file);
        f << R"([ null, {}, { "b": true }, { "i": 9876, "j": [9,9,9] } ])";
        f.flush();
        f.close();
        REQUIRE(filesystem::exists(file));

        /* Expect vector entries to be populated, even without content */
        REQUIRE_NOTHROW(conf.load());
        REQUIRE(conf->size() == 4);

        REQUIRE(conf->at(0) == test::visitable{});

        REQUIRE(conf->at(1) == test::visitable{});

        REQUIRE(conf->at(2) == test::visitable{});

        REQUIRE(conf->at(3).i == 9876);
        REQUIRE(conf->at(3).j == json::parse(R"([9,9,9])"));
    }

    SECTION("map of visitable structs: reload to pick up file change")
    {
        {
            ofstream f(file);
            f << R"({ "profile1": null, "profile2": { "s": "foo", "i": 9876 } })";
            f.flush();
        }
        REQUIRE(filesystem::exists(file));

        jstore::tree<map<string, test::visitable>> conf(file, on_error);

        REQUIRE(conf.root().count("profile1") == 1);
        REQUIRE(conf.root().at("profile2").b == true);
        REQUIRE(conf.root().at("profile2").s == "foo");
        REQUIRE(conf.root().at("profile2").i == 9876);

        {
            ofstream f(file);
            f << R"({ "profile2": { "b": false, "s": "bar" } })";
            f.flush();
        }

        conf.load();

        REQUIRE(conf.root().count("profile1") == 0);
        REQUIRE(conf.root().at("profile2").b == false);
        REQUIRE(conf.root().at("profile2").s == "bar");
        REQUIRE(conf.root().at("profile2").i == 99);
    }

    SECTION("visitable struct: load restores default value of unsaved members")
    {
        {
            ofstream f(file);
            f << R"({ "b": false })";
            f.flush();
        }
        REQUIRE(filesystem::exists(file));

        jstore::tree<test::visitable> conf(file, on_error);
        REQUIRE(conf.root().b == false);
        REQUIRE(conf.root().s == "string"); /* Default value */
        REQUIRE(conf.root().i == 99); /* Default value */

        /* Change values in memory */
        conf.root().b = true;
        conf.root().s = "foo";
        conf.root().i = 123;

        /* Reload to restore saved values. Unsaved values should revert to defaults. */
        REQUIRE_NOTHROW(conf.load());
        REQUIRE(conf.root().b == false);
        REQUIRE(conf.root().s == "string"); /* Default value */
        REQUIRE(conf.root().i == 99); /* Default value */
    }

} /* load */


TEST_CASE("jstore::save", "[jstore]")
{
    const filesystem::path file = "/tmp/test/jstore/data.json";

    filesystem::remove_all(file);
    filesystem::create_directories(file.parent_path());

    SECTION("no file")
    {
        REQUIRE_FALSE(filesystem::exists(file));

        jstore::tree<map<string, test::visitable>> conf(file, on_error);

        conf.save();
        REQUIRE_FALSE(filesystem::exists(file));
    }

    SECTION("remove existing file when no content")
    {
        {
            ofstream f(file);
            f << R"([ "xxx", "yyy" ])";
            f.flush();
        }
        REQUIRE(filesystem::exists(file));

        jstore::tree<map<string, test::visitable>> conf(file, on_error);

        conf.save();
        REQUIRE_FALSE(filesystem::exists(file));
    }

    SECTION("array: replace existing entries")
    {
        {
            ofstream f(file);
            f << R"([ "xxx", "yyy" ])";
            f.flush();
        }
        REQUIRE(filesystem::exists(file));

        jstore::tree<vector<string>> conf(file, on_error);
        REQUIRE(conf.root().size() == 2);

        conf.root().clear();

        conf.root().push_back("zzz");

        conf.save();
        REQUIRE(filesystem::exists(file));

        json j;
        {
            ifstream f(file);
            f >> j;
        }

        REQUIRE(j == json::parse(R"([ "zzz" ])"));
    }

    SECTION("map: replace existing string keys")
    {
        {
            ofstream f(file);
            f << R"({ "xxx": 1, "yyy": 2 })";
            f.flush();
        }
        REQUIRE(filesystem::exists(file));

        jstore::tree<map<string, int>> conf(file, on_error);
        REQUIRE(conf.root().size() == 2);

        conf.root().clear();

        conf.root()["zzz"] = 3;

        conf.save();
        REQUIRE(filesystem::exists(file));

        json j;
        {
            ifstream f(file);
            f >> j;
        }

        REQUIRE(j == json::parse(R"({ "zzz": 3 })"));
    }

    SECTION("map: replace existing non-string keys")
    {
        {
            ofstream f(file);
            f << R"([ [1, "xxx"], [2, "yyy"] ])";
            f.flush();
        }
        REQUIRE(filesystem::exists(file));

        jstore::tree<map<int, string>> conf(file, on_error);
        REQUIRE(conf.root().size() == 2);

        conf.root().clear();

        conf.root()[3] = "zzz";

        conf.save();
        REQUIRE(filesystem::exists(file));

        json j;
        {
            ifstream f(file);
            f >> j;
        }

        REQUIRE(j == json::parse(R"([ [3, "zzz"] ])"));
    }

    SECTION("map of visitable structs: omit defaults")
    {
        jstore::tree<map<string, test::visitable>> conf(file, on_error);

        conf.root()["profile1"];

        conf.root()["profile2"];

        conf.root()["profile3"];

        conf.root()["profile4"].i = 9876;
        conf.root()["profile4"].j = json::parse(R"([9,9,9])");

        conf.save();
        REQUIRE(filesystem::exists(file));

        json j;
        {
            ifstream f(file);
            f >> j;
        }

        REQUIRE(j == json::parse(R"({ "profile1": {}, "profile2": {}, "profile3": {}, "profile4": { "i": 9876, "j": [9,9,9] } })"));
    }

    SECTION("vector of visitable struct: omit defaults")
    {
        jstore::tree<vector<test::visitable>> conf(file, on_error);

        conf->push_back({});

        conf->push_back({});

        conf->push_back({});

        test::visitable v;
        v.i = 9876;
        v.j = json::parse(R"([9,9,9])");
        conf->push_back(move(v));

        conf.save();
        REQUIRE(filesystem::exists(file));


        json j;
        {
            ifstream f(file);
            f >> j;
        }

        REQUIRE(j == json::parse(R"([ {}, {}, {}, { "i": 9876, "j": [9,9,9] } ])"));
    }

    SECTION("visitable struct: clear saved defaults")
    {
        {
            ofstream f(file);
            f << R"({ "b": true, "s": "string", "i": 9876 })";
            f.flush();
        }
        REQUIRE(filesystem::exists(file));

        jstore::tree<test::visitable> conf(file, on_error);
        REQUIRE(conf.root().b == true);
        REQUIRE(conf.root().s == "string");
        REQUIRE(conf.root().i == 9876);

        conf.save();
        REQUIRE(filesystem::exists(file));


        json j;
        {
            ifstream f(file);
            f >> j;
        }

        REQUIRE(j == json::parse(R"({ "i": 9876 })"));
    }

    SECTION("visitable struct: preserve unknown keys")
    {
        {
            ofstream f(file);
            f << R"({ "s": "foo", "unknownKey": true })";
            f.flush();
        }
        REQUIRE(filesystem::exists(file));

        jstore::tree<test::visitable> conf(file, on_error);
        REQUIRE(conf.root().s == "foo");

        conf.save();
        REQUIRE(filesystem::exists(file));


        json j;
        {
            ifstream f(file);
            f >> j;
        }

        REQUIRE(j == json::parse(R"({ "s": "foo", "unknownKey": true })"));
    }

} /* save */
