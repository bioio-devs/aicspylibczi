#ifndef _PYLIBCZI_PB_HELPERS_H
#define _PYLIBCZI_PB_HELPERS_H

#include <iostream>
#include <memory>
#include <nanobind/nanobind.h>
#include <nanobind/ndarray.h>
#include <nanobind/stl/map.h>
#include <nanobind/stl/pair.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
#include <set>
#include <vector>

#include "Image.h"
#include "ImageFactory.h"
#include "Reader.h"
#include "exceptions.h"

namespace nb = nanobind;
namespace pb_helpers {

nb::object
packArray(pylibczi::ImagesContainerBase::ImagesContainerBasePtr& base_ptr_);
nb::list
packStringArray(pylibczi::SubblockMetaVec& metadata_);
std::vector<std::pair<char, size_t>>
getAndFixShape(pylibczi::ImagesContainerBase* bptr_);

template<typename T>
nb::object
memoryToNpArray(pylibczi::ImagesContainerBase* bptr_, std::vector<std::pair<char, size_t>>& charSizes_)
{
  pylibczi::ImagesContainer<T>* tptr = bptr_->getBaseAsTyped<T>();

  std::vector<size_t> shape(charSizes_.size(), 0);
  std::transform(
    charSizes_.begin(), charSizes_.end(), shape.begin(), [](const std::pair<char, size_t>& a_) { return a_.second; });

  T* mptr = tptr->releaseMemory();

  nb::capsule freeWhenDone(mptr, [](void* f_) noexcept {
    T* ptr = reinterpret_cast<T*>(f_);
    delete[] ptr;
  });

  return nb::cast(nb::ndarray<nb::numpy, T>(mptr, shape.size(), shape.data(), freeWhenDone));
}

}

#endif //_PYLIBCZI_PB_HELPERS_H
