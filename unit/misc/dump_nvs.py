#!/usr/bin/env python
import argparse
import struct
import json

def read_nvs_entries(in_filename):
    pages = []
    with open(in_filename, mode="rb") as f_in:
        rawstatus = f_in.read(4)
        while rawstatus:
            (status, ) = struct.unpack("<I", rawstatus)
            if (status == 0xfffffffc) or (status == 0xfffffffe):
                (seq, ) = struct.unpack("<I", f_in.read(4))
                pages.append({ "seq": seq, "offset": f_in.tell() + 32 - (4 + 4)} )
                f_in.seek(4096 - 4 - 4, 1)
            else:
                f_in.seek(4096 - 4, 1)
            rawstatus = f_in.read(4)

        pages.sort(key=lambda page: page["seq"])

        for page in pages:
            f_in.seek(page["offset"], 0)
            entry_bitmap = struct.unpack("<32B", f_in.read(32))
            idx = 0
            while idx < 126:
                entry_status = (entry_bitmap[idx >> 2] >> ((idx & 3) * 2)) & 3
                if entry_status == 2:
                    entry = f_in.read(32)
                    yield entry
                    (span, ) = struct.unpack_from("B", entry, offset=2)
                    for _ in range(1, span):
                        entry = f_in.read(32)
                        yield entry
                    idx += span
                else:
                    f_in.seek(32, 1)
                    idx += 1


# Parse the commandline
ap = argparse.ArgumentParser(
        formatter_class = argparse.ArgumentDefaultsHelpFormatter,
        description = "Dump the contents of an NVS image",
        )
ap.add_argument("-i", "--input", required=True, help="the input partition image file")
args = vars(ap.parse_args())


#with open(args["output"], mode="wb") as f_out:
#        f_out.write(rawentry)

rawentries = read_nvs_entries(args["input"])
for rawentry in rawentries:
    entry = {}
    (entry["ns"], entry["type"], span, rsv, crc, key) = struct.unpack("<BBBBI16s8x", rawentry)
    entry["key"] = key.rstrip(b"\x00").decode("utf8")
    if entry["type"] == 0x01:
        (entry["data"],) = struct.unpack_from("B", rawentry, 24)
    elif entry["type"] == 0x11:
        (entry["data"],) = struct.unpack_from("b", rawentry, 24)
    elif entry["type"] == 0x02:
        (entry["data"],) = struct.unpack_from("<H", rawentry, 24)
    elif entry["type"] == 0x12:
        (entry["data"],) = struct.unpack_from("<h", rawentry, 24)
    elif entry["type"] == 0x04:
        (entry["data"],) = struct.unpack_from("<I", rawentry, 24)
    elif entry["type"] == 0x14:
        (entry["data"],) = struct.unpack_from("<i", rawentry, 24)
    elif entry["type"] == 0x08:
        (entry["data"],) = struct.unpack_from("<Q", rawentry, 24)
    elif entry["type"] == 0x18:
        (entry["data"],) = struct.unpack_from("<q", rawentry, 24)
    elif (entry["type"] == 0x21) or (entry["type"] == 0x41):
        (size, ) = struct.unpack_from("<H", rawentry, 24)
        data = bytearray()
        for idx in range(1, span):
            data.extend(next(rawentries))
        if entry["type"] == 0x41:
            entry["data"] = data[0:size]
        else:
            entry["data"] = data[0:size - 1].decode("utf8")
    print(entry)
        









