#ifndef _AICSPYLIBCZI_PB_CASTER_DIMINDEX_H
#define _AICSPYLIBCZI_PB_CASTER_DIMINDEX_H

#include <nanobind/nanobind.h>

#include "DimIndex.h"

namespace nanobind {
namespace detail {
template<>
struct type_caster<pylibczi::DimIndex>
{
  NB_TYPE_CASTER(pylibczi::DimIndex, const_name("str"))

  /** Python->C++: the first character of a python str selects the dimension. */
  bool from_python(handle src_, uint8_t, cleanup_list*) noexcept
  {
    const char* letterPtr = PyUnicode_AsUTF8(src_.ptr());
    if (letterPtr == nullptr) {
      PyErr_Clear();
      return false;
    }
    value = pylibczi::charToDimIndex(letterPtr[0]);
    return value != pylibczi::DimIndex::invalid;
  }

  /** C++->Python: the dimension as a one character python str. */
  static handle from_cpp(pylibczi::DimIndex src_, rv_policy, cleanup_list*) noexcept
  {
    std::string tmp(1, pylibczi::dimIndexToChar(src_));
    return PyUnicode_FromString(tmp.c_str());
  }
};
}
} // namespace nanobind::detail

#endif //_AICSPYLIBCZI_PB_CASTER_DIMINDEX_H
