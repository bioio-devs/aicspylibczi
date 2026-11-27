#ifndef _PYLIBCZI_PB_CASTER_BYTESIO_H
#define _PYLIBCZI_PB_CASTER_BYTESIO_H

#include "CSimpleStreamImplFromFd.h"
#include <nanobind/nanobind.h>

namespace nanobind {
namespace detail {
template<>
struct type_caster<std::shared_ptr<libCZI::IStream>>
{
  NB_TYPE_CASTER(std::shared_ptr<libCZI::IStream>, const_name("IOBufferedReader"))

  /** Python->C++: wrap the file descriptor of a python file object in a libCZI stream. */
  bool from_python(handle src_, uint8_t, cleanup_list*) noexcept
  {
    int fDesc = PyObject_AsFileDescriptor(src_.ptr());
    if (fDesc == -1) {
      PyErr_Clear();
      return false;
    }
    value = std::shared_ptr<libCZI::IStream>(new pb_helpers::CSimpleStreamImplFromFd(fDesc));
    return value != nullptr;
  }
};
}
} // namespace nanobind::detail

#endif //_PYLIBCZI_PB_CASTER_BYTESIO_H
