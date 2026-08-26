# Parent class for python wrapper to libczi file for accessing Zeiss czi image and metadata.

import io
import multiprocessing
from pathlib import Path
from typing import BinaryIO, Dict, Optional, Tuple, Union
from urllib.parse import urlparse

import numpy as np
import xml.etree.ElementTree as ET

from . import types


def remote_reads_available() -> bool:
    """
    Test whether this installation can read CZI files from http/https URLs.

    Returns
    -------
    bool
        True if CziFile accepts http/https URLs.

    """
    import _aicspylibczi

    return _aicspylibczi.curl_stream_available()


class CziFile(object):
    """Zeiss CZI file object.

    Args:
      |  czi_filename (str): Filename of czifile to access, or an http/https URL.

    Kwargs:
      |  verbose (bool): Print information and times during czi file access.
      |  stream_options (dict): libCZI stream options, only valid for URLs.

    .. note::

       Utilizes compiled wrapper to libCZI for accessing the CZI file.

    """

    # xxx - likely this is a Zeiss bug,
    #   units for the scale in the xml file are not correct (says microns, given in meters)
    # scale_units = 1e6

    # Dims as defined in libCZI
    #
    # Z = 1  # The Z-dimension.
    # C = 2  # The C-dimension ("channel").
    # T = 3  # The T-dimension ("time").
    # R = 4  # The R-dimension ("rotation").
    # S = 5  # The S-dimension ("scene").
    # I = 6  # The I-dimension ("illumination").
    # H = 7  # The H-dimension ("phase").
    # V = 8  # The V-dimension ("view").

    # aicspylibczi extended dims -- These may be returned but can not be used to request subblocks from the file
    # A  # The A-dimension ("samples" - RGB images).
    # X  # The X-dimension
    # Y  # The Y-dimension
    ####
    ZISRAW_DIMS = {"Z", "C", "T", "R", "S", "I", "H", "V", "B"}

    REMOTE_SCHEMES = frozenset({"http", "https"})

    def __init__(
        self,
        czi_filename: types.FileLike,
        verbose: bool = False,
        stream_options: Optional[Dict[str, Union[str, int, bool]]] = None,
    ):
        self.czifile_verbose = verbose

        import _aicspylibczi

        self.czilib = _aicspylibczi

        if self.is_remote(czi_filename):
            self._bytes = None
            self.reader = self.czilib.Reader.from_url(
                str(czi_filename), stream_options or {}
            )
        else:
            if stream_options:
                raise ValueError(
                    "stream_options are only supported for http/https URLs, "
                    f"received: {czi_filename}"
                )
            # Convert to BytesIO (bytestream)
            self._bytes = self.convert_to_buffer(czi_filename)
            self.reader = self.czilib.Reader(self._bytes)

        self.meta_root = None

    @property
    def shape_is_consistent(self):
        """
        Query if the file shape is consistent across scenes.

        Returns
        -------
        bool
            true if consistent, false the scenes have different dimension shapes

        """
        return self.reader.has_consistent_shape()

    @property
    def dims(self):
        """
        Get the dimensions present the binary data (not the metadata)
        M, Y, X, A are included for completeness but can not be used as constraints.

        Returns
        -------
        str
            A string containing Dimensions letters present, ie "BSTZYX"

        Note
        ----
        Dimensions defined in libCZI -
            V - view
            H - phase
            I - illumination
            S - scene
            R - rotation
            T - time
            C - channel
            Z - z plane (height)

        Dimensions added by aicspylibczi -
            M - mosaic tile, mosaic images only
            Y - image height
            X - image width
            A - samples, BGR/RGB images only

        """
        return self.reader.read_dims_string()

    def get_dims_shape(self):
        """
        Get the dimensions for the opened file from the binary data (not the metadata)

        Returns
        -------
        list[dict]
            A list of dictionaries containing Dimension / depth. If the shape is consistent across Scenes then
            the list will have only one Dictionary. If the shape is inconsistent the the list will have a dictionary
             for each Scene. A consistently shaped file with 3 scenes, 7 time-points
            and 4 Z slices containing images of (h,w) = (325, 475) would return
            [
             {'S': (0, 3), 'T': (0,7), 'X': (0, 475), 'Y': (0, 325), 'Z': (0, 4)}
            ].
            The result for a similarly shaped file but with different number of time-points per scene would yield
            [
             {'S': (0, 1), 'T': (0,8), 'X': (0, 475), 'Y': (0, 325), 'Z': (0, 4)},
             {'S': (1, 2), 'T': (0,6), 'X': (0, 475), 'Y': (0, 325), 'Z': (0, 4)},
             {'S': (2, 3), 'T': (0,7), 'X': (0, 475), 'Y': (0, 325), 'Z': (0, 4)}
            ]
            For the same initial file but with an RGB pixel type the result would be
            [
             {'S': (0, 3), 'T': (0,7), 'X': (0, 475), 'Y': (0, 325), 'Z': (0, 4), 'A': (0,3)}
            ].

        """
        return self.reader.read_dims()

    @property
    def pixel_type(self):
        """
        The pixelType of the images. If the pixelType is different in the different subblocks it returns Invalid.

        Returns
        -------
        A string containing the name of the type of each pixel. If inconsistent it returns invalid.

        """
        return self.reader.pixel_type

    def get_tile_bounding_box(self, **kwargs):
        """
        Get a single tile (subblock) bounding box (pyramid=0) for the specified dimensions.
        For non-mosaic files.

        Parameters
        ----------
        kwargs
            The keywords below allow you to specify the dimensions that you wish to match. If you
            under-specify the constraints you can easily end up with a massive image stack.
                       Z = 1   # The Z-dimension.
                       C = 2   # The C-dimension ("channel").
                       T = 3   # The T-dimension ("time").
                       R = 4   # The R-dimension ("rotation").
                       S = 5   # The S-dimension ("scene").
                       I = 6   # The I-dimension ("illumination").
                       H = 7   # The H-dimension ("phase").
                       V = 8   # The V-dimension ("view").

        Returns
        -------
        bbox
            A Bounding Box, bbox, of type BBox.
            bbox.x = The x coordinate of the upper left corner of the bounding box.
            bbox.y = The y coordinate of the upper left corner of the bounding box.
            bbox.w = The width of the bounding box.
            bbox.h = The height of the bounding box.

        """
        plane_constraints = self._get_coords_from_kwargs(kwargs)
        dims, bbox = self.reader.read_tile_bounding_box(plane_constraints)
        return bbox

    def get_scene_bounding_box(self, index: int = 0):
        """
        Get the bounding box (pyramid=0) for the specified scene. For non-mosaic files.
        This should be equivalent to the results from get_tile_bounding_box but requiring only one arguments.

        Parameters
        ----------
        index
             the scene index, if omitted it defaults to Zero

        Returns
        -------
        bbox
             A Bounding Box, bbox, of type BBox.
             bbox.x = The x coordinate of the upper left corner of the bounding box.
             bbox.y = The y coordinate of the upper left corner of the bounding box.
             bbox.w = The width of the bounding box.
             bbox.h = The height of the bounding box.

        """
        bbox = self.reader.read_scene_bounding_box(index)
        return bbox

    def get_all_tile_bounding_boxes(self, **kwargs):
        """
        Get one or more tiles (subblocks) bounding boxes (pyramid=0) for the specified dimensions.
        For non-mosaic files.

        Parameters
        ----------
        kwargs
            The keywords below allow you to specify the dimensions that you wish to match. If you
            under-specify the constraints you can easily end up with a massive image stack.
                       Z = 1   # The Z-dimension.
                       C = 2   # The C-dimension ("channel").
                       T = 3   # The T-dimension ("time").
                       R = 4   # The R-dimension ("rotation").
                       S = 5   # The S-dimension ("scene").
                       I = 6   # The I-dimension ("illumination").
                       H = 7   # The H-dimension ("phase").
                       V = 8   # The V-dimension ("view").

        Returns
        -------
        dict[tile_info, bbox]
            A dictionary with keys of type TileInfo and values of type BBox.
            For a key, ie tile, of type Tile:
                tile.dimension_coordinates = A dictionary of Dimensions for the tile
            For a value, ie bbox, of type BBox:
                bbox.x = The x coordinate of the upper left corner of the bounding box.
                bbox.y = The y coordinate of the upper left corner of the bounding box.
                bbox.w = The width of the bounding box.
                bbox.h = The height of the bounding box.

        """
        plane_constraints = self._get_coords_from_kwargs(kwargs)
        return self.reader.read_all_tile_bounding_boxes(plane_constraints)

    def get_all_scene_bounding_boxes(self):
        """
        Get one or more tiles (subblocks) bounding boxes (pyramid=0) for the specified dimensions.
        For non-mosaic files.

        Returns
        -------
        dict[int, bbox]
            A dictionary with keys of type Int and values of type BBox.
            The integer key is the Scene Index.
            For a value, ie bbox, of type BBox:
                bbox.x = The x coordinate of the upper left corner of the bounding box.
                bbox.y = The y coordinate of the upper left corner of the bounding box.
                bbox.w = The width of the bounding box.
                bbox.h = The height of the bounding box.

        """
        return self.reader.read_all_scene_bounding_boxes()

    def get_mosaic_tile_bounding_box(self, **kwargs):
        """
        Get a single tile (subblock) bounding box (pyramid=0) for the specified dimensions.
        For mosaic files.

        Parameters
        ----------
        kwargs
            The keywords below allow you to specify the dimensions that you wish to match. If you
            under-specify the constraints you can easily end up with a massive image stack.
                       Z = 1   # The Z-dimension.
                       C = 2   # The C-dimension ("channel").
                       T = 3   # The T-dimension ("time").
                       R = 4   # The R-dimension ("rotation").
                       S = 5   # The S-dimension ("scene").
                       I = 6   # The I-dimension ("illumination").
                       H = 7   # The H-dimension ("phase").
                       V = 8   # The V-dimension ("view").
                       M = 10  # The M_index, this is only valid for Mosaic files!

        Returns
        -------
        bbox
            A Bounding Box, bbox, of type BBox.
            bbox.x = The x coordinate of the upper left corner of the bounding box.
            bbox.y = The y coordinate of the upper left corner of the bounding box.
            bbox.w = The width of the bounding box.
            bbox.h = The height of the bounding box.

        """
        plane_constraints = self._get_coords_from_kwargs(kwargs)
        m_index = self._get_m_index_from_kwargs(kwargs)
        ssorter, bbox = self.reader.read_mosaic_tile_bounding_box(
            plane_constraints, m_index
        )
        return bbox

    def get_mosaic_scene_bounding_box(self, index: int = 0):
        """
        Get the bounding box (pyramid=0) for the specified scene. For mosaic files.
        This is not equivalent to the results from get_mosaic_tile_bounding_box.

        Parameters
        ----------
        index
             the scene index, if omitted it defaults to Zero

        Returns
        -------
        bbox
            A Bounding Box, bbox, of type BBox.
            bbox.x = The x coordinate of the upper left corner of the bounding box.
            bbox.y = The y coordinate of the upper left corner of the bounding box.
            bbox.w = The width of the bounding box.
            bbox.h = The height of the bounding box.

        """
        return self.reader.read_mosaic_scene_bounding_box(index)

    def get_all_mosaic_tile_bounding_boxes(self, **kwargs):
        """
        Get one or more tiles (subblocks) bounding boxes (pyramid=0) for the specified dimensions.
        For mosaic files.

        Parameters
        ----------
        kwargs
            The keywords below allow you to specify the dimensions that you wish to match. If you
            under-specify the constraints you can easily end up with a massive image stack.
                       Z = 1   # The Z-dimension.
                       C = 2   # The C-dimension ("channel").
                       T = 3   # The T-dimension ("time").
                       R = 4   # The R-dimension ("rotation").
                       S = 5   # The S-dimension ("scene").
                       I = 6   # The I-dimension ("illumination").
                       H = 7   # The H-dimension ("phase").
                       V = 8   # The V-dimension ("view").

        Returns
        -------
        dict[tile_info, bbox]
            A dictionary with keys of type TileInfo and values of type BBox.
            For a key, ie tle, of type Tile:
                tle.dimension_coordinates = A dictionary of Dimensions for the tile
            For a value, ie bbox, of type BBox:
                bbox.x = The x coordinate of the upper left corner of the bounding box.
                bbox.y = The y coordinate of the upper left corner of the bounding box.
                bbox.w = The width of the bounding box.
                bbox.h = The height of the bounding box.

        """
        plane_constraints = self._get_coords_from_kwargs(kwargs)
        # no m_index parameter
        return self.reader.read_all_mosaic_tile_bounding_boxes(plane_constraints)

    def get_all_mosaic_scene_bounding_boxes(self):
        """
        Get the scene (subblocks) bounding boxes (pyramid=0) for the specified dimensions.
        For mosaic files.

        Returns
        -------
        dict[int, bbox]
            A dictionary with keys of type Int and values of type BBox.
            The integer key is the Scene Index.
            For a value, ie bbox, of type BBox:
                bbox.x = The x coordinate of the upper left corner of the bounding box.
                bbox.y = The y coordinate of the upper left corner of the bounding box.
                bbox.w = The width of the bounding box.
                bbox.h = The height of the bounding box.

        """
        return self.reader.read_all_mosaic_scene_bounding_boxes()

    def get_mosaic_bounding_box(self):
        """
        Get the bounding box for the entire mosaic image.

        Returns
        -------
        bbox
            A Bounding Box, bbox, of type BBox.
            bbox.x = The x coordinate of the upper left corner of the bounding box.
            bbox.y = The y coordinate of the upper left corner of the bounding box.
            bbox.w = The width of the bounding box.
            bbox.h = The height of the bounding box.

        """
        return self.reader.read_mosaic_bounding_box()

    @property
    def size(self):
        """
        This returns the Size of each dimension in the dims string. So if S had valid indexes of [0, 1, 2, 3, 4]
        the returned tuple would have a value of 5 in the same position as the S occurs in the dims string.

        Returns
        -------
        tuple
            a tuple of dimension sizes. If the data has inconsistent shape the list will only contain -1 values and
            the user needs to use dims_shape() to get the indexes.

        """
        return tuple(self.reader.read_dims_sizes())

    def is_mosaic(self):
        """
        Test if the loaded file is a mosaic file

        Returns
        -------
        bool
            True | False ie is this a mosaic file

        """
        return self.reader.is_mosaic()

    @staticmethod
    def is_remote(file: types.FileLike) -> bool:
        """
        Test if the given target is an http/https URL rather than a local file.

        Parameters
        ----------
        file
            The target passed to the constructor.

        Returns
        -------
        bool
            True if the target is an http or https URL.

        """
        if not isinstance(file, str):
            return False
        return urlparse(file).scheme.lower() in CziFile.REMOTE_SCHEMES

    @staticmethod
    def convert_to_buffer(file: types.FileLike) -> Union[BinaryIO, np.ndarray]:
        if isinstance(file, (str, Path)):
            # This will both fully expand and enforce that the filepath exists
            f = Path(file).expanduser().resolve(strict=True)

            # This will check if the above enforced filepath is a directory
            if f.is_dir():
                raise IsADirectoryError(f)

            return open(f, "rb")

        # Convert bytes
        elif isinstance(file, bytes):
            return io.BytesIO(file)

        # Set bytes
        elif isinstance(file, (io.BytesIO, io.BufferedReader, io.IOBase, np.ndarray)):
            return file

        # Raise
        else:
            raise TypeError(
                f"Reader only accepts types: [str, pathlib.Path, bytes, io.BytesIO, io.IOBase], received: {type(file)}"
            )

    @property
    def meta(self):
        """
        Extract the metadata block from the czi file.

        Returns
        -------
        xml.etree.ElementTree.Element
            The root element of the metadata tree

        """
        if self.meta_root is None:
            meta_str = self.reader.read_meta()
            self.meta_root = ET.fromstring(meta_str)

        return self.meta_root

    def read_subblock_metadata(self, unified_xml: bool = False, **kwargs):
        """
        Read the subblock specific metadata, ie time subblock was acquired / position at acquisition time etc.

        Parameters
        ----------
        unified_xml: bool
            If True return a single unified xml tree containing the requested subblock.
            If False return a list of tuples (dims, xml)
        kwargs
            The keywords below allow you to specify the dimensions that you wish to match. If you
            under-specify the constraints you can easily end up with a massive image stack.
                       Z = 1   # The Z-dimension.
                       C = 2   # The C-dimension ("channel").
                       T = 3   # The T-dimension ("time").
                       R = 4   # The R-dimension ("rotation").
                       S = 5   # The S-dimension ("scene").
                       I = 6   # The I-dimension ("illumination").
                       H = 7   # The H-dimension ("phase").
                       V = 8   # The V-dimension ("view").
                       M = 10  # The M_index, this is only valid for Mosaic files!

        Returns
        -------
        [(dict, str)] if unified_xml is False
            an array of tuples containing a dimension dictionary and the corresponding subblock metadata
        xml.etree.ElementTree.Element if unified_xml is True
            an xml document containing the requested subblock metadata.

        """
        plane_constraints = self._get_coords_from_kwargs(kwargs)
        m_index = self._get_m_index_from_kwargs(kwargs)
        subblock_meta = self.reader.read_meta_from_subblock(plane_constraints, m_index)
        if not unified_xml:
            return subblock_meta
        root = ET.Element("Subblocks")
        for pair in subblock_meta:
            new_element = ET.Element("Subblock")
            for dim, number in pair[0].items():
                new_element.set(dim, str(number))
            if "S" not in pair[0]:
                new_element.set("S", "0")
            new_element.append(ET.XML(pair[1]))
            root.append(new_element)
        return root

    def read_image(self, region: Tuple = None, **kwargs):
        """
        Read the subblocks in the CZI file and for any subblocks that match all the constraints in kwargs return
        that data. This allows you to select channels/scenes/time-points/Z-slices etc. Note if passed a BGR image
        then the dims of the object will returned by this function and the implicit BGR will be expanded into an
        A dimension. A is samples per pixel and will only be present for BGR images. This is logically more consistent
        than mixing the samples into the channels as was done before aicspylibczi-3.0.0.

        Parameters
        ----------
        region: Tuple
            The (x, y, width, height) of a sub-region to restrict the read.
        **kwargs
            The keywords below allow you to specify the dimensions that you wish to match. If you
            under-specify the constraints you can easily end up with a massive image stack.
                 Z = 1   # The Z-dimension.
                 C = 2   # The C-dimension ("channel").
                 T = 3   # The T-dimension ("time").
                 R = 4   # The R-dimension ("rotation").
                 S = 5   # The S-dimension ("scene").
                 I = 6   # The I-dimension ("illumination").
                 H = 7   # The H-dimension ("phase").
                 V = 8   # The V-dimension ("view").
                 M = 10  # The M_index, this is only valid for Mosaic files!
            Specify the number of cores to use for multithreading with cores.
                cores = 3 # use 3 cores for threaded reading of the image.

        Returns
        -------
        (numpy.ndarray, [Dimension, Size])
            a tuple of (numpy.ndarray, a list of (Dimension, size)) the second element of the tuple is to make
            sure the numpy.ndarray is interpretable. An example of the list is
            [('S', 1), ('T', 1), ('C', 2), ('Z', 25), ('Y', 1024), ('X', 1024)]
            so if you probed the numpy.ndarray with .shape you would get (1, 1, 2, 25, 1024, 1024).

        Notes
        -----
        The M Dimension is a representation of the m_index used inside libCZI. Unfortunately this can be sparsely
        packed for a given selection which causes problems when indexing memory. Consequently the M Dimension may
        not match the m_index that is being used in libCZI or displayed in Zeiss' Zen software.
        """
        plane_constraints = self._get_coords_from_kwargs(kwargs)
        m_index = self._get_m_index_from_kwargs(kwargs)
        cores = self._get_cores_from_kwargs(kwargs)

        image, shape = self.reader.read_selected(
            plane_constraints, m_index, cores, self._bbox_from_region(region)
        )
        return image, shape

    def read_mosaic(
        self,
        region: Tuple = None,
        scale_factor: float = 1.0,
        background_color: Tuple = None,
        **kwargs,
    ):
        """
        Reads a mosaic file and returns an image corresponding to the specified dimensions. If the file is more than
        a 2D sheet of pixels, meaning only one channel, z-slice, time-index, etc then the kwargs must specify the
        dimension with more than one possible value.

        **Example:** Read in the C=1 channel of a mosaic file at 1/10th the size

            czi = CziFile(filename)
            img = czi.read_mosaic(scale_factor=0.1, C=1)

        Parameters
        ----------
        region
            A bounding box specifying the extraction box (x0, y0, width, height) specified in pixels
        scale_factor
            The amount to scale the data by, 0.1 would mean an image 1/10 the height and width of native, if you
            get distortions it seems to be due to a bug in Zeiss's libCZI I'm trying to track it down but for now
            if you use scale_factor=1.0 it should work properly.
        background_color
            Background color used when pixel is outside of a sublock. If omitted, it defaults to black
            (r,g,b)=(0.0,0.0,0.0). Each color component is a float value between 0.0 and 1.0.
        kwargs
            The keywords below allow you to specify the dimension plane that constrains the 2D data. If the
            constraints are underspecified the function will fail. ::
                    Z = 1   # The Z-dimension.
                    C = 2   # The C-dimension ("channel").
                    T = 3   # The T-dimension ("time").
                    R = 4   # The R-dimension ("rotation").
                    S = 5   # The S-dimension ("scene").
                    I = 6   # The I-dimension ("illumination").
                    H = 7   # The H-dimension ("phase").
                    V = 8   # The V-dimension ("view").

        Returns
        -------
        numpy.ndarray
            (1, height, width)
        """
        plane_constraints = self._get_coords_from_kwargs(kwargs)

        region = self._bbox_from_region(region)

        if background_color is None:
            background_color = self.czilib.RgbFloat()
            background_color.r = 0.0
            background_color.g = 0.0
            background_color.b = 0.0
        else:
            assert len(background_color) == 3
            tmp = self.czilib.RgbFloat()
            tmp.r = background_color[0]
            tmp.g = background_color[1]
            tmp.b = background_color[2]
            background_color = tmp

        img = self.reader.read_mosaic(
            plane_constraints, scale_factor, region, background_color
        )

        return img

    def _bbox_from_region(self, region: Tuple = None):
        """
        Convert an (x, y, width, height) tuple into the BBox the C++ layer expects.
        """
        bbox = self.czilib.BBox()
        if region is None:
            bbox.w = -1
            bbox.h = -1
            return bbox

        assert len(region) == 4
        bbox.x = region[0]
        bbox.y = region[1]
        bbox.w = region[2]
        bbox.h = region[3]
        return bbox

    def _get_coords_from_kwargs(self, kwargs):
        plane_constraints = self.czilib.DimCoord()
        [
            plane_constraints.set_dim(k, v)
            for (k, v) in kwargs.items()
            if k in CziFile.ZISRAW_DIMS
        ]
        return plane_constraints

    def _get_m_index_from_kwargs(self, kwargs):
        m_index = -1
        if "M" in kwargs:
            if not self.is_mosaic():
                raise self.czilib.PylibCZI_CDimCoordinatesOverspecifiedException(
                    "M Dimension is specified but the file is not a mosaic file!"
                )
            m_index = kwargs.get("M")
        return m_index

    @staticmethod
    def _get_cores_from_kwargs(kwargs):
        cores = multiprocessing.cpu_count() - 1
        if "cores" in kwargs:
            cores = kwargs.get("cores")
        return cores
