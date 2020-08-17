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

#ifndef STEREO_H_
#define STEREO_H_

#include <cmath>
#include <array>

#include "Global.h"

namespace BoxyLady {

class Stereo {
private:
	std::array<double, 2> amps_;
public:
	double& operator[](int index) {
		return amps_[index];
	}
	const double& operator[](int index) const {
		return amps_[index];
	}
	Stereo(double left, double right) {
		amps_[Left] = left;
		amps_[Right] = right;
	}
	Stereo(double amp) {
		amps_[0] = amps_[1] = amp;
	}
	Stereo() {
		amps_[0] = amps_[1] = 1.0;
	}
	static Stereo Position(double position) {
		return (position < 0.0) ?
				Stereo(1.0, 1.0 + position) : Stereo(1.0 - position, 1.0);
	}
	Stereo operator*(const Stereo rhs) const {
		Stereo stereo;
		for (int i = 0; i < 2; i++)
			stereo.amps_[i] = rhs.amps_[i] * amps_[i];
		return stereo;
	}
	Stereo operator*(double rhs) const {
		Stereo stereo;
		for (int i = 0; i < 2; i++)
			stereo.amps_[i] = rhs * amps_[i];
		return stereo;
	}
	void Swap() {
		double temp = amps_[0];
		amps_[0] = amps_[1];
		amps_[1] = temp;
	}
};

class Crosser {
private:
	std::array<std::array<double, 2>, 2> amps_;
public:
	double& operator()(int source, int dest) {
		return amps_[source][dest];
	}
	double operator()(int source, int dest) const {
		return amps_[source][dest];
	}
	Crosser(double parallel = 1.0, double crossed = 0.0) {
		amps_[0][0] = amps_[1][1] = parallel;
		amps_[1][0] = amps_[0][1] = crossed;
	}
	Crosser(const Stereo parallel, const Stereo crossed = Stereo(0)) {
		amps_[0][0] = parallel[0];
		amps_[1][1] = parallel[1];
		amps_[0][1] = crossed[0];
		amps_[1][0] = crossed[1];
	}
	inline Stereo Amp2(const Stereo);
	inline double Amp1(double);
	void AssertPosition() const {
		for (int source = 0; source < 2; source++)
			for (int dest = 0; dest < 2; dest++)
				if (amps_[source][dest] < 0)
					throw EParser().msg(
							"Cannot fade logarithmically with negative values. ");
	}
	void Dump() const {
		std::cout << "x:" << (amps_[0][0]) << " " << (amps_[0][1]) << " "
				<< (amps_[1][0]) << " " << (amps_[1][1]) << "\n";
	}
};

class Fader {
private:
	Crosser start_, end_;
	bool linear_, mirrored_;
public:
	Fader(double start = 1.0, double end = 0.0) :
			start_(start), end_(end), linear_(false), mirrored_(false) {
	}
	Fader(const Crosser start, const Crosser end) :
			start_(start), end_(end), linear_(false), mirrored_(false) {
	}
	Fader(const Crosser input) :
			start_(input), end_(input), linear_(false), mirrored_(false) {
	}
	static Fader FadeOut() {
		return Fader(Crosser(1.0, 0.0), Crosser(0.01, 0.0));
	}
	static Fader FadeIn() {
		return Fader(Crosser(0.01, 0.0), Crosser(1.0, 0.0));
	}
	static Fader PanSwap() {
		return Fader(Crosser(1.0, 0.0), Crosser(0.0, 1.0)).Linear();
	}
	static Fader PanCentre() {
		return Fader(Crosser(1.0, 0.0), Crosser(0.5, 0.5)).Linear();
	}
	static Fader PanEdge() {
		return Fader(Crosser(1.0, 0.0), Crosser(1.0, -1.0)).Linear();
	}
	static Fader Amp(double amp) {
		return Fader(Crosser(amp)).Linear();
	}
	static Fader AmpLR(const Stereo stereo) {
		return Fader(Crosser(stereo)).Linear();
	}
	static Fader AmpStereo(const Stereo parallel, const Stereo crossed) {
		return Fader(Crosser(parallel, crossed)).Linear();
	}
	static Fader AmpInverse() {
		return Fader(Crosser(-1.0)).Linear();
	}
	static Fader AmpInverseLR() {
		return Fader(Crosser(0.0, 1.0)).Linear();
	}
	Fader& Reverse() {
		const Crosser temp = end_;
		end_ = start_;
		start_ = temp;
		return *this;
	}
	Fader& Linear() {
		linear_ = true;
		return *this;
	}
	Fader& Logarithmic() {
		start_.AssertPosition();
		end_.AssertPosition();
		linear_ = false;
		return *this;
	}
	Fader& Mirror() {
		mirrored_ = true;
		return *this;
	}
	inline Crosser AmpTime(double) const;
	void Dump() const;
};

inline Stereo Crosser::Amp2(Stereo stereo) {
	Stereo result(0.0);
	for (int source = 0; source < 2; source++)
		for (int dest = 0; dest < 2; dest++)
			result[dest] += stereo[source] * amps_[source][dest];
	return result;
}

inline double Crosser::Amp1(double amp) {
	double result = 0;
	for (int source = 0; source < 2; source++)
		for (int dest = 0; dest < 2; dest++)
			result += amps_[source][dest] * 0.5;
	return result * amp;
}

inline Crosser Fader::AmpTime(double time) const {
	if (mirrored_) {
		time = (time > 0.5) ? (2.0 - 2.0 * time) : 2.0 * time;
	}
	Crosser result;
	for (int source = 0; source < 2; source++)
		for (int dest = 0; dest < 2; dest++)
			if (linear_)
				result(source, dest) = end_(source, dest) * time
						+ start_(source, dest) * (1.0 - time);
			else
				result(source, dest) = exp(
						log(end_(source, dest) + 1e-7) * time
								+ log(start_(source, dest) + 1e-7)
										* (1.0 - time));
	return result;
}

}

#endif /* STEREO_H_ */
