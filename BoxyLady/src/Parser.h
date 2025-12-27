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

#ifndef PARSER_H_
#define PARSER_H_

#include <vector>

#include "Global.h"
#include "Sound.h"
#include "Blob.h"
#include "Dictionary.h"
#include "Envelope.h"
#include "Tuning.h"
#include "Articulation.h"
#include "Builders.h"

namespace BoxyLady {

enum class context_mode {nomode, seq, chord};
enum class t_mode {tempo, time};
enum class verbosity_type {none, errors, messages, verbose};

class Slider {
private:
	float_type stop_ {0.0}, rate_ {0.0};
	bool active_ {false};
public:
	explicit Slider() = default;
	void Build(Blob&, float_type, bool = false);
	explicit Slider(Blob& blob, float_type now) {
		Build(blob, now);
	}
	void Update(float_type, float_type, float_type&) noexcept;
	void Update(float_type, float_type, Stereo&);
	float_type getRate() const noexcept {
		return rate_;
	}
	float_type getStop() const noexcept {
		return stop_;
	}
	std::string toString(float_type) const;
};

class AmpAdjust {
public:
	bool active_ {false};
	float_type exponent_ {1.0}, standard_ {1.0};
	explicit AmpAdjust() = default;
	explicit AmpAdjust(float_type exponent, float_type standard) noexcept :
			active_{true}, exponent_{exponent}, standard_{standard} {
	}
	float_type Amplitude(float_type frequency) const noexcept {
		return active_ ? pow(standard_ / frequency, exponent_) : 1.0;
	}
};

class ParseParams {
	friend class Parser;
private:
	PitchGamut gamut_ {PitchGamut().TET12()};
	ArticulationGamut articulation_gamut_ {ArticulationGamut().Default()};
	NoteArticulation articulation_;
	BeatGamut beat_gamut_;
	AmpAdjust amp_adjust_;
	float_type tempo_ {120.0}, transpose_ {1.0}, arpeggio_ {0.0};
	NoteValue last_note_;
	NoteDuration current_duration_;
	std::string instrument_ {""}, post_process_ {""};
	float_type amp_ {1.0}, amp2_ {1.0}, last_frequency_multiplier_ {1.0}, beat_time_ {0.0}, gate_ {0.002};
	float_type precision_amp_ {0.0}, precision_pitch_ {0.0}, precision_time_ {0.0}, offset_time_ {0.0}, fidato_ {1.0};
	context_mode mode_ {context_mode::nomode};
	t_mode tempo_mode_ {t_mode::tempo};
	bool ignore_pitch_ {false}, slur_ {false}, bar_check_ {false};
	Slider rall_, cresc_, salendo_, pan_, staccando_, fidando_;
	AutoStereo auto_stereo_;
public:
	float_type TimeDuration(NoteDuration, float_type) const;
};

class Parser {
private:
	enum class parse_exit {exit, end, error};
	ParseParams params_;
	bool supervisor_, portable_, echo_shell_;
	Dictionary dictionary_;
	std::string mp3_encoder_, file_play_, terminal_, ls_;
	static verbosity_type verbosity_;
	class VerbosityScope {
	private:
		verbosity_type stacked_verbosity_;
	public:
		explicit VerbosityScope() : stacked_verbosity_{verbosity_ } {}
		~VerbosityScope() {verbosity_ = stacked_verbosity_;}
		VerbosityScope(const VerbosityScope&) = delete;
    	VerbosityScope& operator=(const VerbosityScope&) = delete;
	};
	music_size default_sample_rate_, instrument_sample_rate_;
	float_type instrument_duration_, max_instrument_duration_, instrument_frequency_multiplier_, standard_pitch_;
	void CheckSystem();
	Window BuildWindow(Blob&) const;
	Filter BuildFilter(Blob&) const;
	FilterVector BuildFilters(Blob&) const;
	SampleType BuildSampleType(Blob&, SampleType = SampleType());
	verbosity_type BuildVerbosity(std::string);
	dic_item_protection BuildProtection(std::string);
	void Rename(Blob&);
	int ExternalCommand(Blob&) const;
	int ExternalTerminal(Blob&) const;
	void GetWD() const;
	void SetWD(Blob&) const;
	int Ls(Blob&) const;
	void ReadSound(Blob&);
	void WriteSound(Blob&);
	void Mp3Encode(std::string, std::string, MetadataList);
	void Metadata(Blob&);
	void DefaultMetadata(Blob&);
	int PlayFile(std::string, std::string);
	void CommandReplaceString(std::string&, std::string, std::string, std::string);
	void PlayEntry(Blob&);
	void ExternalProcessing(Blob&);
	void ShowPrint(Blob&);
	void ShowConfig(Blob&);
	void LoadLibrary(std::string, verbosity_type, bool builtin);
	parse_exit ParseImmediate();
	parse_exit ParseBlobs(Blob&);
	void ParseConfig(Blob&);
	void Clone(Blob&);
	void Combine(Blob&);
	void Mix(Blob&);
	void Split(Blob&);
	void Rechannel(Blob&);
	void Cut(Blob&);
	void Paste(Blob&);
	void ShowState(Blob&, ParseParams, float_type);
	void Histogram(Blob&);
	void CorrelationPlot(Blob&);
	void Fade(Blob&);
	void CrossFade(Blob&);
	void Balance(Blob&);
	void EchoEffect(Blob&);
	void Reverse(Blob& blob) {
		dictionary_.FindSound(blob).Reverse();
	}
	void Tremolo(Blob&);
	void Resize(Blob&);
	void Create(Blob&);
	void Instrument(Blob&);
	void KarplusStrong(Blob&);
	void Chowning(Blob&);
	void Modulator(Blob&, OptRef<Sound> = std::nullopt);
	void Synth(Blob&, OptRef<Sound> = std::nullopt);
	void Smatter(Sound&, Blob&);
	void Sines(Sound&, Blob&, float_type);
	void Defrag();
	void SetAccess(Blob&);
	void Delete(Blob&);
	void ApplyEnvelope(Blob&);
	void Distort(Blob&);
	void Chorus(Blob&);
	void Offset(Blob&);
	void RingModulation(Blob&);
	void Flange(Blob&);
	void BitCrusher(Blob&);
	void LowPass(Blob& blob) {
		dictionary_.FindSound(blob).ApplyFilter(BuildLowPass(blob));
	}
	void HighPass(Blob& blob) {
		dictionary_.FindSound(blob).ApplyFilter(BuildHighPass(blob));
	}
	void BandPass(Blob& blob) {
		dictionary_.FindSound(blob).ApplyFilter(BuildBandPass(blob));
	}
	void FourierGain(Blob& blob) {
		dictionary_.FindSound(blob).ApplyFilter(BuildFourierGain(blob));
	}
	void FourierBandpass(Blob& blob) {
		dictionary_.FindSound(blob).ApplyFilter(BuildFourierBandpass(blob));
	}
	void PitchScale(Blob& blob) {
		dictionary_.FindSound(blob).ApplyFilter(BuildPitchScale(blob));
	}
	Filter BuildLowPass(Blob&) const;
	Filter BuildHighPass(Blob&) const;
	Filter BuildBandPass(Blob&) const;
	Filter BuildFourierGain(Blob&) const;
	Filter BuildFourierBandpass(Blob&) const;
	Filter BuildPitchScale(Blob& blob) const {
		return Filter::PitchScale(blob["f"].asFloat(0.001, 1000.0));
	}
	void FilterSweep(Blob&);
	void Integrate(Blob&);
	void Clip(Blob&);
	void Abs(Blob&);
	void Fold(Blob&);
	void OctaveEffect(Blob&);
	void FourierShift(Blob&);
	void FourierClean(Blob&);
	void FourierCleanPass(Blob&);
	void FourierLimit(Blob&);
	void FourierScale(Blob&);
	void FourierPower(Blob&);
	void Bias(Blob&);
	void Debias(Blob&);
	void Flags(Blob&, Sound&);
	void Flags(Blob&);
	void Repeat(Blob&);
	void QuickMusic(Blob&);
	void MakeMusic(Blob&);
	void MakeMacro(Blob&, macro_type, bool = false);
	void ReadCIN(Blob&);
	void Increment(Blob&, int);
	void Condition(Blob&, ParseParams&);
	void GlobalDefaults(Blob&);
	void DoMessage(std::string, verbosity_type = verbosity_type::none, std::initializer_list<Screen::escape> = {Screen::escape::yellow}) const;
	void TryMessage(std::string, Blob&, std::initializer_list<Screen::escape> ={Screen::escape::yellow});
	void UpdateSliders(float_type, float_type, ParseParams&);
	float_type NotesModeBlob(Blob&, Sound&, ParseParams&, float_type, bool);
	void PlayNote(std::string, Blob&, Sound&, ParseParams&, float_type&, float_type&, bool);
	void CheckBeats(ParseParams&, NoteArticulation&);
	void CheckBar(Blob&, ParseParams&);
	void NestNotesModeBlob(Blob&, Sound&, ParseParams&, float_type&, float_type&, bool);
	void DoNotesMacro(std::string, Sound&, ParseParams&, float_type&, float_type&, bool);
	void DoAmpAdjust(Blob&, ParseParams&);
	void StereoRandom(Blob&, ParseParams&);
	void AmpRandom(Blob&, ParseParams&);
	void Transpose(Blob&, ParseParams&);
	void TransposeRandom(Blob&, ParseParams&);
	void Intonal(Blob&, ParseParams&);
	void ForEach(Blob&, Sound&, ParseParams&, float_type&, float_type&, bool);
	void OneOf(Blob&, Sound&, ParseParams&, float_type&, float_type&, bool, int);
	void Arpeggiate(Blob&, Sound&, ParseParams&, float_type&, float_type&, bool);
	void Shuffle(Blob&, Sound&, ParseParams&, float_type&, float_type&, bool);
	Blob& GetMutableBlob(std::string, Blob&, bool);
	void Scramble(Blob &);
	void CallChange(Blob&);
	void Mingle(Blob &);
	void Rotate(Blob &);
	void Replicate(Blob &);
	void Indirect(Blob &);
	void Unfold(Blob&, Sound&, ParseParams&, float_type&, float_type&, bool, bool);
	void Switch(Blob&, Sound&, ParseParams&, float_type&, float_type&, bool, bool);
	void Trill(Blob&, Sound&, ParseParams&, float_type&, float_type&, bool);
	void DoS(Blob&, ParseParams&);
	void Clear();
public:
	explicit Parser() {
		Clear();
	}
	parse_exit ParseString(std::string);
	void Supervisor(bool supervisor) noexcept {supervisor_ = supervisor;};
};

class ParseLaunch {
private:
	StringVector args_;
	static std::string BackSlashEscape(std::string input);
public:
	explicit ParseLaunch(int arg_count, char **arg_vector) :
		args_ {StringVector(arg_vector, arg_vector + arg_count)} {}
	int Start();
};

} //end namespace BoxyLady

#endif /* PARSER_H_ */
