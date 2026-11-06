"""Package Arduino source and an already built APK; never include credentials."""
from pathlib import Path
import argparse
import hashlib
import shutil
import subprocess
import zipfile

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--version',default='1.0.0')
parser.add_argument('--output',type=Path)
args=parser.parse_args()
repo=Path(__file__).resolve().parent.parent
out=(args.output or repo/'build/release').resolve()
out.mkdir(parents=True,exist_ok=True)
apk=repo/'app/build/app/outputs/flutter-apk/app-release.apk'
if not apk.is_file():raise SystemExit('Build the release APK first: cd app; flutter build apk --release')
names=subprocess.check_output(['git','ls-files','-z','--cached','--others','--exclude-standard'],cwd=repo).decode().split('\0')
docs={'docs/'+n for n in ['CALIBRATION.md','PHONE_SETUP.md','PROTOCOL.md','SECURITY.md','TESTING.md','AUDIT.md','sample-sd-demo.csv']}
selected=sorted({n for n in names if n and (n.startswith(('arduino/','tests/arduino/')) or n in docs or n in {'.gitignore','tools/build-arduino.ps1'})})
if any(Path(n).name=='Secrets.h' for n in selected):raise SystemExit('Refusing to package a configured Secrets.h')
archive=out/f'ParcelGuardian-Arduino-v{args.version}.zip'
root=f'ParcelGuardian-Arduino-v{args.version}'
with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as z:
    for name in selected:z.write(repo/name,f'{root}/{name}')
    z.write(repo/'arduino/README.md',f'{root}/README.md')
android=out/f'ParcelGuardian-Android-v{args.version}.apk'
shutil.copy2(apk,android)
checksums=''.join(f'{hashlib.sha256(p.read_bytes()).hexdigest()}  {p.name}\n' for p in [archive,android])
(out/'SHA256SUMS.txt').write_text(checksums,encoding='utf-8')
with zipfile.ZipFile(archive) as z:
    if z.testzip() is not None:raise SystemExit('Archive CRC validation failed')
    for required in ['arduino/ParcelGuardian/ParcelGuardian.ino','arduino/ParcelGuardian/Secrets.example.h','tools/build-arduino.ps1']:
        if f'{root}/{required}' not in z.namelist():raise SystemExit(f'Missing {required}')
print(f'Packaged {len(selected)} Arduino/support files without credentials.')
for p in [archive,android,out/'SHA256SUMS.txt']:print(p)
