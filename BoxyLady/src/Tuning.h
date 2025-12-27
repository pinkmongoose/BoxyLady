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

#ifndef TUNING_H_
#define TUNING_H_

#include <cmath>
#include <string>
#include <iostream>
#include <sstream>
#include <vector>
#include <map>
#include <bitset>

#include "Blob.h"

namespace BoxyLady {

class PitchGamut;
class NoteDuration;

using FloatVector = std::vector<float_type>;
using IntVector = std::vector<int>;
using StringVector = std::vector<std::string>;
using FloatVectorMap = std::map<std::string, FloatVector>;

class NoteValue {
private:
	int number_ {0};
	float_type accidental_ {0.0};
	int octave_ {4};
public:
	friend class PitchGamut;
	NoteValue(int number, float_type accidental, int octave) noexcept :
			number_{number}, accidental_{accidental}, octave_{octave} {
	}
	NoteValue() = default;
	void setOctave(int octave) noexcept {
		octave_ = octave;
	}
	std::string toString() const {
		std::ostringstream stream;
		std::print(stream, "{} {} {}", number_, accidental_, octave_);
		return stream.str();
	}
};

class PitchGamut {
private:
	StringVector note_names_ {};
	size_t pitch_classes_n_ {0};
	float_type repeat_ratio_ {2.0}, standard_pitch_ {1.0};
	FloatVector note_values_ {}, pitches_ {}, key_signature_ {};
	FloatVectorMap accidentals_ {};
	PitchGamut& Clear();
	int NearestOctave(const NoteValue, const NoteValue) const;
	FloatVector& Accidental(std::string);
	float_type FreqMultFromRank(int, float_type);
	PitchGamut& StandardPitch(std::string = "a''''", float_type = 1.0);
	void StandardAccidentals(float_type);
	PitchGamut& EqualTemper(float_type);
	PitchGamut& GeneralNotes(size_t, FloatVector, StringVector, float_type);
	PitchGamut& GeneralET(size_t, FloatVector, float_type, float_type, StringVector, std::string = "a''''", float_type spf = 1.0);
	PitchGamut& MeantoneNotes(size_t, size_t);
	PitchGamut& TwotoneNotes(size_t, size_t, size_t);
	PitchGamut& ETWestern(size_t, FloatVector, float_type, float_type = 2.0);
	PitchGamut& Regular(float_type, int, std::string, float_type);
	PitchGamut& MeantoneRegular(float_type, std::string, float_type);
	PitchGamut& RegularWestern(size_t, FloatVector, float_type, float_type, int, std::string = "c");
	PitchGamut& GeneralRegular(size_t, FloatVector, float_type, StringVector, float_type,
			int, std::string = "c", float_type = 2.0, std::string = "a''''", float_type spf = 1.0);
	PitchGamut& Generator(float_type, float_type, int, int, int);
	PitchGamut& NormalisePitches();
	PitchGamut& RotatePitches(int);
	PitchGamut& General12(FloatVector, std::string);
	float_type PitchIndex(NoteValue);
	float_type PitchIndex(std::string name) {
		return PitchIndex(NoteAbsolute(name));
	}
public:
	PitchGamut& TuningBlob(Blob&, bool);
	PitchGamut& ParseBlob(Blob&, bool);
	NoteValue NoteAbsolute(std::string);
	NoteValue NoteRelative(std::string, const NoteValue);
	NoteValue Offset(NoteValue, int, float_type, int) const;
	float_type FreqMultFromNote(NoteValue);
	float_type FreqMultStandard(NoteValue);
	void List(Blob&);
	PitchGamut& TET10();
	PitchGamut& TET12() {
		return MeantoneNotes(2, 1).EqualTemper(2.0).StandardPitch();
	}
	PitchGamut& TET14();
	PitchGamut& Pelog();
	PitchGamut& Slendro();
	PitchGamut& Harmonic12(std::string);
	PitchGamut& Ptolemy12(std::string);
	PitchGamut& BPLambda();
	PitchGamut& WCAlpha();
	PitchGamut& WCBeta();
	PitchGamut& WCGamma();
};

} //end namespace BoxyLady

#endif /* TUNING_H_ */
