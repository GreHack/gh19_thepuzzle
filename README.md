# GreHack 2019 - Reverse Prechall

## Clone data repository

In `gh19_thepuzzle` repository, do:

```
git clone https://github.com/GreHack/gh19_thepuzzle_data data

```

## Requirements

r2 and r2pipe are needed for the obfuscation

```
# Compile radare2
git clone https://github.com/radare/radare2
cd radare2
./sys/user.sh
./configure --prefix=~/.local
make -j4

# Install r2pipe in user home
pip3 install --user r2pipe
```

Then edit `R2_PATH` value in `2pac.py` to the directory where `radare2` executable
is located, or set `R2_PATH` to `None` if the executable is in your environment
PATH variable.


## Building

Type in, at the root directory:

```
make clean && make
```

