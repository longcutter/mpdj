// MPDJ (2026-11-16): feature switches, see mpdjFeatures.h
#include "mpdjFeatures.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

namespace
{
const char* const kNames[(int)MpdjFeature::Count] = { "screensaver", "visuals", "backlight", "taplists",
                                                      "capture",     "grooves", "swing",     "generators",
                                                      "skipback",    "bounce",  "dynamics",  "padslices",
                                                      "fill",        "recovery" };
bool s_parsed = false;
bool s_off[(int)MpdjFeature::Count] = {};

void parse()
{
    if (s_parsed)
        return;
    s_parsed = true;
    const char* v = getenv("MPDJ_OFF");
    if (!v || !*v)
        return;
    char buf[256];
    snprintf(buf, sizeof buf, "%s", v);
    for (char* tok = strtok(buf, ", "); tok; tok = strtok(nullptr, ", "))
    {
        bool found = false;
        for (int i = 0; i < (int)MpdjFeature::Count; i++)
            if (!strcasecmp(tok, kNames[i]) || !strcasecmp(tok, "all"))
            {
                s_off[i] = true;
                found = true;
            }
        if (!found)
        {
            printf("features: MPDJ_OFF: unknown \"%s\" (known: all", tok);
            for (const char* n : kNames)
                printf(", %s", n);
            printf(")\n");
        }
    }
}
} // namespace

bool mpdjOn(MpdjFeature f)
{
    parse();
    return !s_off[(int)f];
}

void mpdjFeaturesReport()
{
    parse();
    char line[256] = "";
    for (int i = 0; i < (int)MpdjFeature::Count; i++)
        if (s_off[i])
        {
            strncat(line, line[0] ? ", " : "", sizeof line - strlen(line) - 1);
            strncat(line, kNames[i], sizeof line - strlen(line) - 1);
        }
    if (line[0])
        printf("features: off: %s\n", line);
}
