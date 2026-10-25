"""Package Arduino's merged image and regenerate its integrity manifest."""
import hashlib
import json
import shutil
from pathlib import Path
root=Path(__file__).resolve().parent
source=root/'bin'/'Wisp.ino.merged.bin'
target=root/'bin'/'Wisp-0.2.2-ESP32-S3-ePaper-3.97.bin'
shutil.copyfile(source,target)
manifest={'version':'0.2.2','board':'Waveshare ESP32-S3-ePaper-3.97','chip':'esp32s3','offset':0,'flash_size':'16MB','partition_layout':'default_8MB (uses lower 8MB)','file':target.name,'sha256':hashlib.sha256(target.read_bytes()).hexdigest(),'size':target.stat().st_size,'app_file':'Wisp.ino.bin','app_sha256':hashlib.sha256((root/'bin'/'Wisp.ino.bin').read_bytes()).hexdigest(),'hardware_tested':False,'arduino_esp32':'3.3.2','arduinojson':'7.4.2','vendor_commit':'9b12d40731a80213b927ee8a421cae4082952819'}
(root/'bin'/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print(json.dumps(manifest,indent=2))
