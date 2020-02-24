#!/bin/env python3
#
# This is a helper script that fetches the current language and keyboard tables
# and writes the result to a C compatible struct.
#
import os
import sys
import requests
import numpy as np
import traceback
from bs4 import BeautifulSoup
from bs4 import element

intro = '''/* This file is auto generated from
 *
 * https://docs.microsoft.com/en-us/windows/win32/intl/language-identifier-constants-and-strings
 *
 * please do not edit but use ./scripts/fetch_language_identifiers.py to regenerate!
 */

'''

def parse_html(text):
    soup = BeautifulSoup(text, 'html.parser')
    table = soup.find("table")
    head = table.find('thead').find('tr')
    headers = []
    for th in head:
        if type(th) == element.Tag:
            headers += th

    body = table.find('tbody')
    languages = []

    for tr in body:
        if type(tr) == element.Tag:
            entry = []
            for th in tr:
                if type(th) == element.Tag:
                    if th.string:
                        entry += [th.string]
                    else:
                        entry += ['']
            languages += [entry]
    return [headers, languages]

def is_base(num, base):
    try:
        v = int(num, base)
        return True
    except ValueError:
        return False

def padhexa(v):
    s = hex(v)
    return '0x' + s[2:].zfill(8)

def write_struct(fp, struct, name, url, base, inv = False, typemap = None):
    li = requests.get(url)
    if li.status_code != requests.codes.ok:
        print('Could not fetch ' + str(url) + ', reponse code ' + str(li.status_code))
        sys.exit(1)
    headers, languages = parse_html(li.text)

    fp.write('const ' + str(struct) + ' ' + str(name) + '[] =\n')
    fp.write('{\n')
    fp.write('/* ')
    for h in headers:
        fp.write('\t[')
        fp.write(h)
        fp.write(']\t')
    fp.write('*/\n')
    last = [None] * 32
    for language in languages:
        fp.write('\t{ ')
        line = ''
        pos = 0
        for e in language:
            try:
                v = int(e, base=base)
                switcher = {
                        0: padhexa(v),
                        2: bin(v),
                        8: oct(v),
                        10: str(v),
                        16: padhexa(v)
                        }
                h = str(switcher.get(base))
                if h != "None":
                  last[pos] = h
                if inv:
                  line = h + ', ' + line
                else:
                  line += h + ', '
            except ValueError:
                if typemap and typemap[pos] != str:
                    line += str(last[pos]) + ',\t'
                else:
                    if e == "":
                      line += '"' + str(last[pos]) + '",\t'
                    else:
                      line += '"' + e + '",\t'
                      if e != "None":
                        last[pos] = str(e)
            pos = pos + 1
        fp.write(line[:-2] + '},\n')
    fp.write('};\n')
    fp.write('\n')

def update_lang_identifiers(fp):
#   [Language identifier]   [Primary language]    [Prim. lang. identifier]    [Prim. lang. symbol]    [Sublanguage]   [Sublang. identifier]   [Sublang. symbol]
    write_struct(fp, 'LanguageIdentifier', 'language_identifiers', 'https://docs.microsoft.com/en-us/windows/win32/intl/language-identifier-constants-and-strings', 16, False, [int, str, int, str, str, int, str])

def update_code_pages(fp):
    write_struct(fp, 'CodePage', 'code_pages', 'https://docs.microsoft.com/en-us/windows/win32/intl/code-page-identifiers', 10)

def update_input_locales(fp):
    write_struct(fp, 'KeyboardIdentifier', 'keyboard_identifiers', 'https://docs.microsoft.com/en-us/previous-versions/windows/it-pro/windows-vista/cc766503(v=ws.10)', 0)
    write_struct(fp, 'RDP_KEYBOARD_LAYOUT', 'RDP_KEYBOARD_LAYOUT_TABLE', 'https://docs.microsoft.com/en-us/windows-hardware/manufacture/desktop/windows-language-pack-default-values', 16, True)

try:
    with open('language_identifiers.c', 'w') as fp:
        fp.write(intro)
        update_lang_identifiers(fp)
        update_code_pages(fp)
        update_input_locales(fp)
except:
    print('exception cought')
    traceback.print_exc()
