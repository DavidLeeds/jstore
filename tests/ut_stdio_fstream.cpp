/*
 * Copyright (c) 2024 David Leeds <davidesleeds@gmail.com>
 *
 * jstore is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include <jstore/stdio_fstream.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace std;
using namespace jstore;

struct temp_file {
    temp_file(filesystem::path path) :
        path_(move(path))
    {
        cleanup();
        filesystem::create_directories(path_.parent_path());
    }

    ~temp_file()
    {
        cleanup();
    }

    const filesystem::path &path() const
    {
        return path_;
    }

private:
    void cleanup()
    {
        error_code ec;
        filesystem::remove_all(path_, ec);
    }

    filesystem::path path_;
};


TEST_CASE("jstore::stdio_fstream", "[jstore]")
{
    temp_file test_file { filesystem::temp_directory_path() / "test" / "stdio_fstream" / "file" };

    SECTION("default constructor")
    {
        stdio_fstream s;

        REQUIRE_FALSE(s.is_open());
        REQUIRE(s.fd() == -1);

        /* Sets failure state after stream operation on closed stream */
        REQUIRE(s.good());
        s << 99;
        REQUIRE_FALSE(s.good());
    }

    SECTION("open/close")
    {
        stdio_fstream s(test_file.path());

        REQUIRE(s.is_open());
        REQUIRE(s.fd() > -1);

        int fd = s.fd();

        s.close();
        REQUIRE_FALSE(s.is_open());
        REQUIRE(s.fd() == -1);

        s.open(test_file.path());
        REQUIRE(s.is_open());
        REQUIRE(s.fd() == fd); /* Assume the same fd is used if no leak occurred */
    }

    SECTION("assign/close")
    {
        FILE *fp = fopen(test_file.path().c_str(), "w+");

        REQUIRE(fp != nullptr);

        stdio_fstream s(fp);
        REQUIRE(s.is_open());
        REQUIRE(s.fd() == fileno(fp));

        REQUIRE(s.good());
        s << 99;
        REQUIRE(s.good());

        s.close();
        REQUIRE_FALSE(s.is_open());
        REQUIRE(s.fd() == -1);

        s.assign(fp);
        REQUIRE(s.is_open());
        REQUIRE(s.fd() == fileno(fp));

        fclose(fp);

        REQUIRE(s.good());
        s << 99;
        REQUIRE(s.good());
    }

    SECTION("write")
    {
        stdio_fstream s(test_file.path(), ios_base::out);

        REQUIRE(s.is_open());
        REQUIRE(s.fd() > -1);
        REQUIRE(s.good());

        s << 99;
        REQUIRE(s.good());
        s.close();

        /* Check the content using a standard fstream */
        ifstream f(test_file.path());
        int input = 0;
        f >> input;
        REQUIRE_FALSE(f.fail());
        REQUIRE(input == 99);
    }

    SECTION("read")
    {
        /* Write content using standard fstream */
        ofstream f(test_file.path());
        f << 99;
        REQUIRE(f.good());
        f.close();

        stdio_fstream s(test_file.path(), ios_base::in);

        REQUIRE(s.is_open());
        REQUIRE(s.fd() > -1);
        REQUIRE(s.good());

        /* Expect write fail when in read mode */
        s << 99;
        REQUIRE(s.fail());
        s.clear();

        int input = 0;
        s >> input;
        REQUIRE_FALSE(s.fail());
        REQUIRE(input == 99);
    }

    SECTION("append")
    {
        const size_t max_count = 100;

        for (size_t i = 0; i < max_count; ++i) {
            stdio_fstream s(test_file.path(), ios_base::out | ios_base::app);

            s << i;
            s << " "; /* Whitespace delimiter */
            REQUIRE(s.good());
        }

        stdio_fstream s(test_file.path(), ios_base::in);

        for (size_t i = 0; i < max_count; ++i) {
            REQUIRE(s.good());

            size_t input = 0;
            s >> input;
            REQUIRE(input == i);
        }
    }

    SECTION("truncate")
    {
        /* Write content using standard fstream */
        {
            ofstream f(test_file.path());
            f << 99;
            REQUIRE(f.good());
        }

        /* Open in read/write mode (no truncation) */
        {
            stdio_fstream s(test_file.path(), ios_base::in | ios_base::out);
            REQUIRE(s.is_open());
            REQUIRE(s.good());
        }

        /* Expect content untouched */
        {
            ifstream f(test_file.path());
            int input = 0;
            f >> input;
            REQUIRE_FALSE(f.fail());
            REQUIRE(input == 99);
        }

        /* Open in write-only mode (defaults to truncate) */
        {
            stdio_fstream s(test_file.path(), ios_base::out);
            REQUIRE(s.is_open());
            REQUIRE(s.good());
        }

        /* Expect content truncated */
        {
            ifstream f(test_file.path());
            f.seekg(0, ios_base::end);
            REQUIRE(f.tellg() == 0);
        }

        /* Write content using standard fstream */
        {
            ofstream f(test_file.path());
            f << 999;
            REQUIRE(f.good());
        }

        /* Open in read/write/truncate mode (explicit truncation) */
        {
            stdio_fstream s(test_file.path(), ios_base::in | ios_base::out | ios_base::trunc);
            REQUIRE(s.is_open());
            REQUIRE(s.good());
        }

        /* Expect content truncated */
        {
            ifstream f(test_file.path());
            f.seekg(0, ios_base::end);
            REQUIRE(f.tellg() == 0);
        }
    }

    SECTION("fsync")
    {
        const size_t max_count = 1000;

        for (size_t i = 0; i < max_count; ++i) {
            filesystem::path temp_path(test_file.path().string() + "." + to_string(i));

            stdio_fstream s(temp_path, ios_base::out);
            s << i;
            s.fsync();
            REQUIRE(s.good());

            REQUIRE_NOTHROW(filesystem::rename(temp_path, test_file.path()));

            /* Check the content using a standard fstream */
            ifstream f(test_file.path());
            size_t input = ~0;
            f >> input;
            REQUIRE_FALSE(f.fail());
            REQUIRE(input == i);
        }
    }

    SECTION("move constructor")
    {
        stdio_fstream s(test_file.path());
        int fd = s.fd();

        stdio_fstream s2(move(s));

        REQUIRE_FALSE(s.is_open());
        REQUIRE(s.fd() == -1);
        REQUIRE(s2.is_open());
        REQUIRE(s2.fd() == fd);
        REQUIRE(s2.good());

        s2 << 99;
        REQUIRE(s2.good());
    }

    SECTION("move assignment")
    {
        stdio_fstream s(test_file.path());
        int fd = s.fd();

        stdio_fstream s2;

        s2 = move(s);

        REQUIRE_FALSE(s.is_open());
        REQUIRE(s.fd() == -1);
        REQUIRE(s2.is_open());
        REQUIRE(s2.fd() == fd);
        REQUIRE(s2.good());

        s2 << 99;
        REQUIRE(s2.good());
    }

}
