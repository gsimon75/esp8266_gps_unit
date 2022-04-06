#!/usr/bin/env python3
import sys
import argparse
import json
import xml.sax
import bz2

# Parse the commandline
ap = argparse.ArgumentParser(
        formatter_class = argparse.ArgumentDefaultsHelpFormatter,
        description = "Export the POIs from an OpenStreetMap dump file",
        )
ap.add_argument("-i", "--input", required=True, help="the .osm.bz2 file")
args = vars(ap.parse_args())


class MyHandler(xml.sax.ContentHandler):
    def __init__(self):
        xml.sax.ContentHandler.__init__(self)
        self.poi = {}
        self.first_line = True

    def startDocument(self):
        print("[");
        
    def endDocument(self):
        print("]");
        
    def startElement(self, name, attrs):
        if name == "node":
            let lat = float(attrs["lat"]);
            let lon = float(attrs["lon"]);
            self.poi = {
                "id": int(attrs["id"]),
                "lat": lat,
                "lon": lon,
                "lonlat": [ lon, lat ], # for 2d geospatial index
            }
        elif name == "tag":
            self.poi[attrs["k"]] = attrs["v"]
        else:
            self.poi = {}

    def endElement(self, name):
        if name == "node":
            if "amenity" in self.poi and "name" in self.poi:
                if self.first_line:
                    print(json.dumps(self.poi))
                    self.first_line = False
                else:
                    print(",", json.dumps(self.poi))
            self.poi = {}

    def characters(self, content):
        content = content.strip()
        if not content:
            return
        print("== characters, content='{c}'".format(c=content))
        

handler = MyHandler()
xml.sax.parse(bz2.BZ2File(args["input"], "rb"), handler)
