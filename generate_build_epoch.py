import os
from datetime import datetime, timezone

# File to generate
header_file = os.path.join("include", "build_epoch.h")

# Generate UNIX timestamp and human-readable timestamp
build_epoch = int(datetime.now(timezone.utc).timestamp())
human_readable_time = datetime.fromtimestamp(build_epoch, tz=timezone.utc).strftime("%A %d %B %Y %H:%M")

# Create or overwrite the header file
with open(header_file, "w") as f:
    f.write(f"#ifndef BUILD_EPOCH_H\n")
    f.write(f"#define BUILD_EPOCH_H\n\n")
    f.write(f"#define BUILD_EPOCH {build_epoch} // {human_readable_time}\n\n")
    f.write(f"#endif // BUILD_EPOCH_H\n")

print(f"Generated {header_file} with BUILD_EPOCH={build_epoch} ({human_readable_time})")
