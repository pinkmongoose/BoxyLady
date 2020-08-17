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

#ifndef ARTICULATION_H_
#define ARTICULATION_H_

#include <string>
#include <map>

#include "Blob.h"
#include "Stereo.h"
#include "Envelope.h"
#include "Global.h"
#include "Waveform.h"

namespace BoxyLady {

inline constexpr double MaxStaccato { 10.0 }, Quarter { 0.25 };

enum class articulation_type {
	amp, staccato, start_slur, stop_slur, glide, env, stereo,
	phaser, scratcher, tremolo, portamento, envelope_compress,
	reverb, d_play, bar, n,
};

class PitchGamut;
class NoteArticulation;
class ArticulationGamut;
class Beat;
class BeatGamut;
class NoteValue;
class NoteDuration;

typedef std::map<std::string, NoteArticulation> NoteArticulationMap;
typedef Flags<articulation_type> ArticulationFlags;
typedef std::map<std::string, Beat> BeatMap;

class NoteDuration {
private:
	double d_;
public:
	NoteDuration(std::string);
	NoteDuration(double x = Quarter) :
			d_(x) {
	}
	double getDuration() const {
		return d_;
	}
};

struct Beat {
	std::string articulations;
	NoteDuration duration, width, offset;
};

class BeatGamut {
private:
	BeatMap beats_;
	Beat& getBeat(std::string);
public:
	BeatGamut& Clear() {
		beats_.clear();
		return *this;
	}
	void List1(const Beat) const;
	BeatGamut() {
		Clear();
	}
	BeatGamut& ParseBlob(Blob&, double&, bool);
	void List(double) const;
	BeatMap& getBeats() {
		return beats_;
	}
};

class NoteArticulation {
private:
	ArticulationFlags flags_;
	double amp_, staccato_, portamento_time_;
	bool start_slur_, stop_slur_, glide_, envelope_compress_, reverb_, bar_;
	Envelope envelope_;
	Stereo stereo_;
	Phaser phaser_;
	Scratcher scratcher_;
	Wave tremolo_;
	NoteDuration duration_;
	template<typename T>
	void Overwrite1(T&, T&, NoteArticulation&, articulation_type);
public:
	friend class Parser;
	friend class ArticulationGamut;
	void Clear();
	NoteArticulation() {
		Clear();
	}
	NoteArticulation(std::string input) {
		Blob Q;
		Q.Parse(input);
		Parse(Q);
	}
	void Parse(Blob&);
	void Overwrite(NoteArticulation);
};

class ArticulationGamut {
private:
	NoteArticulationMap articulations_;
	ArticulationGamut& Clear() {
		articulations_.clear();
		return *this;
	}
	void StandardArticulations();
	NoteArticulation& Articulation(std::string);
public:
	NoteArticulation Note(std::string);
	ArticulationGamut() {
		Clear();
	}
	ArticulationGamut& ParseBlob(Blob&, bool);
	ArticulationGamut& Default() {
		Clear();
		StandardArticulations();
		return *this;
	}
	void List() const;
	static void List1(const NoteArticulation, bool = false);
};

}

#endif /* ARTICULATION_H_ */
