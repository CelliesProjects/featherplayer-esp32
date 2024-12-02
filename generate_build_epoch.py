import os
from datetime import datetime, timedelta
import time

# File to generate
header_file = os.path.join("include", "build_epoch.h")

# Get the system's local timezone offset
local_offset = time.localtime().tm_gmtoff
# Get the sign of the offset and format it
sign = "+" if local_offset >= 0 else "-"
abs_offset = abs(local_offset)
offset_hours = abs_offset // 3600
offset_minutes = (abs_offset % 3600) // 60

# Generate UNIX timestamp and human-readable timestamp in the local timezone
build_epoch = int(datetime.now().timestamp())
local_time = datetime.fromtimestamp(build_epoch) + timedelta(seconds=local_offset)
human_readable_time = local_time.strftime(f"%A %d %B %Y %H:%M UTC{sign}{offset_hours:02}:{offset_minutes:02}")

# Create or overwrite the header file
with open(header_file, "w") as f:
    f.write("#ifndef BUILD_EPOCH_H\n")
    f.write("#define BUILD_EPOCH_H\n\n")
    f.write(f"#define BUILD_EPOCH {build_epoch} // {human_readable_time}\n\n")
    f.write("#endif // BUILD_EPOCH_H\n")

print(f"Generated {header_file} with BUILD_EPOCH={build_epoch} ({human_readable_time})")
