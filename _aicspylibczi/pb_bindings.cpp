#include <pybind11/gil.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

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

PYBIND11_MODULE(_aicspylibczi, m)
{

  namespace py = pybind11;

  m.doc() = "aicspylibczi C++ extension for reading ZISRAW/CZI files"; // optional
                                                                       // module
                                                                       // docstring

  py::register_exception<pylibczi::FilePtrException>(m, "PylibCZI_BytesIO2FilePtrException");
  py::register_exception<pylibczi::PixelTypeException>(m, "PylibCZI_PixelTypeException");
  py::register_exception<pylibczi::RegionSelectionException>(m, "PylibCZI_RegionSelectionException");
  py::register_exception<pylibczi::ImageAccessUnderspecifiedException>(m,
                                                                       "PylibCZI_ImageAccessUnderspecifiedException");
  py::register_exception<pylibczi::ImageIteratorException>(m, "PylibCZI_ImageIteratorException");
  py::register_exception<pylibczi::ImageSplitChannelException>(m, "PylibCZI_ImageSplitChannelException");
  py::register_exception<pylibczi::ImageCopyAllocFailed>(m, "PylibCZI_ImageCopyAllocFailed");
  py::register_exception<pylibczi::CdimSelectionZeroImagesException>(m, "PylibCZI_CDimSpecSelectedNoImagesException");
  py::register_exception<pylibczi::CDimCoordinatesOverspecifiedException>(
    m, "PylibCZI_CDimCoordinatesOverspecifiedException");
  py::register_exception<pylibczi::CDimCoordinatesUnderspecifiedException>(
    m, "PylibCZI_CDimCoordinatesUnderspecifiedException");

  m.def("curl_stream_available",
        &pylibczi::curlStreamAvailable,
        "True if this build can read CZI files from http/https URLs.");
  m.def("stream_option_names",
        &pylibczi::streamOptionNames,
        "The libCZI property names accepted as stream options by Reader.from_url.");

  // Releases the GIL for the duration of the bound C++ call so a slow read
  // (e.g. an HTTP range request on a remote CZI) doesn't stall other Python
  // threads. See https://pybind11.readthedocs.io/en/stable/advanced/misc.html#global-interpreter-lock-gil
  using ReleaseGil = py::call_guard<py::gil_scoped_release>;

  py::class_<libCZI::IntRect>(m, "BBox")
    .def(py::init<>())
    .def("__eq__",
         [](const libCZI::IntRect& a, const libCZI::IntRect& b) {
           return (a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h);
         })
    .def_readwrite("x", &libCZI::IntRect::x)
    .def_readwrite("y", &libCZI::IntRect::y)
    .def_readwrite("w", &libCZI::IntRect::w)
    .def_readwrite("h", &libCZI::IntRect::h);

  py::class_<pylibczi::Reader>(m, "Reader")
    .def(py::init<std::shared_ptr<libCZI::IStream>>(), ReleaseGil())
    .def_static(
      "from_url",
      [](const std::string& url_, const py::dict& options_) {
        pylibczi::throwIfCurlStreamUnavailable();
        auto propertyBag = pb_helpers::streamPropertyBagFromDict(options_);
        py::gil_scoped_release release;
        auto stream = pylibczi::createStreamFromUrl(url_, propertyBag);
        return std::unique_ptr<pylibczi::Reader>(new pylibczi::Reader(std::move(stream)));
      },
      py::arg("url"),
      py::arg("options") = py::dict(),
      "Open a CZI file from an http/https URL.")
    .def("is_mosaic", &pylibczi::Reader::isMosaic, ReleaseGil())
    .def("has_consistent_shape", &pylibczi::Reader::shapeIsConsistent, ReleaseGil())
    .def("read_dims", &pylibczi::Reader::readDimsRange, ReleaseGil())
    .def("read_dims_string", &pylibczi::Reader::dimsString, ReleaseGil())
    .def("read_dims_sizes", &pylibczi::Reader::dimSizes, ReleaseGil())
    .def("read_meta", &pylibczi::Reader::readMeta, ReleaseGil())
    .def("read_selected",
         &pylibczi::Reader::readSelected,
         py::arg("plane_coord"),
         py::arg("index_m") = -1,
         py::arg("cores") = 3,
         py::arg("region") = libCZI::IntRect{ 0, 0, -1, -1 },
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
    .def_property_readonly("pixel_type", &pylibczi::Reader::pixelType, ReleaseGil());

  py::class_<pylibczi::IndexMap>(m, "IndexMap")
    .def(py::init<>())
    .def("is_m_index_valid", &pylibczi::IndexMap::isMIndexValid)
    .def("dim_index", &pylibczi::IndexMap::dimIndex)
    .def("m_index", &pylibczi::IndexMap::mIndex);

  py::class_<libCZI::CDimCoordinate>(m, "DimCoord").def(py::init<>()).def("set_dim", &libCZI::CDimCoordinate::Set);

  py::class_<libCZI::RgbFloatColor>(m, "RgbFloat")
    .def(py::init<>())
    .def_readwrite("r", &libCZI::RgbFloatColor::r)
    .def_readwrite("g", &libCZI::RgbFloatColor::g)
    .def_readwrite("b", &libCZI::RgbFloatColor::b);

  py::class_<pylibczi::SubblockSortable>(m, "TileInfo")
    //   .def(py::init<pylibczi::SubblockSortable>())
    .def_property_readonly("dimension_coordinates", &pylibczi::SubblockSortable::getDimsAsChars)
    .def_property_readonly("m_index", &pylibczi::SubblockSortable::mIndex);
}
