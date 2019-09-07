
from random import randint, choice
from rc4 import RC4_crypt 
import struct
import subprocess
import string
from os import chmod
import sys

TUPAC_BEG_MARKER = "GiveUs2PacBack"
TUPAC_END_MARKER = "LetTheLegendResurrect"

PACKED = list()

FUNCS = dict()
UFUNCS = list()

IN_BINARY = "main"
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

def tupac(binary, unpacked, floc_beg, key):
    print("[2PAC] Packing function at Ox%x with key %s" % (floc_beg, key.encode("hex")))
    # Look for final marker
    pac_beg = binary.find(TUPAC_BEG_MARKER, floc_beg + 1)
    pac_end = binary.find(TUPAC_END_MARKER, pac_beg + len(TUPAC_BEG_MARKER))
    floc_end = binary.find("\xc3", pac_end + len(TUPAC_END_MARKER))

    # Invert some jumps, for fun, just before encryption (old style)
    # binary = tupac_invert_jumps(binary, floc_beg, floc_end, DBG_SCRIPT)

    # Convert str to array because fuck it
    parray = [ord(c) for c in binary]
    uarray = [ord(c) for c in unpacked]

    # Replace jumps by a jump on themselves, because that's funny
    tupac_delete_jump(parray, floc_beg, floc_end, DBG_SCRIPT)
    tupac_delete_jump(uarray, floc_beg, floc_end, None)

    # Convert back array to str because fuck it
    pbinary = ''.join(map(chr, parray))
    ubinary = ''.join(map(chr, uarray))

    # Nop the markers
    patched = str(pbinary)
    upatched = str(ubinary)
    for i in xrange(pac_beg, pac_beg + len(TUPAC_BEG_MARKER)):
        assert(patched[i] == TUPAC_BEG_MARKER[i - pac_beg])
        patched = patched[:i] + '\x90' + patched[i+1:]
        upatched = upatched[:i] + '\x90' + upatched[i+1:]
        assert(patched[i] == '\x90')
    for i in xrange(pac_end, pac_end + len(TUPAC_END_MARKER)):
        assert(patched[i] == TUPAC_END_MARKER[i - pac_end])
        patched = patched[:i] + '\x90' + patched[i+1:]
        upatched = upatched[:i] + '\x90' + upatched[i+1:]
        assert(patched[i] == '\x90')

    # Now pack the function
    unpacked = str(upatched)
    crypt = RC4_crypt(key, patched[floc_beg:floc_end + 1])
    #print('Packing -> ', patched[floc_beg:floc_beg+4], crypt[:4])
    packed = patched[:floc_beg] + crypt + patched[floc_end + 1:]

    # print("after: " + patched[floc_beg:floc_end + 1].encode("hex"))
    # Pack the function
    ## for i in xrange(floc_beg, floc_end + 1):
    ##    patched = patched[:i] + chr(ord(patched[i]) ^ 0x42) + patched[i+1:]
    PACKED.append(floc_beg)
    return packed, unpacked


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
    #FUNCS['reverse_jump'] = get_symbol_info(file, 'reverse_jump')

    # Read the binary
    with open(file, "rb") as f:
        return f.read()


def bin_replace(data, before, after):
    if len(before) != len(after):
        print('/!\ WARNING: You are probably doing something shitty')
    where = data.find(before)
    off = where + len(before)
    return data[:where] + after + data[off:]


#def tupac_invert_jumps(data, floc_beg, floc_end, script):
#    size = floc_end - floc_beg
#    for i in range(size):
#        val = struct.unpack('B', data[floc_beg + i])[0] # Thank you Python2... Lol.
#        if val == 0x7c:
#            print('[2PAC] Replacing jumps at 0x{:x}'.format(floc_beg + i))
#            data = bin_replace(data, '\x7c', '\xff') # We can put anything
#            with open(script, 'a') as f:
#                f.write("b {} {}\n".format(floc_beg + i, FUNCS['reverse_jump'][0]))
#    return data


def tupac_delete_jump(data, floc_beg, floc_end, script):
    return # XXX FIXME
    size = floc_end - floc_beg
    repatch = []
    for i in range(floc_beg, floc_beg + size):
        # TODO This is completely stupid, it can be part of an address...
        if data[i] == 0xeb:
            original = data[i + 1]
            jump_destination = 0xfe # randint(0, 0xff)
            data[i + 1] = jump_destination
            if script is not None:
                with open(script, 'a') as f:
                    funcname = next(gen_function_name())
                    f.write('begin {}\nw {} {}\nend\n'.format(funcname, i + 1, original))
                    f.write('bh {} {}\n'.format(i, funcname))
                    # TODO Must add another handler *AFTER* the jump so theses changes
                    # are not sensitive to memory dump
                    repatch.append('w {} {}'.format(i + 1, jump_destination))
                print('[2PAC] Deleting jump at 0x{:x} ({:x} -> {:x})'.format(i, original, data[i + 1]))
    if len(repatch) > 0 and script:
        with open(script, 'a') as f:
            funcname = next(gen_function_name())
            f.write('begin {}\n'.format(funcname) + '\n'.join(repatch) + '\nend\n')
            f.write('bh {} {}\n'.format(floc_end, funcname))



def main():
    global IN_BINARY, DBG_SCRIPT
    KEYFILE = 'include/gen/rc4_keys.txt'
    # Get arguments
    if len(sys.argv) > 1:
        IN_BINARY = sys.argv[1]
    if len(sys.argv) > 2:
        DBG_SCRIPT = sys.argv[2]
    if len(sys.argv) > 3:
        KEYFILE = sys.argv[3]
    DST_BINARY = '2pac_' + IN_BINARY
    print("[*] Starting 2pac on '{}', outputs in '{}' and '{}'".format(IN_BINARY, DST_BINARY, DBG_SCRIPT))

    # Read binary and overwrite dest script
    binary = read_binary(IN_BINARY)
    with open(DBG_SCRIPT, "w") as f:
        f.write('')

    # Find keys and unpack
    keys = import_keys(KEYFILE)
    bookmark = binary.find(TUPAC_BEG_MARKER)
    packed = binary
    unpacked = binary
    while bookmark != -1:
        while binary[bookmark:bookmark+4] != "\x55\x48\x89\xe5":
            bookmark -= 1
        key = keys.pop(randint(0, len(keys) - 1))
        packed, unpacked = tupac(packed, unpacked, bookmark, key)
        bookmark = packed.find(TUPAC_BEG_MARKER, bookmark + 1)

    # Get address of unpacking routine
    unpacker, _ = get_symbol_info(IN_BINARY, 'unpack$')
    with open(DBG_SCRIPT, 'a') as f:
        for func_addr in PACKED:
            f.write("b {} {}\n".format(func_addr, unpacker))

    # Everything is done, write the binary and add a continue to the script
    with open(DST_BINARY, "wb") as f:
        f.write(packed)
    with open(IN_BINARY + '.unpacked', 'wb') as f:
        f.write(unpacked)
    chmod(IN_BINARY + '.unpacked', 0o755)
    with open(DBG_SCRIPT, 'a') as f:
        f.write("c\n")

if __name__ == '__main__':
    main()
