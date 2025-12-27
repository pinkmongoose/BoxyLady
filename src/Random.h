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

#include <ctime>
#include <random>
#include <functional>

#ifndef RANDOM_H_
#define RANDOM_H_

namespace Darren {

template <std::floating_point T>
class Random {
private:
	inline static constexpr int DefaultSeedValue {3 * 5 * 7 * 11 * 13 * 17 * 19 * 23}; 
	std::mt19937 generator_;
	std::uniform_real_distribution<T> uniform_;
	std::function<T()> Uniform01;
public:
	explicit Random() {
		generator_.seed(DefaultSeedValue);
		uniform_ = std::uniform_real_distribution<T>(0.0, 1.0);
		Uniform01 = std::bind(uniform_, generator_);
	}
	inline T uniform() noexcept {
		return Uniform01();
	}
	inline T uniform(T max) noexcept {
		return Uniform01() * max;
	}
	inline T uniform(T min, T max) noexcept {
		return Uniform01() * (max - min) + min;
	}
	inline bool Bernoulli(T probability) noexcept {
		return Uniform01() < probability;
	}
	void SetSeed(int x) {
		generator_.seed(x);
	}
	void AutoSeed() {
		generator_.seed(static_cast<int>(std::time(nullptr)));
	}
	std::mt19937& generator() {return generator_;};
};

} //end namespace Darren

#endif /* RANDOM_H_ */
