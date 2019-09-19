
from random import randint
from datetime import datetime
from hashlib import sha256
import sys
from PIL import Image

mapping = "#GreHack19"

flag_len = 8
flag = randint(10**(flag_len - 1), 10**flag_len - 1)
assert(len(str(flag)) == flag_len)
str_flag = "".join(map(lambda a: mapping[int(a)], str(flag)))
h = sha256()
h.update(str_flag)

print("[{}] flag: {} | {} | sha256: {}".format(datetime.now(), flag, str_flag, h.hexdigest()))

hash_flag = ", ".join(map(lambda a: hex(ord(a) ^ 0x19), h.digest()))

header = """
#ifndef __FLAG_H__
#define __FLAG_H__

#define FLAG_LEN {}

static unsigned char flag_hash[] = {{ {} }};

#endif
""".format(flag_len, hash_flag)

open("include/gen/flag.h", "w").write(header)

### Generate flag image to test challenge

MARGIN_LEFT = 20
MARGIN_RIGHT = 20
MARGIN = 10
MARGIN_BOTTOM = 10
MARGIN_TOP = 10
SQ = 56

img_ref = list()

for i in xrange(10):
	try:
		img_ref.append(open("script/data/{}.txt".format(i)).readlines())
	except:
		img_ref.append(None)
		pass

def generate_digit(pix, i, d):
	if img_ref[d] is None:
		return
	w = MARGIN_LEFT + (SQ + MARGIN) * i
	h = MARGIN_TOP
	for i, line in enumerate(img_ref[d]):
		for j, c in enumerate(line[:-1]):
			if c != ' ':
				pix[w + 2*j, h + 2*i] = (0, 0, 0)
				pix[w + 2*j + 1, h + 2*i] = (0, 0, 0)
				pix[w + 2*j, h + 2*i + 1] = (0, 0, 0)
				pix[w + 2*j + 1, h + 2*i + 1] = (0, 0, 0)


flag = map(int, str(flag))
img = Image.new('RGB', (SQ * len(flag) + MARGIN * (len(flag) - 1) + MARGIN_LEFT + MARGIN_RIGHT, SQ + MARGIN_TOP + MARGIN_BOTTOM), color=(255, 255, 255))
pix = img.load()
for i, d in enumerate(flag):
	generate_digit(pix, i, d)

img.save("data/flag.png")
