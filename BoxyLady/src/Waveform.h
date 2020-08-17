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

#ifndef WAVEFORM_H_
#define WAVEFORM_H_

namespace BoxyLady {

class Sequence;

inline constexpr double LongTime { 60.0 },
	MaxSequenceLength { 3600.0 },
	TwoPi = std::atan(1.0) * 8.0;

typedef int16_t music_type;
typedef std::vector<music_type> MusicVector;

class Wave {
private:
	double freq_, amp_, offset_;
public:
	double& freq() {
		return freq_;
	}
	double& amp() {
		return amp_;
	}
	double& offset() {
		return offset_;
	}
	const double& freq() const {
		return freq_;
	}
	const double& amp() const {
		return amp_;
	}
	const double& offset() const {
		return offset_;
	}
	Wave(double freq_val = 0.0, double amp_val = 0.0, double offset_val = 0.0) :
			freq_(freq_val), amp_(amp_val), offset_(offset_val) {
	}
};

class Phaser: public Wave {
	double bend_factor_, bend_time_;
public:
	double& bend_factor() {
		return bend_factor_;
	}
	double& bend_time() {
		return bend_time_;
	}
	const double& bend_factor() const {
		return bend_factor_;
	}
	const double& bend_time() const {
		return bend_time_;
	}
	Phaser(double freq_val = 0.0, double amp_val = 0.0, double offset_val = 0.0,
			double bend_factor_val = 1.0, double bend_time_val = LongTime) :
			Wave(freq_val, amp_val, offset_val), bend_factor_(bend_factor_val), bend_time_(
					bend_time_val) {
	}
};

class Scratcher {
private:
	bool active_;
	Sequence *sequence_;
	std::string name_;
	double amp_, offset_;
	bool loop_;
public:
	std::string& name() {
		return name_;
	}
	Sequence*& sequence() {
		return sequence_;
	}
	double& amp() {
		return amp_;
	}
	double& offset() {
		return offset_;
	}
	bool active() {
		return active_;
	}
	bool loop() {
		return loop_;
	}
	Scratcher(Sequence *source, std::string name_val, double amp_val,
			double offset_val, bool loop_val) :
			active_(true), sequence_(source), name_(name_val), amp_(amp_val), offset_(
					offset_val), loop_(loop_val) {
	}
	Scratcher() :
			active_(false), sequence_(nullptr), name_(""), amp_(0.0), offset_(
					0.0), loop_(false) {
	}
	std::string toString();
};

}

#endif /* WAVEFORM_H_ */
