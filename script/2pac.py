
from random import randint, choice
import struct

TUPAC_BEG_MARKER = "GiveUs2PacBack"
TUPAC_END_MARKER = "LetTheLegendResurrect"

def tupac(binary, floc_beg):
    print("[2PAC] Packing function at Ox%x" % floc_beg)
    # Look for final marker
    pac_beg = binary.find(TUPAC_BEG_MARKER, floc_beg + 1)
    pac_end = binary.find(TUPAC_END_MARKER, pac_beg + len(TUPAC_BEG_MARKER))
    floc_end = binary.find("\xc3", pac_end + len(TUPAC_END_MARKER))
    patched = str(binary)
    # Nop the markers
    for i in xrange(pac_beg, pac_beg + len(TUPAC_BEG_MARKER)):
        assert(patched[i] == TUPAC_BEG_MARKER[i - pac_beg])
        patched = patched[:i] + '\x90' + patched[i+1:]
        assert(patched[i] == '\x90')
    for i in xrange(pac_end, pac_end + len(TUPAC_END_MARKER)):
        assert(patched[i] == TUPAC_END_MARKER[i - pac_end])
        patched = patched[:i] + '\x90' + patched[i+1:]
        assert(patched[i] == '\x90')
    # Pack the function
    for i in xrange(floc_beg, floc_end + 1):
        patched = patched[:i] + chr(ord(patched[i]) ^ 0x42) + patched[i+1:]
    return patched

with open("main", "rb") as f:
    binary = f.read()

bookmark = binary.find(TUPAC_BEG_MARKER)
while bookmark != -1:
    while binary[bookmark:bookmark+4] != "\x55\x48\x89\xe5":
        bookmark -= 1
    binary = tupac(binary, bookmark)
    bookmark = binary.find(TUPAC_BEG_MARKER, bookmark + 1)
    assert(bookmark == -1)
    break

with open("2pac_main", "wb") as f:
    f.write(binary)
