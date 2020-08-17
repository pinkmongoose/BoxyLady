/*
 * Fraction.h
 *
 *  Created on: 24 Jun 2020
 *      Author: darren
 */

#ifndef FRACTION_H_
#define FRACTION_H_

#include <ostream>
#include <sstream>
#include <string>
#include <climits>

#include "Global.h"

namespace BoxyLady {

inline constexpr double DefaultTolerance = pow(2.0,
		6.0 / physics::CentsPerOctave);

class Fraction {
private:
	int numerator_, denominator_;
	double ratio_;
	std::string name_;
	void Format(std::ostringstream &stream, int decimals = 3) const {
		stream.precision(decimals);
		stream.setf(std::ios_base::fixed);
	}
	static bool InLimit(int, int);
	bool TryName(int, int, std::string);
public:
	Fraction(double, double = DefaultTolerance, int = INT_MAX);
	std::string RatioString() const;
	std::string FractionString(bool simple = true);
	std::string IntervalString();
	std::string CentsString() const;
	double ratio() const {
		return static_cast<double>(numerator_)
				/ static_cast<double>(denominator_);
	}
};

}

#endif /* FRACTION_H_ */
