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

#include <sstream>
#include <cmath>
#include <utility>

#include "Envelope.h"
#include "Global.h"

namespace BoxyLady {

void Envelope::Prepare(music_size SampRate, float_type gate_time) noexcept {
	auto TimeSamples = [SampRate](float_type time) noexcept -> music_size {
		return static_cast<music_size>(SampRate * time);
	};
	gate_samples_ = TimeSamples(gate_time);
	attack_samples_ = TimeSamples(attack_time_);
	hold_samples_ = TimeSamples(hold_time_);
	decay_samples_ = TimeSamples(decay_time_);
	sustain_samples_ = TimeSamples(sustain_time_);
	fade_samples_ = TimeSamples(fade_time_);
	hold_start_ = attack_samples_;
	decay_start_ = hold_start_ + hold_samples_;
	sustain_start_ = decay_start_ + decay_samples_;
	fade_start_ = sustain_start_ + sustain_samples_;
	fade_end_ = fade_start_ + fade_samples_;
}

void Envelope::Squish(music_size length) noexcept {
	const music_size try_fade_start {length - fade_samples_};
	fade_start_ = (std::cmp_less(try_fade_start, sustain_start_)) ? sustain_start_ : try_fade_start;
	fade_end_ = fade_start_ + fade_samples_;
	sustain_samples_ = fade_start_ - sustain_start_;
}

std::string Envelope::toString() {
	if (!active_) return "(off)";
	std::string buffer;
	std::ostringstream output(buffer, std::istringstream::out);
	std::print(output, "({} {} {} {} {} {} {} {} {})",
		attack_time_, attack_amp_, hold_time_, hold_amp_, decay_time_,
		decay_amp_, sustain_time_, sustain_amp_, fade_time_);
	return output.str();
}

Envelope Envelope::TriangularWindow(float_type start, float_type peak, float_type end) noexcept {
	Envelope temp;
	temp.attack_amp_ = temp.decay_amp_ = temp.sustain_amp_ = 0.0;
	temp.hold_amp_ = 1.0;
	temp.attack_time_ = start;
	temp.hold_time_ = peak - start;
	temp.decay_time_ = end - peak;
	temp.sustain_time_ = temp.fade_time_ = float_type_max;
	temp.gate_samples_ = 0;
	temp.active_ = true;
	return temp;
}

} //end namespace BoxyLady
