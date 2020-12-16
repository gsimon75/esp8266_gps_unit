#!/usr/bin/env python
import argparse
import os

# Parse the commandline
ap = argparse.ArgumentParser(
        formatter_class = argparse.ArgumentDefaultsHelpFormatter,
        description = "Generates a descriptor file for a partition image",
        )
ap.add_argument("-i", "--input", required=True, help="the input partition image file")
ap.add_argument("-o", "--output", default="/dev/stdout", help="the output descriptor file")
ap.add_argument("-e", "--source-epoch", help="the timestamp of the build")

args = vars(ap.parse_args())

if not ("output" in args):
    idx = args["input"].rfind(".")
    if idx < 0:
        idx = len(args["input"])
    args["output"] = args["input"][0:idx] + ".desc"


# fletcher16 checksum parts
sum1 = 0
sum2 = 0

blocksize = 65536

with open(args["input"], mode="rb") as f_in:
    data = f_in.read(blocksize)
    while data:
        for i in range(0, len(data)):
            sum1 = (sum1 + data[i]) % 255
            sum2 = (sum2 + sum1) % 255
        data = f_in.read(blocksize)
    fletcher16 = (sum2 << 8) | sum1

    st = os.fstat(f_in.fileno())
    input_basename = args["input"]
    idx = input_basename.rfind("/")
    if idx > 0:
        input_basename = input_basename[idx + 1:]

    epoch = args["epoch"] if "epoch" in args else int(st.st_mtime);

    with open(args["output"], mode="wt") as f_out:
        print("name: {fn}".format(fn=input_basename), file=f_out)
        print("mtime: {t}".format(t=epoch), file=f_out)
        print("size: {s}".format(s=st.st_size), file=f_out)
        print("fletcher16: {c}".format(c=fletcher16), file=f_out)

