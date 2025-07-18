import gzip
from pathlib import Path

webui_dir = Path('src/webui').resolve()
print(f"Searching for HTML files in: {webui_dir}")

for html_file in webui_dir.rglob('*.html'):
    gzip_path = html_file.with_suffix(html_file.suffix + '.gz')
    
    print(f"Gzipping: {html_file.as_posix()}")
    
    with open(html_file, 'rb') as f_in:
        with gzip.open(gzip_path, 'wb') as f_out:
            f_out.writelines(f_in)
