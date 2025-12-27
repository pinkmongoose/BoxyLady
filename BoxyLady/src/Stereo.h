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

#ifndef STEREO_H_
#define STEREO_H_

#include <cmath>
#include <array>
#include <string>

#include "Global.h"

namespace BoxyLady {

static constexpr size_t left {0}, right {1};
static constexpr std::array<std::string, 2> ChannelNames {"left","right"}; 

class Stereo {
private:
	std::array<float_type, 2> amps_ {{1.0, 1.0}};
public:
	float_type& operator[](int index) {
		return amps_[index];
	}
	const float_type& operator[](int index) const {
		return amps_[index];
	}
	explicit Stereo(float_type amp_left, float_type amp_right) {
		amps_[left] = amp_left;
		amps_[right] = amp_right;
	}
	explicit Stereo(float_type amp) {
		amps_[left] = amps_[right] = amp;
	}
	explicit Stereo() = default;
	static Stereo Position(float_type position) {
		return (position < 0.0) ? Stereo(1.0, 1.0 + position) : Stereo(1.0 - position, 1.0);
	}
	static Stereo Left() {
		return Stereo {1.0, 0.0};
	}
	static Stereo Right() {
		return Stereo {0.0, 1.0};
	}
	inline Stereo operator*(const Stereo rhs) const {
		return Stereo(amps_[left] * rhs.amps_[left], amps_[right] * rhs.amps_[right]);
	}
	inline Stereo operator*(float_type rhs) const {
		return Stereo(amps_[left] * rhs, amps_[right] * rhs); 
	}
	inline void Swap() {
		std::swap(amps_[left], amps_[right]);
	}
};

class MatrixMixer {
private:
	std::array<std::array<float_type, 2>, 2> amps_;
public:
	template<typename Self>
	decltype(auto) operator()(this Self& self, int source, int dest) {
    	return self.amps_[source][dest];
	}
	explicit MatrixMixer(float_type parallel = 1.0, float_type crossed = 0.0) {
		amps_[left][left] = amps_[right][right] = parallel;
		amps_[right][left] = amps_[left][right] = crossed;
	}
	explicit MatrixMixer(const Stereo parallel, const Stereo crossed = Stereo(0)) {
		amps_[left][left] = parallel[left];
		amps_[right][right] = parallel[right];
		amps_[left][right] = crossed[left];
		amps_[right][left] = crossed[right];
	}
	inline Stereo Amp2(const Stereo);
	inline float_type Amp1(float_type);
	void AssertPosition() const {
		for (size_t source {0}; source < 2; source++)
			for (size_t dest {0}; dest < 2; dest++)
				if (amps_[source][dest] < 0)
					throw EError("Cannot fade logarithmically with negative values. ");
	}
};

class CrossFader {
private:
	static inline constexpr float_type fade_out_value {0.01}, tiny_amp {1.0e-7}; 
	MatrixMixer start_, end_;
	bool linear_, mirrored_;
public:
	explicit CrossFader(float_type start = 1.0, float_type end = 0.0) :
			start_(start), end_(end), linear_(false), mirrored_(false) {
	}
	explicit CrossFader(const MatrixMixer start, const MatrixMixer end) :
			start_(start), end_(end), linear_(false), mirrored_(false) {
	}
	explicit CrossFader(const MatrixMixer input) :
			start_(input), end_(input), linear_(false), mirrored_(false) {
	}
	static CrossFader FadeOut() {
		return CrossFader(MatrixMixer(1.0, 0.0), MatrixMixer(fade_out_value, 0.0));
	}
	static CrossFader FadeIn() {
		return CrossFader(MatrixMixer(fade_out_value, 0.0), MatrixMixer(1.0, 0.0));
	}
	static CrossFader PanSwap() {
		return CrossFader(MatrixMixer(1.0, 0.0), MatrixMixer(0.0, 1.0)).Linear();
	}
	static CrossFader PanCentre() {
		return CrossFader(MatrixMixer(1.0, 0.0), MatrixMixer(0.5, 0.5)).Linear();
	}
	static CrossFader PanEdge() {
		return CrossFader(MatrixMixer(1.0, 0.0), MatrixMixer(1.0, -1.0)).Linear();
	}
	static CrossFader Amp(float_type amp) {
		return CrossFader(MatrixMixer(amp)).Linear();
	}
	static CrossFader AmpStereo(const Stereo stereo) {
		return CrossFader(MatrixMixer(stereo)).Linear();
	}
	static CrossFader AmpCross(const Stereo parallel, const Stereo crossed) {
		return CrossFader(MatrixMixer(parallel, crossed)).Linear();
	}
	static CrossFader AmpInverse() {
		return CrossFader(MatrixMixer(-1.0)).Linear();
	}
	static CrossFader AmpInverseLR() {
		return CrossFader(MatrixMixer(0.0, 1.0)).Linear();
	}
	CrossFader& Reverse() {
		std::swap(start_, end_);
		return *this;
	}
	CrossFader& Linear() {
		linear_ = true;
		return *this;
	}
	CrossFader& Logarithmic() {
		start_.AssertPosition();
		end_.AssertPosition();
		linear_ = false;
		return *this;
	}
	CrossFader& Mirror() {
		mirrored_ = true;
		return *this;
	}
	inline MatrixMixer AmpTime(float_type) const;
};

inline Stereo MatrixMixer::Amp2(Stereo stereo) {
	return Stereo(stereo[left]*amps_[left][left] + stereo[right]*amps_[right][left],
		stereo[right]*amps_[right][right] + stereo[left]*amps_[left][right]);
}
/*	Stereo result(0.0);
	for (int source {0}; source < 2; source++)
		for (int dest {0}; dest < 2; dest++)
			result[dest] += stereo[source] * amps_[source][dest];
	return result;
} */

inline float_type MatrixMixer::Amp1(float_type amp) {
	return amp * 0.5 * (amps_[left][left] + amps_[right][left] + amps_[right][right] + amps_[left][right]);
}
/*	float_type result {0.0};
	for (int source {0}; source < 2; source++)
		for (int dest {0}; dest < 2; dest++)
			result += amps_[source][dest] * 0.5;
	return result * amp;
} */

inline MatrixMixer CrossFader::AmpTime(float_type time) const {
	if (mirrored_) {
		time = (time > 0.5) ? (2.0 - 2.0 * time) : 2.0 * time;
	}
	MatrixMixer result;
	for (size_t source {0}; source < 2; source++)
		for (size_t dest {0}; dest < 2; dest++)
			if (linear_)
				result(source, dest) = end_(source, dest) * time
						+ start_(source, dest) * (1.0 - time);
			else
				result(source, dest) = exp(
						log(end_(source, dest) + tiny_amp) * time
								+ log(start_(source, dest) + tiny_amp)
										* (1.0 - time));
	return result;
}

} //end namespace BoxyLady

#endif /* STEREO_H_ */
