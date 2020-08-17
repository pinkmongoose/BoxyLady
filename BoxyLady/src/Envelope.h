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

#ifndef ENVELOPE_H_
#define ENVELOPE_H_

#include <string>

namespace BoxyLady {

class Blob;

class Envelope {
	friend Envelope BuildEnvelope(Blob&);
private:
	int hold_start_, decay_start_, sustain_start_, fade_start_, fade_end_,
			attack_samples_, hold_samples_, sustain_samples_, decay_samples_,
			fade_samples_, gate_samples_;
	double attack_time_, attack_amp_, hold_time_, hold_amp_, decay_time_,
			decay_amp_, sustain_time_, sustain_amp_, fade_time_;
	bool active_;
public:
	static double gate_time;
	Envelope() :
			hold_start_(0), decay_start_(0), sustain_start_(0), fade_start_(0),
			fade_end_(0), attack_samples_(0), hold_samples_(0), sustain_samples_(0),
			decay_samples_(0), fade_samples_(0), gate_samples_(0), attack_time_(0),
			attack_amp_(0), hold_time_(0), hold_amp_(0), decay_time_(0),
			decay_amp_(0), sustain_time_(0), sustain_amp_(0), fade_time_(0), active_(false) {}
	void Prepare(int);
	void Squish(int);
	double Amp(int);
	double Amp(int, int);
	std::string toString();
	bool active() {
		return active_;
	}
	static Envelope TriangularWindow(double, double, double);
};

}

#endif /* ENVELOPE_H_ */
