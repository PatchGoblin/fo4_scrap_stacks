import struct

import pytest

from tools.addrlib import AddressLibrary


def write_v0(path, pairs):
    """Writes the F4SE old-gen format: u64 count, then (u64 id, u64 offset) pairs."""
    with open(path, "wb") as f:
        f.write(struct.pack("<Q", len(pairs)))
        for id_, off in pairs:
            f.write(struct.pack("<QQ", id_, off))


def test_looks_up_offset_by_id(tmp_path):
    bin_path = tmp_path / "version-1-10-163-0.bin"
    write_v0(bin_path, [(5, 0x1000), (646841, 0x2B3C40), (9, 0x20)])

    lib = AddressLibrary.load(bin_path)

    assert lib.offset(646841) == 0x2B3C40
    assert lib.offset(5) == 0x1000


def test_reverse_lookup_finds_id_for_offset(tmp_path):
    bin_path = tmp_path / "version-1-10-163-0.bin"
    write_v0(bin_path, [(5, 0x1000), (646841, 0x2B3C40)])

    lib = AddressLibrary.load(bin_path)

    assert lib.id_at(0x2B3C40) == 646841
    assert lib.id_at(0x1234) is None


def test_unknown_id_raises_with_the_id_in_the_message(tmp_path):
    bin_path = tmp_path / "version-1-10-163-0.bin"
    write_v0(bin_path, [(5, 0x1000)])

    lib = AddressLibrary.load(bin_path)

    with pytest.raises(KeyError, match="424242"):
        lib.offset(424242)


def test_truncated_file_is_rejected(tmp_path):
    bin_path = tmp_path / "version-1-10-163-0.bin"
    with open(bin_path, "wb") as f:
        f.write(struct.pack("<Q", 3))
        f.write(struct.pack("<QQ", 1, 2))

    with pytest.raises(ValueError, match="truncated"):
        AddressLibrary.load(bin_path)
