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

R2_PATH = None # getenv('HOME') + '/.local/bin'
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
    while (offset < end):
        instr = r2.cmdj('pdj 1 @ {}'.format(offset))[0]
        # print(hex(instr['offset']), instr['disasm'], instr['bytes'], instr['size'])
        if instr['type'] == 'invalid':
            log('ERROR: Got an invalid instruction at 0x{:x}!'.format(offset))
            break
        opcode = bytes.fromhex(instr['bytes'])

        #####################
        # Obfuscation is here
        #####################

        # TODO Handle jge, jl sf = of ; sf != of
        # TODO Handle jle, jg zf = 1 OR sf != of ; zf = 0 ^ sf = of
        # TODO Handle [0x76, 0x77]: # jbe, ja
        if instr['type'] in ['jmp', 'ujmp', 'cjmp'] and opcode[0] in [0x74, 0x75]: # je, jne
            if randint(0, 1):
                offset += instr['size']
                continue

            ## Patch2: Inverse jumps and patch flags during runtime
            log('Patch2 at 0x{:x}'.format(offset))

            new_opcode = bytes([opcode[0] ^ 1, opcode[1]])
            fname = next(gen_func_name())
            cmds.append('begin {}\nf z\nend'.format(fname))
            cmds.append('bh {} {}'.format(offset, fname))

            # Patch the binary
            assert len(new_opcode) == len(opcode)
            assert new_opcode != opcode
            r2.cmd('wx {} @ {}'.format(new_opcode.hex(), offset))

        elif instr['type'] in ['jmp', 'ujmp', 'cjmp'] and instr['size'] == 2:
            # Do not patch every jump, just do it randomly
            if randint(0, 10) != 0:
                offset += instr['size']
                continue

            ## Patch1: Make jump destination random
            log('Patch1 at 0x{:x}: "{}" ({})'.format(offset, instr['disasm'], instr['bytes']))

            n = instr['size'] - 1
            destination = [randint(0, 255) for _ in range(n)]
            original = int.from_bytes(opcode[1:], byteorder='big')
            new_opcode = opcode[:1] + bytes(destination)

            # Add calls to patch the bytes
            fname = next(gen_func_name())
            cmds.append('begin {}\nw {} {}\nend'.format(fname, offset + 1, original))
            cmds.append('bh {} {}'.format(offset, fname))
            assert opcode != new_opcode
            assert len(new_opcode) == len(opcode)

            # Add calls to repatch bad bytes at the end of the function
            # To avoid people from just dumping the binary
            repatch.append((offset + 1, randint(0, 255)))

            # Patch the binary
            r2.cmd('wx {} @ {}'.format(new_opcode.hex(), offset))


        offset += instr['size']

    # Add a debugger function called at the end of a function
    # that will repatch everything done
    if repatch:
        fcname = next(gen_func_name())
        cmds.append('begin {}'.format(fcname))
        for patch in repatch:
            cmds.append('w {} {}'.format(patch[0], patch[1]))
        cmds.append('end')
        cmds.append('bh {} {}'.format(end, fcname))
    return cmds


def obfuscate_all(source, output):
    # Do a safe copy of the binary and open r2 in write mode
    with open(source, 'rb') as f:
        data = f.read()
    with open(output, 'wb') as f:
        f.write(data)
    # Open the file in write mode with r2
    global r2
    r2 = r2pipe.open(output, ['-w'], radare2home=R2_PATH)

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


def tupac(binary, keyfile, filename):
    # Initialize r2 for reading
    global r2
    r2 = r2pipe.open(binary, radare2home=R2_PATH)

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
        log('Packing function from 0x{:x} to 0x{:x} with key "{}"'.format(func_beg, func_end, key.hex()))

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

        data = data[:func_beg] + crypt + data[func_end + 1:]

        ## Add a call to the unpacking handler for this offset
        commands.append('b {} {}'.format(func_beg, unpacker_addr))
        ## Add a call to repack the function at the end
        commands.append('b {} {}'.format(func_end, unpacker_addr, func_beg))

        # Now go to the next function
        bookmark = data.find(TUPAC_BEG_MARKER, bookmark + len(TUPAC_BEG_MARKER))

    # Write the modified binary
    with open(filename, 'wb') as f:
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
    patched_binary = input_binary + '.unpacked'

    # Start the obfuscation
    cmds = obfuscate_all(input_binary, patched_binary)

    # Save current binary and do the packing
    cmds += tupac(patched_binary, input_keys, input_binary + '.2pac')

    # Write debugging script
    with open(output_script, 'w') as f:
        txt = '\n'.join(cmds) + '\n'
        f.write(txt)
        f.write('c\n')


if __name__ == '__main__':
    main(sys.argv)
