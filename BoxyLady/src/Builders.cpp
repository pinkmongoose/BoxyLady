//============================================================================
// Name        : BoxyLady
// Author      : Darren Green
// Copyright   : (C) Darren Green 2011-2025
// Description : Music sequencer
//
// License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>
// This is free software; you are free to change and redistribute it.
// There is NO WARRANTY, to the extent permitted by law.
// Contact: darren.green@stir.ac.uk http://pinkmongoose.co.uk
//============================================================================

#include <utility>

#include "Builders.h"

namespace BoxyLady {

Envelope BuildEnvelope(Blob& blob) {
	blob.AssertFunction();
	const size_t env_count {blob.children_.size()};
	size_t index {0};
	if (env_count == 1)
		if (blob.hasFlag("off"))
			return Envelope();
	if ((env_count > 9) || (env_count == 4) || (env_count == 6) || (env_count == 8))
		throw EError("Envelopes must be 1, 2, 3, 5, 7, or 9 parameters long.");
	Envelope env;
	env.attack_time_ = blob[index++].asFloat(0, HourLength);
	if (env_count >= 3) {
		env.attack_amp_ = BuildAmplitude(blob[index++]);
	} else {
		env.attack_amp_ = 1.0;
	}
	if (env_count >= 9) {
		env.hold_time_ = blob[index++].asFloat(0, HourLength);
		env.hold_amp_ = BuildAmplitude(blob[index++]);
	} else {
		env.hold_time_ = 0;
		env.hold_amp_ = env.attack_amp_;
	}
	if (env_count >= 5) {
		env.decay_time_ = blob[index++].asFloat(0, HourLength);
		env.decay_amp_ = BuildAmplitude(blob[index++]);
	} else {
		env.decay_time_ = 0;
		env.decay_amp_ = env.hold_amp_;
	}
	if (env_count >= 7) {
		env.sustain_time_ = blob[index++].asFloat(0, HourLength);
		env.sustain_amp_ = BuildAmplitude(blob[index++]);
	} else {
		env.sustain_time_ = 0;
		env.sustain_amp_ = env.decay_amp_;
	}
	if (env_count >= 2) {
		env.fade_time_ = blob[index++].asFloat(0, HourLength);
	} else {
		env.fade_time_ = env.attack_time_;
	}
	env.active_ = true;
	return env;
}

Stereo BuildStereo(Blob& blob) {
	blob.AssertFunction();
	if (blob.isAtomic()) {
		if (const auto atom {blob.atom()}; atom == "off")
			return Stereo();
		else if (atom == "L")
			return Stereo::Left();
		else if (atom == "R")
			return Stereo::Right();
		else if (atom == "C")
			return Stereo(sqrt(0.5));
		float_type amp = BuildAmplitude(blob);
		return Stereo(amp);
	}
	return Stereo(BuildAmplitude(blob[0]), BuildAmplitude(blob[1]));
}

Wave BuildWave(Blob& blob) {
	blob.AssertFunction();
	const size_t child_count {blob.children_.size()};
	if (child_count > 3)
		throw EError("Too many arguments to waveform.\n" + blob.ErrorString());
	if (blob.hasFlag("off"))
		return Wave();
	switch (blob.children_.size()) {
		case 0: throw EError("Not enough arguments to waveform.\n" + blob.ErrorString());
		case 1: return Wave(blob[0].asFloat(), 1.0);
		case 2: return Wave(blob[0].asFloat(), BuildAmplitude(blob[1]));
		case 3: return Wave(blob[0].asFloat(), BuildAmplitude(blob[1]), blob[2].asFloat());
		std::unreachable();
	}
	std::unreachable();
}

Phaser BuildPhaser(Blob& blob, size_t max_args) {
	blob.AssertFunction();
	const size_t child_count {blob.children_.size()};
	if (child_count > max_args)
		throw EError("Too many arguments to phaser/vibrato/pitch bend.\n" + blob.ErrorString());
	if (blob.hasFlag("off"))
		return Phaser();
	switch (child_count) {
		case 0: throw EError("Not enough arguments to phaser/vibrato/pitch bend.\n" + blob.ErrorString());
		case 1: return Phaser(blob[0].asFloat(), 1.0);
		case 2: return Phaser(blob[0].asFloat(), blob[1].asFloat());
		case 3: return Phaser(blob[0].asFloat(), blob[1].asFloat(), blob[2].asFloat());
		case 4: return Phaser(blob[0].asFloat(), blob[1].asFloat(), blob[2].asFloat(), blob[3].asFloat());
		case 5: return Phaser(blob[0].asFloat(), blob[1].asFloat(), blob[2].asFloat(), blob[3].asFloat(), blob[4].asFloat());
		std::unreachable();
	}
	std::unreachable();
}

float_type BuildAmplitude(Blob& blob) {
	if (blob.key_ == "dB")
		return pow(10.0, blob.asFloat(-1000.0, 1000.0) / 20.0);
	else if (blob.key_ == "Np")
		return exp(blob.asFloat(-1000.0, 1000.0));
	else if (blob.isFunction()) {
		if (blob.hasKey("dB"))
			return pow(10.0, blob["dB"].asFloat(-1000.0, 1000.0) / 20.0);
		else if (blob.hasKey("Np"))
			return exp(blob["Np"].asFloat(-1000.0, 1000.0));
		else
			return blob.asFloat();
	} else
		return blob.asFloat();
}

float_type BuildFrequency(Blob& blob) {
	constexpr float_type MaxAccidental {100.0};
	if (blob.key_ == "f")
		return blob[0].asFloat(0, MaxAccidental) / blob[1].asFloat(0, MaxAccidental);
	else if (blob.key_ == "c")
		return pow(2.0, blob.asFloat() / physics::CentsPerOctave);
	else if (blob.key_ == "O")
		return pow(2.0, blob.asFloat());
	else if (blob.key_ == "mO")
		return pow(2.0, blob.asFloat() / 1000.0);
	else if (blob.isFunction()) {
		if (blob.hasKey("f"))
			return blob["f"][0].asFloat(0, MaxAccidental) / blob["f"][1].asFloat(0, MaxAccidental);
		else if (blob.hasKey("c"))
			return pow(2.0, blob["c"].asFloat() / physics::CentsPerOctave);
		else
			return blob.asFloat();
	} else
		return blob.asFloat();
}

} //end namespace BoxyLady
