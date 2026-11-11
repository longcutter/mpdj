# MPDJ sideload test S2b: can an SDL2 program draw on the MPC's panel by itself
# (SDL's KMSDRM video driver, no X or Wayland)? Decides SunVox.
# Run inside the Debian root with MPDJ and the MPC app stopped:
#   apt-get install -y --no-install-recommends python3-sdl2 libsdl2-2.0-0 libgl1-mesa-dri
#   python3 /root/s2/kmsdrm.py
#
# Runs the same picture twice, each with SDL started fresh:
#   pass 1: SDL's defaults (red / green / blue bands, white bar sweeping DOWN)
#   pass 2: GL alpha channel requested (blue / green / red bands, bar sweeping UP)
# The display layer honours alpha, so a buffer whose alpha stays 0 shows as black.
# KMS_SECS = seconds per pass (default 4). KMS_PASSES=1 or 2 runs one pass only.
import ctypes, os, sys, time

os.environ.setdefault("SDL_VIDEODRIVER", "kmsdrm")
SECS = float(os.environ.get("KMS_SECS", "4"))
import sdl2

def err():
    return sdl2.SDL_GetError().decode(errors="replace")

v = sdl2.SDL_version()
sdl2.SDL_GetVersion(ctypes.byref(v))
print(f"SDL {v.major}.{v.minor}.{v.patch}")
print("video drivers built in:", " ".join(sdl2.SDL_GetVideoDriver(i).decode() for i in range(sdl2.SDL_GetNumVideoDrivers())))

def run_pass(n, alpha):
    if alpha:
        sdl2.SDL_GL_SetAttribute(sdl2.SDL_GL_ALPHA_SIZE, 8)
    if sdl2.SDL_Init(sdl2.SDL_INIT_VIDEO) != 0:
        print(f"pass {n}: SDL_Init failed: {err()}")
        return
    mode = sdl2.SDL_DisplayMode()
    sdl2.SDL_GetCurrentDisplayMode(0, ctypes.byref(mode))
    w, h = mode.w or 800, mode.h or 1280
    win = sdl2.SDL_CreateWindow(b"MPDJ S2b", 0, 0, w, h, sdl2.SDL_WINDOW_FULLSCREEN | sdl2.SDL_WINDOW_SHOWN)
    ren = sdl2.SDL_CreateRenderer(win, -1, sdl2.SDL_RENDERER_ACCELERATED) if win else None
    if not ren:
        print(f"pass {n}: window / renderer failed: {err()}")
        sdl2.SDL_Quit()
        return
    got = {}
    for name, attr in (("red", sdl2.SDL_GL_RED_SIZE), ("alpha", sdl2.SDL_GL_ALPHA_SIZE)):
        val = ctypes.c_int(-1)
        sdl2.SDL_GL_GetAttribute(attr, ctypes.byref(val))
        got[name] = val.value
    cols = ((220, 40, 40), (40, 200, 40), (40, 80, 230))
    if n == 2:
        cols = cols[::-1]
    print(f"pass {n}: {'alpha requested' if alpha else 'SDL defaults'}, GL red {got['red']} bits, alpha {got['alpha']} bits; "
          f"{'red/green/blue, bar down' if n == 1 else 'blue/green/red, bar up'}")
    t0 = time.time(); frames = 0
    ev = sdl2.SDL_Event()
    while time.time() - t0 < SECS:
        for i, c in enumerate(cols):
            sdl2.SDL_SetRenderDrawColor(ren, *c, 255)
            sdl2.SDL_RenderFillRect(ren, sdl2.SDL_Rect(0, i * h // 3, w, h // 3))
        f = (time.time() - t0) / SECS
        y = int((f if n == 1 else 1 - f) * (h - 40))
        sdl2.SDL_SetRenderDrawColor(ren, 255, 255, 255, 255)
        sdl2.SDL_RenderFillRect(ren, sdl2.SDL_Rect(0, y, w, 40))
        sdl2.SDL_RenderPresent(ren)
        frames += 1
        while sdl2.SDL_PollEvent(ctypes.byref(ev)):
            pass
    print(f"pass {n}: {frames / SECS:.0f} fps")
    sdl2.SDL_DestroyRenderer(ren)
    sdl2.SDL_DestroyWindow(win)
    sdl2.SDL_Quit()
    time.sleep(1)

passes = os.environ.get("KMS_PASSES", "12")
if "1" in passes:
    run_pass(1, False)
if "2" in passes:
    run_pass(2, True)
print("done: tell Claude which pass showed colour")
