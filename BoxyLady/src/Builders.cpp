//============================================================================
// Name        : BoxyLady
// Author      : Darren Green
// Copyright   : (C) Darren Green 2011-2020
// Description : Music sequencer
//
// License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>
// This is free software; you are free to change and redistribute it.
// There is NO WARRANTY, to the extent permitted by law.
// Contact: darren.green@stir.ac.uk http://www.pinkmongoose.co.uk
//============================================================================

#include "Builders.h"

namespace BoxyLady {

Envelope BuildEnvelope(Blob &blob) {
	blob.AssertFunction();
	const int env_count = blob.children_.size();
	int index = 0;
	if (env_count == 1)
		if (blob.hasFlag("off"))
			return Envelope();
	if ((env_count > 9) || (env_count == 4) || (env_count == 6)
			|| (env_count == 8))
		throw EParser().msg(
				"Envelopes must be 2, 3, 5, 7, or 9 parameters long.");
	Envelope env;
	env.attack_time_ = blob[index++].asDouble(0, MaxSequenceLength);
	if (env_count >= 3) {
		env.attack_amp_ = BuildAmplitude(blob[index++]);
	} else {
		env.attack_amp_ = 1.0;
	}
	if (env_count >= 9) {
		env.hold_time_ = blob[index++].asDouble(0, MaxSequenceLength);
		env.hold_amp_ = BuildAmplitude(blob[index++]);
	} else {
		env.hold_time_ = 0;
		env.hold_amp_ = env.attack_amp_;
	}
	if (env_count >= 5) {
		env.decay_time_ = blob[index++].asDouble(0, MaxSequenceLength);
		env.decay_amp_ = BuildAmplitude(blob[index++]);
	} else {
		env.decay_time_ = 0;
		env.decay_amp_ = env.hold_amp_;
	}
	if (env_count >= 7) {
		env.sustain_time_ = blob[index++].asDouble(0, MaxSequenceLength);
		env.sustain_amp_ = BuildAmplitude(blob[index++]);
	} else {
		env.sustain_time_ = 0;
		env.sustain_amp_ = env.decay_amp_;
	}
	if (env_count >= 2) {
		env.fade_time_ = blob[index++].asDouble(0, MaxSequenceLength);
	} else {
		env.fade_time_ = env.attack_time_;
	}
	env.active_ = true;
	return env;
}

Stereo BuildStereo(Blob &blob) {
	blob.AssertFunction();
	if (blob.isAtomic()) {
		const std::string atom = blob.atom();
		if (atom == "off")
			return Stereo();
		else if (atom == "L")
			return Stereo(1.0, 0.0);
		else if (atom == "R")
			return Stereo(0.0, 1.0);
		double amp = BuildAmplitude(blob);
		return Stereo(amp);
	}
	return Stereo(BuildAmplitude(blob[0]), BuildAmplitude(blob[1]));
}

Wave BuildWave(Blob &blob) {
	blob.AssertFunction();
	if (blob.hasFlag("off"))
		return Wave();
	else if (blob.children_.size() == 1)
		return Wave(blob[0].asDouble(), 1.0);
	else if (blob.children_.size() == 2)
		return Wave(blob[0].asDouble(), BuildAmplitude(blob[1]));
	else
		return Wave(blob[0].asDouble(), BuildAmplitude(blob[1]),
				blob[2].asDouble());
}

Phaser BuildPhaser(Blob &blob, int max_args) {
	blob.AssertFunction();
	const int child_count = blob.children_.size();
	if (child_count > max_args)
		throw EParser().msg(
				"Too many arguments to phaser/vibrato/pitch bend.\n"
						+ blob.ErrorString());
	if (blob.hasFlag("off"))
		return Phaser();
	if (child_count == 1)
		return Phaser(blob[0].asDouble(), 1.0);
	else if (child_count == 2)
		return Phaser(blob[0].asDouble(), blob[1].asDouble());
	else if (child_count == 3)
		return Phaser(blob[0].asDouble(), blob[1].asDouble(),
				blob[2].asDouble());
	else if (child_count == 4)
		return Phaser(blob[0].asDouble(), blob[1].asDouble(),
				blob[2].asDouble(), blob[3].asDouble());
	else
		return Phaser(blob[0].asDouble(), blob[1].asDouble(),
				blob[2].asDouble(), blob[3].asDouble(), blob[4].asDouble());
}

double BuildAmplitude(Blob &blob) {
	if (blob.key_ == "dB")
		return pow(10.0, blob.asDouble(-1000.0, 1000.0) / 20.0);
	else if (blob.key_ == "Np")
		return exp(blob.asDouble(-1000.0, 1000.0));
	else if (blob.isFunction()) {
		if (blob.hasKey("dB"))
			return pow(10.0, blob["dB"].asDouble(-1000.0, 1000.0) / 20.0);
		else if (blob.hasKey("Np"))
			return exp(blob["Np"].asDouble(-1000.0, 1000.0));
		else
			return blob.asDouble();
	} else
		return blob.asDouble();
}

double BuildFrequency(Blob &blob) {
	constexpr double MaxAccidental = 100.0;
	if (blob.key_ == "f")
		return blob[0].asDouble(0, MaxAccidental)
				/ blob[1].asDouble(0, MaxAccidental);
	else if (blob.key_ == "c")
		return pow(2.0, blob.asDouble() / physics::CentsPerOctave);
	else if (blob.key_ == "O")
		return pow(2.0, blob.asDouble());
	else if (blob.key_ == "mO")
		return pow(2.0, blob.asDouble() / 1000.0);
	else if (blob.isFunction()) {
		if (blob.hasKey("f"))
			return blob["f"][0].asDouble(0, MaxAccidental)
					/ blob["f"][1].asDouble(0, MaxAccidental);
		else if (blob.hasKey("c"))
			return pow(2.0, blob["c"].asDouble() / physics::CentsPerOctave);
		else
			return blob.asDouble();
	} else
		return blob.asDouble();
}

}

