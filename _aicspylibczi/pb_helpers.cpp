#include <memory>
#include <set>
#include <vector>

#include "Reader.h"
#include "exceptions.h"
#include "pb_helpers.h"

namespace pb_helpers {

std::vector<std::pair<char, size_t>>
getAndFixShape(pylibczi::ImagesContainerBase* bptr_)
{
  auto images = bptr_->images();
  images.sort();
  auto charSizes = images.getShape();
  return charSizes;
}

nb::object
packArray(pylibczi::ImagesContainerBase::ImagesContainerBasePtr& base_ptr_)
{
  pylibczi::ImagesContainerBase* icBase = base_ptr_.release();
  std::vector<std::pair<char, size_t>> charSizes;

  charSizes = getAndFixShape(icBase);
  nb::object arr;

  switch (icBase->pixelType()) {
    case libCZI::PixelType::Gray8:
    case libCZI::PixelType::Bgr24:
      arr = memoryToNpArray<uint8_t>(icBase, charSizes);
      break;
    case libCZI::PixelType::Gray16:
    case libCZI::PixelType::Bgr48:
      arr = memoryToNpArray<uint16_t>(icBase, charSizes);
      break;
    case libCZI::PixelType::Gray32:
      arr = memoryToNpArray<uint32_t>(icBase, charSizes);
      break;
    case libCZI::PixelType::Gray32Float:
    case libCZI::PixelType::Bgr96Float:
      arr = memoryToNpArray<float>(icBase, charSizes);
      break;
    default:
      throw pylibczi::PixelTypeException(icBase->pixelType(), "Unsupported pixel type in helper function.");
      // It is highly unlikely the this throw would ever be reached but it's here for completeness. If an unsupported
      // pixel type were encountered it would throw much eariler in the code.
  }
  return arr;
}

nb::list
packStringArray(pylibczi::SubblockMetaVec& metadata_)
{
  metadata_.sort();
  auto charSizes = metadata_.getShape();
  nb::list mylist;
  try {
    for (const auto& x : metadata_) {
      mylist.append(nb::make_tuple(x.getDimsAsChars(), x.getString()));
    }
  } catch (exception& e) {
    std::cout << e.what() << std::endl;
  }
  return mylist;
}

}
