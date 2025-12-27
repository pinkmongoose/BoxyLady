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

enum class articulation_type {
	amp, staccato, start_slur, stop_slur, glide, env, stereo,
	phaser, scratcher, tremolo, portamento, envelope_compress,
	reverb, d_play, bar, n,
};

class NoteArticulation;

using ArticulationFlags = Flags<articulation_type>;

class NoteDuration {
private:
	float_type d_;
public:
	static inline constexpr float_type Quarter {0.25};
	explicit NoteDuration(std::string);
	explicit NoteDuration(Blob&);
	explicit NoteDuration(float_type x = Quarter) noexcept :
			d_(x) {}
	NoteDuration& operator -= (const NoteDuration& right) {
		d_ -= right.d_;
		if (d_ < 0) throw EError("Can't set a negative note duration.");
		return *this;
	}
	float_type getDuration() const noexcept {
		return d_;
	}
};

class BeatGamut {
private:
	struct Beat {
		std::string articulations;
		NoteDuration duration, width, offset;
	};
	std::map<std::string, Beat> beats_ {};
public:
	BeatGamut& ParseBlob(Blob&, float_type&, bool);
	void List(float_type) const;
	std::string BeatArticulations(float_type time) const;
};

class NoteArticulation {
private:
	ArticulationFlags flags_;
	float_type amp_ {1.0}, staccato_ {1.0}, portamento_time_ {0.0};
	bool start_slur_ {false}, stop_slur_ {false}, glide_ {false}, envelope_compress_ {false}, reverb_ {false}, bar_ {false};
	Envelope envelope_;
	Stereo stereo_;
	Phaser phaser_;
	Scratcher scratcher_;
	Wave tremolo_;
	NoteDuration duration_ {0.0};
public:
	friend class Parser;
	friend class ArticulationGamut;
	static inline constexpr float_type MaxStaccato {10.0};
	explicit NoteArticulation() = default;
	explicit NoteArticulation(std::string input) {
		Blob Q;
		Q.Parse(input);
		Parse(Q);
	}
	void Parse(Blob&);
	void Overwrite(NoteArticulation);
};

class ArticulationGamut {
private:
	std::map<std::string, NoteArticulation> articulations_ {};
	void StandardArticulations();
public:
	NoteArticulation Note(std::string) const;
	NoteArticulation FromString(std::string) const;
	ArticulationGamut& ParseBlob(Blob&, bool);
	ArticulationGamut& Default() {
		articulations_ = {};
		StandardArticulations();
		return *this;
	}
	void List() const;
	static std::string List1(const NoteArticulation, bool = false);
};

class AutoStereo {
private:
	enum class auto_stereo {
		none, ascending, descending, organ
	} mode_ {auto_stereo::none};
	float_type centre_ {1.0}, multiplier_ {0.0}, octave_bands_ {1.0};
public:
	AutoStereo& ParseBlob(Blob&, bool);
	Stereo Apply(float_type);
	std::string toString() const;
};

} //end namespace BoxyLady

#endif /* ARTICULATION_H_ */
