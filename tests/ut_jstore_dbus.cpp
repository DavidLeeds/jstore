/*
 * Copyright (c) 2024 David Leeds <davidesleeds@gmail.com>
 *
 * jstore is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include <jstore.hpp>

#include <sdbus-c++/sdbus-c++.h>

#include <catch2/catch_test_macros.hpp>

using namespace std;
using json = nlohmann::json;

static const string SERVICE = "io.davidleeds.Test.JStore";
static const string OBJECT = "/io/davidleeds/Test/JStore";
static const filesystem::path file = "/tmp/test/jstore/data.json";

struct jstore_proxy {
    jstore_proxy(sdbus::IConnection &conn) :
        proxy_(sdbus::createProxy(conn, SERVICE, OBJECT))
    {
        proxy_->uponSignal("ValuesChanged")
                .onInterface(jstore::DBUS_INTERFACE)
                .call([this](const map<string, string> &values) {
                        last_values_changed = values;
                });

        proxy_->finishRegistration();
    }

    string Get(const string &path)
    {
        string result;
        proxy_->callMethod("Get")
                .onInterface(jstore::DBUS_INTERFACE)
                .withArguments(path)
                .storeResultsTo(result);
        return result;
    }

    map<string, string> GetAll()
    {
        map<string, string> result;
        proxy_->callMethod("GetAll")
                .onInterface(jstore::DBUS_INTERFACE)
                .storeResultsTo(result);
        return result;
    }

    void Set(const string &path, const string &value)
    {
        proxy_->callMethod("Set")
                .onInterface(jstore::DBUS_INTERFACE)
                .withArguments(path, value);
    }


    map<string, string> last_values_changed;

private:
    unique_ptr<sdbus::IProxy> proxy_;
};

namespace test_dbus {

/*
 * Visitable struct
 */
struct visitable {
    bool b              = true;
    string s            = "string";
    int i               = 99;
    json j              = json::parse(R"({ "word": "foo", "list": [ 1, 2, 3] })");
    list<int> a         = { 1, 2, 3 };
    map<string, int> m  = { { "x", 11 }, { "y", 22 } };
    map<int, map<string, int>> m2 = {
            { 1, { { "a", 1} } },
            { 2, { { "b", 2} } }
    };
};

} /* namespace test */

VISITABLE_STRUCT(test_dbus::visitable, b, s, i, j, a, m, m2);


TEST_CASE("jstore::dbus", "[jstore]") {

    filesystem::remove_all(file);
    filesystem::create_directories(file.parent_path());

    /* Run test service independently on its own thread */
    unique_ptr<sdbus::IConnection> dbus{sdbus::createSessionBusConnection(SERVICE)};
    dbus->enterEventLoopAsync();

    jstore::tree<test_dbus::visitable> conf{file};

    /* Register D-Bus bindings */
    REQUIRE_NOTHROW(conf.register_dbus(*dbus, OBJECT));

    /* Create D-Bus proxy to interact with the service */
    jstore_proxy proxy{*dbus};


    SECTION("Get") {
        /* Get default values */
        REQUIRE(proxy.Get("") == R"({"a":[1,2,3],"b":true,"i":99,"j":{"list":[1,2,3],"word":"foo"},"m":{"x":11,"y":22},"m2":[[1,{"a":1}],[2,{"b":2}]],"s":"string"})");
        REQUIRE(proxy.Get("b") == R"(true)");
        REQUIRE(proxy.Get("s") == R"("string")");
        REQUIRE(proxy.Get("i") == R"(99)");
        REQUIRE(proxy.Get("j") == conf->j.dump());
        REQUIRE(proxy.Get("a") == R"([1,2,3])");
        REQUIRE(proxy.Get("m") == R"({"x":11,"y":22})");
        REQUIRE(proxy.Get("m2") == R"([[1,{"a":1}],[2,{"b":2}]])");

        /* Access array elements */
        REQUIRE(proxy.Get("a/0") == R"(1)");
        REQUIRE(proxy.Get("a/2") == R"(3)");

        /* Access children of map values */
        REQUIRE(proxy.Get("m/x") == R"(11)");
        REQUIRE(proxy.Get("m/y") == R"(22)");
        REQUIRE(proxy.Get("m2/1/a") == R"(1)");
        REQUIRE(proxy.Get("m2/2/b") == R"(2)");

        /* Change some values */
        conf->b = false;
        conf->s = "foo";
        REQUIRE(proxy.Get("b") == R"(false)");
        REQUIRE(proxy.Get("s") == R"("foo")");

        /* Throw on non-existent value access */
        REQUIRE_THROWS(proxy.Get("nonexistent"));
        REQUIRE_THROWS(proxy.Get("a/3"));
        REQUIRE_THROWS(proxy.Get("m/z"));
        REQUIRE_THROWS(proxy.Get("m2/1/c"));
    }

    SECTION("GetAll") {
        REQUIRE(proxy.GetAll().at("b") == R"(true)");
        REQUIRE(proxy.GetAll().at("s") == R"("string")");
        REQUIRE(proxy.GetAll().at("i") == R"(99)");
        REQUIRE(proxy.GetAll().at("j") == conf->j.dump());
        /* GetAll returns the entire array */
        REQUIRE(proxy.GetAll().at("a") == R"([1,2,3])");
        REQUIRE(proxy.GetAll().at("m/x") == R"(11)");
        /* GetAll returns individual map keys */
        REQUIRE(proxy.GetAll().at("m/x") == R"(11)");
        REQUIRE(proxy.GetAll().at("m/y") == R"(22)");
        REQUIRE(proxy.GetAll().at("m2/1/a") == R"(1)");
        REQUIRE(proxy.GetAll().at("m2/2/b") == R"(2)");
    }

    SECTION("Set") {
        /* Set existing members */
        REQUIRE_NOTHROW(proxy.Set("b", R"(false)"));
        REQUIRE(conf->b == false);

        REQUIRE_NOTHROW(proxy.Set("s", R"("foo")"));
        REQUIRE(conf->s == "foo");

        REQUIRE_NOTHROW(proxy.Set("i", R"(123)"));
        REQUIRE(conf->i == 123);

        REQUIRE_NOTHROW(proxy.Set("j", R"({"fruit":"apple"})"));
        REQUIRE(conf->j == json::parse(R"({"fruit":"apple"})"));

        REQUIRE_NOTHROW(proxy.Set("a", R"([10,20,30,40])"));
        REQUIRE(conf->a == list<int>{ 10, 20, 30, 40 });

        REQUIRE_NOTHROW(proxy.Set("a/0", R"(100)"));
        REQUIRE(conf->a == list<int>{ 100, 20, 30, 40 });

        REQUIRE_NOTHROW(proxy.Set("m", R"({"x":100,"y":200})"));
        REQUIRE(conf->m == map<string, int>{ { "x", 100 }, { "y", 200 } });

        REQUIRE_NOTHROW(proxy.Set("m/x", R"(1000)"));
        REQUIRE(conf->m.at("x") == 1000);

        /* Map key insertion supported on Set */
        REQUIRE_NOTHROW(proxy.Set("m/z", R"(123)"));
        REQUIRE(conf->m.at("z") == 123);

        REQUIRE_NOTHROW(proxy.Set("m2/3/c", R"(999)"));
        REQUIRE(conf->m2.at(3).at("c") == 999);

        /* Throw on set at unreachable or invalid paths */
        REQUIRE_THROWS(proxy.Set("nonexistent", ""));
        REQUIRE_THROWS(proxy.Set("b/1", ""));
        REQUIRE_THROWS(proxy.Set("a/4", ""));
        REQUIRE_THROWS(proxy.Set("m2/foo", R"({"a":"1})"));
        REQUIRE_NOTHROW(proxy.Set("m2/3/c", R"(999)"));
    }

    SECTION("ValuesChanged") {
        REQUIRE(proxy.last_values_changed.empty());

        SECTION("emit 1-N leaf nodes") {
            conf.emit_values_changed(conf->b, conf->s, conf->i);

            /* Wait for signal to be handled */
            for (size_t i = 0; i < 100; ++i) {
                this_thread::sleep_for(10ms);
                if (!proxy.last_values_changed.empty()) {
                    break;
                }
            }

            REQUIRE(proxy.last_values_changed.size() == 3);
            REQUIRE(proxy.last_values_changed.at("b") == R"(true)");
            REQUIRE(proxy.last_values_changed.at("s") == R"("string")");
            REQUIRE(proxy.last_values_changed.at("i") == R"(99)");
        }

        SECTION("emit non-leaf nodes") {
            conf.emit_values_changed(conf->a, conf->m);

            /* Wait for signal to be handled */
            for (size_t i = 0; i < 100; ++i) {
                this_thread::sleep_for(10ms);
                if (!proxy.last_values_changed.empty()) {
                    break;
                }
            }

            REQUIRE(proxy.last_values_changed.size() == 3);

            /* ValuesChanged returns full array contents */
            REQUIRE(proxy.last_values_changed.at("a") == R"([1,2,3])");

            /* ValuesChanged returns individual map keys */
            REQUIRE(proxy.last_values_changed.at("m/x") == R"(11)");
            REQUIRE(proxy.last_values_changed.at("m/y") == R"(22)");
        }

        SECTION("emit empty non-leaf nodes") {
            conf->a.clear();
            conf->m.clear();

            conf.emit_values_changed(conf->a, conf->m);

            /* Wait for signal to be handled */
            for (size_t i = 0; i < 100; ++i) {
                this_thread::sleep_for(10ms);
                if (!proxy.last_values_changed.empty()) {
                    break;
                }
            }

            REQUIRE(proxy.last_values_changed.size() == 2);

            /* ValuesChanged returns full array contents */
            REQUIRE(proxy.last_values_changed.at("a") == R"([])");

            /* ValuesChanged returns full map when the map is empty */
            REQUIRE(proxy.last_values_changed.at("m") == R"({})");
        }

        SECTION("emit all") {
            conf.emit_values_changed(conf.root());

            /* Wait for signal to be handled */
            for (size_t i = 0; i < 100; ++i) {
                this_thread::sleep_for(10ms);
                if (!proxy.last_values_changed.empty()) {
                    break;
                }
            }

            /* ValuesChanged pushes the lowest individually representable elements */
            REQUIRE(proxy.last_values_changed.at("b") == R"(true)");
            REQUIRE(proxy.last_values_changed.at("s") == R"("string")");
            REQUIRE(proxy.last_values_changed.at("i") == R"(99)");
            REQUIRE(proxy.last_values_changed.at("j") == conf->j.dump());
            REQUIRE(proxy.last_values_changed.at("a") == R"([1,2,3])");
            REQUIRE(proxy.last_values_changed.at("m/x") == R"(11)");
            REQUIRE(proxy.last_values_changed.at("m/y") == R"(22)");
            REQUIRE(proxy.last_values_changed.at("m2/1/a") == R"(1)");
            REQUIRE(proxy.last_values_changed.at("m2/2/b") == R"(2)");
        }
    }

} /* dbus */

