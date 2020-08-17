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

#ifndef TUNING_H_
#define TUNING_H_

#include <cmath>
#include <string>
#include <iostream>
#include <sstream>
#include <vector>
#include <map>
#include <bitset>

#include "Fraction.h"
#include "Blob.h"
#include "Envelope.h"
#include "Builders.h"

namespace BoxyLady {

class PitchGamut;
class NoteValue;
class NoteDuration;

typedef std::vector<double> DoubleVector;
typedef std::vector<int> IntVector;
typedef std::vector<std::string> StringVector;
typedef std::map<std::string, DoubleVector> DVMap;

NoteDuration BuildNoteDuration(Blob&);

class NoteValue {
private:
	int number_;
	double accidental_;
	int octave_;
public:
	friend class PitchGamut;
	NoteValue(int number, double accidental, int octave) :
			number_(number), accidental_(accidental), octave_(octave) {
	}
	NoteValue() :
			number_(0), accidental_(0.0), octave_(4) {
	}
	void setOctave(int octave) {
		octave_ = octave;
	}
	std::string toString() const {
		std::ostringstream S;
		S << number_ << " " << accidental_ << " " << octave_;
		return S.str();
	}
};

class PitchGamut {
private:
	StringVector note_names_;
	int pitch_classes_n_;
	double repeat_ratio_;
	DoubleVector note_values_;
	DVMap accidentals_;
	DoubleVector pitches_;
	double standard_pitch_;
	PitchGamut& Clear();
	int NearestOctave(const NoteValue, const NoteValue) const;
	DoubleVector& Accidental(std::string);
	double FreqMultFromNote(NoteValue);
	double FreqMultFromRank(int, double);
	PitchGamut& StandardPitch(std::string = "a''''", double = 1.0);
	void StandardAccidentals(double);
	PitchGamut& EqualTemper(double);
	PitchGamut& GeneralNotes(int, DoubleVector, StringVector, double);
	PitchGamut& GeneralET(int, DoubleVector, double, double, StringVector,
			std::string = "a''''", double spf = 1.0);
	PitchGamut& MeantoneNotes(int, int);
	PitchGamut& TwotoneNotes(int, int, int);
	PitchGamut& PorcupineNotes(int, int);
	PitchGamut& ETWestern(int, DoubleVector, double, double = 2.0);
	PitchGamut& Regular(double, int, std::string, double);
	PitchGamut& MeantoneRegular(double, std::string, double);
	PitchGamut& RegularWestern(int, DoubleVector, double, double, int,
			std::string = "c");
	PitchGamut& GeneralRegular(int, DoubleVector, double, StringVector, double,
			int, std::string = "c", double = 2.0, std::string = "a''''",
			double spf = 1.0);
	PitchGamut& Generator(double, double, int, int, int);
	PitchGamut& NormalisePitches();
	PitchGamut& RotatePitches(int);
	PitchGamut& General12(DoubleVector, std::string);
	double PitchIndex(NoteValue);
	double PitchIndex(std::string name) {
		return PitchIndex(Note(name));
	}
	void List1(int);
public:
	PitchGamut() {
		Clear();
	}
	PitchGamut& TuningBlob(Blob&, bool);
	PitchGamut& ParseBlob(Blob&, bool);
	NoteValue Note(std::string);
	NoteValue Note(std::string, const NoteValue);
	NoteValue Offset(NoteValue, int, double, int) const;
	double FreqMultStandard(NoteValue);
	void List(Blob&);
	PitchGamut& TET10();
	PitchGamut& TET12() {
		return MeantoneNotes(2, 1).EqualTemper(2.0).StandardPitch();
	}
	PitchGamut& TET14();
	PitchGamut& TET15() {
		return TwotoneNotes(3, 2, 1).EqualTemper(2.0).StandardPitch();
	}
	PitchGamut& TET19() {
		return MeantoneNotes(3, 2).EqualTemper(2.0).StandardPitch();
	}
	PitchGamut& TET22() {
		return TwotoneNotes(4, 3, 2).EqualTemper(2.0).StandardPitch();
	}
	;
	PitchGamut& TET31() {
		return MeantoneNotes(5, 3).EqualTemper(2.0).StandardPitch();
	}
	PitchGamut& Pelog();
	PitchGamut& Slendro();
	PitchGamut& Pythagorean12(std::string base_note) {
		return MeantoneNotes(2, 1).MeantoneRegular(3.0 / 2.0, base_note, 2.0);
	}
	PitchGamut& QuarterComma12(std::string base_note) {
		return MeantoneNotes(2, 1).MeantoneRegular(pow(5.0, 0.25), base_note,
				2.0);
	}
	PitchGamut& Harrison12(std::string base_note) {
		return MeantoneNotes(2, 1).MeantoneRegular(1.494412, base_note, 2.0);
	}
	PitchGamut& Harmonic12(std::string);
	PitchGamut& Ptolemy12(std::string);
	PitchGamut& BPLambda();
	PitchGamut& WCAlpha_old();
	PitchGamut& WCBeta_old() {
		return MeantoneNotes(3, 2).EqualTemper(pow(1.5, 19.0 / 11.0)).StandardPitch();
	}
	PitchGamut& WCGamma_old();
	PitchGamut& WCAlpha();
	PitchGamut& WCBeta();
	PitchGamut& WCGamma();
};

}

#endif /* TUNING_H_ */
