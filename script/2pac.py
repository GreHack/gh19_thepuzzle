#!/usr/bin/env python3


import sys
import re
import r2pipe
from rc4 import RC4_crypt
from random import randint, choice, seed, shuffle
from string import ascii_letters
from os import getenv
from struct import pack

TUPAC_BEG_MARKER = b'GiveUs2PacBack'
TUPAC_END_MARKER = b'LetTheLegendResurrect'

R2_PATH = None # getenv('HOME') + '/.local/bin'
r2 = None
IS_TEST = False

DO_NOT_OBFUSCATE = [] # ['kd_compare']

JMP_PATCH_LOC = None
JMP_COUNT = 0
JMP_DEST_SIZE = 4


def log(msg):
    print('[2PAC]', msg)


def gen_func_name(alt=''):
    while True:
        if not alt:
            f = open('/dev/urandom', 'rb')
            alt = f.read(4).hex()
            f.close()
        yield ''.join([choice(ascii_letters) for _ in range(6)]) + alt


def split_integer(num):
    #if num < 0:
    #    return '0' + str(num)
    #return num
    # TODO Make sure the number is not too big to avoid overflows in the C parser
    """ Split an integer into additions, just to bother people """
    assert type(num) == int

    nb_ops = randint(2, 6)
    ops = [choice('-+-+*') for _ in range(nb_ops)]
    nbs = [randint(0, abs(num//(nb_ops + 1))) for _ in range(nb_ops + 1)]
    res = ''.join([str(nbs[i]) + ops[i] for i in range(nb_ops)]) + str(nbs[nb_ops])
    cmp = str(num - eval(res))
    if cmp[0] == '-':
        res += cmp
    else:
        res += '+' + cmp
    cmp = eval(res)
    assert cmp == num
    return res


def patchit():
    if IS_TEST:
        return True
    return randint(0, 5) == 0



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
            if not patchit():
                offset += instr['size']
                continue

            ## Patch2: Inverse jumps and patch flags during runtime
            log('Patch2 at 0x{:x}'.format(offset))

            new_opcode = bytes([opcode[0] ^ 1, opcode[1]])
            fname = next(gen_func_name())
            death_func_name = next(gen_func_name())

            assert len(new_opcode) == len(opcode)
            assert new_opcode != opcode

            cmds.append('begin {}\nf z\nend'.format(fname))
            cmds.append('begin {}\nf z\nx {}\nend'.format(death_func_name, split_integer(offset)))
            cmds.append('bh {} {} {}'.format(split_integer(offset), fname, death_func_name))

            # Patch the binary
            r2.cmd('wx {} @ {}'.format(new_opcode.hex(), offset))

        elif instr['type'] in ['jmp', 'ujmp', 'cjmp'] and instr['size'] == 2:
            # Do not patch every jump, just do it randomly
            # TODO Make sure the first jump in the screenshot function is patched properly
            if not patchit():
                offset += instr['size']
                continue

            ## Patch1: Make jump destination random
            log('Patch1 at 0x{:x}: "{}" ({})'.format(offset, instr['disasm'], instr['bytes']))
            # Get the jump dest memory location
            global JMP_COUNT
            global JMP_PATCH_LOC
            if JMP_PATCH_LOC is None:
                JMP_PATCH_LOC = r2.cmdj('isj. @ loc.mem_jmp_array')['vaddr']
            mem_location = JMP_PATCH_LOC + JMP_COUNT * JMP_DEST_SIZE
            JMP_COUNT += 1

            n = instr['size'] - 1
            assert n <= JMP_DEST_SIZE
            destination = bytes([randint(0, 255) for _ in range(n)])
            original = int.from_bytes(opcode[1:], byteorder='big')
            new_opcode = opcode[:1] + destination
            assert len(new_opcode) == len(opcode)


            # Add calls to patch the bytes
            fname = next(gen_func_name())
            bp_func  = 'begin {}\n'.format(fname)
            bp_func += 'w {} {}\n'.format(split_integer(offset + 1), split_integer(original))
            bp_func += 'wr {} {}\n'.format(split_integer(mem_location), split_integer(JMP_DEST_SIZE))
            bp_func += 'end'
            cmds.append(bp_func)

            dfname = next(gen_func_name())
            bp_dfunc  = 'begin {}\n'.format(dfname)
            bp_dfunc += 'w {} {}\n'.format(split_integer(mem_location), split_integer(original))
            bp_dfunc += 'end'
            cmds.append(bp_dfunc)

            cmds.append('bh {} {} {}'.format(split_integer(offset), fname, dfname))

            # Add calls to repatch bad bytes at the end of the function
            # To avoid people from just dumping the binary
            repatch.append((offset + 1, mem_location, n))

            # Patch the binary
            r2.cmd('wx {} @ {}'.format(new_opcode.hex(), offset))

        elif instr['type'] == 'mov' and 'val' in instr:
            ops = r2.cmdj('aoj @ {}'.format(offset))[0]['opex']['operands']
            if ops[0]['type'] != 'reg':
                offset += instr['size']
                continue
            if not patchit():
                offset += instr['size']
                continue

            ## Patch3: Change mov immediate to something random and fix it on the fly
            log('Patch3 at 0x{:x}: "{}" ({})'.format(offset, instr['disasm'], instr['bytes']))

            # Add calls to fix the reg value on the fly
            reg = ops[0]['value']
            assert ops[1]['type'] == 'imm'
            reg_val = ops[1]['value']

            if instr['size'] == 5:
                op_beg = [opcode[0]]
                new_val = randint(0, 2**32)
                op_end = pack('<I', new_val)
            elif instr['size'] == 6:
                op_beg = [opcode[0], opcode[1]]
                new_val = randint(0, 2**32)
                op_end = pack('<I', new_val)
            elif instr['size'] == 10:
                op_beg = [opcode[0], opcode[1]]
                new_val = randint(0, 2**64)
                op_end = pack('<Q', new_val)
            else:
                print('Error: Unexpected size: {}!'.format(instr['size']))

            diff = reg_val - new_val
            new_opcode = bytes(op_beg) + op_end
            assert opcode != new_opcode
            assert len(opcode) == len(new_opcode)

            fname = next(gen_func_name())
            death_func_name = next(gen_func_name())
            cmds.append('begin {}\na ${} {}\nend'.format(fname, reg, split_integer(diff)))
            original_opcode = int.from_bytes(opcode, byteorder='little')
            cmds.append('begin {}\nw {} {} {}\nend'.format(death_func_name, split_integer(offset), split_integer(original_opcode), split_integer(instr['size'])))
            cmds.append('bh {} {} {}'.format(split_integer(offset + instr['size']), fname, death_func_name))

            # Patch the binary
            r2.cmd('wx {} @ {}'.format(new_opcode.hex(), offset))


        offset += instr['size']

    # Add a debugger function called at the end of a function
    # that will repatch everything done
    # to avoid process dumping
    if repatch:
        fcname = next(gen_func_name())
        cmd = ''
        cmd += 'begin {}'.format(fcname)
        for patch in repatch:
            cmd += '\nn {} {} {}'.format(split_integer(patch[0]), split_integer(patch[1]), split_integer(patch[2]))
        cmd += '\nend'
        cmds.append(cmd)
        cmds.append('bh {} {}'.format(split_integer(end), fcname))
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
        if sym_name not in DO_NOT_OBFUSCATE:
            log('Obfuscating "{}" (0x{:x})'.format(sym_name, func_beg))
            cmds += obfuscate_function(func_beg, func_end)
        else:
            log('Skipping obfuscation for {}'.format(sym_name))

        # Now go to the next function
        bookmark = data.find(TUPAC_BEG_MARKER, bookmark + len(TUPAC_BEG_MARKER))

    # Write random shit in the mem_jmp_array
    r2.cmd('wr 600 @ loc.mem_jmp_array')
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


def tupac(binary, keyfile, filename, pack=True):
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

    # Add callback to skip junk instructions in dbg script
    skipjunk = next(gen_func_name())
    cmd = ''
    cmd += 'begin {}'.format(skipjunk)
    cmd += '\na $rip {}'.format(split_integer(len(TUPAC_BEG_MARKER)-1))
    cmd += '\nend'
    commands.append(cmd)

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
        assert func_beg < func_end

        ## Junk the markers
        data = list(data)
        for i in range(pack_beg, pack_beg + len(TUPAC_BEG_MARKER)):
            assert(data[i] == TUPAC_BEG_MARKER[i - pack_beg])
            data[i] = randint(0, 255)
            # assert(data[i] == randint(0, 255) 0x90)
        for i in range(pack_end, pack_end + len(TUPAC_END_MARKER)):
            assert(data[i] == TUPAC_END_MARKER[i - pack_end])
            data[i] = 0x90
            assert(data[i] == 0x90)

        data = bytes(data)
        if pack:
            ## Pack the function
            crypt = RC4_crypt(key, data[func_beg:func_end+1])

            data = data[:func_beg] + crypt + data[func_end + 1:]

            ## Add a call to the unpacking handler for this offset
            commands.append('b {} {}'.format(split_integer(func_beg), split_integer(unpacker_addr)))
            ## Add a call to repack the function at the end
            commands.append('b {} {}'.format(split_integer(func_end), split_integer(unpacker_addr)))
            ## Add a handler for when the bp will be disabled
            fixjunk = next(gen_func_name())
            cmd = ''
            cmd += 'begin {}'.format(fixjunk)
            # Write a jump
            cmd += '\nw {} {}'.format(split_integer(pack_beg), split_integer(0xeb))
            # Write offset to jump over junk
            cmd += '\nw {} {}'.format(split_integer(pack_beg + 1), split_integer(len(TUPAC_BEG_MARKER) - 2))
            cmd += '\na $rip 1'
            cmd += '\nend'
            commands.append(cmd)
            # Add the bp to get around the junk
            commands.append('bh {} {} {}'.format(split_integer(pack_beg), skipjunk, fixjunk))

        # Now go to the next function
        bookmark = data.find(TUPAC_BEG_MARKER, bookmark + len(TUPAC_BEG_MARKER))

    # Custom check for hide_me and start_timer
    if r2.cmd('is~hide_me'):
        r2.cmd('af @ sym.hide_me')
        instr = r2.cmdj('pdfj @ sym.hide_me')
        for i in instr['ops']:
            if i['type'] == 'call' and 'check_user' in i['disasm']:
                hfunc = next(gen_func_name())
                func_loc = r2.cmdj('isj. @ sym.start_timer')['vaddr']
                commands.append('begin {}\nmc {}\nend'.format(hfunc, split_integer(func_loc)))
                commands.append('bh {} {}'.format(split_integer(i['offset']), hfunc))

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
    if len(argv) < 4:
        print('Usage: {} <input_binary> <output_script> <input_keys>'.format(argv[0]))
        sys.exit(1)
    if len(argv) == 5:
        if argv[4] == 'TEST':
            global IS_TEST
            IS_TEST = True
            log('Enabled in test mode')
    input_binary, output_script, input_keys = argv[1], argv[2], argv[3]
    patched_binary = input_binary + '.unpacked'

    # Start the obfuscation
    cmds = obfuscate_all(input_binary, patched_binary)

    # Save current binary and do the packing
    cmds += tupac(patched_binary, input_keys, input_binary + '.2pac', True)
    # Shuffle it so it's harder to understand the script
    shuffle(cmds)

    # Write debugging script
    with open(output_script, 'w') as f:
        txt = '\n'.join(cmds) + '\n'
        f.write(txt)
        f.write('c\n')


if __name__ == '__main__':
    main(sys.argv)
