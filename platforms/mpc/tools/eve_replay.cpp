// Re-renders a frame captured with MPDJ_EVE_CAPTURE, for renderer testing and timing.
//
//   make -C platforms/mpc replay            (builds for the machine you're on)
//   eve_replay capture.bin out.ppm [iterations] [scale] [width height]
//
// Writes the frame as a PPM and prints the average render time.
#include "eve/eve.h"

#include <chrono>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        fprintf(
          stderr, "usage: %s capture.bin out.ppm [iterations] [scale] [width height]\n", argv[0]);
        return 1;
    }
    static eve::Chip chip;
    if (!chip.loadCapture(argv[1]))
    {
        fprintf(stderr, "could not read capture %s\n", argv[1]);
        return 1;
    }
    int iterations = argc > 3 ? atoi(argv[3]) : 20;
    float scale = argc > 4 ? (float)atof(argv[4]) : 1.6f;
    int w = argc > 6 ? atoi(argv[5]) : 1280, h = argc > 6 ? atoi(argv[6]) : 800;

    std::vector<uint32_t> px((size_t)w * h);
    eve::Target t;
    t.px = px.data();
    t.w = t.stride = w;
    t.h = h;
    t.scale = scale;
    t.offX = (float)(int)((w - eve::kNativeW * scale) / 2);
    t.offY = (float)(int)((h - eve::kNativeH * scale) / 2);

    double total = 0;
    for (int i = 0; i < iterations; i++)
    {
        std::fill(px.begin(), px.end(), 0xFF000000u);
        auto t0 = std::chrono::steady_clock::now();
        chip.renderer.render(chip.front, (int)chip.frontWords, &t, chip.frontHandles);
        total +=
          std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    }
    printf("%u display-list words, %.2f ms per frame over %d frames\n",
           chip.frontWords,
           total / iterations,
           iterations);

    FILE* f = fopen(argv[2], "wb");
    if (!f)
        return 1;
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (uint32_t c : px)
    {
        uint8_t rgb[3] = { (uint8_t)(c >> 16), (uint8_t)(c >> 8), (uint8_t)c };
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
    return 0;
}
