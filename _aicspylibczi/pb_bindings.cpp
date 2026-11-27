#include <nanobind/nanobind.h>
#include <nanobind/stl/map.h>
#include <nanobind/stl/pair.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/unique_ptr.h>
#include <nanobind/stl/vector.h>

#include "IndexMap.h"
#include "Reader.h"
#include "UrlStream.h"
#include "exceptions.h"
#include "inc_libCZI.h"

// the below headers are crucial otherwise the custom casts aren't recognized
#include "pb_caster_BytesIO.h"
#include "pb_caster_DimIndex.h"
#include "pb_caster_ImagesContainer.h"
#include "pb_caster_SubblockMetaVec.h"
#include "pb_caster_libCZI_DimensionIndex.h"
#include "pb_stream_options.h"

NB_MODULE(_aicspylibczi, m)
{

  namespace nb = nanobind;

  m.doc() = "aicspylibczi C++ extension for reading ZISRAW/CZI files";

  nb::exception<pylibczi::FilePtrException>(m, "PylibCZI_BytesIO2FilePtrException");
  nb::exception<pylibczi::PixelTypeException>(m, "PylibCZI_PixelTypeException");
  nb::exception<pylibczi::RegionSelectionException>(m, "PylibCZI_RegionSelectionException");
  nb::exception<pylibczi::ImageAccessUnderspecifiedException>(m, "PylibCZI_ImageAccessUnderspecifiedException");
  nb::exception<pylibczi::ImageIteratorException>(m, "PylibCZI_ImageIteratorException");
  nb::exception<pylibczi::ImageSplitChannelException>(m, "PylibCZI_ImageSplitChannelException");
  nb::exception<pylibczi::ImageCopyAllocFailed>(m, "PylibCZI_ImageCopyAllocFailed");
  nb::exception<pylibczi::CdimSelectionZeroImagesException>(m, "PylibCZI_CDimSpecSelectedNoImagesException");
  nb::exception<pylibczi::CDimCoordinatesOverspecifiedException>(m, "PylibCZI_CDimCoordinatesOverspecifiedException");
  nb::exception<pylibczi::CDimCoordinatesUnderspecifiedException>(m, "PylibCZI_CDimCoordinatesUnderspecifiedException");

  m.def("curl_stream_available",
        &pylibczi::curlStreamAvailable,
        "True if this build can read CZI files from http/https URLs.");
  m.def("stream_option_names",
        &pylibczi::streamOptionNames,
        "The libCZI property names accepted as stream options by Reader.from_url.");

  // Releases the GIL for the duration of the bound C++ call so a slow read
  // (e.g. an HTTP range request on a remote CZI) doesn't stall other Python
  // threads. See https://nanobind.readthedocs.io/en/latest/ownership.html
  using ReleaseGil = nb::call_guard<nb::gil_scoped_release>;

  nb::class_<libCZI::IntRect>(m, "BBox")
    .def(nb::init<>())
    .def("__eq__",
         [](const libCZI::IntRect& a, const libCZI::IntRect& b) {
           return (a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h);
         })
    .def_rw("x", &libCZI::IntRect::x)
    .def_rw("y", &libCZI::IntRect::y)
    .def_rw("w", &libCZI::IntRect::w)
    .def_rw("h", &libCZI::IntRect::h);

  nb::class_<pylibczi::Reader>(m, "Reader")
    .def(nb::init<std::shared_ptr<libCZI::IStream>>(), ReleaseGil())
    .def_static(
      "from_url",
      [](const std::string& url_, const nb::dict& options_) {
        pylibczi::throwIfCurlStreamUnavailable();
        auto propertyBag = pb_helpers::streamPropertyBagFromDict(options_);
        nb::gil_scoped_release release;
        auto stream = pylibczi::createStreamFromUrl(url_, propertyBag);
        return std::unique_ptr<pylibczi::Reader>(new pylibczi::Reader(std::move(stream)));
      },
      nb::arg("url"),
      nb::arg("options") = nb::dict(),
      "Open a CZI file from an http/https URL.")
    .def("is_mosaic", &pylibczi::Reader::isMosaic, ReleaseGil())
    .def("has_consistent_shape", &pylibczi::Reader::shapeIsConsistent, ReleaseGil())
    .def("read_dims", &pylibczi::Reader::readDimsRange, ReleaseGil())
    .def("read_dims_string", &pylibczi::Reader::dimsString, ReleaseGil())
    .def("read_dims_sizes", &pylibczi::Reader::dimSizes, ReleaseGil())
    .def("read_meta", &pylibczi::Reader::readMeta, ReleaseGil())
    .def("read_selected",
         &pylibczi::Reader::readSelected,
         nb::arg("plane_coord"),
         nb::arg("index_m") = -1,
         nb::arg("cores") = 3,
         nb::arg("region") = libCZI::IntRect{ 0, 0, -1, -1 },
         ReleaseGil())
    .def("read_meta_from_subblock", &pylibczi::Reader::readSubblockMeta, ReleaseGil())
    .def("read_mosaic", &pylibczi::Reader::readMosaic, ReleaseGil())
    .def("read_tile_bounding_box", &pylibczi::Reader::tileBoundingBox, ReleaseGil())
    .def("read_scene_bounding_box", &pylibczi::Reader::sceneBoundingBox, ReleaseGil())
    .def("read_all_tile_bounding_boxes", &pylibczi::Reader::tileBoundingBoxes, ReleaseGil())
    .def("read_all_scene_bounding_boxes", &pylibczi::Reader::allSceneBoundingBoxes, ReleaseGil())
    .def("read_mosaic_bounding_box", &pylibczi::Reader::mosaicBoundingBox, ReleaseGil())
    .def("read_mosaic_tile_bounding_box", &pylibczi::Reader::mosaicTileBoundingBox, ReleaseGil())
    .def("read_mosaic_scene_bounding_box", &pylibczi::Reader::mosaicSceneBoundingBox, ReleaseGil())
    .def("read_all_mosaic_tile_bounding_boxes", &pylibczi::Reader::mosaicTileBoundingBoxes, ReleaseGil())
    .def(
      "read_all_mosaic_scene_bounding_boxes", &pylibczi::Reader::allMosaicSceneBoundingBoxes, ReleaseGil())
    .def_prop_ro("pixel_type", &pylibczi::Reader::pixelType, ReleaseGil());

  nb::class_<pylibczi::IndexMap>(m, "IndexMap")
    .def(nb::init<>())
    .def("is_m_index_valid", &pylibczi::IndexMap::isMIndexValid)
    .def("dim_index", &pylibczi::IndexMap::dimIndex)
    .def("m_index", &pylibczi::IndexMap::mIndex);

  nb::class_<libCZI::CDimCoordinate>(m, "DimCoord").def(nb::init<>()).def("set_dim", &libCZI::CDimCoordinate::Set);

  nb::class_<libCZI::RgbFloatColor>(m, "RgbFloat")
    .def(nb::init<>())
    .def_rw("r", &libCZI::RgbFloatColor::r)
    .def_rw("g", &libCZI::RgbFloatColor::g)
    .def_rw("b", &libCZI::RgbFloatColor::b);

  nb::class_<pylibczi::SubblockSortable>(m, "TileInfo")
    //   .def(nb::init<pylibczi::SubblockSortable>())
    .def_prop_ro("dimension_coordinates", &pylibczi::SubblockSortable::getDimsAsChars)
    .def_prop_ro("m_index", &pylibczi::SubblockSortable::mIndex);
}
