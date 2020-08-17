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

#include "Fraction.h"

#include <sstream>
#include <cmath>

#include "Global.h"

namespace BoxyLady {

const std::string RatioSeparator { ":" };

//-----------------

bool Fraction::InLimit(int number, int max_limit) {
	while (number > 1) {
		bool found = false;
		for (int limit = 2; limit <= max_limit; limit++) {
			if (number % limit == 0) {
				number /= limit;
				found = true;
				break;
			}
		}
		if (!found)
			return false;
	}
	return true;
}

Fraction::Fraction(double target, double tol, int max_limit) {
	int sign = 1;
	ratio_ = target;
	if (!ratio_) {
		numerator_ = 0;
		denominator_ = 1;
		return;
	}
	;
	if (ratio_ < 0) {
		ratio_ = -ratio_;
		sign = -1;
	}
	;
	numerator_ = denominator_ = 1;
	for (;;) {
		const double relative = target * static_cast<double>(denominator_)
				/ static_cast<double>(numerator_);
		if (relative > 1.0) {
			if (relative < tol)
				break;
			do {
				numerator_++;
			} while (!InLimit(numerator_, max_limit));
		} else if (relative < 1.0) {
			if (relative > (1.0 / tol))
				break;
			do {
				denominator_++;
			} while (!InLimit(denominator_, max_limit));
		} else
			break;
	}
	numerator_ *= sign;
}

std::string Fraction::RatioString() const {
	std::ostringstream stream;
	Format(stream);
	stream << ratio_;
	return stream.str();
}

std::string Fraction::FractionString(bool simple) {
	std::ostringstream stream;
	Format(stream);
	if (simple) {
		if (!numerator_)
			return "0";
		if ((numerator_ == 1) && (denominator_ == 1))
			return "1";
		if (denominator_ == 1) {
			stream << numerator_;
			return stream.str();
		}
	}
	stream << numerator_ << RatioSeparator << denominator_;
	return stream.str();
}

std::string Fraction::CentsString() const {
	std::ostringstream stream;
	Format(stream, 1);
	stream << physics::CentsPerOctave * log2(ratio_) << "c";
	return stream.str();
}

bool Fraction::TryName(int test_numerator, int test_denominator,
		std::string test_name) {
	if (numerator_ == test_numerator)
		if (denominator_ == test_denominator) {
			name_ = test_name;
			return true;
		}
	return false;
}

std::string Fraction::IntervalString() {
	//Only intervals within an octave are shown, except the tritave
	if (TryName(1, 1, "unison"))
		return name_;
	if (TryName(2, 1, "octave"))
		return name_;
	if (TryName(3, 2, "perfect fifth"))
		return name_;
	if (TryName(3, 1, "tritave"))
		return name_;
	if (TryName(4, 3, "perfect fourth"))
		return name_;
	if (TryName(5, 4, "major third"))
		return name_;
	if (TryName(5, 3, "major sixth"))
		return name_;
	if (TryName(6, 5, "minor third"))
		return name_;
	if (TryName(7, 6, "septimal minor third"))
		return name_;
	if (TryName(7, 5, "lesser septimal tritone"))
		return name_;
	if (TryName(7, 4, "harmonic seventh"))
		return name_;
	if (TryName(8, 7, "septimal major tone"))
		return name_;
	if (TryName(8, 5, "minor sixth"))
		return name_;
	if (TryName(9, 8, "greater tone"))
		return name_;
	if (TryName(9, 7, "septimal major third"))
		return name_;
	if (TryName(9, 5, "large just minor seventh"))
		return name_;
	if (TryName(10, 9, "lesser tone"))
		return name_;
	if (TryName(10, 7, "greater septimal tritone"))
		return name_;
	if (TryName(11, 10, "greater undecimal n2"))
		return name_;
	if (TryName(11, 9, "undecimal neutral second"))
		return name_;
	if (TryName(11, 8, "undecimal super fourth"))
		return name_;
	if (TryName(11, 7, "undecimal minor sixth"))
		return name_;
	if (TryName(11, 6, "undecimal neutral seventh"))
		return name_;
	if (TryName(12, 11, "lesser undecimal n2"))
		return name_;
	if (TryName(12, 7, "septimal major sixth"))
		return name_;
	//From 13-limit and above, only selected intervals are shown
	if (TryName(13, 12, "Greater tridecimal 2/3 tone"))
		return name_;
	if (TryName(14, 13, "Lesser tridecimal 2/3 tone"))
		return name_;
	if (TryName(15, 14, "septimal diatonic semitone"))
		return name_;
	if (TryName(15, 8, "major seventh"))
		return name_;
	if (TryName(16, 15, "minor second"))
		return name_;
	if (TryName(16, 9, "Pythagorean minor seventh"))
		return name_;
	if (TryName(25, 16, "augmented fifth"))
		return name_;
	if (TryName(25, 18, "augmented fourth"))
		return name_;
	if (TryName(25, 24, "chroma"))
		return name_;
	if (TryName(27, 16, "Pythagorean major sixth"))
		return name_;
	if (TryName(32, 25, "diminished fourth"))
		return name_;
	if (TryName(32, 27, "Pythagorean minor third"))
		return name_;
	if (TryName(36, 25, "diminished fifth"))
		return name_;
	if (TryName(45, 32, "augmented fourth"))
		return name_;
	if (TryName(64, 45, "diminished fifth"))
		return name_;
	if (TryName(81, 64, "Pythagorean major third"))
		return name_;
	if (TryName(81, 80, "Syntonic comma"))
		return name_;
	if (TryName(128, 81, "Pythagorean minor sixth"))
		return name_;
	if (TryName(128, 125, "Diesis"))
		return name_;
	if (TryName(243, 128, "Pythagorean major seventh"))
		return name_;
	if (TryName(256, 243, "Pythagorean minor second"))
		return name_;

	return std::string();
}

}

