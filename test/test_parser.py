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


def test_parser():
    tests = [
        '1 + 2 + 3 + 4',
        '5 * (3 -1) + 10',
        '(((1)))',
        '(1) + (3 * 5 + 1) -(18 - 2)',
        # '-19812734', # The program uses uint64_t and prints them as unsigned
        # So the result is different obviously
        '123456789',
        '(0)',
    ]
    tests = [t.encode('utf-8') for t in tests]

    # Open the process and send every input, line by line
    proc = subprocess.Popen(['./test_parser_bin'], stdin=subprocess.PIPE, stdout=subprocess.PIPE, bufsize=1)
    for t in tests:
        proc.stdin.write(t + b'\n')
    proc.stdin.close()

    # Now check the output
    for t in tests:
        res = proc.stdout.readline()
        if int(res) == int(eval(t)):
            logok(t)
        else:
            logfail(t)
    proc.kill()
    sys.exit(fail)


if __name__ == '__main__':
    test_parser()
