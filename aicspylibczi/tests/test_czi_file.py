import io
import numpy as np
import pytest
import xml.etree.ElementTree as ET

from pathlib import Path

from aicspylibczi import CziFile, remote_reads_available
from _aicspylibczi import (
    PylibCZI_CDimCoordinatesOverspecifiedException,
    PylibCZI_RegionSelectionException,
)


@pytest.mark.parametrize(
    "as_string",
    [
        pytest.param(True, marks=pytest.mark.raises(exception=IsADirectoryError)),
        pytest.param(False, marks=pytest.mark.raises(exception=IsADirectoryError)),
    ],
)
def test_is_a_directory(data_dir, as_string):
    infile = data_dir
    if as_string:
        infile = str(data_dir)
    CziFile(infile)


@pytest.mark.parametrize(
    "in_, out_",
    [
        pytest.param(float(10), False, marks=pytest.mark.raises(exception=TypeError)),
        (
            io.BytesIO(b"thisisatestletsseewhathappens"),
            io.BytesIO(b"thisisatestletsseewhathappens"),
        ),
        (
            b"thisisatestletsseewhathappens",
            io.BytesIO(b"thisisatestletsseewhathappens"),
        ),
        (np.zeros(5), np.zeros(5)),
    ],
)
def test_conversion_types(in_, out_):
    ans = CziFile.convert_to_buffer(in_)
    assert ans.__class__ == out_.__class__


@pytest.mark.parametrize(
    "fname, xp_query, expected",
    [
        pytest.param(
            "s_1_t_1_c_1_z_1.czi",
            ".//SizeS",
            1,
            marks=pytest.mark.raises(exception=AttributeError),
        ),
        pytest.param(
            "s_1_t_1_c_1_z_1.czi",
            ".//SizeZ",
            1,
            marks=pytest.mark.raises(exception=AttributeError),
        ),
        pytest.param(
            "s_1_t_1_c_1_z_1.czi",
            ".//SizeT",
            1,
            marks=pytest.mark.raises(exception=AttributeError),
        ),
        ("s_1_t_1_c_1_z_1.czi", ".//SizeC", 1),
        ("s_1_t_1_c_1_z_1.czi", ".//SizeY", 325),
        ("s_1_t_1_c_1_z_1.czi", ".//SizeX", 475),
        ("s_3_t_1_c_3_z_5.czi", ".//SizeS", 3),
        pytest.param(
            "s_3_t_1_c_3_z_5.czi",
            ".//SizeT",
            1,
            marks=pytest.mark.raises(exception=AttributeError),
        ),
        ("s_3_t_1_c_3_z_5.czi", ".//SizeC", 3),
        ("s_3_t_1_c_3_z_5.czi", ".//SizeZ", 5),
    ],
)
def test_metadata(data_dir, fname, xp_query, expected):
    czi = CziFile(str(data_dir / fname))
    meta = czi.meta
    vs = meta.find(xp_query)
    assert int(vs.text) == expected
    meta = czi.meta
    vs = meta.find(xp_query)
    assert int(vs.text) == expected


@pytest.mark.parametrize(
    "fname, expected_img_shape, expected_img_dims",
    [
        (
            "s_1_t_1_c_1_z_1.czi",
            (1, 1, 325, 475),
            [("B", 1), ("C", 1), ("Y", 325), ("X", 475)],
        ),
        (
            "s_3_t_1_c_3_z_5.czi",
            (1, 3, 3, 5, 325, 475),
            [("B", 1), ("S", 3), ("C", 3), ("Z", 5), ("Y", 325), ("X", 475)],
        ),
    ],
)
def test_read_image_from_istream(
    data_dir, fname, expected_img_shape, expected_img_dims
):
    with open(data_dir / fname, "rb") as fp:
        czi = CziFile(czi_filename=fp)
        assert czi.shape_is_consistent
        data = czi.read_image()
        assert data[0].shape == expected_img_shape
        assert data[1] == expected_img_dims


@pytest.mark.parametrize(
    "fname, expected",
    [
        ("s_1_t_1_c_1_z_1.czi", (1, 1, 325, 475)),
        ("s_3_t_1_c_3_z_5.czi", (1, 3, 3, 5, 325, 475)),
        ("mosaic_test.czi", (1, 1, 1, 1, 2, 624, 924)),  # S T C Z M Y X
    ],
)
def test_read_dims_sizes(data_dir, fname, expected):
    with open(data_dir / fname, "rb") as fp:
        czi = CziFile(czi_filename=fp)
        data = czi.size
        assert data == expected


#  non-mosaic bbox functions


@pytest.mark.parametrize(
    "fname, idx, expected",
    [
        ("s_1_t_1_c_1_z_1.czi", 0, (39856, 39272, 475, 325)),
        ("s_3_t_1_c_3_z_5.czi", 0, (39850, 35568, 475, 325)),
        ("s_3_t_1_c_3_z_5.czi", 1, (44851, 35568, 475, 325)),
        ("s_3_t_1_c_3_z_5.czi", 2, (39850, 39272, 475, 325)),
    ],
)
def test_scene_bounding_box(data_dir, fname, idx, expected):
    with open(data_dir / fname, "rb") as fp:
        czi = CziFile(czi_filename=fp)
        data = czi.get_scene_bounding_box(idx)
        assert data.x == expected[0]
        assert data.y == expected[1]
        assert data.w == expected[2]
        assert data.h == expected[3]


@pytest.mark.parametrize(
    "fname, idx, expected",
    [
        ("s_3_t_1_c_3_z_5.czi", 0, (39850, 35568, 475, 325)),
        ("s_3_t_1_c_3_z_5.czi", 1, (44851, 35568, 475, 325)),
        ("s_3_t_1_c_3_z_5.czi", 2, (39850, 39272, 475, 325)),
    ],
)
def test_get_tile_bounding_box(data_dir, fname, idx, expected):
    with open(data_dir / fname, "rb") as fp:
        czi = CziFile(czi_filename=fp)
        bbox = czi.get_tile_bounding_box(S=idx, C=0, Z=0)
        bbox2 = czi.get_scene_bounding_box(idx)
        assert bbox.x == expected[0]
        assert bbox.y == expected[1]
        assert bbox.w == expected[2]
        assert bbox.h == expected[3]
        assert bbox.x == bbox2.x
        assert bbox.y == bbox2.y
        assert bbox.w == bbox2.w
        assert bbox.h == bbox2.h


@pytest.mark.parametrize(
    "fname, idx, expected",
    [
        ("s_1_t_1_c_1_z_1.czi", 0, (39856, 39272, 475, 325)),
        ("s_3_t_1_c_3_z_5.czi", 0, (39850, 35568, 475, 325)),
        ("s_3_t_1_c_3_z_5.czi", 1, (44851, 35568, 475, 325)),
        ("s_3_t_1_c_3_z_5.czi", 2, (39850, 39272, 475, 325)),
    ],
)
def test_scene_bbox(data_dir, fname, idx, expected):
    with open(data_dir / fname, "rb") as fp:
        czi = CziFile(czi_filename=fp)
        data = czi.get_scene_bounding_box(idx)
        assert data.x == expected[0]
        assert data.y == expected[1]
        assert data.w == expected[2]
        assert data.h == expected[3]


@pytest.mark.parametrize(
    "fname, expected", [("s_1_t_1_c_1_z_1.czi", False), ("s_3_t_1_c_3_z_5.czi", False), ]
)
def test_is_mosaic(data_dir, fname, expected):
    czi = CziFile(str(data_dir / fname))
    assert czi.is_mosaic() == expected


@pytest.mark.parametrize(
    "fname, expected",
    [("s_1_t_1_c_1_z_1.czi", (False)), ("s_3_t_1_c_3_z_5.czi", (False)), ],
)
def test_destructor(data_dir, fname, expected):
    czi = CziFile(str(data_dir / fname))
    czi = CziFile(str(data_dir / fname))  # NOQA F841


@pytest.mark.parametrize(
    "fname, expected",
    [("mosaic_test.czi", (0, 0, 1756, 624)), ],  # it's not 2*X because they overlap
)
def test_mosaic_size(data_dir, fname, expected):
    czi = CziFile(str(data_dir / fname))
    ans = czi.get_mosaic_bounding_box()
    assert ans.x == expected[0]
    assert ans.y == expected[1]
    assert ans.w == expected[2]
    assert ans.h == expected[3]


@pytest.mark.parametrize(
    "fname, expected",
    [
        ("s_1_t_1_c_1_z_1.czi", (1, 1, 325, 475)),  # B C Y X
        ("s_3_t_1_c_3_z_5.czi", (1, 3, 3, 5, 325, 475)),  # B S C Z Y X
        ("mosaic_test.czi", (1, 1, 1, 1, 2, 624, 924)),  # S T C Z M Y X
        ("RGB-8bit.czi", (1, 624, 924, 3)),  # T Y X A
    ],
)
def test_read_image(data_dir, fname, expected):
    czi = CziFile(str(data_dir / fname))
    img, shp = czi.read_image()
    assert img.shape == expected


@pytest.mark.parametrize(
    "fname, expected",
    [
        ("s_1_t_1_c_1_z_1.czi", "gray16"),
        ("s_3_t_1_c_3_z_5.czi", "gray16"),
        ("mosaic_test.czi", "gray16"),
        ("RGB-8bit.czi", "bgr24"),
    ],
)
def test_pixel_type(data_dir, fname, expected):
    czi = CziFile(str(data_dir / fname))
    pix_type = czi.pixel_type
    assert pix_type == expected


@pytest.mark.parametrize(
    "fname, args, expected",
    [
        ("mosaic_test.czi", {"M": 0}, (1, 1, 1, 1, 1, 624, 924)),
        ("mosaic_test.czi", {"M": 0, "cores": 2000}, (1, 1, 1, 1, 1, 624, 924)),
    ],
)
def test_read_image_args(data_dir, fname, args, expected):
    czi = CziFile(data_dir / fname)
    img, shp = czi.read_image(**args)
    assert img.shape == expected


@pytest.mark.parametrize(
    "fname, exp_str, exp_dict",
    [
        (
            "s_1_t_1_c_1_z_1.czi",
            "BCYX",
            [{"B": (0, 1), "C": (0, 1), "X": (0, 475), "Y": (0, 325)}],
        ),
        (
            "s_3_t_1_c_3_z_5.czi",
            "BSCZYX",
            [
                {
                    "B": (0, 1),
                    "C": (0, 3),
                    "X": (0, 475),
                    "Y": (0, 325),
                    "S": (0, 3),
                    "Z": (0, 5),
                }
            ],
        ),
        (
            "mosaic_test.czi",
            "STCZMYX",
            [
                {
                    "S": (0, 1),
                    "T": (0, 1),
                    "C": (0, 1),
                    "Z": (0, 1),
                    "M": (0, 2),
                    "Y": (0, 624),
                    "X": (0, 924),
                }
            ],
        ),
        (
            "RGB-8bit.czi",
            "TYXA",
            [{"T": (0, 1), "Y": (0, 624), "X": (0, 924), "A": (0, 3)}],
        ),
    ],
)
def test_read_image_two(data_dir, fname, exp_str, exp_dict):
    czi = CziFile(str(data_dir / fname))
    dims = czi.dims
    dim_dict = czi.get_dims_shape()
    assert dims == exp_str
    assert dim_dict == exp_dict


@pytest.mark.parametrize(
    "fname",
    [
        pytest.param(
            "s_1_t_1_c_1_z_1.czi",
            marks=pytest.mark.raises(
                exception=PylibCZI_CDimCoordinatesOverspecifiedException
            ),
        ),
    ],
)
def test_read_image_mosaic(data_dir, fname):
    czi = CziFile(str(data_dir / fname))
    czi.read_image(M=0)
    assert True


@pytest.mark.parametrize(
    "fname, expected",
    [
        (
            "s_1_t_1_c_1_z_1.czi",
            "<METADATA><Tags><AcquisitionTime>2019-06-27T18:33:41.1154211Z</AcquisitionTime>",
        ),
        (
            "s_3_t_1_c_3_z_5.czi",
            "<METADATA><Tags><AcquisitionTime>2019-06-27T18:39:26.6459707Z</AcquisitionTime>",
        ),
    ],
)
def test_read_subblock_meta(data_dir, fname, expected):
    with open(data_dir / fname, "rb") as fp:
        czi = CziFile(czi_filename=fp)
        data = czi.read_subblock_metadata()
        assert expected in data[0][1]


@pytest.mark.parametrize(
    "fname, expected",
    [
        (
            "s_1_t_1_c_1_z_1.czi",
            b'<?xml version=\'1.0\' encoding=\'utf8\'?>\n<Subblocks><Subblock B="0" C="0" S="0"><METADATA>'
            b"<Tags><AcquisitionTime>2019-06-27T18:33:41.1154211Z"
            b'</AcquisitionTime><DetectorState><CameraState Id=""><CameraDisplayName>Camera 2 Left'
            b"</CameraDisplayName><ApplyCameraProfile>false</ApplyCameraProfile><ApplyImageOrientation>"
            b"true</ApplyImageOrientation><ExposureTime>10004210.526316</ExposureTime><Frame>"
            b"100,376,1900,1300</Frame><ImageOrientation>3</ImageOrientation></CameraState>"
            b"</DetectorState><StageXPosition>+000000043427.9820</StageXPosition><StageYPosition>"
            b"+000000042720.2960</StageYPosition><FocusPosition>+000000009801.2900</FocusPosition>"
            b"<RoiCenterOffsetX>+000000000007.0420</RoiCenterOffsetX><RoiCenterOffsetY>"
            b"+000000000000.5420</RoiCenterOffsetY></Tags><DataSchema><ValidBitsPerPixel>16"
            b"</ValidBitsPerPixel></DataSchema><AttachmentSchema /></METADATA></Subblock></Subblocks>",
        ),
    ],
)
def test_read_unified_subblock_meta(data_dir, fname, expected):
    with open(data_dir / fname, "rb") as fp:
        czi = CziFile(czi_filename=fp)
        data = czi.read_subblock_metadata(unified_xml=True)
        ans = ET.tostring(data, encoding='utf8', method='xml')
        assert expected == ans


@pytest.mark.parametrize(
    "fname, expects",
    [
        (
            "s_1_t_1_c_1_z_1.czi",
            [{"B": (0, 1), "C": (0, 1), "X": (0, 475), "Y": (0, 325)}],
        ),
        (
            "s_3_t_1_c_3_z_5.czi",
            [
                {
                    "B": (0, 1),
                    "C": (0, 3),
                    "X": (0, 475),
                    "Y": (0, 325),
                    "S": (0, 3),
                    "Z": (0, 5),
                }
            ],
        ),
    ],
)
def test_image_shape(data_dir, fname, expects):
    with open(data_dir / fname, "rb") as fp:
        czi = CziFile(czi_filename=fp)
        shape = czi.get_dims_shape()
        assert shape == expects


@pytest.mark.parametrize(
    "fname, unscaled_size, expects",
    [
        (
            "mosaic_test.czi",
            (0, 0, 1756, 624),
            [{"B": (0, 1), "C": (0, 1), "X": (0, 475), "Y": (0, 325)}],
        ),
        (
            "Multiscene_CZI_3Scenes.czi",
            (495412, 354694, 3587, 1926),
            [{"X": (0, 358), "Y": (0, 192)}],
        ),
    ],
)
def test_mosaic_image(data_dir, fname, unscaled_size, expects):
    with open(data_dir / fname, "rb") as fp:
        czi = CziFile(czi_filename=fp)
        sze = czi.get_mosaic_bounding_box()
        assert sze.w == unscaled_size[2]
        assert sze.h == unscaled_size[3]
        img = czi.read_mosaic(scale_factor=0.1, C=0, background_color=(1.0, 1.0, 1.0))
        assert img.shape[0] == 1
        assert img.shape[1] == unscaled_size[3] // 10
        assert img.shape[2] == unscaled_size[2] // 10


@pytest.mark.parametrize("fname", ["mosaic_test.czi"])
def test_read_image_region_selects_intersecting_subblocks(data_dir, fname):
    czi = CziFile(str(data_dir / fname))
    bbox = czi.get_mosaic_bounding_box()

    _, all_tiles = czi.read_image(C=0)
    assert dict(all_tiles)["M"] == 2

    data, shape = czi.read_image(C=0, region=(bbox.x, bbox.y, 64, 64))
    assert dict(shape)["M"] == 1

    assert dict(shape)["Y"] == dict(all_tiles)["Y"]
    assert dict(shape)["X"] == dict(all_tiles)["X"]

    left_tile, _ = czi.read_image(C=0, M=0)
    assert np.array_equal(data, left_tile)


@pytest.mark.parametrize("fname", ["mosaic_test.czi"])
@pytest.mark.raises(exception=PylibCZI_RegionSelectionException)
def test_read_image_region_outside_image_raises(data_dir, fname):
    czi = CziFile(str(data_dir / fname))
    bbox = czi.get_mosaic_bounding_box()
    czi.read_image(C=0, region=(bbox.x + bbox.w + 10, bbox.y, 64, 64))


@pytest.mark.parametrize(
    "fname, expects", [("mosaic_test.czi", (1, int(624 / 2), int(1756 / 2))), ]
)
def test_two_mosaic_image(data_dir, fname, expects):
    with open(data_dir / fname, "rb") as fp:
        czi = CziFile(czi_filename=fp)
        sze = czi.get_mosaic_bounding_box()
        rgion = (sze.x, sze.y, int(sze.w / 2), int(sze.h / 2))
        img = czi.read_mosaic(region=rgion, C=0, M=0)
        assert img.shape == expects


@pytest.mark.parametrize(
    "fname, s_index, m_index, expected",
    [
        ("s_3_t_1_c_3_z_5.czi", 0, -1, (39850, 35568, 475, 325)),
        ("s_3_t_1_c_3_z_5.czi", 1, -1, (44851, 35568, 475, 325)),
        ("s_3_t_1_c_3_z_5.czi", 2, -1, (39850, 39272, 475, 325)),
        ("mosaic_test.czi", 0, 0, (0, 0, 924, 624)),
        ("mosaic_test.czi", 0, 1, (832, 0, 924, 624)),
    ],
)
def test_subblock_rect(data_dir, fname, s_index, m_index, expected):
    with open(data_dir / fname, "rb") as fp:
        czi = CziFile(czi_filename=fp)
        ans = None
        if m_index < 0:
            ans = czi.get_scene_bounding_box(s_index)
            ans2 = czi.get_tile_bounding_box(S=s_index, C=0, Z=0)
            ans3 = czi.get_all_scene_bounding_boxes()
            ans4t = czi.get_all_tile_bounding_boxes(S=s_index, C=0, Z=0)
            assert ans == ans2
            assert ans == ans3[s_index]
            ans4 = list(ans4t.values())
            assert ans.x == ans4[0].x
            assert ans.y == ans4[0].y
            assert ans.w == ans4[0].w
            assert ans.h == ans4[0].h
        else:
            ans = czi.get_mosaic_tile_bounding_box(S=s_index, M=m_index)

        assert ans.x == expected[0]
        assert ans.y == expected[1]
        assert ans.w == expected[2]
        assert ans.h == expected[3]


def test_cores_arg():
    assert CziFile._get_cores_from_kwargs({"cores": 4}) == 4


@pytest.mark.parametrize(
    "fname, c_dims, expected",
    [
        ("mosaic_test.czi", {"S": 0, "M": 0}, [(0, 0, 924, 624), (832, 0, 924, 624)]),
        (
            "Multiscene_CZI_3Scenes.czi",
            {"S": 0, "M": 0},
            [
                (495412, 354694, 256, 256),
                (495643, 354694, 256, 256),
                (495643, 354924, 256, 256),
                (495412, 354924, 256, 256),
            ],
        ),
    ],
)
def test_mosaic_subblock_rect(data_dir, fname, c_dims, expected):
    with open(data_dir / fname, "rb") as fp:
        czi = CziFile(czi_filename=fp)
        data = czi.get_all_mosaic_tile_bounding_boxes(**c_dims)
        assert len(data) == len(expected)
        for idx, (key, rect) in enumerate(data.items()):
            assert rect.x == expected[idx][0]
            assert rect.y == expected[idx][1]
            assert rect.w == expected[idx][2]
            assert rect.h == expected[idx][3]


@pytest.mark.parametrize(
    "fname, p_index, ans_file", [("RGB-8bit.czi", 1, "RGB-8bit_Gplane.npy")]
)
def test_bgr_plane_data_x(data_dir, fname, p_index, ans_file):
    ans = np.load(data_dir / ans_file)
    with open(data_dir / fname, "rb") as fp:
        czi = CziFile(czi_filename=fp)
        img, dims = czi.read_image()
        assert img[0, :, :, p_index].shape == ans.shape
        np.testing.assert_array_almost_equal(img[0, :, :, p_index], ans)


@pytest.mark.parametrize(
    "target, expected",
    [
        ("https://example.com/image.czi", True),
        ("http://example.com/image.czi", True),
        ("HTTPS://example.com/image.czi", True),
        ("image.czi", False),
        ("C:\\images\\image.czi", False),
        ("s3://bucket/image.czi", False),
        (Path("/tmp/image.czi"), False),
    ],
)
def test_is_remote(target, expected):
    assert CziFile.is_remote(target) == expected


def test_remote_reads_available():
    assert remote_reads_available()


@pytest.mark.raises(exception=ValueError)
def test_stream_options_rejected_for_local_file(data_dir):
    CziFile(data_dir / "s_1_t_1_c_1_z_1.czi", stream_options={"timeout": 30})


@pytest.mark.raises(exception=ValueError)
def test_unknown_stream_option():
    CziFile("https://example.com/image.czi", stream_options={"nonsense": 1})


@pytest.mark.parametrize(
    "options",
    [
        {"timeout": 30},
        {"CurlHttp_Timeout": 30},
        {"connect_timeout": 10, "follow_location": True, "user_agent": "aicspylibczi"},
    ],
)
def test_remote_stream_options(data_server, options):
    czi = CziFile(f"{data_server}/s_1_t_1_c_1_z_1.czi", stream_options=options)
    assert czi.dims == "BCYX"


@pytest.mark.parametrize("fname", ["s_1_t_1_c_1_z_1.czi", "s_3_t_1_c_3_z_5.czi"])
def test_remote_matches_local(data_dir, data_server, fname):
    remote = CziFile(f"{data_server}/{fname}")
    with open(data_dir / fname, "rb") as fp:
        local = CziFile(czi_filename=fp)

        assert remote.dims == local.dims
        assert remote.size == local.size
        assert remote.pixel_type == local.pixel_type
        assert remote.get_dims_shape() == local.get_dims_shape()
        assert ET.tostring(remote.meta) == ET.tostring(local.meta)

        remote_img, remote_shape = remote.read_image()
        local_img, local_shape = local.read_image()

    assert remote_shape == local_shape
    np.testing.assert_array_equal(remote_img, local_img)


def test_remote_mosaic(data_dir, data_server):
    remote = CziFile(f"{data_server}/mosaic_test.czi")
    with open(data_dir / "mosaic_test.czi", "rb") as fp:
        local = CziFile(czi_filename=fp)

        assert remote.is_mosaic()
        assert remote.get_mosaic_bounding_box() == local.get_mosaic_bounding_box()
        remote_img = remote.read_mosaic(scale_factor=1.0, C=0)
        local_img = local.read_mosaic(scale_factor=1.0, C=0)

    np.testing.assert_array_equal(remote_img, local_img)


@pytest.mark.raises(exception=RuntimeError)
def test_remote_missing_file(data_server):
    CziFile(f"{data_server}/does_not_exist.czi")
