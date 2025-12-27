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

#include "Fraction.h"

#include <sstream>
#include <cmath>

#include "Global.h"

namespace BoxyLady {

Factors::Factors(int number) {
	if (number < 1)
		throw EError("Can't factorise negative or zero.");
	for (int factor {2}; factor <= number;) {
		if (number % factor == 0) {
			factors_[factor]++;
			number /= factor;
		} else
			++factor;
	}
}

Factors::Factors(Fraction fraction) {
	*this = Factors{fraction.numerator_} - Factors{fraction.denominator_};
}

Factors operator- (Factors numerator, Factors denominator) {
	for (auto factor : denominator.factors_)
		numerator.factors_[factor.first] -= factor.second;
	std::erase_if(numerator.factors_, [](const auto& item) {return item.second == 0;});
	return numerator;
}

std::string Factors::toString() const {
	std::ostringstream stream;
	if (!factors_.empty()) {
		stream << "(";
		bool first {true};
		for (auto factor : factors_) {
			if (!first)
				stream << "*";
			else {
				first = false;
			}
			stream << factor.first;
			if (factor.second != 1)
				stream << "^" << factor.second;
		}
		stream << ")";
	}
	return stream.str();
}

int Factors::MaxFactor() const {
	return (factors_.size() > 0)? factors_.rbegin()->first : 1;
}

bool Factors::isPrime(int number) {
	Factors factors{number};
	return (factors.factors_.size()==1) && (factors.factors_.begin()->second == 1);
}

std::string Factors::toMonzo() const {
	std::ostringstream stream;
	bool first {true};
	stream << "[";
	const int max_factor{MaxFactor()};
	if (max_factor == 1) return "[0>";
	for (int factor{2}; factor <= max_factor; factor++) {
		if (isPrime(factor)) {
			if (!first)
				stream << " ";
			else
				first = false;
			stream << (factors_.contains(factor)? factors_.at(factor) : 0); 
		}
	}
	stream << ">";
	return stream.str();
}

//-----------------

Fraction::Fraction(float_type target, float_type tol, int max_limit) noexcept {
	constexpr int max_terms {10000};
	ratio_ = target;
	if (ratio_ == 0.0) {
		numerator_ = 0;
		denominator_ = 1;
		return;
	}
	sign_ = sgn(ratio_);
	ratio_ *= sign_;
	numerator_ = denominator_ = 1;
	for (;;) {
		const float_type relative {target * static_cast<float_type>(denominator_) / static_cast<float_type>(numerator_)};
		if (relative > 1.0) {
			if (relative < tol)
				break;
			else
				for (numerator_++; !InLimit(numerator_, max_limit); numerator_++);
		} else {
			if (relative > 1.0 / tol)
				break;
			else 
				for (denominator_++; !InLimit(denominator_, max_limit); denominator_++);
		}
		if (numerator_ > max_terms) {
			numerator_ = 0;
			denominator_ = 1;
			tol *= 1.0 + (tol - 1.0) * 1.2;
		}
		//std::cout << numerator_ << "/" << denominator_ << "\n";
	}
	numerator_ *= sign_;
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

std::string Fraction::PitchUnitString(pitch_unit unit) const {
	const std::map<pitch_unit, std::pair<float_type, std::string_view>> pitch_units {
		{pitch_unit::millioctaves, {physics::MillioctavesPerOctave, MillioctaveSymbol}},
		{pitch_unit::cents, {physics::CentsPerOctave, CentSymbol}},
		{pitch_unit::yu, {physics::YuPerOctave, YuSymbol}},
		{pitch_unit::edo12, {12, DegreeSymbol}}, {pitch_unit::edo19, {19, DegreeSymbol}},
		{pitch_unit::edo24, {24, DegreeSymbol}}, {pitch_unit::edo31, {31, DegreeSymbol}},
		{pitch_unit::savart, {physics::SavartsPerOctave, GenericSymbol}},
		{pitch_unit::meride, {physics::MeridesPerOctave, GenericSymbol}},
		{pitch_unit::heptameride, {physics::HeptameridesPerOctave, GenericSymbol}}
	};
	const float_type mult {pitch_units.at(unit).first};
	const std::string_view unit_symbol {pitch_units.at(unit).second};
	std::ostringstream stream;
	Format(stream, 1);
	const float_type size {mult * log2(ratio_)};
	if (size == 0.0) stream << "0";
	else if (size > 0.0) stream << "+" << size;
	else stream << size;
	stream << unit_symbol;
	return stream.str();
}

std::string Fraction::IntervalString() {
	static const std::vector<std::tuple<int, int, std::string_view>> fraction_names {
		{
			//Only intervals within an octave are shown, except the tritave and low octave multiples
			{1, 1, "unison"},
			{2, 1, "octave"},
			{3, 2, "perfect fifth"},
			{4, 3, "perfect fourth"},
			{5, 4, "major third"}, {5, 3, "major sixth"},
			{6, 5, "minor third"},
			{7, 6, "septimal minor third"}, {7, 4, "harmonic seventh"}, {7, 5, "lesser septimal tritone"},
			{8, 7, "septimal major tone"}, {8, 5, "minor sixth"},
			{9, 8, "greater tone"}, {9, 5, "large just minor seventh"}, {9, 7, "septimal major third"},
			{10, 9, "lesser tone"}, {10, 7, "greater septimal tritone"},
			{11, 10, "greater undecimal neutral second"}, {11, 6, "undecimal neutral seventh"}, {11, 7, "undecimal minor sixth"}, 
				{11, 8, "undecimal super fourth"}, {11, 9, "undecimal neutral second"},
			{12, 11, "lesser undecimal neutral second"}, {12, 7, "septimal major sixth"},
			//From 13-numerator and above, only selected intervals are shown, particularly 5-limit and superparticular
			{13, 12, "greater tridecimal 2/3 tone"}, {13, 7, "tridecimal submajor seventh"}, {13, 9, "tridecimal minor fifth"},
				{13, 11, "tridecimal minor third"},
			{14, 13, "lesser tridecimal 2/3 tone"}, {14, 9, "septimal minor sixth"},
			{15, 14, "septimal diatonic semitone"}, {15, 8, "major seventh"},
			{16, 15, "minor second"}, {16, 9, "Pythagorean minor seventh"},
			{17, 16, "just minor semitone"}, {17, 9, "septendecimal major seventh"},
			{18, 13, "tridecimal major fourth"},
			{21, 11, "undecimal major seventh"},
			{22, 21, "undecimal minor semitone"},
			{25, 24, "chroma"}, {25, 16, "augmented fifth"}, {25, 18, "augmented fourth"},
			{27, 16, "Pythagorean major sixth"}, {27, 20, "classic acute fourth"}, {27, 25, "large limma"},
			{32, 21, "septimal super fifth"}, {32, 25, "diminished fourth"}, {32, 27, "Pythagorean minor third"},
			{36, 25, "diminished fifth"},
			{40, 27, "classic grave fifth"},
			{45, 32, "augmented fourth"},
			{48, 25, "diminished octave"},
			{49, 25, "BP eighth"},
			{50, 27, "grave major seventh"},
			{64, 45, "diminished fifth"},
			{75, 64, "augmented second"}, {75, 49, "BP fifth"},
			{81, 80, "syntonic comma"}, {81, 50, "acute minor sixth"}, {81, 64, "Pythagorean major third"},
			{100, 81, "grave major third"},
			{125, 64, "augmented seventh"}, {125, 72, "augmented sixth"}, {125, 81, "narrow augmented fifth"},
			{125, 96, "classical augmented third"}, {125, 108, "augmented second"},
			{128, 75, "diminished seventh"}, {128, 81, "Pythagorean minor sixth"}, {128, 125, "diesis"},
			{135, 128, "major limma"},
			{144, 125, "diminished third"},
			{243, 128, "Pythagorean major seventh"},
			{256, 243, "Pythagorean diatonic semitone"},
			{729, 512, "Pythagorean aug. 4th (tritone)"},
			// some large intervals
			{4, 1, "double octave"}, {8, 1, "triple octave"},
			{16, 1, "quadruple octave"}, {32, 1, "quintuple octave"},
			{3, 1, "tritave"}, 
			{7, 3, "septimal minor tenth"},
			{11, 4, "undecimal super eleventh"},
			{15, 7, "octave + sept. dia. semitone"},
			{32, 15, "minor ninth"},
			{81, 32, "Pythagorean major tenth"},
			{125, 54, "augmented ninth"}
		}
	};
	for (auto& fraction_name : fraction_names)
		if (numerator_ == std::get<0>(fraction_name) && (denominator_ == std::get<1>(fraction_name)))
			return std::string{std::get<2>(fraction_name)};
	return std::string {};
}

} //end namespace BoxyLady
