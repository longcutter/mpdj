# MPDJ sideload test S2b: can an SDL2 program draw on the MPC's panel by itself
# (SDL's KMSDRM video driver, no X or Wayland)? Decides SunVox.
# Run inside the Debian root with MPDJ and the MPC app stopped:
#   apt-get install -y --no-install-recommends python3-sdl2 libsdl2-2.0-0
#   python3 /root/s2/kmsdrm.py
import ctypes, os, sys, time

os.environ.setdefault("SDL_VIDEODRIVER", "kmsdrm")
import sdl2

def err():
    return sdl2.SDL_GetError().decode(errors="replace")

v = sdl2.SDL_version()
sdl2.SDL_GetVersion(ctypes.byref(v))
print(f"SDL {v.major}.{v.minor}.{v.patch}")
drivers = [sdl2.SDL_GetVideoDriver(i).decode() for i in range(sdl2.SDL_GetNumVideoDrivers())]
print("video drivers built in:", " ".join(drivers))

if sdl2.SDL_Init(sdl2.SDL_INIT_VIDEO) != 0:
    print("RESULT: SDL_Init(video) with", os.environ["SDL_VIDEODRIVER"], "failed:", err())
    sys.exit(1)
print("video driver in use:", sdl2.SDL_GetCurrentVideoDriver().decode())
mode = sdl2.SDL_DisplayMode()
if sdl2.SDL_GetCurrentDisplayMode(0, ctypes.byref(mode)) == 0:
    print(f"display 0: {mode.w}x{mode.h} @ {mode.refresh_rate} Hz")

win = sdl2.SDL_CreateWindow(b"MPDJ S2b", 0, 0, mode.w or 800, mode.h or 1280,
                            sdl2.SDL_WINDOW_FULLSCREEN | sdl2.SDL_WINDOW_SHOWN)
if not win:
    print("RESULT: window failed:", err())
    sdl2.SDL_Quit()
    sys.exit(2)

results = []
for name, flags in (("accelerated", sdl2.SDL_RENDERER_ACCELERATED), ("software", sdl2.SDL_RENDERER_SOFTWARE)):
    ren = sdl2.SDL_CreateRenderer(win, -1, flags)
    if not ren:
        print(f"{name} renderer: failed: {err()}")
        results.append((name, False))
        continue
    info = sdl2.SDL_RendererInfo()
    sdl2.SDL_GetRendererInfo(ren, ctypes.byref(info))
    print(f"{name} renderer: {info.name.decode()}")
    # 3 s of colour: red, green, blue thirds, then a moving bar
    t0 = time.time(); frames = 0
    while time.time() - t0 < 3:
        w, h = mode.w or 800, mode.h or 1280
        for i, c in enumerate(((220, 40, 40), (40, 200, 40), (40, 80, 230))):
            sdl2.SDL_SetRenderDrawColor(ren, *c, 255)
            sdl2.SDL_RenderFillRect(ren, sdl2.SDL_Rect(0, i * h // 3, w, h // 3))
        y = int((time.time() - t0) / 3 * h)
        sdl2.SDL_SetRenderDrawColor(ren, 255, 255, 255, 255)
        sdl2.SDL_RenderFillRect(ren, sdl2.SDL_Rect(0, y, w, 40))
        sdl2.SDL_RenderPresent(ren)
        frames += 1
        ev = sdl2.SDL_Event()
        while sdl2.SDL_PollEvent(ctypes.byref(ev)):
            pass
    print(f"{name} renderer: {frames / 3:.0f} fps")
    results.append((name, True))
    sdl2.SDL_DestroyRenderer(ren)

sdl2.SDL_DestroyWindow(win)
sdl2.SDL_Quit()
ok = [n for n, good in results if good]
print("RESULT:", ("SDL draws on the panel by itself (" + ", ".join(ok) + ")") if ok else "window opened but no renderer worked")
