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

#ifndef SEQUENCE_H_
#define SEQUENCE_H_

#include <cmath>
#include <string>
#include <bitset>
#include <vector>

#include "Envelope.h"
#include "Waveform.h"
#include "Global.h"
#include "Random.h"
#include "Stereo.h"
#include "Fourier.h"

namespace BoxyLady {

inline constexpr int CDSampleRate { 44100 }, TelephoneSampleRate { 8000 };
inline constexpr double BigTheta = TwoPi
		* 10000.0, ToleranceTime { 0.001 }, MinSampleRate { 512 },
		MaxSampleRate { 512 * 1024 }, PCMMax_f = static_cast<double>(PCMMax);

extern Darren::Random Rand;

inline double CosTurn(double th) {
	return cos(TwoPi * th);
}

enum class file_format {
	RIFF_wav, boxy, mp3
};
enum class channel_bf {
	none, left, right, both
};
enum class synth_type {
	sine, power, saw, square, triangle, pulse, powertriangle, constant
};
enum class filter_type {
	none, hi, lo, band, amp, distort, ks_blend, ks_reverse, fourier_gain, fourier_bandpass
};
enum class debias_type {
	start, end, mean
};
enum class overlay {
	loop, random, slur_on, slur_off, envelope_compress, resize, n
};
enum class filter_direction {
	mirror, wrap, offset, comb, n
};
enum class seq {
	gate, loop, start_anywhere
};

typedef Flags<filter_direction> FilterFlags;
typedef Flags<overlay> OverlayFlags;

class Sequence;
class MetadataList;
class MetadataPoint;

class SampleType {
	friend class Parser;
	friend class Sequence;
	friend class DicList;
	friend class Dictionary;
private:
	bool loop_, gate_, start_anywhere_;
	int sample_rate_;
	double loop_start_;
	SampleType(bool l, bool g, bool a, int sr, double ls) :
			loop_(l), gate_(g), start_anywhere_(a), sample_rate_(sr), loop_start_(
					ls) {
	}
	SampleType() :
			loop_(false), gate_(true), start_anywhere_(false), sample_rate_(0), loop_start_(
					0) {
	}
};

class MetadataPoint {
	friend class MetadataList;
private:
	std::string key_, mp3_tag_, RIFF_tag_, value_;
public:
	std::string key() {
		return key_;
	}
	std::string mp3_tag() {
		return mp3_tag_;
	}
	std::string RIFF_tag() {
		return RIFF_tag_;
	}
	std::string value() {
		return value_;
	}
};

class MetadataList {
private:
	std::vector<MetadataPoint> metadata_;
public:
	MetadataList();
	std::string& operator[](std::string);
	void Dump() const;
	std::string Mp3CommandUpdate(std::string) const;
	void WriteWavInfo(std::ofstream&) const;
};

class Window {
	double start_, end_;
	bool has_start_, has_end_;
public:
	double& start() {
		return start_;
	}
	double& end() {
		return end_;
	}
	const double& start() const {
		return start_;
	}
	const double& end() const {
		return end_;
	}
	bool hasStart() const {
		return has_start_;
	}
	bool hasEnd() const {
		return has_end_;
	}
	double length() const {
		return end_ - start_;
	}
	void setStart(double start_val) {
		start_ = start_val;
		has_start_ = true;
	}
	void setEnd(double end_val) {
		end_ = end_val;
		has_end_ = true;
	}
	Window(double start_val, double end_val) :
			start_(start_val), end_(end_val), has_start_(true), has_end_(true) {
	}
	Window(double start_val) :
			start_(start_val), end_(0), has_start_(true), has_end_(false) {
	}
	Window() :
			start_(0), end_(0), has_start_(false), has_end_(false) {
	}
};

class Filter {
private:
	filter_type type_;
	double frequency_, bandwidth_, gain_, omega_;
	double low_gain_, low_shoulder_, high_shoulder_, high_gain_;
	FilterFlags flags_;
	Filter& SetGain(double gain_val) {
		gain_ = gain_val;
		return *this;
	}
	Filter& SetOmega(double omega_val, bool mirror, bool wrap) {
		omega_ = omega_val;
		flags_[filter_direction::mirror] = mirror;
		flags_[filter_direction::wrap] = wrap;
		return *this;
	}
	Filter& SetBandPass(double frequency_val, double bandwidth_val,
			double gain_val, bool wrap) {
		frequency_ = frequency_val;
		bandwidth_ = bandwidth_val;
		gain_ = gain_val;
		flags_[filter_direction::wrap] = wrap;
		return *this;
	}
	Filter& SetShoulders(double low_gain, double low_shoulder, double high_shoulder, double high_gain) {
		low_gain_ = low_gain;
		low_shoulder_ = low_shoulder;
		high_shoulder_ = high_shoulder;
		high_gain_ = high_gain;
		return *this;
	}
	Filter(filter_type type_val) :
			type_(type_val), frequency_(0), bandwidth_(0), gain_(0), omega_(0),
			low_gain_(0), low_shoulder_(0), high_shoulder_(0), high_gain_(0),
			flags_(0) {
	}
public:
	FilterFlags flags() const {
		return flags_;
	}
	filter_type type() const {
		return type_;
	}
	double frequency() const {
		return frequency_;
	}
	double bandwidth() const {
		return bandwidth_;
	}
	double gain() const {
		return gain_;
	}
	double omega() const {
		return omega_;
	}
	double low_gain() const {
		return low_gain_;
	}
	double high_gain() const {
		return high_gain_;
	}
	double low_shoulder() const {
		return low_shoulder_;
	}
	double high_shoulder() const {
		return high_shoulder_;
	}
	Filter& SetFlag(filter_direction flag, bool value) {
		flags_[flag] = value;
		return *this;
	}
	bool GetFlag(filter_direction flag) {
		return flags_[flag];
	}
	static Filter LowPass(double omega_val, bool mirror, bool wrap) {
		return Filter(filter_type::lo).SetOmega(omega_val, mirror, wrap);
	}
	static Filter HighPass(double omega_val, bool mirror, bool wrap) {
		return Filter(filter_type::hi).SetOmega(omega_val, mirror, wrap);
	}
	static Filter BandPass(double frequency_val, double bandwith_val,
			double gain_val, bool wrap) {
		return Filter(filter_type::band).SetBandPass(frequency_val,
				bandwith_val, gain_val, wrap);
	}
	static Filter FourierGain(double low_gain, double low_shoulder,
			double high_shoulder, double high_gain) {
		return Filter(filter_type::fourier_gain).SetShoulders(low_gain,
				low_shoulder, high_shoulder, high_gain);
	}
	static Filter FourierBandpass(double frequency_val, double bandwith_val,
			double gain_val) {
		return Filter(filter_type::fourier_bandpass).SetBandPass(frequency_val,
				bandwith_val, gain_val, false);
	}
	static Filter Amp(double gain_val) {
		return Filter(filter_type::amp).SetGain(gain_val);
	}
	static Filter Distort(double gain_val) {
		return Filter(filter_type::distort).SetGain(gain_val);
	}
	static Filter KSBlend(double gain_val) {
		return Filter(filter_type::ks_blend).SetGain(gain_val);
	}
	static Filter KSReverse(double gain_val) {
		return Filter(filter_type::ks_reverse).SetGain(gain_val);
	}
	static Filter BalanceFilters(Filter, Filter, double);
};
typedef std::vector<Filter> FilterVector;

class Sequence {
private:
	MusicVector music_data_;
	int envelope_position_, scratcher_position_;
	double overlay_position_, phaser_position_, tremolo_position_;
	int channels_, sample_rate_, t_samples_, p_samples_, m_samples_,
			loop_start_samples_;
	bool gate_, loop_, start_anywhere_;
	MetadataList metadata_;
	std::pair<int, int> WindowPair(Window) const;
	void WindowFrame(int&, int&) const;
	int Samples(double time) const {
		return static_cast<int>(static_cast<double>(sample_rate_) * time);
	}
	void AssertMusic() const;
	static bool isSimilar(const Sequence&, const Sequence&);
	void Resize(int, int, bool);
	void Cut(int, int);
	music_type Mean(int) const;
	void WindowedOverlay(const Sequence&, Window);
public:
	static double linear_interpolation;
	void Clear();
	Sequence() {
		Clear();
	}
	SampleType getType() const {
		return SampleType(loop_, gate_, start_anywhere_, sample_rate_,
				static_cast<double>(loop_start_samples_)
						/ static_cast<double>(sample_rate_));
	}
	void setType(SampleType T) {
		loop_ = T.loop_;
		gate_ = T.gate_;
		start_anywhere_ = T.start_anywhere_;
		sample_rate_ = T.sample_rate_;
		setLoopStart(T.loop_start_);
	}
	void setGate(bool gate_val) {
		gate_ = gate_val;
	}
	int channels() const {
		return channels_;
	}
	int sample_rate() const {
		return sample_rate_;
	}
	int loop_start_samples() const {
		return loop_start_samples_;
	}
	int t_samples() const {
		return t_samples_;
	}
	int p_samples() const {
		return p_samples_;
	}
	void setLoopStart(double d) {
		loop_start_samples_ = d * static_cast<double>(sample_rate_);
		if (loop_start_samples_ > p_samples_)
			loop_start_samples_ = p_samples_;
		if (loop_start_samples_)
			loop_ = true;
	}
	double Seconds(int samples) const {
		return (sample_rate_) ?
				(static_cast<double>(samples)
						/ static_cast<double>(sample_rate_)) :
				0;
	}
	double get_tSeconds() const {
		return Seconds(t_samples_);
	}
	void set_tSeconds(double time) {
		t_samples_ = Samples(time);
	}
	double get_pSeconds() const {
		return Seconds(p_samples_);
	}
	double get_mSeconds() const {
		return Seconds(m_samples_);
	}
	double MusicDataSize() const {
		return static_cast<double>(p_samples_ * channels_ * 2)
				/ static_cast<double>(1 << 20);
	}
	MetadataList& metadata() {
		return metadata_;
	}
	void LoadFromFile(std::string, file_format = file_format::RIFF_wav);
	void CreateSilenceSeconds(int, int, double, double);
	void CreateSilenceSamples(int, int, int, int);
	void MakeSilent() {
		AssertMusic();
		for (auto &i : music_data_)
			i = 0;
	}
	void SaveToFile(std::string, file_format, bool) const;
	void CopyType(const Sequence&);
	void Combine(const Sequence&, const Sequence&);
	void Mix(const Sequence&, const Sequence&, Stereo, Stereo, int);
	void Split(Sequence&, Sequence&) const;
	void Rechannel(int);
	void Overlay(const Sequence&, int = 0, int = INT_MAX, double = 1.0,
			OverlayFlags = 0, Stereo = Stereo(), Phaser = Phaser(), Envelope =
					Envelope(), Scratcher = Scratcher(), Wave = Wave());
	void Overlay(const Sequence&, Window = Window(), double = 1.0,
			OverlayFlags = 0, Stereo = Stereo(), Phaser = Phaser(), Envelope =
					Envelope(), Scratcher = Scratcher(), Wave = Wave());
	void Resize(double t_time, double p_time, bool rel) {
		Resize(Samples(t_time), Samples(p_time), rel);
	}
	void AutoResize(double);
	void Paste(const Sequence&, Window);
	void Cut(Window);
	void Defrag() {
		music_data_.resize(p_samples_ * channels_);
	}
	void Amp(double amp) {
		CrossFade(Fader::Amp(amp));
	}
	void CrossFade(Fader);
	void DelayAmp(Stereo, Stereo, double);
	void Reverse();
	void Echo(double, double, int, FilterVector, bool);
	void Waveform(const Wave, const Phaser, const Wave, double, synth_type,
			Stereo = Stereo());
	void OffsetSeconds(double, double, bool);
	void WhiteNoise(double, Stereo = Stereo());
	void CrackleNoise(double, Stereo = Stereo());
	void Smatter(Sequence&, double, double, double, bool, double, double, bool,
			double, double);
	void ApplyEnvelope(Envelope, bool = false);
	void Distort(double power) {
		Ring(nullptr, Wave(), true, 0.0, power);
	}
	void Chorus(int, double, const Wave);
	void Ring(Sequence*, Wave, bool, double, double);
	void Flange(double, double);
	void BitCrusher(int);
	void ApplyFilter(Filter);
	void ApplyFilters(FilterVector filters) {
		for (auto filter : filters)
			ApplyFilter(filter);
	}
	void WindowedFilterLayer(Filter, Envelope, Sequence&, Window);
	void WindowedFilter(Filter, Filter, int);
	void LowPass(double);
	void HighPass(double);
	void BandPass(double, double, double);
	void FourierGain(double, double, double, double);
	void FourierBandpass(double, double, double, bool);
	void FourierShift(double);
	void FourierClean(double);
	void Integrate(double, double, double);
	void Clip(double, double);
	void Abs(double = 1.0);
	void Fold(double);
	void Octave(double);
	void Repeat(int, FilterVector = FilterVector());
	void Plot() const;
	void Histogram(bool, bool, double);
	void AutoAmp() {
		Histogram(true, false, 0.0);
	}
	void Debias(debias_type);
};

}

#endif /* SEQUENCE_H_ */
