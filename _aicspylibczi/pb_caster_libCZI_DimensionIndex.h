#ifndef _PYLIBCZI_PB_CASTER_LIBCZI_DIMENSIONINDEX_H
#define _PYLIBCZI_PB_CASTER_LIBCZI_DIMENSIONINDEX_H

#include <nanobind/nanobind.h>

#include "inc_libCZI.h"

namespace nanobind {
namespace detail {
template<>
struct type_caster<libCZI::DimensionIndex>
{
  NB_TYPE_CASTER(libCZI::DimensionIndex, const_name("str"))

  /** Python->C++: the first character of a python str selects the dimension. */
  bool from_python(handle src_, uint8_t, cleanup_list*) noexcept
  {
    const char* letterPtr = PyUnicode_AsUTF8(src_.ptr());
    if (letterPtr == nullptr) {
      PyErr_Clear();
      return false;
    }
    value = libCZI::Utils::CharToDimension(letterPtr[0]);
    return value != libCZI::DimensionIndex::invalid;
  }

  /** C++->Python: the dimension as a one character python str. */
  static handle from_cpp(libCZI::DimensionIndex src_, rv_policy, cleanup_list*) noexcept
  {
    std::string tmp(1, libCZI::Utils::DimensionToChar(src_));
    return PyUnicode_FromString(tmp.c_str());
  }
};
}
} // namespace nanobind::detail

#endif //_PYLIBCZI_PB_CASTER_LIBCZI_DIMENSIONINDEX_H
