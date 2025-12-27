//============================================================================
// Name        : BoxyLady
// Author      : Darren Green
// Copyright   : (C) Darren Green 2011-2025
// Description : Music sequencer
//
// License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>
// This is free software; you are free to change and redistribute it.
// There is NO WARRANTY, to the extent permitted by law.
// Contact: darren.green@stir.ac.uk http://www.pinkmongoose.co.uk
//============================================================================

#ifndef WAVEFORM_H_
#define WAVEFORM_H_

#include <numbers>

namespace BoxyLady {

class Sound;

inline constexpr float_type MinuteLength {60.0}, HourLength {MinuteLength * 60.0}, MaxLength {HourLength * 2.0};

namespace physics {
	inline constexpr float_type TwoPi {std::numbers::pi * 2.0};
} //end namespace Physics

using music_type = int16_t;
using MusicVector = std::vector<music_type>;

//-----------------

inline float_type SinPhi(float_type phi) noexcept {
	return sin(physics::TwoPi * phi);
}

inline float_type ModPhi(float_type phi) noexcept {
	return phi - floor(phi);
}

inline float_type SynthPower(float_type phi, float_type power) noexcept {
	const float_type mod_phi {ModPhi(phi)},
		value {SinPhi(mod_phi)};
	return (value > 0) ? pow(value, power) : -pow(-value, power);
}

inline float_type SynthSaw(float_type phi) noexcept {
	const float_type mod_phi {ModPhi(phi)};
	return mod_phi * 2.0 - 1.0;
}

inline float_type SynthTriangle(float_type phi) noexcept {
	const float_type mod_phi {ModPhi(phi)};
	return (mod_phi < 0.5) ?
		1.0 - 4.0 * mod_phi : 4.0 * mod_phi - 3.0;
}

inline float_type SynthPowerTriangle(float_type phi, float_type power) noexcept {
	const float_type mod_phi {ModPhi(phi)},
		one_less_phi {1.0_flt - mod_phi},
		two_less_power {2.0_flt - power};
	return (mod_phi < (power / 2.0)) ?
		1.0 - 4.0 * mod_phi / power :
		1.0 - 4.0 * one_less_phi / two_less_power;
}

inline float_type SynthSquare(float_type phi) noexcept {
	const float_type mod_phi {ModPhi(phi - 0.25)};
	return (mod_phi < 0.5) ? -1.0 : 1.0;
}

inline float_type SynthPulse(float_type phi, float_type power) noexcept {
	const float_type mod_phi {ModPhi(phi - 0.25)};
	return (mod_phi < (power / 2.0)) ? -1.0 : 1.0;
}

//-----------------

class Wave {
private:
	float_type freq_ {0.0}, amp_ {0.0}, offset_ {0.0};
public:
	explicit Wave(float_type freq_val = 0.0, float_type amp_val = 0.0, float_type offset_val = 0.0) noexcept :
		freq_{freq_val}, amp_{amp_val}, offset_{offset_val} {}
	float_type freq() const noexcept {return freq_;}
	float_type amp() const noexcept {return amp_;}
	float_type offset() const noexcept {return offset_;}
	void offset_freq(float_type  offset) noexcept {freq_ += offset;}
	void scale_freq(float_type factor) noexcept {freq_ *= factor;}
};

class Phaser: public Wave {
	float_type bend_factor_ {1.0}, bend_time_ {MinuteLength};
public:
	float_type bend_factor() const noexcept {return bend_factor_;}
	float_type bend_time() const noexcept {return bend_time_;}
	void set_bend_time(float_type time) noexcept {
		bend_time_ = time;
	}
	void set_bend_factor(float_type factor) noexcept {
		bend_factor_ = factor;
	}
	explicit Phaser(float_type freq_val = 0.0, float_type amp_val = 0.0, float_type offset_val = 0.0,
		float_type bend_factor_val = 1.0, float_type bend_time_val = MinuteLength) noexcept :
		Wave(freq_val, amp_val, offset_val), bend_factor_{bend_factor_val}, bend_time_{bend_time_val} {
	}
};

class Scratcher {
private:
	bool active_ {false};
	Sound *sound_ {nullptr};
	std::string name_ {""};
	float_type amp_ {0.0}, offset_ {0.0};
	bool loop_ {false};
public:
	std::string name() const noexcept {return name_;}
	Sound* sound() noexcept {return sound_;}
	void set_sound(Sound &seq) {
		sound_ = &seq;
	}
	bool active() const noexcept {return active_;}
	bool loop() const noexcept {return loop_;}
	float_type amp() const noexcept {return amp_;}
	float_type offset() const noexcept {return offset_;}
	explicit Scratcher(Sound *source, std::string name_val, float_type amp_val,
		float_type offset_val, bool loop_val) noexcept :
			active_{true}, sound_{source}, name_{name_val}, amp_{amp_val},
			offset_{offset_val}, loop_{loop_val} {}
	explicit Scratcher() = default;
	std::string toString();
};

} //end namespace BoxyLady

#endif /* WAVEFORM_H_ */
