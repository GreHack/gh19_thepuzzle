
from random import randint, choice
from rc4 import RC4_crypt 
import struct
import subprocess

TUPAC_BEG_MARKER = "GiveUs2PacBack"
TUPAC_END_MARKER = "LetTheLegendResurrect"

PACKED = list()

def import_keys(fpath):
    with open(fpath, "r") as f:
        data = f.read()
    i = 0
    while i < len(data) - 1:
        if data[i] == '\\' and data[i+1] == 'x':
	    data = data[:i] + chr(int(data[i+2:i+4], 16)) + data[i+4:]
	i += 1
    return map(lambda s: s[1:-1], data.split(",\n"))

def tupac(binary, floc_beg, key):
    print("[2PAC] Packing function at Ox%x with key %s" % (floc_beg, key.encode("hex")))
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
    patched = patched[:floc_beg] + RC4_crypt(key, patched[floc_beg:floc_end + 1]) + patched[floc_end + 1:]
    # print("after: " + patched[floc_beg:floc_end + 1].encode("hex"))
    # Pack the function
    ## for i in xrange(floc_beg, floc_end + 1):
    ##    patched = patched[:i] + chr(ord(patched[i]) ^ 0x42) + patched[i+1:]
    PACKED.append(floc_beg)
    return patched

with open("main", "rb") as f:
    binary = f.read()

keys = import_keys("include/gen/rc4_keys.txt")

bookmark = binary.find(TUPAC_BEG_MARKER)
while bookmark != -1:
    while binary[bookmark:bookmark+4] != "\x55\x48\x89\xe5":
        bookmark -= 1
    binary = tupac(binary, bookmark, keys.pop(randint(0, len(keys) - 1)))
    bookmark = binary.find(TUPAC_BEG_MARKER, bookmark + 1)
    assert(bookmark == -1)
    break

with open("2pac_main", "wb") as f:
    f.write(binary)

# Get address of unpacking routine
unpacker = int(subprocess.check_output("readelf -s main | grep -E 'unpack$' | awk '{print $2}' | sed -re 's|^0+||'", shell=True), 16)

with open("dbg/2pac.debugging_script", 'w') as f:
    for func_addr in PACKED:
        f.write("b {} {}\n".format(func_addr, unpacker))
    f.write("c\n")
