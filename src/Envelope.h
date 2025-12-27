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

#ifndef ENVELOPE_H_
#define ENVELOPE_H_

#include <string>

#include "Global.h"

namespace BoxyLady {

using music_size = size_t;
using music_pos = long long int;

constexpr music_pos music_pos_max {longlong_max};

class Blob;
class Envelope {
	friend Envelope BuildEnvelope(Blob&);
private:
	music_pos hold_start_ {0}, decay_start_ {0}, sustain_start_ {0}, fade_start_ {0}, fade_end_ {0},
			attack_samples_ {0}, hold_samples_ {0}, sustain_samples_ {0}, decay_samples_ {0},
			fade_samples_ {0}, gate_samples_ {0};
	float_type attack_time_ {0.0}, attack_amp_ {0.0}, hold_time_ {0.0}, hold_amp_ {0.0}, decay_time_ {0.0},
			decay_amp_ {0.0}, sustain_time_ {0.0}, sustain_amp_ {0.0}, fade_time_ {0.0};
	bool active_ {false};
public:
	explicit Envelope() = default;
	void Prepare(music_size, float_type = 0.0) noexcept;
	void Squish(music_size) noexcept;
	inline float_type Amp(music_pos pos) noexcept {
		if (!active_) return 1.0;
		float_type amp {0.0};
		if (pos < hold_start_)
			amp = (static_cast<float_type>(pos) / static_cast<float_type>(attack_samples_)) * attack_amp_;
		else if (pos < decay_start_)
			amp = (static_cast<float_type>(pos - hold_start_)) / static_cast<float_type>(hold_samples_) * (hold_amp_ - attack_amp_) + attack_amp_;
		else if (pos < sustain_start_)
			amp = (static_cast<float_type>(pos - decay_start_)) / static_cast<float_type>(decay_samples_) * (decay_amp_ - hold_amp_) + hold_amp_;
		else if (pos < fade_start_)
			amp = (static_cast<float_type>(pos - sustain_start_)) / static_cast<float_type>(sustain_samples_) * (sustain_amp_ - decay_amp_) + decay_amp_;
		else if (pos < fade_end_)
			amp = (1.0 - (static_cast<float_type>(pos - fade_start_)) / static_cast<float_type>(fade_samples_)) * sustain_amp_;
		return amp;
	}
	inline float_type Amp(music_pos pos, music_pos end_pos) noexcept {
		float_type amp {Amp(pos)};
		if (pos < gate_samples_)
			amp *= static_cast<float_type>(pos) / static_cast<float_type>(gate_samples_);
		if (end_pos < gate_samples_)
			amp *= static_cast<float_type>(end_pos) / static_cast<float_type>(gate_samples_);
		return amp;
	}
	std::string toString();
	bool active() const noexcept {return active_;}
	music_size activeLength() const noexcept {
		return (active_)? fade_end_ : 0;
	}
	static Envelope TriangularWindow(float_type, float_type, float_type) noexcept;
};

} //end namespace BoxyLady

#endif /* ENVELOPE_H_ */
