#!/usr/bin/env python

# MIT License
#
# Copyright (c) 2017-2019 Stefano Leucci and Marco Bressan
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

from __future__ import print_function

def leftmost_bit_tie(x, sum):
    pos=0
    for d in [ -1 if c=='0' else 1 for c in "{0:08b}".format(x) ]:
        sum+=d
        pos+=1

        if sum==0:
            return  (True, pos)
    return (False, sum)

print("#ifndef MOTIVO_LEFTMOST_BIT_TIE_LUT")
print("#define MOTIVO_LEFTMOST_BIT_TIE_LUT")

print()

n=0
print("const uint8_t leftmost_bit_tie_LUT0[] = {", end="");
for x in range(0b10000000, 0b11111111 + 1):
    print(", " if n!=0 else "", end="" if n%16!=0 else "\n")
    n+=1

    (found, info) = leftmost_bit_tie(x,0)
    if found:
        print(("%d" % (255-info)).rjust(3), end="")
    else:
        print("%3d" % (info-1), end="")
print("\n};\n")

for tableno in range(1,4):
    print("const uint8_t leftmost_bit_tie_LUT%d[] = {" % tableno, end="");
    n=0
    end = min(tableno*8, (4-tableno)*8)
    for i in range(1, end+1):
        for x in range(0, 0b11111111+1):
            print(", " if n!=0 else "", end="" if n%16!=0 else "\n")
            n+=1

            (found, info) = leftmost_bit_tie(x, i)
            if found:
                print(("%d" % (255-(info+tableno*8))).rjust(3), end="")
            else:
                if info<=(3-tableno)*8:
                    print("%3d" % (info-1), end="")
                else:
                    print("128", end="") #255-127
    print("\n};\n")

print("#endif //MOTIVO_LEFTMOST_BIT_TIE_LUT")