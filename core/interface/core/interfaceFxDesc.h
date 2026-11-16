
#ifndef INTERFACE_INTERFACEFXDESC_H_
#define INTERFACE_INTERFACEFXDESC_H_

#ifdef FEATURE_SYNTH

const char* const fxDescriptions[FX_COUNT] = {
    // Sequencer::strFxConsts::enFxType::FX_TYPE_NONE,
    "Destination: Sample, Synth, MIDI \n\nIndicates that there's no effect used on the desired "
    "step.",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_OFF,
    "Destination: Sample, Synth, MIDI \n\nToggle off the effect used on previous step.",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_VOLUME,
    "Destination: Sample, Synth, MIDI \n\n"
    "Sets the Volume of a Program. "
    "For MIDI programs this sends the NOTE ON Velocity parameter. "
    "For Synth programs sets the Macro 7. \n\n"
    "From 0% to 100%.",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_PANNING,
    "Destination: Sample, Synth \n\n"
    "Sets the Panning of a Program or Synth.\n\n"
    "From -50 (L) to +50 (R).",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_MICROTUNING,
    "Destination: Sample, Synth, MIDI \n\nSets the microtuning adjustments. "
    "For MIDI programs and Synths this sends the pitchbend message."
    "\n\n"
    "From -99 cents to +99 cents.",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_GLIDE,
    "Destination: Sample \n\nSets the time it takes for the pitch of previous note to travel to "
    "the "
    "pitch of corresponding "
    "note."
    "\n"
    "\n"
    "From 0-49% (0-1s) \nto 50-100% (1-15s).",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_TEMPO,
    "Destination: Pattern"
    "\n\n"
    "Changes pattern tempo. Handy to create custom swings or grooves."
    "\n\n"
    "From 10 BPM to 400 BPM.\n"
    "Also includes TSTP value which stops the playback. TSTP value is excluded when Tempo FX is "
    "used in combination with Random FX Value (f).",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_SWING,
    "Destination: Pattern"
    "\n\n"
    "Changes pattern swing."
    "\n"
    "\n"
    "From 25% to 75%.",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_MICROMOVE,
    "Destination: Sample, Synth, MIDI \n\nSlightly moves (nudges) forward the step position."
    "\n"
    "\n"
    "From 0% to 100%.",
    //	Sequencer::strFxConsts::enFxType::FX_TYPE_GATE_LENGTH,
    "Destination: Sample, Synth, MIDI \n\nSets length of note if you need shorter than step"
    "\n"
    "\n"
    "From 0% to 100%.",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_CHANCE,
    "Destination: Sample, Synth, MIDI \n\nSets a chance or condition for a step being played."
    "\n\n"
    "0-100: Probability %\n"
    "GL / !L: Group / Not Last\n"
    "1:1-6:6: Play N / Skip M\n"
    "S11-S66: Skip N / Play M",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_ROLL,
    "Destination: Sample, Synth, MIDI \n\nSets the type and value of the Roll (step divider) "
    "parameter."
    "\n"
    "\nFrom 0 to 16 for each Roll type."
    "\nR - regular Roll."
    "\nRv - decreasing volume Roll."
    "\nRV - increasing volume Roll."
    "\nRn - decreasing note Roll."
    "\nRN - increasing note Roll."
    "\nRR - random note Roll.",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_ARP,
    "Destination: Synth, MIDI \n\nCombine with the Chord effect (0xx) to arpeggiate in the "
    "given tempo divider, either in raising /, falling \\, or "
    "random R pitches.",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_RANDOM_NOTE,
    "Destination: Sample, Synth, MIDI \n\nSets the range of randomization for Note with each "
    "sequencer pass."
    "\n"
    "\n"
    "From +/- 0 to +/- 100",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_RANDOM_INSTRUMENT,
    "Destination: Sample, Synth, MIDI "
    "\n\n"
    "Sets the range of randomization for Program, Synth or MIDI channel number with each "
    "sequencer pass."
    "\n"
    "\n"
    "From +/- 0 to +/- 47",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_RANDOM_VALUE,
    "Destination: Sample, Synth, MIDI \n\nSets the range of randomization for the FX that is in "
    "the "
    "same step with each sequencer pass. "
    "For example, if randomizer value is 10 and affected FX value is 100, the algorithm will pick "
    "the value from 90 to 110."
    "\n"
    "\n"
    "From +/- 0 to +/-255.",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_RANDOM_VOLUME,
    "Destination: Sample, Synth, MIDI "
    "\n\n"
    "Sets the range of randomization for Volume (Sample), Velocity (MIDI) or Macro 7 (Synth) with "
    "each sequencer pass."
    "\n"
    "\n"
    "From +/- 0 to +/- 100",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_REVERSE_PLAYBACK,
    "Destination: Sample \n\nChanges the playback direction of the sample."
    "\n"
    "\n"
    "Values: <<< and >>>.",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_POSITION,
    "Destination: Sample \n\nIn relation to the chosen sample playback type - changes the position "
    "of: Sample start or "
    "Wavetable position or Granular position."
    "\n"
    "\n"
    "From 0% to 100%.",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_SAMPLE_SLICE,
    "Destination: Sample \n\nSets the slice number of a program with a Slice playback type."
    "\n"
    "\n"
    "From 1 to 48.",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_VOLUME_LFO,
    "Destination: Sample \n\nSets the LFO rate for program Volume. LFO is synced to project "
    "Tempo."
    "\n"
    "\n"
    "Values: 24; 16; 12; 8; 6; 4; 3; 2; 3/2; 1; 3/4; 1/2; 3/8; 1/3; 1/4; 316 (3/16); 1/6; 1/8; "
    "/12 (1/12); /16 (1/16); /24 (1/24); /32 (1/32); /48 (1/48); /64 (1/64).",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_PANNING_LFO,
    "Destination: Sample "
    "\n\n"
    "Sets the LFO rate for program Panning."
    "\n\n"
    "Values: 128; 96; 64; 48; 32; 24; 16; 12; 8; 6; 4; 3; 2; 3/2; 1; 3/4; 1/2; 3/8; 1/3; 1/4; "
    "316 (3/16); 1/6; 1/8; /12 (1/12); /16 (1/16); /24 (1/24); /32 (1/32); /48 (1/48); /64 (1/64).",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_FILTER_LFO,
    "Destination: Sample \n\nSets the LFO rate for the selected Filter type. LFO is synced to "
    "project Tempo."
    "\n"
    "\n"
    "Values: 128; 96; 64; 48; 32; 24; 16; 12; 8; 6; 4; 3; 2; 3/2; 1; 3/4; 1/2; 3/8; 1/3; 1/4; "
    "316 (3/16); 1/6; 1/8; /12 (1/12); /16 (1/16); /24 (1/24); /32 (1/32); /48 (1/48); /64 (1/64).",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_POSITION_LFO,
    "Destination: Sample \n\nSets the LFO rate for the selected playback type (Wavetable, "
    "Granular). "
    "Position in respect "
    "to the project tempo (BPM)."
    "\n"
    "\n"
    "Values: 128; 96; 64; 48; 32; 24; 16; 12; 8; 6; 4; 3; 2; 3/2; 1; 3/4; 1/2; 3/8; 1/3; 1/4; "
    "316 (3/16); 1/6; 1/8; /12 (1/12); /16 (1/16); /24 (1/24); /32 (1/32); /48 (1/48); /64 (1/64).",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_FINETUNE_LFO,
    "Destination: Sample \n\nSets the LFO rate for program Finetune. LFO is synced to project "
    "Tempo."
    "\n"
    "\n"
    "Values: 128; 96; 64; 48; 32; 24; 16; 12; 8; 6; 4; 3; 2; 3/2; 1; 3/4; 1/2; 3/8; 1/3; 1/4; "
    "316 (3/16); 1/6; 1/8; /12 (1/12); /16 (1/16); /24 (1/24); /32 (1/32); /48 (1/48); /64 (1/64).",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_OVERDRIVE,
    "Destination: Sample \n"
    "\n"
    "Sets the amount of program Overdrive.\n"
    "\n"
    "From 0% to 100%.",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_FILTER_LOWPASS,
    "Destination: Sample \n"
    "\n"
    "Sets the cutoff amount of the program Low-pass Filter."
    "\n"
    "\n"
    "From 0% to 100%.",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_FILTER_BANDPASS,
    "Destination: Sample \n"
    "\n"
    "Sets the cutoff amount of the program Band-pass Filter."
    "\n"
    "\n"
    "From 0% to 100%.",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_FILTER_HIGHPASS,
    "Destination: Sample \n"
    "\n"
    "Sets the cutoff amount of the program High-pass Filter."
    "\n"
    "\n"
    "From 0% to 100%.",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_DELAY_SEND,
    "Destination: Sample, Synth \n\nSets the amount of Delay send."
    "\n"
    "\n"
    "From 0% to 100%.",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_REVERB_SEND,
    "Destination: Sample, Synth \n\nSets the amount of Reverb send."
    "\n"
    "\n"
    "From 0% to 100%.",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_BIT_DEPTH,
    "Destination: Sample \n"
    "\n"
    "Sets the program bit depth."
    "\n"
    "\n"
    "From 4 bits to 16 bits.",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_TUNE,
    "Destination: Sample \n"
    "\n"
    "Sets the program tuning adjustments."
    "\n"
    "\n"
    "From -24 semitones \nto 24 semitones.",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_SLIDE_UP,
    "Destination: Sample \n\nSets the slide up amount in steps of 1/16th a semitone."
    "\n"
    "\n"
    "From 0 to 255.",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_SLIDE_DOWN,
    "Destination: Sample \n\nSets the slide down amount in steps of 1/16th a semitone."
    "\n"
    "\n"
    "From 0 to 255.",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_AMP_ENV,
    "Destination: Sample \n\n"
    "Sets Volume Envelope Attack and Decay/Release times simultaneously.\n\n"
    "Value is represented as two hexadecimal digits (AR):\n"
    "First digit (A): Attack time (0: 0ms stab .. F: 8s rise).\n"
    "Second digit (R): Decay/Release time (0: 0ms .. F: 8s).\n"
    "In Wavetable mode, modulates speed and time warp.",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_FILTER_ENV,
    "Destination: Sample \n\n"
    "Sets Filter Envelope Attack and Decay/Release times simultaneously.\n\n"
    "Value is represented as two hexadecimal digits (AR):\n"
    "First digit (A): Attack time (0: 0ms stab .. F: 8s rise).\n"
    "Second digit (R): Decay/Release time (0: 0ms .. F: 8s).\n"
    "In Wavetable mode, modulates filter speed and time warp.",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_GRANULAR_LENGTH,
    "Destination: Sample \n\n"
    "Sets the Grain Length for a Program in Granular Playback mode.\n\n"
    "0-50% maps 1ms to 100ms, 50-100% maps 100ms to 1000ms (clamped to sample length).\n\n"
    "From 0% to 100%.",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_SEND_CC_A,
    "Destination: MIDI, Synth \n\nSends MIDI CC number set to A in MIDI Program Parameters."
    "\n"
    "\n"
    "For Synth programs sets the Macro 1."
    "\n"
    "\n"
    "From 0 to 127.",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_SEND_CC_B,
    "Destination: MIDI, Synth \n\nSends MIDI CC number set to B in MIDI Program Parameters."
    "\n"
    "\n"
    "For Synth programs sets the Macro 2."
    "\n"
    "\n"
    "From 0 to 127.",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_SEND_CC_C,
    "Destination: MIDI, Synth \n\nSends MIDI CC number set to C in MIDI Program Parameters."
    "\n"
    "\n"
    "For Synth programs sets the Macro 3."
    "\n"
    "\n"
    "From 0 to 127.",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_SEND_CC_D,
    "Destination: MIDI, Synth \n\nSends MIDI CC number set to D in MIDI Program Parameters."
    "\n"
    "\n"
    "For Synth programs sets the Macro 4."
    "\n"
    "\n"
    "From 0 to 127.",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_SEND_CC_E,
    "Destination: MIDI, Synth \n\nSends MIDI CC number set to E in MIDI Program Parameters."
    "\n"
    "\n"
    "For Synth programs sets the Macro 5."
    "\n"
    "\n"
    "From 0 to 127.",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_SEND_CC_F,
    "Destination: MIDI, Synth \n\nSends MIDI CC number set to F in MIDI Program Parameters."
    "\n"
    "\n"
    "For Synth programs sets the Macro 6."
    "\n"
    "\n"
    "From 0 to 127.",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_MIDI_CHORD,
    "Destination: MIDI, Synth \n\nSends many MIDI notes or sets up Arp."
    "\n"
    "Numbers represent semitones above the base note."
    " For example, C-4 047 will play a major chord:"
    " C-4 (base note), E-4 (base+4), and G-4 (base+7)."
    "\nTry:"
    "\n027: Sus2   057: Sus4"
    "\n037: Min    047: Maj"
    "\n05C: Open4  07C: Open5"
    "\n0379: Min6  037A: Min7"
    "\n0479: Maj6  047A: Maj7"
    "\n047B: MajMaj7"
    "\nAfter 057B, inversions:"
    "\n038: Maj/1st  059: Maj/2nd"
    "\n049: Min/1st  058: Min/2nd",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_BREAK_PATTERN,
    "Destination: Pattern \n\n"
    "Breaks current pattern playback and jumps to the next pattern.",
};
#else
const char* const fxDescriptions[FX_COUNT] = {
    // Sequencer::strFxConsts::enFxType::FX_TYPE_NONE,
    "Destination: Sample, MIDI \n\nIndicates that there's no effect used on the desired "
    "step.",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_OFF,
    "Destination: Sample, MIDI \n\nToggle off the effect used on previous step.",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_VOLUME,
    "Destination: Sample, MIDI \n\nSets the Volume of a Program. For MIDI programs "
    "this sends the NOTE ON Velocity "
    "parameter. \n\nFrom 0% to 100%.",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_PANNING,
    "Destination: Sample \n\nSets the Panning of a Program.\n\nFrom -50 (L) to +50 (R).",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_MICROTUNING,
    "Destination: Sample, MIDI \n\nSets the microtuning adjustments."
    "\n"
    "\nFrom -99 cents to +99 cents."
    "\n"
    "\n"
    "For MIDI programs sends pitchbend message.",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_GLIDE,
    "Destination: Sample \n\nSets the time it takes for the pitch of previous note to travel to "
    "the "
    "pitch of corresponding "
    "note."
    "\n"
    "\n"
    "From 0-49% (0-1s) \nto 50-100% (1-15s).",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_TEMPO,
    "Destination: Pattern"
    "\n\n"
    "Changes pattern tempo. Handy to create custom swings or grooves."
    "\n"
    "\n"
    "From 10 BPM to 400 BPM.\n"
    "Also includes TSTP value which stops the playback. TSTP value is excluded when Tempo FX is "
    "used in combination with Random FX Value (f).",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_SWING,
    "Destination: Pattern"
    "\n\n"
    "Changes pattern swing."
    "\n"
    "\n"
    "From 25% to 75%.",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_MICROMOVE,
    "Destination: Sample, MIDI \n\nSlightly moves (nudges) forward the step position."
    "\n"
    "\n"
    "From 0% to 100%.",
    //	Sequencer::strFxConsts::enFxType::FX_TYPE_GATE_LENGTH,
    "Destination: Sample, MIDI \n\nSets length of note if you need shorter than step"
    "\n"
    "\n"
    "From 0% to 100%.",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_CHANCE,
    "Destination: Sample, MIDI \n\nSets a chance or condition for a step being played."
    "\n\n"
    "0-100: Probability %\n"
    "GL / !L: Group / Not Last\n"
    "1:1-6:6: Play N / Skip M\n"
    "S11-S66: Skip N / Play M",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_ROLL,
    "Destination: Sample, MIDI \n\nSets the type and value of the Roll (step divider) "
    "parameter."
    "\n"
    "\nFrom 0 to 16 for each Roll type."
    "\nR - regular Roll."
    "\nRv - decreasing volume Roll."
    "\nRV - increasing volume Roll."
    "\nRn - decreasing note Roll."
    "\nRN - increasing note Roll."
    "\nRR - random note Roll.",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_ARP,
    "Destination:  MIDI \n\nCombine with the Chord effect (0xx) to arpeggiate in the "
    "given tempo divider, either in raising /, falling \\, or "
    "random R pitches.",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_RANDOM_NOTE,
    "Destination: Sample, MIDI \n\nSets the range of randomization for Note with each "
    "sequencer pass."
    "\n"
    "\n"
    "From +/- 0 to +/- 100",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_RANDOM_INSTRUMENT,
    "Destination: Sample, MIDI \n\nSets the range of randomization for Program number "
    "with each sequencer pass."
    "\n"
    "\n"
    "From +/- 0 to +/- 47",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_RANDOM_VALUE,
    "Destination: Sample, MIDI \n\nSets the range of randomization for the FX that is in the "
    "same step with each sequencer pass. "
    "For example, if randomizer value is 10 and affected FX value is 100, the algorithm will pick "
    "the value from 90 to 110."
    "\n"
    "\n"
    "From +/- 0 to +/-255.",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_RANDOM_VOLUME,
    "Destination: Sample, MIDI \n\nSets the range of randomization for Volume with each "
    "sequencer pass."
    "\n"
    "\n"
    "From +/- 0 to +/- 100",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_REVERSE_PLAYBACK,
    "Destination: Sample \n\nChanges the playback direction of the sample."
    "\n"
    "\n"
    "Values: <<< and >>>.",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_POSITION,
    "Destination: Sample \n\nIn relation to the chosen sample playback type - changes the position "
    "of: Sample start or "
    "Wavetable position or Granular position."
    "\n"
    "\n"
    "From 0% to 100%.",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_SAMPLE_SLICE,
    "Destination: Sample \n\nSets the slice number of a program with a Slice playback type."
    "\n"
    "\n"
    "From 1 to 48.",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_VOLUME_LFO,
    "Destination: Sample \n\nSets the LFO rate for program Volume. LFO is synced to project "
    "Tempo."
    "\n"
    "\n"
    "Values: 24; 16; 12; 8; 6; 4; 3; 2; 3/2; 1; 3/4; 1/2; 3/8; 1/3; 1/4; 316 (3/16); 1/6; 1/8; "
    "/12 (1/12); /16 (1/16); /24 (1/24); /32 (1/32); /48 (1/48); /64 (1/64).",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_PANNING_LFO,
    "Destination: Sample \n\nSets the LFO rate for Panning. LFO is synced to project Tempo."
    "\n"
    "\n"
    "Values: 128; 96; 64; 48; 32; 24; 16; 12; 8; 6; 4; 3; 2; 3/2; 1; 3/4; 1/2; 3/8; 1/3; 1/4; "
    "316 (3/16); 1/6; 1/8; /12 (1/12); /16 (1/16); /24 (1/24); /32 (1/32); /48 (1/48); /64 (1/64).",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_FILTER_LFO,
    "Destination: Sample \n\nSets the LFO rate for the selected Filter type. LFO is synced to "
    "project Tempo."
    "\n"
    "\n"
    "Values: 128; 96; 64; 48; 32; 24; 16; 12; 8; 6; 4; 3; 2; 3/2; 1; 3/4; 1/2; 3/8; 1/3; 1/4; "
    "316 (3/16); 1/6; 1/8; /12 (1/12); /16 (1/16); /24 (1/24); /32 (1/32); /48 (1/48); /64 (1/64).",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_POSITION_LFO,
    "Destination: Sample \n\nSets the LFO rate for the selected playback type (Wavetable, "
    "Granular). "
    "Position in respect "
    "to the project tempo (BPM)."
    "\n"
    "\n"
    "Values: 128; 96; 64; 48; 32; 24; 16; 12; 8; 6; 4; 3; 2; 3/2; 1; 3/4; 1/2; 3/8; 1/3; 1/4; "
    "316 (3/16); 1/6; 1/8; /12 (1/12); /16 (1/16); /24 (1/24); /32 (1/32); /48 (1/48); /64 (1/64).",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_FINETUNE_LFO,
    "Destination: Sample \n\nSets the LFO rate for program Finetune. LFO is synced to project "
    "Tempo."
    "\n"
    "\n"
    "Values: 128; 96; 64; 48; 32; 24; 16; 12; 8; 6; 4; 3; 2; 3/2; 1; 3/4; 1/2; 3/8; 1/3; 1/4; "
    "316 (3/16); 1/6; 1/8; /12 (1/12); /16 (1/16); /24 (1/24); /32 (1/32); /48 (1/48); /64 (1/64).",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_OVERDRIVE,
    "Destination: Sample \n\nSets the amount of Overdrive."
    "\n"
    "\n"
    "From 0% to 100%.",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_FILTER_LOWPASS,
    "Destination: Sample \n\nSets the cutoff amount of the Low-pass Filter."
    "\n"
    "\n"
    "From 0% to 100%.",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_FILTER_BANDPASS,
    "Destination: Sample \n\nSets the cutoff amount of the Band-pass Filter."
    "\n"
    "\n"
    "From 0% to 100%.",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_FILTER_HIGHPASS,
    "Destination: Sample \n\nSets the cutoff amount of the High-pass Filter."
    "\n"
    "\n"
    "From 0% to 100%.",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_DELAY_SEND,
    "Destination: Sample \n\nSets the amount of Delay send."
    "\n"
    "\n"
    "From 0% to 100%.",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_REVERB_SEND,
    "Destination: Sample \n\nSets the amount of Reverb send."
    "\n"
    "\n"
    "From 0% to 100%.",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_BIT_DEPTH,
    "Destination: Sample \n\nSets the bit depth."
    "\n"
    "\n"
    "From 4 bits to 16 bits.",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_TUNE,
    "Destination: Sample \n\nSets the tuning adjustments."
    "\n"
    "\n"
    "From -24 semitones \nto 24 semitones.",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_SLIDE_UP,
    "Destination: Sample \n\nSets the slide up amount in steps of 1/16th a semitone."
    "\n"
    "\n"
    "From 0 to 255.",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_SLIDE_DOWN,
    "Destination: Sample \n\nSets the slide down amount in steps of 1/16th a semitone."
    "\n"
    "\n"
    "From 0 to 255.",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_AMP_ENV,
    "Destination: Sample \n\n"
    "Sets Volume Envelope Attack and Decay/Release times simultaneously.\n\n"
    "Value is represented as two hexadecimal digits (AR):\n"
    "First digit (A): Attack time (0: 0ms stab .. F: 8s rise).\n"
    "Second digit (R): Decay/Release time (0: 0ms .. F: 8s).\n"
    "In Wavetable mode, modulates speed and time warp.",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_FILTER_ENV,
    "Destination: Sample \n\n"
    "Sets Filter Envelope Attack and Decay/Release times simultaneously.\n\n"
    "Value is represented as two hexadecimal digits (AR):\n"
    "First digit (A): Attack time (0: 0ms stab .. F: 8s rise).\n"
    "Second digit (R): Decay/Release time (0: 0ms .. F: 8s).\n"
    "In Wavetable mode, modulates filter speed and time warp.",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_GRANULAR_LENGTH,
    "Destination: Sample \n\n"
    "Sets the Grain Length for a Program in Granular Playback mode.\n\n"
    "0-50% maps 1ms to 100ms, 50-100% maps 100ms to 1000ms (clamped to sample length).\n\n"
    "From 0% to 100%.",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_SEND_CC_A,
    "Destination: MIDI \n\nSends MIDI CC number set to A in MIDI Program Parameters."
    "\n"
    "\n"
    "From 0 to 127.",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_SEND_CC_B,
    "Destination: MIDI \n\nSends MIDI CC number set to B in MIDI Program Parameters."
    "\n"
    "\n"
    "From 0 to 127.",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_SEND_CC_C,
    "Destination: MIDI \n\nSends MIDI CC number set to C in MIDI Program Parameters."
    "\n"
    "\n"
    "From 0 to 127.",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_SEND_CC_D,
    "Destination: MIDI \n\nSends MIDI CC number set to D in MIDI Program Parameters."
    "\n"
    "\n"
    "From 0 to 127.",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_SEND_CC_E,
    "Destination: MIDI \n\nSends MIDI CC number set to E in MIDI Program Parameters."
    "\n"
    "\n"
    "From 0 to 127.",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_SEND_CC_F,
    "Destination: MIDI \n\nSends MIDI CC number set to F in MIDI Program Parameters."
    "\n"
    "\n"
    "From 0 to 127.",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_MIDI_CHORD,
    "Destination: MIDI \n\nSends many MIDI notes or sets up Arp."
    "\n"
    "Numbers represent semitones above the base note."
    " For example, C-4 047 will play a major chord:"
    " C-4 (base note), E-4 (base+4), and G-4 (base+7)."
    "\nTry:"
    "\n027: Sus2   057: Sus4"
    "\n037: Min    047: Maj"
    "\n05C: Open4  07C: Open5"
    "\n0379: Min6  037A: Min7"
    "\n0479: Maj6  047A: Maj7"
    "\n047B: MajMaj7"
    "\nAfter 057B, inversions:"
    "\n038: Maj/1st  059: Maj/2nd"
    "\n049: Min/1st  058: Min/2nd",
    // Sequencer::strFxConsts::enFxType::FX_TYPE_BREAK_PATTERN,
    "Destination: Pattern \n\n"
    "Breaks current pattern playback and jumps to the next pattern.",
};
#endif

#endif /* INTERFACE_INTERFACEFXDESC_H_ */
