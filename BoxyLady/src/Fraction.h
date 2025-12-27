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

#ifndef FRACTION_H_
#define FRACTION_H_

#include <ostream>
#include <sstream>
#include <string>
#include <map>

#include "Global.h"

namespace BoxyLady {

enum class pitch_unit {cents, millioctaves, yu, edo12, edo19, edo24, edo31, savart, meride, heptameride};

class Fraction;
class Factors;

class Factors {
private:
	std::map<int, int> factors_ {};
	friend Factors operator- (Factors, Factors);
public:
	explicit Factors(int);
	explicit Factors(Fraction);
	explicit Factors() = default;
	std::string toString() const;
	int MaxFactor() const;
	static bool isPrime(int);
	std::string toMonzo() const;
};

class Fraction {
	friend class Factors;
private:
	static inline constexpr std::string_view RatioSeparator {":"}, CentSymbol {"¢"}, MillioctaveSymbol {"m"}, 
		YuSymbol {"yu"}, DegreeSymbol {"°"}, GenericSymbol {"¤"};
	int numerator_, denominator_, sign_;
	float_type ratio_;
	void Format(std::ostringstream& stream, int decimals = 3) const {
		stream.precision(decimals);
		stream.setf(std::ios_base::fixed);
	}
	static bool InLimit(int number, int max_limit) {
		return Factors(number).MaxFactor() <= max_limit;
	}
public:
	explicit Fraction(float_type, float_type, int = int_max) noexcept;
	explicit Fraction(float_type target) noexcept : numerator_{0}, denominator_{0}, sign_{0}, ratio_{target} {};
	std::string RatioString() const;
	std::string FractionString(bool simple = true);
	std::string IntervalString();
	std::string PitchUnitString(pitch_unit) const;
	float_type ratio() const noexcept {
		return static_cast<float_type>(numerator_) / static_cast<float_type>(denominator_);
	}
	int sgnLog() const noexcept {
		return sgn(ratio_ - 1.0);
	}
};

} //end namespace BoxyLady

#endif /* FRACTION_H_ */
