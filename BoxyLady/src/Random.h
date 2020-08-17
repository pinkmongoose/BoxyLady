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

#include <ctime>
#include <random>
#include <functional>

#ifndef RANDOM_H_
#define RANDOM_H_

namespace Darren {

inline constexpr int DefaultSeedValue { 3 * 5 * 7 * 11 * 13 * 17 * 19 * 23 };

class Random {
private:
	std::mt19937 generator_;
	std::uniform_real_distribution<double> uniform_;
	std::function<double()> Uniform01;
public:
	Random() {
		generator_.seed(DefaultSeedValue);
		uniform_ = std::uniform_real_distribution<double>(0.0, 1.0);
		Uniform01 = std::bind(uniform_, generator_);
	}
	inline double uniform() {
		return Uniform01();
	}
	inline double uniform(double max) {
		return Uniform01() * max;
	}
	inline double uniform(double min, double max) {
		return Uniform01() * (max - min) + min;
	}
	void SetSeed(int x) {
		generator_.seed(x);
	}
	void AutoSeed() {
		generator_.seed(static_cast<int>(time(NULL)));
	}
};

}

#endif /* RANDOM_H_ */
