#!/usr/bin/env python3
import mmap
import sys
import re
from os import path

# location of `index.html`, base for all `src="..."` relative paths
base = "www"

# skip the `<link href=js/app.js rel=preload as=script>` tags
preload_tag = re.compile(b"^<link\\s.*\\brel=\"?preload[\" >]")

# process the `<script src=js/chunk-vendors.js></script>` tags
script_include_quoted = re.compile(b"^<script\\s.*\\bsrc=\"([^\"]*)\"")

# ... and the `<script src="js/chunk-vendors.js"></script>` as well
script_include_unquoted = re.compile(b"^<script\\s.*\\bsrc=([^\"][^ >]*)")

# don't read the files, map them into memory instead
with open(path.join(base, "index.html"), "rb") as f, mmap.mmap(f.fileno(), 0, mmap.MAP_PRIVATE, mmap.PROT_READ) as mm:
    start = 0   # processed this far
    end = mm.size()

    while start < end:
        # try to find the start of the next tag
        tagstart = mm.find(b"<", start)
        if tagstart < 0:    # no more tags, only text -> dump it
            sys.stdout.buffer.write(mm[start:])
            break
        if start < tagstart:    # some text before the next tag -> dump it
            sys.stdout.buffer.write(mm[start:tagstart])

        # try to find the end of this tag
        tagend = mm.find(b">", tagstart)
        if tagend < 0:      # unclosed tag, should not happen, but anyway -> dump it
            sys.stdout.buffer.write(mm[tagstart:])
            break

        start = tagend + 1  # next item follows after this tag

        # process this tag
        tag = mm[tagstart:start]

        if preload_tag.match(tag):
            pass    # skip it
        else:
            # check if it's a script include
            match = script_include_quoted.match(tag) or script_include_unquoted.match(tag)
            if match:
                scriptname = path.join(base, match.group(1).decode())

                # resolve the include: map it and dump its content
                with open(scriptname, "rb") as s, mmap.mmap(s.fileno(), 0, mmap.MAP_PRIVATE, mmap.PROT_READ) as script:
                    sys.stdout.buffer.write(b"<script>")
                    sys.stdout.buffer.write(script)

            else:
                # some other tag -> dump it as it is
                sys.stdout.buffer.write(tag)

# vim: set sw=4 ts=4 indk= et:
