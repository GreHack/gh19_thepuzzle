
from random import randint, choice
from rc4 import RC4_crypt 
import struct
import subprocess
import string
from os import chmod

TUPAC_BEG_MARKER = "GiveUs2PacBack"
TUPAC_END_MARKER = "LetTheLegendResurrect"

PACKED = list()

FUNCS = dict()
UFUNCS = list()

DBG_SCRIPT = "dbg/2pac.debugging_script"

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

    # Invert some jumps, for fun, just before encryption (old style)
    # binary = tupac_invert_jumps(binary, floc_beg, floc_end, DBG_SCRIPT)

    # Convert str to array because fuck it
    array = [ord(c) for c in binary]

    # Replace jumps by a jump on themselves, because that's funny
    tupac_delete_jump(array, floc_beg, floc_end, DBG_SCRIPT)

    # Convert back array to str because fuck it
    binary = ''.join(map(chr, array))

    # Nop the markers
    patched = str(binary)
    for i in xrange(pac_beg, pac_beg + len(TUPAC_BEG_MARKER)):
        assert(patched[i] == TUPAC_BEG_MARKER[i - pac_beg])
        patched = patched[:i] + '\x90' + patched[i+1:]
        assert(patched[i] == '\x90')
    for i in xrange(pac_end, pac_end + len(TUPAC_END_MARKER)):
        assert(patched[i] == TUPAC_END_MARKER[i - pac_end])
        patched = patched[:i] + '\x90' + patched[i+1:]
        assert(patched[i] == '\x90')

    # Now pack the function
    unpacked = str(patched)
    patched = patched[:floc_beg] + RC4_crypt(key, patched[floc_beg:floc_end + 1]) + patched[floc_end + 1:]

    # print("after: " + patched[floc_beg:floc_end + 1].encode("hex"))
    # Pack the function
    ## for i in xrange(floc_beg, floc_end + 1):
    ##    patched = patched[:i] + chr(ord(patched[i]) ^ 0x42) + patched[i+1:]
    PACKED.append(floc_beg)
    return patched, unpacked


def gen_function_name():
    while True:
        name = ''.join(choice(string.ascii_lowercase) for _ in range(10))
        if name in UFUNCS:
            continue
        UFUNCS.append(name)
        yield name


def get_symbol_info(binary, symbol):
    t = subprocess.check_output(
        "readelf -s {:s} | grep -E '{:s}' | awk '{{print $2 \"_\" $3}}'".format(binary, symbol),
        shell=True).strip().split('_')
    # Return offset, size
    return int(t[0], 16), int(t[1], 16)


def read_binary(file):
    # Init the constants
    global FUNCS
    FUNCS['reverse_jump'] = get_symbol_info(file, 'reverse_jump')

    # Read the binary
    with open(file, "rb") as f:
        return f.read()


def bin_replace(data, before, after):
    if len(before) != len(after):
        print('/!\ WARNING: You are probably doing something shitty')
    where = data.find(before)
    off = where + len(before)
    return data[:where] + after + data[off:]


def tupac_invert_jumps(data, floc_beg, floc_end, script):
    size = floc_end - floc_beg
    for i in range(size):
        val = struct.unpack('B', data[floc_beg + i])[0] # Thank you Python2... Lol.
        if val == 0x7c:
            print('[2PAC] Replacing jumps at 0x{:x}'.format(floc_beg + i))
            data = bin_replace(data, '\x7c', '\xff') # We can put anything
            with open(script, 'a') as f:
                f.write("b {} {}\n".format(floc_beg + i, FUNCS['reverse_jump'][0]))
    return data


def tupac_delete_jump(data, floc_beg, floc_end, script):
    size = floc_end - floc_beg
    for i in range(floc_beg, floc_beg + size):
        if data[i] == 0xeb:
            original = data[i + 1]
            jump_destination = 0xfe # randint(0, 0xff)
            data[i + 1] = jump_destination
            with open(script, 'a') as f:
                funcname = next(gen_function_name())
                f.write('begin {}\nw {} {}\nend\n'.format(funcname, i + 1, original))
                f.write('bh {} {}\n'.format(i, funcname))
                # TODO Must add another handler *AFTER* the jump so theses changes
                # are not sensitive to memory dump
            print('[2PAC] Deleting jump at 0x{:x} ({:x} -> {:x})'.format(i, original, data[i + 1]))



def main():
    IN_BINARY = "main"
    DST_BINARY = "2pac_main"
    binary = read_binary(IN_BINARY)
    with open(DBG_SCRIPT, "w") as f:
        f.write('')

    # Find keys and unpack
    keys = import_keys("include/gen/rc4_keys.txt")
    bookmark = binary.find(TUPAC_BEG_MARKER)
    packed = binary
    while bookmark != -1:
        while binary[bookmark:bookmark+4] != "\x55\x48\x89\xe5":
            bookmark -= 1

        packed, unpacked = tupac(packed, bookmark, keys.pop(randint(0, len(keys) - 1)))
        bookmark = packed.find(TUPAC_BEG_MARKER, bookmark + 1)

    # Get address of unpacking routine
    unpacker, _ = get_symbol_info(IN_BINARY, 'unpack$')
    with open(DBG_SCRIPT, 'a') as f:
        for func_addr in PACKED:
            f.write("b {} {}\n".format(func_addr, unpacker))

    # Everything is done, write the binary and add a continue to the script
    with open(DST_BINARY, "wb") as f:
        f.write(packed)
    # TODO FIXME
    with open(IN_BINARY + '.unpacked', 'wb') as f:
        f.write(unpacked)
    chmod(IN_BINARY + '.unpacked', 0755)
    with open(DBG_SCRIPT, 'a') as f:
        f.write("c\n")

if __name__ == '__main__':
    main()
