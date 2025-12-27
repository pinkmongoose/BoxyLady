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

inline constexpr int PCMMax {32767}, PCMMin {-32768}, PCMRange {PCMMax- PCMMin + 1};
inline constexpr float_type PCMMax_f = static_cast<float_type>(PCMMax);
inline constexpr int StereoChannels {2}, SingleChannel {1}, MaxChannels {2};

extern Darren::Random<float_type> Rand;

enum class file_format {
	RIFF_wav, boxy, mp3
};
enum class synth_type {
	sine, power, saw, square, triangle, pulse, powertriangle, constant
};
enum class filter_type {
	none, hi, lo, band, amp, distort, ks_blend, ks_reverse,
	fourier_gain, fourier_bandpass, fourier_clean, fourier_cleanpass, fourier_limit,
	narrow_stereo, pitch_scale, inverse_lr
};
enum class overlay {
	loop, random, slur_on, slur_off, envelope_compress, resize, trim, gate, n
};
enum class filter_direction {
	wrap, offset, comb, n
};

using FilterFlags = Flags<filter_direction>;
using OverlayFlags = Flags<overlay>;

class Sound;
class MetadataList;
class MetadataPoint;

struct SampleType {
	bool loop {false}, start_anywhere {false};
	music_size sample_rate {0};
	float_type loop_start {0.0};
};

struct MetadataPoint {
	std::string mp3_tag {""}, RIFF_tag {""}, value {""};
};

class MetadataList {
private:
	std::map<std::string, MetadataPoint> metadata_;
public:
	explicit MetadataList();
	std::string& operator[](std::string);
	void Dump(bool = false) const;
	std::string Mp3CommandUpdate(std::string) const;
	void WriteWavInfo(std::ofstream&) const;
	void EditListItem(std::string, std::string, std::string, std::string = "");
};

class Window {
	float_type start_ {0.0}, end_ {0.0};
	bool has_start_ {false}, has_end_ {false};
public:
	float_type& start() noexcept {return start_;}
	float_type& end() noexcept {return end_;}
	const float_type& start() const noexcept {return start_;}
	const float_type& end() const noexcept {return end_;}
	bool hasStart() const noexcept {return has_start_;}
	bool hasEnd() const noexcept {return has_end_;}
	float_type length() const noexcept {
		return end_ - start_;
	}
	void setStart(float_type start_val) noexcept {
		start_ = start_val;
		has_start_ = true;
	}
	void setEnd(float_type end_val) noexcept {
		end_ = end_val;
		has_end_ = true;
	}
	explicit Window(float_type start_val, float_type end_val) noexcept :
			start_{start_val}, end_{end_val}, has_start_{true}, has_end_{true} {}
	explicit Window(float_type start_val) noexcept :
			start_{start_val}, end_{0.0}, has_start_{true}, has_end_{false} {}
	explicit Window() = default;
};

class Filter {
private:
	filter_type type_;
	float_type frequency_, bandwidth_, gain_, omega_;
	float_type low_gain_, low_shoulder_, high_shoulder_, high_gain_;
	FilterFlags flags_;
	Filter& SetGain(float_type gain_val) noexcept {
		gain_ = gain_val;
		return *this;
	}
	Filter& SetOmega(float_type omega_val, bool wrap) {
		omega_ = omega_val;
		flags_.Set({{filter_direction::wrap, wrap}});
		return *this;
	}
	Filter& SetBandPass(float_type frequency_val, float_type bandwidth_val, float_type gain_val, bool wrap) {
		frequency_ = frequency_val;
		bandwidth_ = bandwidth_val;
		gain_ = gain_val;
		flags_[filter_direction::wrap] = wrap;
		return *this;
	}
	Filter& SetShoulders(float_type low_gain, float_type low_shoulder, float_type high_shoulder, float_type high_gain) noexcept {
		low_gain_ = low_gain;
		low_shoulder_ = low_shoulder;
		high_shoulder_ = high_shoulder;
		high_gain_ = high_gain;
		return *this;
	}
	explicit Filter(filter_type type_val) noexcept :
		type_{type_val}, frequency_{0.0}, bandwidth_{0.0}, gain_{0.0}, omega_{0.0},
		low_gain_{0.0}, low_shoulder_{0.0}, high_shoulder_{0.0}, high_gain_{0.0},
		flags_{0} {}
public:
	FilterFlags flags() const {return flags_;}
	filter_type type() const noexcept {return type_;}
	float_type frequency() const noexcept {return frequency_;}
	float_type bandwidth() const noexcept {return bandwidth_;}
	float_type gain() const noexcept {return gain_;}
	float_type omega() const noexcept {return omega_;}
	float_type low_gain() const noexcept {return low_gain_;}
	float_type high_gain() const noexcept {return high_gain_;}
	float_type low_shoulder() const noexcept {return low_shoulder_;}
	float_type high_shoulder() const noexcept {return high_shoulder_;}
	Filter& SetFlag(filter_direction flag, bool value) {
		flags_[flag] = value;
		return *this;
	}
	bool GetFlag(filter_direction flag) const {
		return flags_[flag];
	}
	static Filter LowPass(float_type omega_val, bool wrap) {
		return Filter(filter_type::lo).SetOmega(omega_val, wrap);
	}
	static Filter HighPass(float_type omega_val, bool wrap) {
		return Filter(filter_type::hi).SetOmega(omega_val, wrap);
	}
	static Filter BandPass(float_type frequency_val, float_type bandwith_val, float_type gain_val, bool wrap) {
		return Filter(filter_type::band).SetBandPass(frequency_val, bandwith_val, gain_val, wrap);
	}
	static Filter FourierGain(float_type low_gain, float_type low_shoulder, float_type high_shoulder, float_type high_gain) noexcept {
		return Filter(filter_type::fourier_gain).SetShoulders(low_gain, low_shoulder, high_shoulder, high_gain);
	}
	static Filter FourierBandpass(float_type frequency_val, float_type bandwith_val, float_type gain_val) {
		return Filter(filter_type::fourier_bandpass).SetBandPass(frequency_val, bandwith_val, gain_val, false);
	}
	static Filter FourierClean(float_type clean_level) noexcept {
		return Filter(filter_type::fourier_clean).SetGain(clean_level);
	}
	static Filter FourierCleanPass(float_type clean_level) noexcept {
		return Filter(filter_type::fourier_cleanpass).SetGain(clean_level);
	}
	static Filter FourierLimit(float_type clean_level) noexcept {
		return Filter(filter_type::fourier_limit).SetGain(clean_level);
	}
	static Filter PitchScale(float_type f) noexcept {
		return Filter(filter_type::pitch_scale).SetGain(f);
	}
	static Filter Amp(float_type gain_val) noexcept {
		return Filter(filter_type::amp).SetGain(gain_val);
	}
	static Filter Distort(float_type gain_val) noexcept {
		return Filter(filter_type::distort).SetGain(gain_val);
	}
	static Filter KSBlend(float_type gain_val) noexcept {
		return Filter(filter_type::ks_blend).SetGain(gain_val);
	}
	static Filter KSReverse(float_type gain_val) noexcept {
		return Filter(filter_type::ks_reverse).SetGain(gain_val);
	}
	static Filter NarrowStereo(float_type gain_val) noexcept {
		return Filter(filter_type::narrow_stereo).SetGain(gain_val);
	}
	static Filter InverseLR() noexcept {
		return Filter(filter_type::inverse_lr);
	}
	static Filter BalanceFilters(Filter, Filter, float_type);
};
using FilterVector = std::vector<Filter>;

class Sound {
private:
	MusicVector music_data_;
	music_size envelope_position_, scratcher_position_;
	float_type overlay_position_, phaser_position_, tremolo_position_;
	int channels_;
	music_size sample_rate_, t_samples_, p_samples_, m_samples_, loop_start_samples_;
	bool loop_, start_anywhere_;
	MetadataList metadata_;
	std::pair<music_pos, music_pos> WindowPair(Window) const noexcept;
	void WindowFrame(music_pos&, music_pos&) const noexcept;
	music_size Samples(float_type time) const noexcept {
		return static_cast<music_size>(static_cast<float_type>(sample_rate_) * time);
	}
	static bool isSimilar(const Sound&, const Sound&) noexcept;
	void Resize(music_size, music_size, bool);
	void Cut(music_pos, music_pos);
	music_type Mean(int) const;
	void Overlay(const Sound&, music_pos = 0, music_pos = music_pos_max, float_type = 1.0,
		OverlayFlags = OverlayFlags{0}, Stereo = Stereo(), Phaser = Phaser(), Envelope =
			Envelope(), Scratcher = Scratcher(), Wave = Wave(), float_type = 0.0);
	void WindowedOverlay(const Sound&, Window);
	class Overlayer {
	private:		
		const Sound& source_;
		Sound& destination_;
		music_pos start_, stop_;
		float_type pitch_factor_;
		OverlayFlags flags_;
		Stereo stereo_;
		Phaser phaser_;
		Envelope envelope_;
		Scratcher scratcher_;
		Wave tremolo_;
		float_type gate_;
	public:
		explicit Overlayer(const Sound& source, Sound& destination) noexcept : source_{source}, destination_{destination},
			start_{0}, stop_{music_pos_max}, pitch_factor_{1.0}, flags_{0}, stereo_{}, phaser_{}, envelope_{}, scratcher_{}, tremolo_{}, gate_{0.0} {};
		void operator () () {
			destination_.Overlay(source_, start_, stop_, pitch_factor_, flags_, stereo_, phaser_, envelope_, scratcher_, tremolo_, gate_);
		}
		Overlayer& start(music_pos start) noexcept {start_ = start; return *this;}
		Overlayer& stop(music_pos stop) noexcept {stop_ = stop; return *this;}
		Overlayer& window(Window window) noexcept {
			auto [start, stop] {destination_.WindowPair(window)};
			start_ = start; stop_ = stop;
			return *this;
		}
		Overlayer& pitch_factor(float_type pitch_factor) noexcept {pitch_factor_ = pitch_factor; return *this;}
		Overlayer& flags(OverlayFlags flags) {flags_ = flags; return *this;}
		Overlayer& stereo(Stereo stereo) noexcept {stereo_ = stereo; return *this;}
		Overlayer& phaser(Phaser phaser) noexcept {phaser_ = phaser; return *this;}
		Overlayer& envelope(Envelope envelope) noexcept {envelope_ = envelope; return *this;}
		Overlayer& scratcher(Scratcher scratcher) noexcept {scratcher_ = scratcher; return *this;}
		Overlayer& tremolo(Wave tremolo) noexcept {tremolo_ = tremolo; return *this;}
		Overlayer& gate(float_type gate) {
			flags_[overlay::gate] = (gate_ = gate) != 0.0;
			return *this;
		}
	};
public:
	enum class debias_type {start, end, mean};
	static inline constexpr int MinSampleRate {512}, MaxSampleRate {512 * 1024};
	static inline constexpr music_size CDSampleRate {44100}, DVDSampleRate {48000}, TelephoneSampleRate {8000},
		AmigaSampleRate {14065};
	static float_type linear_interpolation;
	static MetadataList default_metadata_;
	void Clear();
	explicit Sound() {
		Clear();
	}
	void AssertMusic() const;
	Overlayer DoOverlay(const Sound& source) {
		return Overlayer {source, *this};
	}
	SampleType getType() const noexcept {
		return SampleType(loop_, start_anywhere_, sample_rate_,
			static_cast<float_type>(loop_start_samples_) / static_cast<float_type>(sample_rate_));
	}
	void setType(SampleType T) noexcept {
		loop_ = T.loop;
		start_anywhere_ = T.start_anywhere;
		sample_rate_ = T.sample_rate;
		setLoopStart(T.loop_start);
	}
	int channels() const noexcept {return channels_;}
	music_size sample_rate() const noexcept {return sample_rate_;}
	music_size loop_start_samples() const noexcept {return loop_start_samples_;}
	music_size t_samples() const noexcept {return t_samples_;}
	music_size p_samples() const noexcept {return p_samples_;}
	void setLoopStart(float_type d) noexcept {
		if (d > 1.0) loop_start_samples_ = d;
		//else loop_start_samples_ = d * static_cast<float_type>(sample_rate_);
		else loop_start_samples_ = d * static_cast<float_type>(p_samples_);
		if (loop_start_samples_ >= p_samples_)
			loop_start_samples_ = p_samples_-1;
		if (loop_start_samples_)
			loop_ = true;
	}
	float_type Seconds(music_size samples) const noexcept {
		return (sample_rate_ != 0) ? (static_cast<float_type>(samples) / static_cast<float_type>(sample_rate_)) : 0;
	}
	float_type get_tSeconds() const noexcept {
		return Seconds(t_samples_);
	}
	void set_tSeconds(float_type time) {
		if (p_samples_ < Samples(time)) Resize(time, time, false);
		else t_samples_ = Samples(time);
	}
	float_type get_pSeconds() const noexcept {
		return Seconds(p_samples_);
	}
	float_type get_mSeconds() const noexcept {
		return Seconds(m_samples_);
	}
	float_type MusicDataSize() const noexcept {
		return static_cast<float_type>(p_samples_ * channels_ * 2) / static_cast<float_type>(1 << 20);
	}
	MetadataList& metadata() noexcept {return metadata_;}
	void LoadFromFile(std::string, file_format = file_format::RIFF_wav, bool = false);
	void CreateSilenceSeconds(int, music_size, float_type, float_type);
	void CreateSilenceSamples(int, music_size, music_size, music_size);
	void MakeSilent() {
		AssertMusic();
		std::fill(music_data_.begin(), music_data_.end(), 0);
	}
	void SaveToFile(std::string, file_format, bool) const;
	void CopyType(const Sound&) noexcept;
	void Combine(const Sound&, const Sound&);
	void Mix(const Sound&, const Sound&, Stereo, Stereo, int);
	void Split(Sound&, Sound&) const;
	void Rechannel(int);
	void Resize(float_type t_time, float_type p_time, bool rel) {
		Resize(Samples(t_time), Samples(p_time), rel);
	}
	void AutoResize(float_type);
	void Paste(const Sound&, Window);
	void Cut(Window);
	void Defrag() {
		Resize(t_samples_, p_samples_, false);
	}
	void Amp(float_type amp) {
		CrossFade(CrossFader::Amp(amp));
	}
	void CrossFade(CrossFader);
	void DelayAmp(Stereo, Stereo, float_type);
	void Reverse();
	void EchoEffect(float_type, float_type, int, FilterVector, bool);
	void Waveform(const Wave, const Phaser, const Wave, float_type, synth_type, Stereo = Stereo());
	void OffsetSeconds(float_type, float_type, bool);
	void WhiteNoise(float_type, Stereo = Stereo());
	void RedNoise(float_type, Stereo = Stereo());
	void VelvetNoise(float_type, float_type, Stereo = Stereo());
	void CrackleNoise(float_type, Stereo = Stereo());
	void Smatter(Sound&, float_type, float_type, float_type, bool, float_type, float_type, bool,
			float_type, float_type, bool, bool);
	void ApplyEnvelope(Envelope, bool = false, float_type = 0.0);
	void Distort(float_type power) {
		Ring(std::nullopt, Wave(), true, 0.0, power);
	}
	void Chorus(int, float_type, const Wave);
	void Ring(OptRef<Sound>, Wave, bool, float_type, float_type);
	void Flange(float_type, float_type);
	void BitCrusher(int);
	void ApplyFilter(Filter);
	void ApplyFilters(FilterVector filters) {
		for (auto filter : filters) ApplyFilter(filter);
	}
	void WindowedFilterLayer(Filter, Envelope, Sound&, Window);
	void WindowedFilter(Filter, Filter, int);
	void LowPass(float_type);
	void HighPass(float_type);
	void BandPass(float_type, float_type, float_type);
	void FourierSplit(std::function<void (Fourier&)>);
	void FourierGain(float_type, float_type, float_type, float_type);
	void FourierBandpass(float_type, float_type, float_type, bool);
	void FourierShift(float_type);
	void FourierClean(float_type, bool, bool);
	void FourierScale(float_type);
	void PitchScale(float_type);
	void MonoPitchScale(float_type);
	void FourierPower(float_type);
	void Integrate(float_type, float_type, float_type);
	void Clip(float_type, float_type);
	void Abs(float_type = 1.0);
	void Fold(float_type);
	void Octave(float_type);
	void Repeat(int, FilterVector = FilterVector());
	void CorrelPlot() const;
	void Plot() const;
	void Histogram(bool, bool, float_type);
	void AutoAmp() {
		Histogram(true, false, 0.0);
	}
	void Debias(debias_type);
};

} //end namespace BoxyLady

#endif /* SEQUENCE_H_ */
