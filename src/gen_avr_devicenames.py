#!/usr/bin/env python3
"""Regenerate src/avr_devicenames.inc from avrdude-avr-init.xml.

The AVR device index sent via STK_PARAM_DEVICE_IDENTITY must match the STM32
firmware g_avrDeviceTable, which is generated from the same XML in document
order.  Run this script whenever avrdude-avr-init.xml is updated, then rebuild.
"""
import argparse, os, xml.etree.ElementTree as ET

def main():
    default_xml = r'E:\Codex\Project\Keil\Reference_Local\STM32F103VET6_Programmer_V1\PROGRAMMER\avrdude-avr-init.xml'
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--xml', default=default_xml, help='path to avrdude-avr-init.xml')
    ap.add_argument('--out', default=os.path.join(os.path.dirname(os.path.abspath(__file__)), 'avr_devicenames.inc'))
    args = ap.parse_args()

    root = ET.parse(args.xml).getroot()
    ids = [p.get('id') for p in root.findall('part') if p.get('id')]
    lines = ['/* Auto-generated from avrdude-avr-init.xml - %d devices */' % len(ids)]
    lines.append('#define AVR_DEVICE_COUNT %d' % len(ids))
    lines.append('')
    lines.append('static const char *const avr_device_ids[AVR_DEVICE_COUNT] = {')
    for i, pid in enumerate(ids):
        sep = ',' if i + 1 < len(ids) else ''
        lines.append('    "%s"%s' % (pid, sep))
    lines.append('};')
    lines.append('')
    with open(args.out, 'w', encoding='utf-8', newline='\n') as f:
        f.write('\n'.join(lines))
    print('wrote %d device ids to %s' % (len(ids), args.out))

if __name__ == '__main__':
    main()
