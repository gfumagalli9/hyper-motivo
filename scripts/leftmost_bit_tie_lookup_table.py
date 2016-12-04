#!/usr/bin/env python
from __future__ import print_function

def leftmost_bit_tie(x, sum):
    pos=0
    for d in [ -1 if c=='0' else 1 for c in "{0:08b}".format(x) ]:
        sum+=d
        pos+=1

        if sum==0:
            return  (True, pos)
    return (False, sum)

print("#ifndef LEFTMOST_BIT_TIE_LUT")
print("#define LEFTMOST_BIT_TIE_LUT")

print()

n=0
print("const int8_t leftmost_bit_tie_LUT0[] = {", end="");
for x in range(0b10000000, 0b11111111 + 1):
    print(", " if n!=0 else "", end="" if n%16!=0 else "\n")
    n+=1

    (found, info) = leftmost_bit_tie(x,0)
    if found:
        print(("~%d" % info).rjust(4), end="")
    else:
        print("%4d" % (info-1), end="")
print("\n};\n")

for tableno in range(1,4):
    print("const int8_t leftmost_bit_tie_LUT%d[] = {" % tableno, end="");
    n=0
    end = min(tableno*8, (4-tableno)*8)
    for i in range(1, end+1):
        for x in range(0, 0b11111111+1):
            print(", " if n!=0 else "", end="" if n%16!=0 else "\n")
            n+=1

            (found, info) = leftmost_bit_tie(x, i)
            if found:
                print(("~%d" % (info+tableno*8)).rjust(4), end="")
            else:
                if info<=(3-tableno)*8:
                    print("%4d" % (info-1), end="")
                else:
                    print("~127", end="")
    print("\n};\n")

print("#endif")
print()