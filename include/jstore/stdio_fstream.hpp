/*
 * Copyright (c) 2024 David Leeds <davidesleeds@gmail.com>
 *
 * jstore is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#pragma once

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <ext/stdio_filebuf.h>
#include <cstdio>
#include <cassert>
#include <filesystem>


namespace jstore {

/*
 * Clone of std::fstream that uses the stdio library internally, and
 * provides full access to the underlying file descriptor.
 */

template <typename CharType, typename Traits = std::char_traits<CharType>>
class basic_stdio_fstream : public std::basic_iostream<CharType, Traits> {
    /* Non-standard types */
    using filebuf_type  = __gnu_cxx::stdio_filebuf<CharType, Traits>;
    using iostream_type = std::basic_iostream<CharType, Traits>;

public:
    /* Types */
    using char_type     = CharType;
    using traits_type   = Traits;
    using int_type      = typename traits_type::int_type;
    using pos_type      = typename traits_type::pos_type;
    using off_type      = typename traits_type::off_type;

    /*
     *  Default constructor.
     */
    basic_stdio_fstream() :
        iostream_type(), filebuf_()
    {
        this->init(&filebuf_);
    }

    /*
     * Create an input/output file stream.
     */
    explicit basic_stdio_fstream(const char *path,
            std::ios_base::openmode mode = std::ios_base::in | std::ios_base::out) :
        iostream_type(), filebuf_()
    {
        this->init(&filebuf_);
        this->open(path, mode);
    }

    /*
     * Create an input/output file stream.
     */
    explicit basic_stdio_fstream(const std::string &path,
            std::ios_base::openmode mode = std::ios_base::in | std::ios_base::out) :
        basic_stdio_fstream(path.c_str(), mode)
    {
    }

    /*
     * Create an input/output file stream.
     */
    basic_stdio_fstream(const std::filesystem::path &path,
            std::ios_base::openmode mode = std::ios_base::in | std::ios_base::out) :
        basic_stdio_fstream(path.c_str(), mode)
    {
    }

    /*
     * Create an input/output file stream and associate it with a valid
     * stdio FILE * stream. The FILE * will NOT be automatically closed
     * on destruction, and its lifetime must be managed independently.
     */
    basic_stdio_fstream(FILE *fp,
            std::ios_base::openmode mode = std::ios_base::in | std::ios_base::out) :
                iostream_type(), filebuf_()
    {
        this->init(&filebuf_);
        this->assign(fp, mode);
    }

    basic_stdio_fstream(const basic_stdio_fstream &) = delete;

    basic_stdio_fstream(basic_stdio_fstream &&rhs) :
        iostream_type(std::move(rhs)), filebuf_(std::move(rhs.filebuf_))
    {
        iostream_type::set_rdbuf(&filebuf_);
    }

    basic_stdio_fstream &operator=(const basic_stdio_fstream &) = delete;

    basic_stdio_fstream &operator=(basic_stdio_fstream &&rhs)
    {
        iostream_type::operator=(std::move(rhs));
        filebuf_ = std::move(rhs.filebuf_);
        return *this;
    }

    void swap(basic_stdio_fstream &rhs)
    {
        iostream_type::swap(rhs);
        filebuf_.swap(rhs.filebuf_);
    }

    /*
     * Return true if the underlying file is open.
     */
    [[nodiscard]] bool is_open() const
    {
        return filebuf_.is_open();
    }

    /*
     * Open a file. Sets basic_ios::failbit on failure.
     */
    void open(const char *path,
            std::ios_base::openmode mode = std::ios_base::in | std::ios_base::out)
    {
        uint32_t flags = O_CLOEXEC;
        bool read = (mode & std::ios_base::in);
        bool write = ((mode & std::ios_base::out) ||
                      (mode & std::ios_base::app) ||
                      (mode & std::ios_base::ate));

        if (read && write) {
            flags |= O_RDWR;
        } else if (read) {
            flags |= O_RDONLY;
        } else if (write) {
            flags |= O_WRONLY;
        } else {
            /* Open mode is required */
            assert(false);
        }

        if (mode & std::ios_base::out) {
            flags |= O_CREAT;
        }

        if (mode & std::ios_base::app) {
            flags |= O_APPEND;
        }

        if (mode == std::ios_base::out || (mode & std::ios_base::trunc)) {
            flags |= O_TRUNC;
        }

        auto fd = ::open(path, flags, DEFFILEMODE);
        if (fd < 0) {
            this->setstate(std::ios_base::failbit);
            return;
        }

        if (mode & std::ios_base::ate) {
            if (::lseek(fd, 0, SEEK_END) < 0) {
                ::close(fd);
                this->setstate(std::ios_base::failbit);
                return;
            }
        }

        try {
            filebuf_type filebuf(fd, mode);

            filebuf_.swap(filebuf);
            this->clear();
        } catch (const std::exception &e) {
            ::close(fd);
            this->setstate(std::ios_base::failbit);
        }
    }

    /*
     * Open a file. Sets basic_ios::failbit on failure.
     */
    void open(const std::string &path,
            std::ios_base::openmode mode = std::ios_base::in | std::ios_base::out)
    {
        open(path.c_str(), mode);
    }

    /*
     * Open a file. Sets basic_ios::failbit on failure.
     */
    void open(const std::filesystem::path &path,
            std::ios_base::openmode mode = std::ios_base::in | std::ios_base::out)
    {
        open(path.c_str(), mode);
    }

    /*
     * Associate the stdio_fstream with a valid stdio FILE * stream. The FILE *
     * will NOT be automatically closed on destruction, and its lifetime must
     * be managed independently.
     * Sets basic_ios::failbit on failure.
     */
    void assign(FILE *fp,
            std::ios_base::openmode mode = std::ios_base::in | std::ios_base::out)
    {
        if (!fp) {
            this->setstate(std::ios_base::failbit);
            return;
        }

        if (mode & std::ios_base::ate) {
            if (::fseek(fp, 0, SEEK_END) < 0) {
                this->setstate(std::ios_base::failbit);
                return;
            }
        }

        try {
            filebuf_type filebuf(fp, mode);

            filebuf_.swap(filebuf);
            this->clear();
        } catch (const std::exception &e) {
            this->setstate(std::ios_base::failbit);
        }
    }

    /*
     *  Close the file. Sets basic_ios::failbit on failure.
     */
    void close()
    {
        if (!filebuf_.close()) {
            this->setstate(std::ios_base::failbit);
        }
    }

    /*
     * Return the underlying file descriptor.
     */
    [[nodiscard]] int fd()
    {
        if (filebuf_.is_open()) {
            return filebuf_.fd();
        }

        return -1;
    }

    /*
     * Flush and write all buffered and cached data to the underlying storage device.
     * This call blocks until the operation is complete.
     */
    void fsync()
    {
        if (filebuf_.is_open()) {
            this->sync();
            if (::fsync(filebuf_.fd()) < 0) {
                this->setstate(std::ios_base::failbit);
            }
        }
    }

private:
    filebuf_type filebuf_;
};


using stdio_fstream     = basic_stdio_fstream<char>;
using stdio_wfstream    = basic_stdio_fstream<wchar_t>;

} /* namespace jstore */
