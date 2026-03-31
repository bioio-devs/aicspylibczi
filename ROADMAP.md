# AICSPyLibCZI Roadmap

## For 3.\* Releases

The `aicspylibczi` roadmap captures current development priorities within the project
and should serve as a guide for core developers, to encourage contribution for new
contributors, to provide insight to external developers who are interested in using
`aicspylibczi` in their work, and to provide deeper insights to developers using
`aicsimageio` which uses this library to read ZEISSRAW format CZI files.

The _mission_ of `aicspylibczi` is to provide a robust method of access to Zeiss's
CZI microscopy files. Our approach is to use Zeiss's libCZI and leverage that to
create a C++ tool that extracts the desired image data and metadata in a structure
that performs well with respect to speed and memory usage and enables mapping into
a python numpy.ndarray. The bindings to create a python module are done using
pybind11 and are then wrapped in python class providing a more pythonic interface
into the module that the direct bindings.

Our goal with the 3.\* API changes is:

- Unify the function/property naming conventions
- Modify the library to support sAmples, the A dimension for RGB/BGR images
- Enable reading of attachment data
- Provide sufficient information to enable `aicspylibczi` to load a mosaic image as a dask.ndarray
- Enable automated benchmark scripts

## Unify the function/property naming conventions

Before 3.\* it was pointed out the interface was a bit inconsistent. We attempt to address
that here by using only bounding boxes and providing matched methods specific for non-mosaic
images and mosaic images.

## Modify the library to support Samples, the A channel for RGB/BGR images

For RGB (BGR) images the 3.\* creates a new return dimension `A` which has size 3.
This was changed from expanding them out as additional channels because that was
fraught with complications. It should be noted that in `aicsimageio` 4.\* `S` stands
for `Samples` but because in `aicspylibczi` `S` is used for `Scene` so `A` is used
to represent `sAmples`.

## Enable reading of attachment data

Attachments can be embedded in subblocks along with the image. This data can take
numerous forms, a small jpeg, datetime values, etc. Retrieval of this information
will be implemented after the initial release.

## Provide enable dask friendly tile reconstruction for mosaic files.

`aicsimageio` leverages dask to enable working with very large images.
Mosaic files are often problematic in this regard. Though `aicspylibczi`
can reconstruct mosaics but because they are so large there are cases
where something better is required. The tile information and individual
tile reading will be using in `aicsimageio` to create an `dask.ndarray`
which can be operated on like other dask objects enabling handling of
truely huge mosaic images with `aicsimageio`

## Enable automated benchmark scripts

Scripts exist to perform benchmarking but they do not currently utilize
quilt for large files and can thus only be run locally. We would like to
change this to be more like `aicsimageio` so that benchmarking can be added
as a GitHub Action. This will not be in the initial 3.0 release but will
come later.

## About This Document

This document is meant to be a snapshot or high-level objectives and reasoning for the
library during our 3.\* series of releases.

For more low-level implementation details, features, bugs, documentation requests, etc,
please see our [issue tracker](https://github.com/bioio-devs/aicspylibczi/issues).
