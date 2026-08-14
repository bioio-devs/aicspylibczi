#include <iterator>
#include <set>
#include <thread>
#include <tuple>
#include <utility>

#include "ImageFactory.h"
#include "ImagesContainer.h"
#include "Reader.h"
#include "StreamImplLockingRead.h"
#include "SubblockMetaVec.h"
#include "Threadpool.h"
#include "exceptions.h"
#include "inc_libCZI.h"

namespace pylibczi {

// this ISteam type needs to be threadsafe like StreamImplLockingRead the examples in libCZI are not threadsafe
Reader::Reader(std::shared_ptr<libCZI::IStream> istream_)
  : m_czireader(new CCZIReader)
  , m_specifyScene(true)
{
  m_czireader->Open(std::move(istream_), nullptr);
  m_statistics = m_czireader->GetStatistics();
  m_pixelType = libCZI::PixelType::Invalid; // get the pixeltype of the first readable subblock

  checkSceneShapes();
}

Reader::Reader(const wchar_t* file_name_)
  : m_czireader(new CCZIReader)
  , m_specifyScene(true)
  , m_pixelType(libCZI::PixelType::Invalid)
{
  std::shared_ptr<libCZI::IStream> sp;
  sp = std::shared_ptr<libCZI::IStream>(new StreamImplLockingRead(file_name_));
  m_czireader->Open(sp, nullptr);
  m_statistics = m_czireader->GetStatistics();
  // create a reference for finding one or more subblock indices from a CDimCoordinate
  checkSceneShapes();
}

void
Reader::checkSceneShapes()
{
  auto dShapes = readDimsRange();
  m_specifyScene = !consistentShape(dShapes);
}

std::string
Reader::readMeta()
{
  auto mds = m_czireader->ReadMetadataSegment();
  auto md = mds->CreateMetaFromMetadataSegment();
  std::string xml = md->GetXml();
  return xml;
}

bool
Reader::isMosaic() const
{
  return (m_statistics.maxMindex > 0);
}

/// @brief get_shape_from_fp returns the Dimensions of a ZISRAW/CZI when provided a ICZIReader object
/// @param czi: a shared_ptr to an initialized CziReader object
/// @return A Python Dictionary as a PyObject*
Reader::DimsShape
Reader::readDimsRange()
{
  DimsShape ans;
  int sceneStart(0), sceneSize(0);
  if (!m_statistics.dimBounds.TryGetInterval(libCZI::DimensionIndex::S, &sceneStart, &sceneSize)) {
    ans.push_back(sceneShape(-1));
    return ans;
  }
  for (int i = sceneStart; i < sceneStart + sceneSize; i++)
    ans.push_back(sceneShape(i));
  if (!m_specifyScene && ans.size() > 1) {
    ans[0][DimIndex::S].second = (*(ans.rbegin()))[DimIndex::S].second;
    ans.resize(1); // remove the exta channels
  }
  return ans;
}

/// @brief get the size of each dimension
/// @return vector of the integer sizes for each dimension
std::vector<int>
Reader::dimSizes()
{
  std::string dString = dimsString();
  if (m_specifyScene)
    return std::vector<int>(dString.size(), -1);

  DimIndexRangeMap tbl;
  m_statistics.dimBounds.EnumValidDimensions([&](libCZI::DimensionIndex di_, int start_, int size_) -> bool {
    tbl.emplace(dimensionIndexToDimIndex(di_),
                std::make_pair(start_, size_)); // changed from [start, end) to be [start, end]
    return true;
  }); // sorts the letters into ascending order by default { Z, C, T, S }
  std::vector<int> ans(tbl.size());
  // use rbegin and rend to change it to ascending order.
  transform(tbl.rbegin(), tbl.rend(), ans.begin(), [](const auto& pr_) { return pr_.second.second; });

  if (isMosaic()) {
    ans.push_back(m_statistics.maxMindex + 1);
  } // The M-index is zero based

  libCZI::IntRect sbsize = getSceneYXSize();
  ans.push_back(sbsize.h);
  ans.push_back(sbsize.w);

  pixelType();
  if (ImageFactory::numberOfSamples(m_pixelType) > 1)
    ans.push_back((int)ImageFactory::numberOfSamples(m_pixelType));

  return ans;
}

std::tuple<bool, int, int>
Reader::scenesStartSize() const
{
  int sceneStart = 0;
  int sceneSize = 0;
  bool scenesDefined = m_statistics.dimBounds.TryGetInterval(libCZI::DimensionIndex::S, &sceneStart, &sceneSize);
  return { scenesDefined, sceneStart, sceneSize };
}

bool
Reader::consistentShape(DimsShape& dShape_)
{
  bool regularShape = true;
  for (int i = 1; regularShape && i < dShape_.size(); i++) {
    for (auto kVal : dShape_[i]) {
      if (kVal.first == DimIndex::S)
        continue;
      auto found = dShape_[0].find(kVal.first);
      if (found == dShape_[0].end())
        regularShape = false;
      else
        regularShape &= (kVal == *found);
    }
  }
  return regularShape;
}

Reader::DimIndexRangeMap
Reader::sceneShape(int scene_index_)
{
  bool sceneBool(false);
  int sceneStart(0), sceneSize(0);
  tie(sceneBool, sceneStart, sceneSize) = scenesStartSize();
  pixelType(); // this may look like it isn't used but it's setting m_pixelType

  DimIndexRangeMap tbl;

  if ((!sceneBool || sceneSize == 1)) {
    // scenes are not defined so the dimBounds define the shape
    m_statistics.dimBounds.EnumValidDimensions([&tbl](libCZI::DimensionIndex di_, int start_, int size_) -> bool {
      tbl.emplace(dimensionIndexToDimIndex(di_),
                  std::make_pair(start_, size_ + start_)); // changed from [start, end) to be [start, end]
      return true;
    });
    if (isMosaic()) {
      tbl.emplace(charToDimIndex('M'), std::make_pair(m_statistics.minMindex, m_statistics.maxMindex + 1));
    }
    auto xySize = getSceneYXSize();
    tbl.emplace(charToDimIndex('Y'), std::make_pair(0, xySize.h));
    tbl.emplace(charToDimIndex('X'), std::make_pair(0, xySize.w));
  } else {
    if (scene_index_ < sceneStart || sceneStart + sceneSize <= scene_index_) {
      std::stringstream ss;
      ss << "Scene index " << scene_index_ << " ∉ " // x ∉ Y means x is not an element of Y, or x is not in Y
         << "[" << sceneStart << ", " << sceneStart + sceneSize << ")";
      throw CDimCoordinatesOverspecifiedException(ss.str());
    }
    libCZI::CDimCoordinate cDim{ { libCZI::DimensionIndex::S, scene_index_ } };
    SubblockSortable sceneToFind(&cDim, -1, false);
    SubblockIndexVec matches = getMatches(sceneToFind);

    // get the condensed set of values
    std::map<DimIndex, set<int>> definedDims;
    for (const auto& x : matches) {
      x.first.coordinatePtr()->EnumValidDimensions([&definedDims](libCZI::DimensionIndex di_, int val_) -> bool {
        definedDims[dimensionIndexToDimIndex(di_)].emplace(val_);
        return true;
      });
      if (isMosaic()) {
        definedDims[DimIndex::M].emplace(x.first.mIndex());
      }
    }
    for (auto x : definedDims) {
      tbl.emplace(x.first, std::make_pair(*x.second.begin(), *x.second.rbegin() + 1));
    }

    auto xySize = getSceneYXSize(scene_index_);
    tbl.emplace(DimIndex::Y, std::make_pair(0, xySize.h));
    tbl.emplace(DimIndex::X, std::make_pair(0, xySize.w));
  }
  if (ImageFactory::numberOfSamples(m_pixelType) > 1) {
    tbl.emplace(charToDimIndex('A'), std::make_pair(0, ImageFactory::numberOfSamples(m_pixelType)));
  }

  return tbl;
}

std::vector<libCZI::IntRect>
Reader::getAllSceneYXSize(int scene_index_, bool get_all_matches_)
{
  std::vector<libCZI::IntRect> result;
  bool hasScene = m_statistics.dimBounds.IsValid(libCZI::DimensionIndex::S);

  libCZI::CDimCoordinate scene_coord; // default constructor
  if (hasScene && scene_index_ >= 0) {
    scene_coord = libCZI::CDimCoordinate({ { libCZI::DimensionIndex::S, scene_index_ } });
  }
  SubblockSortable subblocksToFind(&scene_coord, -1, false);
  SubblockIndexVec matches = getMatches(subblocksToFind);

  int embeddedSceneIndex = 0;
  for (const auto& x : matches) {
    if (hasScene) {
      x.first.coordinatePtr()->TryGetPosition(libCZI::DimensionIndex::S, &embeddedSceneIndex);
      if (embeddedSceneIndex == scene_index_) {
        libCZI::SubBlockInfo sbkInfo;
        if (!m_czireader->TryGetSubBlockInfo(x.second, &sbkInfo))
          continue;
        result.emplace_back(sbkInfo.logicalRect);
        if (!get_all_matches_)
          return result;
      }
    }
  }
  if (hasScene && !result.empty())
    return result;

  m_czireader->EnumerateSubBlocks([&result, get_all_matches_](int index, const libCZI::SubBlockInfo& info) -> bool {
    if (!isPyramid0(info))
      return true;

    result.emplace_back(info.logicalRect);
    return get_all_matches_;
  });
  return result;
}

libCZI::PixelType
Reader::getFirstPixelType()
{
  libCZI::PixelType pixelType = libCZI::PixelType::Invalid;
  m_czireader->EnumerateSubBlocks([&pixelType](int index_, const libCZI::SubBlockInfo& info_) -> bool {
    if (!isPyramid0(info_))
      return true;
    pixelType = info_.pixelType;
    return false;
  });
  return pixelType;
}

/// @brief get the Dimensions in the order they appear in
/// @return a string containing the Dimensions for the image data object
std::string
Reader::dimsString()
{
  std::string ans;
  m_statistics.dimBounds.EnumValidDimensions([&ans](libCZI::DimensionIndex di_, int start_, int size_) -> bool {
    ans += Reader::dimToChar(di_);
    return true;
  });

  std::sort(ans.begin(), ans.end(), [](const char a_, const char b_) {
    return libCZI::Utils::CharToDimension(a_) > libCZI::Utils::CharToDimension(b_);
  });

  if (isMosaic())
    ans += "M";

  ans += "YX";

  pixelType();
  if (ImageFactory::numberOfSamples(m_pixelType) > 1)
    ans += "A";

  return ans;
}

std::pair<ImagesContainerBase::ImagesContainerBasePtr, std::vector<std::pair<char, size_t>>>
Reader::readSelected(libCZI::CDimCoordinate& plane_coord_,
                     int index_m_,
                     unsigned int cores_,
                     libCZI::IntRect region_)
{
  int pos;
  if (m_specifyScene && !plane_coord_.TryGetPosition(libCZI::DimensionIndex::S, &pos)) {
    throw ImageAccessUnderspecifiedException(0,
                                             1,
                                             "Scenes must be read individually "
                                             "for this file, scenes have inconsistent YX shapes!");
  }
  bool hasRegion = (region_.w > 0 && region_.h > 0);
  if (hasRegion)
    isValidRegion(region_, m_statistics.boundingBox);

  SubblockSortable subblocksToFind(&plane_coord_, index_m_, isMosaic());
  // SubblockIndexVec is actually a set this is crucial to preserve the image order
  SubblockIndexVec matches = getMatches(subblocksToFind, hasRegion ? &region_ : nullptr);
  if (matches.empty()) {
    throw RegionSelectionException(region_, m_statistics.boundingBox, "No subblocks intersect the requested region!");
  }
  m_pixelType = matches.begin()->first.pixelType();
  size_t bgrScaling = ImageFactory::numberOfSamples(m_pixelType);

  libCZI::IntRect w_by_h = getSceneYXSize();
  size_t n_of_pixels = matches.size() * w_by_h.w * w_by_h.h; // bgrScaling is handled internally * bgrScaling;
  ImageFactory imageFactory(m_pixelType, n_of_pixels);

  imageFactory.setMosaic(isMosaic());
  size_t memOffset = 0;

  /*
   * On windows the python code says there are far more cores than the C++ code. For that reason we have
   * implemented this in such a way that it rescales to a workable value when necessary.
   */
  unsigned int min_cores = 1; // for the case when hardware_concurency fails and returns 0
  unsigned int number_of_cores = std::max(min_cores, std::min(cores_, std::thread::hardware_concurrency() - 1));
  {
    std::vector<std::future<bool>> jobs;
    Tasks tasks;
    for_each(matches.begin(), matches.end(), [&](const SubblockIndexVec::value_type& match_) {
      int sb_index = match_.second;

      jobs.push_back(tasks.queue([this, &imageFactory, sb_index, memOffset]() -> bool {
        auto subblock = m_czireader->ReadSubBlock(sb_index);
        const libCZI::SubBlockInfo& info = subblock->GetSubBlockInfo();
        if (m_pixelType != info.pixelType)
          throw PixelTypeException(info.pixelType,
                                   "Selected subblocks have inconsistent PixelTypes."
                                   " You must select subblocks with consistent PixelTypes.");
        // the throw above covers a possible edge case which the file has multiple pixel types. If this is
        // the case the exception is intentionally sent back to the user to deal with as they will have to
        // select subblocks with consistent pixelType. There's no way to know which of the conflicting
        // types they wanted.

        auto bitmap = subblock->CreateBitmap();
        libCZI::IntSize size = bitmap->GetSize();
        // constructImage fixes BRG image data now via channels != 3 condition
        imageFactory.constructImage(bitmap, size, &info.coordinate, info.logicalRect, memOffset, info.mIndex);
        return true;
      }));

      memOffset += bgrScaling * w_by_h.w * w_by_h.h;
    });
    tasks.start(number_of_cores);
    for_each(jobs.begin(), jobs.end(), [](auto& x) { x.get(); });
  }

  if (imageFactory.numberOfImages() == 0) {
    throw pylibczi::CdimSelectionZeroImagesException(
      plane_coord_, m_statistics.dimBounds, "No pyramid0 selectable subblocks.");
  }
  auto charShape = imageFactory.getFixedShape();
  ImageVector& imageVector = imageFactory.images();
  imageVector.sort(); // this sort is mostly cosmetic, putting the imgs in memory order.
  return std::make_pair(imageFactory.transferMemoryContainer(), charShape);
}

SubblockMetaVec
Reader::readSubblockMeta(libCZI::CDimCoordinate& plane_coord_, int index_m_)
{
  SubblockMetaVec metaSubblocks;
  metaSubblocks.setMosaic(isMosaic());

  SubblockSortable subBlockToFind(&plane_coord_, index_m_, isMosaic());
  SubblockIndexVec matches = getMatches(subBlockToFind);

  for_each(matches.begin(), matches.end(), [&](const SubblockIndexVec::value_type& match_) {
    size_t metaSize = 0;
    auto subblock = m_czireader->ReadSubBlock(match_.second);
    auto sharedPtrString = subblock->GetRawData(libCZI::ISubBlock::Metadata, &metaSize);
    metaSubblocks.emplace_back(
      match_.first.coordinatePtr(), match_.first.mIndex(), isMosaic(), (char*)(sharedPtrString.get()), metaSize);
  });

  return metaSubblocks;
}

// private methods

static bool
doIntersect(const libCZI::IntRect& a_, const libCZI::IntRect& b_)
{
  int x0 = std::max(a_.x, b_.x);
  int y0 = std::max(a_.y, b_.y);
  int x1 = std::min(a_.x + a_.w, b_.x + b_.w);
  int y1 = std::min(a_.y + a_.h, b_.y + b_.h);
  return (x1 > x0 && y1 > y0);
}

Reader::SubblockIndexVec
Reader::getMatches(SubblockSortable& match_, const libCZI::IntRect* region_)
{
  SubblockIndexVec ans;
  m_czireader->EnumerateSubBlocks([&](int index_, const libCZI::SubBlockInfo& info_) -> bool {
    SubblockSortable subInfo(&(info_.coordinate), info_.mIndex, isMosaic(), info_.pixelType);
    if (isPyramid0(info_) && match_ == subInfo) {
      if (region_ == nullptr || doIntersect(*region_, info_.logicalRect))
        ans.emplace(std::pair<SubblockSortable, int>(subInfo, index_));
    }
    return true; // Enumerate through every subblock
  });

  if (ans.empty()) {
    // check for invalid Dimension specification
    match_.coordinatePtr()->EnumValidDimensions([&](libCZI::DimensionIndex di_, int value_) {
      if (!m_statistics.dimBounds.IsValid(di_)) {
        std::stringstream tmp;
        tmp << dimToChar(di_) << " Not present in defined file Coordinates!";
        throw CDimCoordinatesOverspecifiedException(tmp.str());
      }

      int start(0), size(0);
      m_statistics.dimBounds.TryGetInterval(di_, &start, &size);
      if (value_ < start || value_ >= (start + size)) {
        std::stringstream tmp;
        tmp << dimToChar(di_) << " value " << value_ << "invalid, ∉ [" << start << ", " << start + size << ")"
            << std::endl;
        throw CDimCoordinatesOverspecifiedException(tmp.str());
      }
      return true;
    });
  }
  return ans;
}

bool
Reader::isValidRegion(const libCZI::IntRect& in_box_, const libCZI::IntRect& czi_box_)
{
  bool ans = true;
  // check origin is in domain
  if (in_box_.x < czi_box_.x || czi_box_.x + czi_box_.w < in_box_.x)
    ans = false;
  if (in_box_.y < czi_box_.y || czi_box_.y + czi_box_.h < in_box_.y)
    ans = false;

  // check  (x1, y1) point is in domain
  int x1 = in_box_.x + in_box_.w;
  int y1 = in_box_.y + in_box_.h;
  if (x1 < czi_box_.x || czi_box_.x + czi_box_.w < x1)
    ans = false;
  if (y1 < czi_box_.y || czi_box_.y + czi_box_.h < y1)
    ans = false;

  if (!ans)
    throw RegionSelectionException(in_box_, czi_box_, "Requested region not in image!");
  if (in_box_.w < 1 || 1 > in_box_.h)
    throw RegionSelectionException(in_box_, czi_box_, "Requested region must have non-negative width and height!");

  return ans;
}

ImagesContainerBase::ImagesContainerBasePtr
Reader::readMosaic(libCZI::CDimCoordinate plane_coord_,
                   float scale_factor_,
                   libCZI::IntRect im_box_,
                   libCZI::RgbFloatColor backGroundColor_)
{
  // handle the case where the function was called with region=None (default to all)
  if (im_box_.w == -1 && im_box_.h == -1)
    im_box_ = m_statistics.boundingBox;
  isValidRegion(im_box_, m_statistics.boundingBox); // if not throws RegionSelectionException

  if (plane_coord_.IsValid(libCZI::DimensionIndex::S)) {
    throw CDimCoordinatesOverspecifiedException("Do not set S when reading mosaic files!");
  }

  if (!plane_coord_.IsValid(libCZI::DimensionIndex::C)) {
    throw CDimCoordinatesUnderspecifiedException("C is not set, to read mosaic files you must specify C.");
  }
  SubblockSortable subBlockToFind(&plane_coord_,
                                  -1); // just check that the dims match something ignore that it's a mosaic file
  SubblockIndexVec matches = getMatches(subBlockToFind); // this does the checking
  m_pixelType = matches.begin()->first.pixelType();
  size_t bgrScaling = ImageFactory::numberOfSamples(m_pixelType);
  auto accessor = m_czireader->CreateSingleChannelScalingTileAccessor();

  // Use default options except for backGroundColor (default is none)
  libCZI::ISingleChannelScalingTileAccessor::Options options;
  options.Clear();
  options.backGroundColor = backGroundColor_;

  // multiTile accessor is not compatible with S, it composites the Scenes and the mIndexs together
  auto multiTileComposite = accessor->Get(im_box_, &plane_coord_, scale_factor_, &options);

  libCZI::IntSize size = multiTileComposite->GetSize();
  size_t pixels_in_image = size.h * size.w * bgrScaling;
  // the original pixels_in_image calculation was done using the file statistics container from libCZI but that
  // gives an incorrect size for the image which seems like a bug in libCZI
  // do not use m_statistics.boundingBoxLayer0Only.w*m_statistics.boundingBoxLayer0Only.h*bgrScaling;
  ImageFactory imageFactory(m_pixelType, pixels_in_image);
  imageFactory.constructImage(multiTileComposite, size, &plane_coord_, im_box_, 0, -1);
  // set is mosaic?
  return imageFactory.transferMemoryContainer();
}

Reader::TilePair
Reader::tileBoundingBox(libCZI::CDimCoordinate& plane_coord_)
{
  SubblockSortable subblocksToFind(&plane_coord_, -1, false);
  TileBBoxMap ans = tileBoundingBoxesWith(subblocksToFind);

  if (ans.size() > 1)
    throw CDimCoordinatesUnderspecifiedException("More than 1 tile matched. Be more specific.");

  return *(ans.begin());
}

Reader::TileBBoxMap
Reader::tileBoundingBoxes(libCZI::CDimCoordinate& plane_coord_)
{
  SubblockSortable subblocksToFind(&plane_coord_, -1, false);
  return tileBoundingBoxesWith(subblocksToFind);
}

// private method
Reader::TileBBoxMap
Reader::tileBoundingBoxesWith(SubblockSortable& subblocksToFind_)
{
  TileBBoxMap ans;
  SubblockIndexVec matches = getMatches(subblocksToFind_);

  if (matches.size() == 0)
    throw CDimCoordinatesOverspecifiedException("Tile dimensions overspecified, no matching tiles found.");

  auto extractor = [&](const SubblockIndexVec::value_type& match_) {
    libCZI::SubBlockInfo sbkInfo;
    if (!m_czireader->TryGetSubBlockInfo(match_.second, &sbkInfo))
      throw ImageAccessUnderspecifiedException(0, 1, "Subblock index not found while reading tile bounding boxes.");
    return TileBBoxMap::value_type(match_.first, sbkInfo.logicalRect);
  };

  transform(matches.begin(), matches.end(), std::inserter(ans, ans.end()), extractor);
  return ans;
}

libCZI::IntRect
Reader::sceneBoundingBox(unsigned int scene_index_)
{
  // implicit Scene
  if (m_statistics.sceneBoundingBoxes.size() == 0)
    return m_statistics.boundingBoxLayer0Only;

  // explicit scenes
  auto found = m_statistics.sceneBoundingBoxes.find(scene_index_);
  if (found == m_statistics.sceneBoundingBoxes.end())
    throw SceneIndexException(
      scene_index_, m_statistics.sceneBoundingBoxes.begin(), m_statistics.sceneBoundingBoxes.end());

  return found->second.boundingBoxLayer0;
}

Reader::SceneBBoxMap
Reader::allSceneBoundingBoxes()
{
  // implicit Scene
  if (m_statistics.sceneBoundingBoxes.size() == 0)
    return std::map<unsigned int, libCZI::IntRect>{ { 0, m_statistics.boundingBoxLayer0Only } };

  SceneBBoxMap ans;

  std::transform(m_statistics.sceneBoundingBoxes.begin(),
                 m_statistics.sceneBoundingBoxes.end(),
                 std::inserter(ans, end(ans)),
                 [](const std::map<int, libCZI::BoundingBoxes>::value_type& bboxes_pair_) {
                   return std::map<unsigned int, libCZI::IntRect>::value_type(bboxes_pair_.first,
                                                                              bboxes_pair_.second.boundingBoxLayer0);
                 });
  return ans;
}

libCZI::IntRect
Reader::mosaicBoundingBox() const
{
  if (!isMosaic())
    throw IsNotMosaicException("Use the non-mosaic specific bounding box functions.");

  return m_statistics.boundingBoxLayer0Only;
}

Reader::TilePair
Reader::mosaicTileBoundingBox(libCZI::CDimCoordinate& plane_coord_, int index_m_)
{
  if (!isMosaic())
    throw IsNotMosaicException("Use the non-mosaic specific bounding box functions.");

  SubblockSortable subblocksToFind(&plane_coord_, index_m_, isMosaic());
  TileBBoxMap ans = tileBoundingBoxesWith(subblocksToFind);

  if (ans.size() > 1)
    throw CDimCoordinatesUnderspecifiedException("More than 1 tile matched. Be more specific.");

  return *(ans.begin());
}

Reader::TileBBoxMap
Reader::mosaicTileBoundingBoxes(libCZI::CDimCoordinate& plane_coord_)
{
  if (!isMosaic())
    throw IsNotMosaicException("Use the non-mosaic specific bounding box functions.");

  SubblockSortable subblocksToFind(&plane_coord_, -1, true);
  return tileBoundingBoxesWith(subblocksToFind);
}

libCZI::IntRect
Reader::mosaicSceneBoundingBox(unsigned int scene_index_)
{
  if (!isMosaic())
    throw IsNotMosaicException("Use the non-mosaic specific bounding box functions.");

  return sceneBoundingBox(scene_index_);
}

Reader::SceneBBoxMap
Reader::allMosaicSceneBoundingBoxes()
{
  if (!isMosaic())
    throw IsNotMosaicException("Use the non-mosaic specific bounding box functions.");

  return allSceneBoundingBoxes();
}

}
