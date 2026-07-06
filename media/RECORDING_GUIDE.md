# Recording Guide — Demo Media

This guide covers how to capture the four demo clips referenced in the project README.

## Tools

| Platform | Tool | Link |
|----------|------|------|
| Windows | **ScreenToGif** (recommended for GIF) | <https://www.screentogif.com/> |
| Windows | **OBS Studio** (recommended for MP4) | <https://obsproject.com/> |
| Linux | **peek** or **OBS Studio** | — |
| macOS | **OBS Studio** or built-in screen recording | — |

> **Tip:** ScreenToGif is the easiest path on Windows — it records, edits, and exports to GIF in one app with a built-in frame editor to trim and optimise.

## Clips to Record

Record **4 clips**, each **20–40 seconds** long:

| # | Filename | What to show |
|---|----------|--------------|
| 1 | `realtime_3d.gif` | Launch the Realtime 3D mode. Orbit the camera around a few orbitals. Press **W/S** to cycle through several values of $n$, and **E/D** to change $l$, so viewers can see different orbital shapes. |
| 2 | `2d_bohr.gif` | Launch the 2D Bohr Model. Show the circular orbits and any electron motion. |
| 3 | `excitation.gif` | Launch the Atom Excitation mode. Show the electron absorbing a photon and jumping to a higher energy level. |
| 4 | `raytracer.gif` | Launch the Raytracer mode. Show a single orbital rendering — keep the particle count moderate so it doesn't freeze. A slow rotation is ideal. |

## Recording Settings

- **Window size:** Set the simulation window to **800 × 600** before recording.
- **Desktop background:** Use a **dark/black** wallpaper to reduce visual noise.
- **Frame rate:** 15–20 fps is fine for GIFs (keeps file size down). 30 fps for MP4.
- **Crop:** Crop the recording to the simulation window only — no taskbar, no title bar if possible.

## File Size Targets

| Format | Target per clip |
|--------|----------------|
| GIF | **< 5 MB** |
| MP4 | **< 2 MB** |

### Reducing GIF Size

- Lower the frame rate (15 fps is usually enough).
- Reduce colour count in ScreenToGif's save dialog (128 or 64 colours).
- Trim any idle frames at the start and end.
- Resize to 800 × 600 or smaller if the original capture was larger.

### Reducing MP4 Size

With `ffmpeg`:

```bash
ffmpeg -i raw.mp4 -vf "scale=800:600" -c:v libx264 -crf 28 -preset slow -an output.mp4
```

## Output Location

Place all four files in the `media/` directory at the project root:

```
media/
├── realtime_3d.gif
├── 2d_bohr.gif
├── excitation.gif
└── raytracer.gif
```

The README already contains image embed placeholders pointing to these exact paths — once the files are added the demos will render automatically on GitHub.
