import sys


def RC4_key_schedule(key):
    s = list(range(0, 256))
    j = 0
    for i in range(0, 256):
        j = (j ^ s[i] ^ key[i % len(key)]) % 256
        # print("i = %02x ; j = %02x ; s[i] = %02x ; s[j] = %02x" % (i, j, s[i], s[j]))
        s[i], s[j] = s[j], s[i]
    return s


def RC4_crypt(key, clear):
    s = RC4_key_schedule(key)
    i, j = 0, 0
    data = []
    for b in clear:
        i = (i + 1) % 256
        j = (j + s[i]) % 256
        s[i], s[j] = s[j], s[i]
        x = s[(s[i] + s[j]) % 256]
        data.append(x ^ b)
    return bytes(data)


if __name__ == "__main__":
	if len(sys.argv) < 4:
		print("usage: {} <key> <in> <out>".format(sys.argv[0]))
		exit(1)
	open(sys.argv[3], 'w').write(RC4_crypt(sys.argv[1], open(sys.argv[2]).read()))

