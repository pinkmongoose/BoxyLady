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

#include "Envelope.h"

#include <sstream>
#include <cfloat>

namespace BoxyLady {

double Envelope::gate_time { 0.002 };

void Envelope::Prepare(int SampRate) {
	gate_samples_ = static_cast<int>(static_cast<double>(SampRate) * gate_time);
	attack_samples_ = static_cast<int>(static_cast<double>(SampRate)
			* attack_time_);
	hold_samples_ =
			static_cast<int>(static_cast<double>(SampRate) * hold_time_);
	decay_samples_ = static_cast<int>(static_cast<double>(SampRate)
			* decay_time_);
	sustain_samples_ = static_cast<int>(static_cast<double>(SampRate)
			* sustain_time_);
	fade_samples_ =
			static_cast<int>(static_cast<double>(SampRate) * fade_time_);
	hold_start_ = attack_samples_;
	decay_start_ = hold_start_ + hold_samples_;
	sustain_start_ = decay_start_ + decay_samples_;
	fade_start_ = sustain_start_ + sustain_samples_;
	fade_end_ = fade_start_ + fade_samples_;
}

void Envelope::Squish(int length) {
	const int try_fade_start = length - fade_samples_;
	fade_start_ =
			(try_fade_start < sustain_start_) ? sustain_start_ : try_fade_start;
	fade_end_ = fade_start_ + fade_samples_;
	sustain_samples_ = fade_start_ - sustain_start_;
}

double Envelope::Amp(int pos) {
	if (!active_)
		return 1.0;
	double amp = 0;
	if (pos < hold_start_)
		amp = (static_cast<double>(pos) / static_cast<double>(attack_samples_))
				* attack_amp_;
	else if (pos < decay_start_)
		amp = (static_cast<double>(pos - hold_start_))
				/ static_cast<double>(hold_samples_) * (hold_amp_ - attack_amp_)
				+ attack_amp_;
	else if (pos < sustain_start_)
		amp = (static_cast<double>(pos - decay_start_))
				/ static_cast<double>(decay_samples_) * (decay_amp_ - hold_amp_)
				+ hold_amp_;
	else if (pos < fade_start_)
		amp = (static_cast<double>(pos - sustain_start_))
				/ static_cast<double>(sustain_samples_)
				* (sustain_amp_ - decay_amp_) + decay_amp_;
	else if (pos < fade_end_)
		amp = (1.0
				- (static_cast<double>(pos - fade_start_))
						/ static_cast<double>(fade_samples_)) * sustain_amp_;
	return amp;
}

double Envelope::Amp(int pos, int end_pos) {
	double amp = Amp(pos);
	if (pos < gate_samples_)
		amp = static_cast<double>(amp) * static_cast<double>(pos)
				/ static_cast<double>(gate_samples_);
	if (end_pos < gate_samples_)
		amp = static_cast<double>(amp) * static_cast<double>(end_pos)
				/ static_cast<double>(gate_samples_);
	return amp;
}

std::string Envelope::toString() {
	if (!active_)
		return "(off)";
	std::string buffer;
	std::ostringstream File(buffer, std::istringstream::out);
	File << "(" << attack_time_ << " " << attack_amp_ << " " << hold_time_
			<< " " << hold_amp_ << " " << decay_time_ << " " << decay_amp_
			<< " " << sustain_time_ << " " << sustain_amp_ << " " << fade_time_
			<< ")";
	return File.str();
}

Envelope Envelope::TriangularWindow(double start, double peak, double end) {
	Envelope temp;
	temp.attack_amp_ = temp.decay_amp_ = temp.sustain_amp_ = 0.0;
	temp.hold_amp_ = 1.0;
	temp.attack_time_ = start;
	temp.hold_time_ = peak - start;
	temp.decay_time_ = end - peak;
	temp.sustain_time_ = temp.fade_time_ = DBL_MAX;
	temp.active_ = true;
	return temp;
}

}
