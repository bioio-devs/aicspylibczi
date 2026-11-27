#ifndef _PYLIBCZI_PB_CASTER_SUBBLOCKMETAVEC_H
#define _PYLIBCZI_PB_CASTER_SUBBLOCKMETAVEC_H

#include "SubblockMetaVec.h"
#include "pb_helpers.h"

namespace nanobind {
namespace detail {
template<>
struct type_caster<pylibczi::SubblockMetaVec>
{
  NB_TYPE_CASTER(pylibczi::SubblockMetaVec, const_name("List"))

  /** Python->C++ is not supported, this caster only returns metadata to python. */
  bool from_python(handle, uint8_t, cleanup_list*) noexcept { return false; }

  /** C++->Python: convert the subblock metadata to a list of (dims, xml string) tuples. */
  static handle from_cpp(pylibczi::SubblockMetaVec src_, rv_policy, cleanup_list*)
  {
    return pb_helpers::packStringArray(src_).release();
  }
};
}
}
#endif //_PYLIBCZI_PB_CASTER_SUBBLOCKMETAVEC_H
