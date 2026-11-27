#ifndef _PYLIBCZI_PB_CASTER_IMAGESCONTAINER_H
#define _PYLIBCZI_PB_CASTER_IMAGESCONTAINER_H

#include "ImagesContainer.h"
#include "pb_helpers.h"

namespace nanobind {
namespace detail {
template<>
struct type_caster<pylibczi::ImagesContainerBase::ImagesContainerBasePtr>
{
  NB_TYPE_CASTER(pylibczi::ImagesContainerBase::ImagesContainerBasePtr, const_name("numpy.ndarray"))

  /** Python->C++ is not supported, this caster only returns images to python. */
  bool from_python(handle, uint8_t, cleanup_list*) noexcept { return false; }

  /** C++->Python: convert the images in the container to a numpy.ndarray. */
  static handle from_cpp(pylibczi::ImagesContainerBase::ImagesContainerBasePtr&& src_, rv_policy, cleanup_list*)
  {
    return pb_helpers::packArray(src_).release();
  }
};
}
} // namespace nanobind::detail

#endif //_PYLIBCZI_PB_CASTER_IMAGEVECTOR_H
