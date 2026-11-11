// MPDJ sideload test S1: SuperCollider load on the MPC (run by s1.sh)
(
var secs = ("S1_SECS".getenv ? "300").asInteger;
var t0, rep, voices = List[], loops = List[], sine, b;
s = Server.default;
s.options.numInputBusChannels = 2;
s.options.numOutputBusChannels = 2;
s.options.sampleRate = 44100;
s.options.memSize = 65536;
s.waitForBoot({
	SynthDef(\s1sine, { |freq = 440, amp = 0.1| Out.ar(0, SinOsc.ar(freq, 0, amp).dup) }).add;
	SynthDef(\s1voice, { |freq = 220, amp = 0.05, pan = 0|
		var env = EnvGen.kr(Env.perc(0.01, 1.5), Impulse.kr(0.5 + (freq % 7 * 0.1)));
		var sig = RLPF.ar(Saw.ar(freq * [1, 1.005]).sum + Pulse.ar(freq / 2, 0.3), LFNoise1.kr(0.3).range(400, 3000), 0.3);
		Out.ar(0, Pan2.ar(sig * env * amp, pan));
	}).add;
	SynthDef(\s1loop, { |buf, rate = 1, amp = 0.2, pan = 0|
		Out.ar(0, Pan2.ar(PlayBuf.ar(1, buf, rate * BufRateScale.kr(buf), loop: 1) * amp, pan));
	}).add;
	s.sync;
	b = Buffer.loadCollection(s, Array.fill(88200, { |i| sin(i * 2pi * 110 / 44100) * (1 - ((i % 11025) / 11025)) * 0.8 }));
	s.sync;
	t0 = Main.elapsedTime;
	rep = { |phase|
		"S1 t=% phase=% voices=% avgCPU=% peakCPU=% ugens=%".format(
			(Main.elapsedTime - t0).round(1), phase, voices.size,
			s.avgCPU.round(0.1), s.peakCPU.round(0.1), s.numUGens).postln
	};
	"S1 phase 1: sine 440 Hz".postln;
	sine = Synth(\s1sine);
	4.do { 5.wait; rep.("sine") };
	sine.free;
	"S1 phase 2: 8 voices + 2 looping buffer players".postln;
	8.do { |i| voices.add(Synth(\s1voice, [\freq, [110, 138.6, 164.8, 220, 277.2, 329.6, 440, 554.4][i], \pan, i.linlin(0, 7, -0.8, 0.8)])) };
	2.do { |i| loops.add(Synth(\s1loop, [\buf, b, \rate, [1, 0.5][i], \pan, [-0.5, 0.5][i]])) };
	((secs - 80) / 5).asInteger.max(1).do { 5.wait; rep.("8v+2loops") };
	"S1 phase 3: add 8 quiet voices every 5 s until peak CPU passes 75".postln;
	while { (voices.size < 160) and: { s.peakCPU < 75 } } {
		8.do { voices.add(Synth(\s1voice, [\freq, rrand(55, 880), \pan, rrand(-0.8, 0.8), \amp, 0.01])) };
		5.wait; rep.("ramp");
	};
	"S1 RESULT voices when the ramp stopped: % (peak CPU %)".format(voices.size, s.peakCPU.round(0.1)).postln;
	voices.do(_.free); loops.do(_.free);
	1.wait;
	"S1 done".postln;
	s.quit; 2.wait; 0.exit;
});
)
