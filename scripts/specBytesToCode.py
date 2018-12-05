#!/usr/bin/python
#
#  A script to convert blob from the MS spec to array of byte to use in unitary tests
#
#       00000000 c7 01 00 01 20 54 e2
#       00000008 c7 01 00 01 20 54 e2
#    taken from the spec, will give:
#       0xc7, 0x01, 0x00, 0x01, 0x20, 0x54, 0xe2,
#       0xc7, 0x01, 0x00, 0x01, 0x20, 0x54, 0xe2,
#
#   Notes:
#       * the script reads the two first lines to detect the number of items per lines, so you need a blob with at least 2 lines
#       * the script detects if items are hex values by searching for + or -
#
#    sample usage:
#     $ python scripts/specBytesToCode.py < image.txt > image.c
#     then go edit image.c and paste that in your code 
import sys


def getOffset(l):
    token = l.split(' ')[0]
    return int(token, 16) 
    
def isHex(l):
    return l.find('+') == -1 and l.find('-') == -1

if __name__ == '__main__':
    
    lines = []
    itemPerLine = 16
    doHex = True
    
    # parse the offset to know how many items per line we have
    l1 = sys.stdin.readline().strip()
    l2 = sys.stdin.readline().strip()
    itemsPerLine = getOffset(l2) - getOffset(l1)
    
    #
    doHex = isHex(l1)
    
    for l in [l1, l2] + sys.stdin.readlines():  
        # 00000000 c7 01 00 01 20 54 e2 cc 00 jh.kjkjhkhk
        l = l.strip() # in case we have spaces before the offset
        pos = l.find(' ')
        l = l[pos+1:]
        items = []
        
        tokens = l.strip().split(' ')
        ntokens = 0
        for t in tokens:
            if not t: # empty token
                continue
            
            if ntokens == itemPerLine:
                break
            
            item = ''
            if doHex:
                item += '0x'
            item += t
            
            items.append(item)
            
            ntokens += 1
    
        lines.append(', '.join(items))
    
    print(",\n".join(lines))
