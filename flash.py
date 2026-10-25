#!/usr/bin/env python3
"""Flash the precompiled ESP32-S3 image; no Arduino installation needed."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import datetime

ROOT = Path(__file__).resolve().parent

def verify():
    manifest=json.loads((ROOT/'bin'/'manifest.json').read_text())
    path=ROOT/'bin'/manifest['file']
    digest=hashlib.sha256(path.read_bytes()).hexdigest()
    if digest != manifest['sha256']:
        raise SystemExit('Firmware checksum mismatch. Extract a fresh copy of the package.')
    if manifest['chip'] != 'esp32s3' or manifest['offset'] != 0:
        raise SystemExit('Unsupported firmware manifest')
    print(f"Verified: {path.name} ({path.stat().st_size:,} bytes)")
    return path

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--port',help='e.g. /dev/cu.usbmodem1101 or COM4')
    p.add_argument('--upgrade',action='store_true',help='Update Deskling 0.1/0.2 or Wisp while preserving settings and files; requires matching partition layout')
    p.add_argument('--verify-only',action='store_true')
    p.add_argument('--yes',action='store_true',help='Skip the typed FLASH confirmation')
    p.add_argument('--skip-backup',action='store_true',help='Do not back up the existing 16 MB flash')
    p.add_argument('--baud',default='460800')
    args=p.parse_args();firmware=verify()
    if args.verify_only:return
    try:
        from serial.tools import list_ports
        import esptool
    except ImportError:
        raise SystemExit('Install with: python -m pip install esptool==5.1.0')
    port=args.port
    if not port:
        ports=list(list_ports.comports())
        if not ports:raise SystemExit('No serial ports found. Connect a USB data cable; hold BOOT while powering on if needed.')
        for i,item in enumerate(ports,1):print(f'{i}. {item.device} — {item.description}')
        try:choice=int(input('Choose the board port number: '))
        except ValueError:raise SystemExit('Please enter a port number.')
        if choice<1 or choice>len(ports):raise SystemExit('Invalid port number.')
        port=ports[choice-1].device
    if args.upgrade:
        manifest=json.loads((ROOT/'bin'/'manifest.json').read_text())
        firmware=ROOT/'bin'/manifest['app_file']
        if hashlib.sha256(firmware.read_bytes()).hexdigest()!=manifest['app_sha256']:
            raise SystemExit('Application checksum mismatch')
    print('\nTarget: Waveshare ESP32-S3-ePaper-3.97 ONLY. This replaces its existing firmware.')
    if not args.yes and input('Type FLASH to continue: ').strip()!='FLASH':raise SystemExit('Cancelled.')
    command=[sys.executable,'-m','esptool','--chip','esp32s3','--port',port,'--baud',args.baud]
    if not args.skip_backup:
        backup=ROOT/'backups';backup.mkdir(exist_ok=True)
        path=backup/('before-wisp-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S')+'.bin')
        print('Saving the existing flash first. This can take several minutes.')
        subprocess.run(command+['read-flash','0','0x1000000',str(path)],check=True)
        print('Backup saved:',path)
    if args.upgrade:
        import tempfile
        with tempfile.TemporaryDirectory() as temp:
            table=Path(temp)/'partition.bin'
            subprocess.run(command+['read-flash','0x8000','0x1000',str(table)],check=True)
            expected=(ROOT/'bin'/'Wisp.ino.partitions.bin').read_bytes()
            if table.read_bytes()[:len(expected)]!=expected:
                raise SystemExit('Partition layout differs. Use a full installation after saving your data.')
        subprocess.run(command+['write-flash','0x10000',str(firmware)],check=True)
    else:
        subprocess.run(command+['write-flash','0x0',str(firmware)],check=True)
    print('\nFlashing verified by esptool. Release BOOT and power cycle if the screen does not update.')
    print('Then join the Wisp Wi-Fi network shown on the screen and follow README.md.')

if __name__=='__main__':
    try:main()
    except (KeyboardInterrupt,EOFError):print('\nCancelled.');sys.exit(1)
    except subprocess.CalledProcessError:print('Flashing stopped. Try BOOT + power cycle, or --baud 115200.');sys.exit(1)
