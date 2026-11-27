#include "ImageFactory.h"

#include "TypedImage.h"
#include <memory>

namespace pylibczi {

ImageFactory::CtorMap ImageFactory::s_pixelToImageConstructor{
  { PixelType::Gray8,
    [](std::vector<size_t> shape_,
       PixelType pixel_type_,
       const libCZI::CDimCoordinate* plane_coordinate_,
       libCZI::IntRect box_,
       ImagesContainerBase* bptr,
       size_t mem_index_,
       int index_m_) {
      auto typedPtr = bptr->getBaseAsTyped<uint8_t>();
      return std::shared_ptr<TypedImage<uint8_t>>(new TypedImage<uint8_t>(
        std::move(shape_), pixel_type_, plane_coordinate_, box_, typedPtr->getPointerAtIndex(mem_index_), index_m_));
    } },
  { PixelType::Bgr24,
    [](std::vector<size_t> shape_,
       PixelType pixel_type_,
       const libCZI::CDimCoordinate* plane_coordinate_,
       libCZI::IntRect box_,
       ImagesContainerBase* bptr,
       size_t mem_index_,
       int index_m_) {
      auto typedPtr = bptr->getBaseAsTyped<uint8_t>();
      return std::shared_ptr<TypedImage<uint8_t>>(new TypedImage<uint8_t>(std::move(shape_),
                                                                          PixelType::Gray8,
                                                                          plane_coordinate_,
                                                                          box_,
                                                                          typedPtr->getPointerAtIndex(mem_index_),
                                                                          index_m_));
    } },
  { PixelType::Gray16,
    [](std::vector<size_t> shape_,
       PixelType pixel_type_,
       const libCZI::CDimCoordinate* plane_coordinate_,
       libCZI::IntRect box_,
       ImagesContainerBase* bptr,
       size_t mem_index_,
       int index_m_) {
      auto typedPtr = bptr->getBaseAsTyped<uint16_t>();
      return std::shared_ptr<TypedImage<uint16_t>>(new TypedImage<uint16_t>(
        std::move(shape_), pixel_type_, plane_coordinate_, box_, typedPtr->getPointerAtIndex(mem_index_), index_m_));
    } },
  { PixelType::Gray32,
    [](std::vector<size_t> shape_,
       PixelType pixel_type_,
       const libCZI::CDimCoordinate* plane_coordinate_,
       libCZI::IntRect box_,
       ImagesContainerBase* bptr,
       size_t mem_index_,
       int index_m_) {
      auto typedPtr = bptr->getBaseAsTyped<uint32_t>();
      return std::shared_ptr<TypedImage<uint32_t>>(new TypedImage<uint32_t>(
        std::move(shape_), pixel_type_, plane_coordinate_, box_, typedPtr->getPointerAtIndex(mem_index_), index_m_));
    } },
  { PixelType::Gray32,
    [](std::vector<size_t> shape_,
       PixelType pixel_type_,
       const libCZI::CDimCoordinate* plane_coordinate_,
       libCZI::IntRect box_,
       ImagesContainerBase* bptr,
       size_t mem_index_,
       int index_m_) {
      auto typedPtr = bptr->getBaseAsTyped<uint32_t>();
      return std::shared_ptr<TypedImage<uint32_t>>(new TypedImage<uint32_t>(
        std::move(shape_), pixel_type_, plane_coordinate_, box_, typedPtr->getPointerAtIndex(mem_index_), index_m_));
    } },
  { PixelType::Bgr48,
    [](std::vector<size_t> shape_,
       PixelType pixel_type_,
       const libCZI::CDimCoordinate* plane_coordinate_,
       libCZI::IntRect box_,
       ImagesContainerBase* bptr,
       size_t mem_index_,
       int index_m_) {
      auto typedPtr = bptr->getBaseAsTyped<uint16_t>();
      return std::shared_ptr<TypedImage<uint16_t>>(new TypedImage<uint16_t>(std::move(shape_),
                                                                            PixelType::Gray16,
                                                                            plane_coordinate_,
                                                                            box_,
                                                                            typedPtr->getPointerAtIndex(mem_index_),
                                                                            index_m_));
    } },
  { PixelType::Gray32Float,
    [](std::vector<size_t> shape_,
       PixelType pixel_type_,
       const libCZI::CDimCoordinate* plane_coordinate_,
       libCZI::IntRect box_,
       ImagesContainerBase* bptr,
       size_t mem_index_,
       int index_m_) {
      auto typedPtr = bptr->getBaseAsTyped<float>();
      return std::shared_ptr<TypedImage<float>>(new TypedImage<float>(
        std::move(shape_), pixel_type_, plane_coordinate_, box_, typedPtr->getPointerAtIndex(mem_index_), index_m_));
    } },
  { PixelType::Bgr96Float,
    [](std::vector<size_t> shape_,
       PixelType pixel_type_,
       const libCZI::CDimCoordinate* plane_coordinate_,
       libCZI::IntRect box_,
       ImagesContainerBase* bptr,
       size_t mem_index_,
       int index_m_) {
      auto typedPtr = bptr->getBaseAsTyped<float>();
      return std::shared_ptr<TypedImage<float>>(new TypedImage<float>(std::move(shape_),
                                                                      PixelType::Gray32Float,
                                                                      plane_coordinate_,
                                                                      box_,
                                                                      typedPtr->getPointerAtIndex(mem_index_),
                                                                      index_m_));
    } }
};

size_t
ImageFactory::sizeOfPixelType(PixelType pixel_type_)
{
  switch (pixel_type_) {
    case PixelType::Gray8:
    case PixelType::Bgr24:
      return sizeof(uint8_t);
    case PixelType::Gray16:
    case PixelType::Bgr48:
      return sizeof(uint16_t);
    case PixelType::Gray32:
      return sizeof(uint32_t);
    case PixelType::Gray32Float:
    case PixelType::Bgr96Float:
      return sizeof(float);
    default:
      throw PixelTypeException(pixel_type_, "sizeOfPixelType: Pixel Type unsupported by libCZI.");
  }
}

size_t
ImageFactory::numberOfSamples(libCZI::PixelType pixel_type_)
{
  switch (pixel_type_) {
    case PixelType::Gray8:
    case PixelType::Gray16:
    case PixelType::Gray32:
    case PixelType::Gray32Float:
      return 1;
    case PixelType::Bgr24:
    case PixelType::Bgr48:
    case PixelType::Bgr96Float:
      return 3;
    case PixelType::Invalid:
      return 0;
    default:
      throw PixelTypeException(pixel_type_, "numberOfSamples: Pixel Type unsupported by libCZI.");
  }
}

std::shared_ptr<Image>
ImageFactory::constructImage(const std::shared_ptr<libCZI::IBitmapData>& bitmap_ptr_,
                             libCZI::IntSize size_,
                             const libCZI::CDimCoordinate* plane_coordinate_,
                             libCZI::IntRect box_,
                             size_t mem_index_,
                             int index_m_)
{
  PixelType pixelType = bitmap_ptr_->GetPixelType();

  std::vector<size_t> shape;
  size_t samples_per_pixel = numberOfSamples(pixelType);

  shape.emplace_back(size_.h);
  shape.emplace_back(size_.w);
  if (samples_per_pixel > 1)
    shape.emplace_back(samples_per_pixel);

  auto imageFactoryFunction = s_pixelToImageConstructor.at(pixelType);
  std::shared_ptr<Image> image =
    imageFactoryFunction(shape, pixelType, plane_coordinate_, box_, m_imgContainer.get(), mem_index_, index_m_);
  if (image == nullptr)
    throw std::bad_alloc();
  image->loadImage(bitmap_ptr_, size_, samples_per_pixel);
  m_imgContainer->addImage(image);
  return image;
}

std::vector<std::pair<char, size_t>>
ImageFactory::getFixedShape(void)
{
  /*!
   * @brief In order to deal with the BGR images in a way that doesn't cause bad
   * API behavior the solution I landed upon was to use the CZI dims. What I
   * mean by this is that if queried about the shape of a BGR image treat the
   * BRG channel as 1 although it's actually 3 channels packed together. This
   * function is then for internal used as it expands BGR images by multiplying
   * C by 3. This is different than the shape that is returned but is needed in
   * order to tell numpy the shape of the ndarray memory.
   *
   * This will result in an API change in that shape of a BRG image would come
   * back with 7 channels for example but the numpy ndarray will have 21
   * channels.
   */
  auto images = m_imgContainer->images();
  images.sort();
  auto charSizes = images.getShape();
  return charSizes;
}

}
