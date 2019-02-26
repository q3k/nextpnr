/*
 *  nextpnr -- Next Generation Place and Route
 *
 *  Copyright (C) 2019  Serge Bazanski <q3k@symbioticeda.com>
 *
 *  Permission to use, copy, modify, and/or distribute this software for any
 *  purpose with or without fee is hereby granted, provided that the above
 *  copyright notice and this permission notice appear in all copies.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 *  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 *  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 *  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 *  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 *  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */

#ifndef SPARSEVECTOR_H
#define SPARSEVECTOR_H

#include <sys/mman.h>
#include <stdio.h>

// This file implements an mmap MAP_ANON based vector-like structure.
// Compared to a classic stl::vector, when pushing elements it will never have
// to reallocate and copy over elements - instead, it will continue mmaping
// more ANON storage at the end of the array.

NEXTPNR_NAMESPACE_BEGIN

template <typename T> class SparseVector;

template <typename T> class SparseVectorIterator
{
  public:
    SparseVectorIterator operator++()
    {
        cursor_++;
        return *this;
    }

    //SparseVectorIterator operator++(int)
    //{
    //    SparseVectorIterator prior(*this);
    //    cursor_++;
    //    return *this;
    //}

    bool operator!=(const SparseVectorIterator &other) const
    {
        return cursor_ != other.cursor_;
    }

    bool operator==(const SparseVectorIterator &other) const
    {
        return cursor_ == other.cursor_;
    }

    T operator*() const
    {
        return vector_->get(cursor_);
    }

    SparseVectorIterator(const SparseVector<T> *v, unsigned long long int c) : cursor_(c), vector_(v)
    {
    }

  protected:
    unsigned long long int cursor_;
    const SparseVector<T> *vector_;
};

template <typename T> class SparseVector
{
  public:
    SparseVector(size_t initial_allocation=4000000000) : start_(nullptr), cursor_(0)
    {
        length_ = initial_allocation;
        start_ = (T *)mmap(NULL, length_, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
        NPNR_ASSERT((void *)start_ != MAP_FAILED);
    }
    ~SparseVector()
    {
        if (start_ != nullptr) {
            munmap(start_, length_);
            start_ = nullptr;
        }
    }
    void push_back(const T& elem)
    {
        NPNR_ASSERT((cursor_ * sizeof(T)) < length_);
        start_[cursor_++] = elem;
    }
    T get(unsigned long long int index) const
    {
        return start_[index];
    }
    SparseVectorIterator<T> begin() const
    {
        return SparseVectorIterator<T>(this, 0);
    }
    SparseVectorIterator<T> end() const
    {
        return SparseVectorIterator<T>(this, cursor_);
    }

  private:
    mutable T *start_;
    size_t length_;
    unsigned long long int cursor_;
};

NEXTPNR_NAMESPACE_END

#endif // SPARSEVECTOR_H
