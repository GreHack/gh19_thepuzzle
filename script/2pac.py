#!/usr/bin/env python3


import sys
import re
import r2pipe
from rc4 import RC4_crypt
from random import randint, choice, seed
from string import ascii_letters
from os import getenv

TUPAC_BEG_MARKER = b'GiveUs2PacBack'
TUPAC_END_MARKER = b'LetTheLegendResurrect'

R2_PATH = getenv('HOME') + '/.local/bin'
r2 = None


def log(msg):
    print('[2PAC]', msg)


def gen_func_name(alt=''):
    while True:
        if not alt:
            f = open('/dev/urandom', 'rb')
            alt = f.read(4).hex()
            f.close()
        yield ''.join([choice(ascii_letters) for _ in range(6)]) + alt


def obfuscate_function(beg, end):
    offset = beg
    cmds = []
    repatch = []
    seed(0) # TODO Remove this for more randomness
    while (offset < end):
        instr = r2.cmdj('pdj 1 @ {}'.format(offset))[0]
        # print(hex(instr['offset']), instr['disasm'], instr['bytes'], instr['size'])
        if instr['type'] == 'invalid':
            log('ERROR: Got an invalid instruction at 0x{:x}!'.format(offset))
            break
        opcode = bytes.fromhex(instr['bytes'])

        # Obfuscation is here
        if instr['type'] in ['jmp', 'ujmp']:
            destination = randint(0, 255)
            original = opcode[1]

            new_opcode = opcode[:1] + bytes([destination])
            assert(len(new_opcode) == len(opcode))
            r2.cmd('wx {} @ {}'.format(new_opcode.hex(), offset))

            fname = next(gen_func_name())
            # Add calls to patch the bytes
            cmds.append('begin {}\nw {} {}\nend'.format(fname, offset + 1, original))
            cmds.append('bh {} {}'.format(offset, fname))
            # Add calls to repatch bad bytes at the end of the function
            # To avoid people from just dumping the binary
            repatch.append((offset + 1, randint(0, 255)))

        offset += instr['size']

    # Add a debugger function called at the end of a function
    # that will repatch everything done
    fcname = next(gen_func_name())
    cmds.append('begin {}'.format(fcname))
    for patch in repatch:
        cmds.append('w {} {}'.format(patch[0], patch[1]))
    cmds.append('end')
    cmds.append('bh {} {}'.format(end, fcname))
    return cmds


def obfuscate_all(binary):
    # Do a safe copy of the binary and open r2 in write mode
    with open(binary, 'rb') as f:
        data = f.read()
    with open(binary + '.unpacked', 'wb') as f:
        f.write(data)
    # Open the file in write mode with r2
    global r2
    r2 = r2pipe.open(binary + '.unpacked', ['-w'], radare2home=R2_PATH)

    cmds = []
    bookmark = data.find(TUPAC_BEG_MARKER)
    while bookmark != -1:
        # Get function limits
        func_beg = bookmark + len(TUPAC_BEG_MARKER)
        func_end = data.find(TUPAC_END_MARKER, bookmark + len(TUPAC_BEG_MARKER))

        # Do obfuscate that function
        sym_name = r2.cmdj('isj. @ {}'.format(func_beg + 2))['name']
        log('Obfuscating "{}" (0x{:x})'.format(sym_name, func_beg))
        cmds += obfuscate_function(func_beg, func_end)

        # Now go to the next function
        bookmark = data.find(TUPAC_BEG_MARKER, bookmark + len(TUPAC_BEG_MARKER))

    r2.quit()
    return cmds


def import_keys(fpath):
    keys = []
    with open(fpath, 'rb') as f:
        while True:
            k = f.readline()
            if not k or b'AYO' in k: break
            k = re.findall(b'"(.*)"', k)[0].replace(b'\\x', b'')
            k = bytes.fromhex(k.decode())
            keys.append(k)
    if not len(keys):
        log('ERROR: Could not load any key!')
    return keys


def tupac(binary, keyfile):
    # Initialize r2 for reading
    global r2
    r2 = r2pipe.open(binary + '.unpacked', radare2home=R2_PATH)
    seed(0) # TODO Remove this for more randomness

    with open(binary, 'rb') as f:
        data = f.read()

    keys = import_keys(keyfile)
    commands = []

    unpacker_addr = [sym['vaddr'] for sym in r2.cmdj('isj') if sym['type'] == 'FUNC' and sym['name'] == 'unpack']
    assert(unpacker_addr)
    unpacker_addr = unpacker_addr[0]

    bookmark = data.find(TUPAC_BEG_MARKER)
    while bookmark != -1:
        while data[bookmark:bookmark+4] != b'\x55\x48\x89\xe5':
            bookmark -= 1
        key = keys.pop(randint(0, len(keys) - 1))

        # Do the packing here
        func_beg = bookmark
        pack_beg = data.find(TUPAC_BEG_MARKER, func_beg + 1)
        pack_end = data.find(TUPAC_END_MARKER, pack_beg)
        func_end = data.find(b'\xc3', pack_end + len(TUPAC_END_MARKER))
        log('Packing function at 0x{:x} with key "{}"'.format(func_beg, key.hex()))

        ## Nop the markers
        data = list(data)
        for i in range(pack_beg, pack_beg + len(TUPAC_BEG_MARKER)):
            assert(data[i] == TUPAC_BEG_MARKER[i - pack_beg])
            data[i] = 0x90
            assert(data[i] == 0x90)
        for i in range(pack_end, pack_end + len(TUPAC_END_MARKER)):
            assert(data[i] == TUPAC_END_MARKER[i - pack_end])
            data[i] = 0x90
            assert(data[i] == 0x90)

        ## Pack the function
        data = bytes(data)
        crypt = RC4_crypt(key, data[func_beg:func_end+1])
        with open('/tmp/fu', 'wb') as f:
            f.write(crypt)
        data = data[:func_beg] + crypt + data[func_end + 1:]

        ## Add a call to the unpacking handler for this offset
        commands.append('b {} {}'.format(func_beg, unpacker_addr))

        # Now go to the next function
        bookmark = data.find(TUPAC_BEG_MARKER, bookmark + len(TUPAC_BEG_MARKER))

    # Write the modified binary
    with open(binary + '.2pac', 'wb') as f:
        f.write(data)

    r2.quit()

    return commands


def main(argv: list):
    input_binary = ''
    output_script = ''
    input_keys = ''

    # Read program arguments
    if len(argv) != 4:
        print('Usage: {} <input_binary> <output_script> <input_keys>'.format(argv[0]))
        sys.exit(1)
    input_binary, output_script, input_keys = argv[1], argv[2], argv[3]

    # Start the obfuscation
    cmds = obfuscate_all(input_binary)

    # Save current binary and do the packing
    cmds += tupac(input_binary, input_keys)

    # Write debugging script
    with open(output_script, 'w') as f:
        txt = '\n'.join(cmds) + '\n'
        f.write(txt)
        f.write('c\n')


if __name__ == '__main__':
    main(sys.argv)
