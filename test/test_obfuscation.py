#!/usr/bin/env python3


import subprocess
import sys

GREEN = '\033[92m'
RED = '\033[91m'
ENDC = '\033[0m'

fail = 0


def logok(t):
    print(GREEN + '[OK] ' + ENDC + t.decode('utf-8'))


def logfail(t):
    global fail
    fail = 1
    print(RED + '[XX] ' + ENDC + t.decode('utf-8'))


def test_obfuscation():
    # Open the process and send every input, line by line
    proc = subprocess.Popen(['./test_obfuscation_bin', '2pac.debugging_script'], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, bufsize=1)
    t = b'Obfuscation'

    # Close stdin
    proc.stdin.close()

    # Now check the output
    res = proc.stdout.readline().decode()
    if res and int(res) == 10:
        logok(t)
    else:
        logfail(t)
    proc.kill()


if __name__ == '__main__':
    test_obfuscation()
    sys.exit(fail)
