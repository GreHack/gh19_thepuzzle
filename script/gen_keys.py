#-*- coding:utf-8 -*-

from random import randint

NKEY = 500
KLEN = 3

with open("include/gen/rc4_consts.txt", "w") as f:
	f.write("#define NKEY {}\n".format(NKEY))
	f.write("#define KLEN {}\n".format(KLEN))

with open("include/gen/rc4_keys.txt", "w") as f:
	for i in xrange(NKEY-1):
		f.write(("\"%s\"" % "".join(["\\x%02x" % (randint(0, 255)) for j in xrange(KLEN)])).encode("ascii"))
		if i < NKEY - 1:
			f.write(",")
		f.write("\n")
