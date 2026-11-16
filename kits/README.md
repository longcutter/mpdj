# MPDJ kits

Three kits made from the MPDJ Kit sounds (`tools/kitgen`, our own synthesis, MIT like the rest of MPDJ's additions):

| Kit | Pads | Sounds (pad order) |
| --- | --- | --- |
| MPDJ Kit | 16 | Kick, Snare, Closed Hat, Open Hat, Clap, Rim, Low Tom, Mid Tom, High Tom, Crash, Ride, Pedal Hat, Cowbell, Clave, Shaker, Boom |
| MPDJ 12 | 12 | Kick, Snare, Clap, Rim, Closed Hat, Pedal Hat, Open Hat, Ride, Low Tom, Mid Tom, High Tom, Crash |
| MPDJ 8 | 8 | Kick, Snare, Closed Hat, Open Hat, Clap, Rim, Shaker, Boom |

In each kit the hats share choke group 1; pad colors go by family (kicks red, snares orange, hats yellow, toms green, cymbals cyan, percussion violet).

Copy the folders to `MPDJ/Kits` on the card. In the Program Loader, select a kit folder and press **Add Kit** (screen button 1): its programs load into consecutive slots from the selected slot, and `kit.txt` sets the pads that point at them. File > **Save Kit** writes a bank's 16 slots as a new kit.

A kit is a folder of the Tracker's own program files (`NN name.pti`: sample and program settings) plus `kit.txt`, one line per sound: `NN mode group cut color` (mode `auto` / `oneshot` / `gate`, choke group 0-31, cut on release 0 / 1, color 0 = Auto or 1-16). These were made with MPDJ itself: the kitgen WAVs loaded in pad order, saved with Save Kit, then the pad lines written into `kit.txt`.
