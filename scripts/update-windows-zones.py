#!/usr/bin/env python3

import os
import urllib.request
import xml.etree.ElementTree as ET

name = os.path.realpath(__file__)
base = os.path.normpath(os.path.join(os.path.dirname(name), '..'))
rname = os.path.relpath(name, base)
zfile = os.path.join(base, 'winpr/libwinpr/timezone/WindowsZones.c')
url = 'https://raw.githubusercontent.com/unicode-org/cldr/latest/common/supplemental/windowsZones.xml'

try:
    with urllib.request.urlopen(url) as response:
        xml = response.read()
        root = ET.fromstring(xml)
        entries = []
        for child in root.iter('mapZone'):
            tzid = child.get('type')
            windows = child.get('other')
            entries += ['\t{ "' + windows + '", "' + tzid + '" },\n']
        entries.sort()

        with open(zfile, 'w') as f:
            f.write('/*\n')
            f.write(' * Automatically generated with ' + str(rname) + '\n')
            f.write(' */\n')
            f.write('\n')
            f.write('#include "WindowsZones.h"\n')
            f.write('\n')
            f.write('const WINDOWS_TZID_ENTRY WindowsTimeZoneIdTable[] =\n')
            f.write('{\n')
            for entry in entries:
                f.write(entry)
            f.write('};\n')
            f.write('\n');
            f.write('const size_t WindowsTimeZoneIdTableNrElements = ARRAYSIZE(WindowsTimeZoneIdTable);\n')
except Exception as e:
    print('----------------------------------------------------')
    print(str(e))
    print('----------------------------------------------------')
