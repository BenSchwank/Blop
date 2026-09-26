import subprocess
from pathlib import Path
from PIL import Image
import imageio_ffmpeg as iof

video = r"c:\Users\NaqsZ\Downloads\Aufzeichnung 2026-09-13 121724.mp4"
out = Path(r"c:\Users\NaqsZ\OneDrive\Arbeit_Schule\coding\Blop\Git\Blop\.tmp-video-analysis")
out.mkdir(parents=True, exist_ok=True)

ffmpeg = iof.get_ffmpeg_exe()
print("ffmpeg:", ffmpeg)

probe = subprocess.run([ffmpeg, "-i", video], capture_output=True, text=True)
info = probe.stderr
for line in info.splitlines():
    if "Stream #0" in line or "Duration" in line or "Video:" in line:
        print(line.strip())

pattern = str(out / "frame_%04d.jpg")
cmd = [ffmpeg, "-y", "-i", video, "-vf", "fps=1", "-q:v", "3", pattern]
print("running extract...")
r = subprocess.run(cmd, capture_output=True, text=True)
print("exit", r.returncode)
if r.returncode != 0:
    print(r.stderr[-2000:])
else:
    files = sorted(out.glob("frame_*.jpg"))
    print("frames:", len(files))
    if files:
        im = Image.open(files[0])
        print("size", im.size)
        # Also make a contact sheet every 5s for overview
        sheet_dir = out / "overview"
        sheet_dir.mkdir(exist_ok=True)
        for i, f in enumerate(files):
            if i % 5 == 0 or i == len(files) - 1:
                # copy/rename with seconds
                dest = sheet_dir / f"t{i:03d}s.jpg"
                Image.open(f).save(dest, quality=70)
                print("saved", dest.name)
