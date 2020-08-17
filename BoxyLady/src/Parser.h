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

#ifndef PARSER_H_
#define PARSER_H_

#include "Global.h"
#include "Sequence.h"
#include "Blob.h"
#include "Dictionary.h"
#include "Envelope.h"
#include "Tuning.h"
#include "Articulation.h"
#include "Builders.h"

namespace BoxyLady {

inline constexpr int FailStart { 1 };
enum class context_mode {
	nomode, seq, chord
};
enum class parse_exit {
	exit, end, error
};
enum class macro_type {
	macro
};
enum class t_mode {
	tempo, time
};

class Slider {
private:
	double stop_, rate_;
	bool active_;
public:
	Slider() :
			stop_(0), rate_(0), active_(false) {
	}
	void Build(Blob&, double, bool = false);
	Slider(Blob &blob, double now) {
		Build(blob, now);
	}
	void Update(double, double, double&);
	void Update(double, double, Stereo&);
	void Clear() {
		rate_ = stop_ = 0;
		active_ = false;
	}
	double getRate() const {
		return rate_;
	}
	std::string toString(double) const;
};

class AmpAdjust {
public:
	bool active_;
	double exponent_, standard_;
	AmpAdjust() :
			active_(false), exponent_(1.0), standard_(1.0) {
	}
	AmpAdjust(double exponent, double standard) :
			active_(true), exponent_(exponent), standard_(standard) {
	}
	double Amplitude(double frequency) const {
		return active_ ? pow(standard_ / frequency, exponent_) : 1.0;
	}
};

class ParseParams {
	friend class Parser;
private:
	PitchGamut gamut_;
	ArticulationGamut articulation_gamut_;
	NoteArticulation articulation_;
	BeatGamut beat_gamut_;
	AmpAdjust amp_adjust_;
	double tempo_, transpose_, arpeggio_;
	NoteValue last_note_;
	NoteDuration current_duration_;
	std::string instrument_, post_process_;
	double amp_, amp2_, last_frequency_multiplier_, beat_time_;
	double precision_amp_, precision_pitch_, precision_time_, offset_time_;
	context_mode mode_;
	t_mode tempo_mode_;
	bool ignore_pitch_, slur_, bar_check_;
	Slider rall_, cresc_, salendo_, pan_, staccando_;
public:
	ParseParams() :
			gamut_(PitchGamut().TET12()), articulation_gamut_(
					ArticulationGamut().Default()), articulation_(), beat_gamut_(), amp_adjust_(), tempo_(
					120), transpose_(1.0), arpeggio_(0.0), instrument_(""), post_process_(
					""), amp_(1.0), amp2_(1.0), last_frequency_multiplier_(1.0), beat_time_(
					0.0), precision_amp_(0), precision_pitch_(0), precision_time_(
					0), offset_time_(0), mode_(context_mode::nomode), tempo_mode_(
					t_mode::tempo), ignore_pitch_(false), slur_(false), bar_check_(
					false) {
	}
	double TimeDuration(NoteDuration) const;
};

class Parser {
private:
	ParseParams params_;
	bool supervisor_;
	Dictionary dictionary_;
	std::string mp3_encoder_, file_play_;
	static verbosity_type verbosity_;
	static int default_sample_rate_;
	static double instrument_duration_, max_instrument_duration_,
			synth_frequency_multiplier_, standard_pitch_;
	Window BuildWindow(Blob&) const;
	Filter BuildFilter(Blob&) const;
	FilterVector BuildFilters(Blob&) const;
	static SampleType BuildSampleType(Blob&, SampleType = SampleType());
	static verbosity_type BuildVerbosity(std::string);
	static dic_item_protection BuildProtection(std::string);
	void RenameEntry(Blob&);
	int ExternalCommand(Blob&) const;
	void LoadPatch(Blob&);
	void SavePatch(Blob&);
	void Mp3Encode(Blob&);
	void Mp3Encode(std::string, std::string, MetadataList);
	void Metadata(Blob&);
	int DoPlay(std::string, std::string);
	void CommandReplaceString(std::string&, std::string, std::string,
			std::string);
	void Play(Blob&);
	void ExternalProcessing(Blob&);
	void DoEcho(Blob&);
	void LoadLibrary(std::string);
	parse_exit ParseImmediate();
	parse_exit ParseBlobs(Blob&);
	void Clone(Blob&);
	void Combine(Blob&);
	void Mix(Blob&);
	void Split(Blob&);
	void Rechannel(Blob&);
	void Cut(Blob&);
	void Paste(Blob&);
	void ShowState(Blob&, ParseParams, double);
	void Histogram(Blob&);
	void Fade(Blob&);
	void CrossFade(Blob&);
	void Balance(Blob&);
	void Echo(Blob&);
	void Reverse(Blob &blob) {
		dictionary_.FindSequence(blob).Reverse();
	}
	void Tremolo(Blob&);
	void Resize(Blob&);
	void DoCreate(Blob&);
	void DoInstrument(Blob&);
	void KarplusStrong(Blob&);
	void Chowning(Blob&);
	void Modulator(Blob&, Sequence* = 0);
	void Synth(Blob&, Sequence* = 0);
	void DoSmatter(Sequence*, Blob&);
	void DoSines(Sequence*, Blob&, double);
	void DoDefrag();
	void DoAccess(Blob&);
	void DoDelete(Blob&);
	void DoEnvelope(Blob&);
	void Distort(Blob&);
	void Chorus(Blob&);
	void Offset(Blob&);
	void RingModulation(Blob&);
	void Flange(Blob&);
	void BitCrusher(Blob&);
	void LowPass(Blob &blob) {
		dictionary_.FindSequence(blob).ApplyFilter(BuildLowPass(blob));
	}
	void HighPass(Blob &blob) {
		dictionary_.FindSequence(blob).ApplyFilter(BuildHighPass(blob));
	}
	void BandPass(Blob &blob) {
		dictionary_.FindSequence(blob).ApplyFilter(BuildBandPass(blob));
	}
	void FourierGain(Blob &blob) {
		dictionary_.FindSequence(blob).ApplyFilter(BuildFourierGain(blob));
	}
	void FourierBandpass(Blob &blob) {
		dictionary_.FindSequence(blob).ApplyFilter(BuildFourierBandpass(blob));
	}
	Filter BuildLowPass(Blob&) const;
	Filter BuildHighPass(Blob&) const;
	Filter BuildBandPass(Blob&) const;
	Filter BuildFourierGain(Blob&) const;
	Filter BuildFourierBandpass(Blob&) const;
	Filter BUildAmpFilter(Blob &blob) const {
		return Filter::Amp(BuildAmplitude(blob));
	}
	Filter BuildDistort(Blob &blob) const {
		return Filter::Distort(blob.asDouble(0.001, 1000.0));
	}
	Filter BuildKSBlend(Blob &blob) const {
		return Filter::KSBlend(blob.asDouble(0.0, 1.0));
	}
	Filter BuildKSReverse(Blob &blob) const {
		return Filter::KSReverse(blob.asDouble(0.0, 1.0));
	}
	void FilterSweep(Blob&);
	void Integrate(Blob&);
	void Clip(Blob&);
	void Abs(Blob&);
	void Fold(Blob&);
	void OctaveEffect(Blob&);
	void FourierShift(Blob&);
	void FourierClean(Blob&);
	void Bias(Blob&);
	void Debias(Blob&);
	void Flags(Blob&, Sequence* = 0);
	void Repeat(Blob&);
	void QuickMusic(Blob&);
	void MakeMusic(Blob&);
	void MakeMacro(Blob&, macro_type);
	void GlobalDefaults(Blob&);
	void Message(std::string, verbosity_type = verbosity_type::none) const;
	void ShowMessage(std::string, Blob&) const;
	void UpdateSliders(double, double, ParseParams&);
	Window NotesModeBlob(Blob&, Sequence&, ParseParams&, double, bool);
	void DoNote(std::string, Blob&, Sequence&, ParseParams&, double&, double&,
			bool);
	void CheckBeats(ParseParams &params, NoteArticulation &articulation) {
		for (auto beat : params.beat_gamut_.getBeats())
			CheckBeat(params, articulation, beat.second);
	}
	void CheckBeat(ParseParams&, NoteArticulation&, Beat);
	void CheckBar(Blob&, ParseParams&);
	void NestNotesModeBlob(Blob&, Sequence&, ParseParams&, double&, double&,
			bool);
	void DoNotesMacro(std::string, Sequence&, ParseParams&, double&, double&,
			bool);
	void DoAmpAdjust(Blob&, ParseParams&);
	void DoStereoRandom(Blob&, ParseParams&);
	void DoAmpRandom(Blob&, ParseParams&);
	void DoTranspose(Blob&, ParseParams&);
	void DoTransposeRandom(Blob&, ParseParams&);
	void DoIntonal(Blob&, ParseParams&);
	void DoForEach(Blob&, Sequence&, ParseParams&, double&, double&, bool);
	void DoOneOf(Blob&, Sequence&, ParseParams&, double&, double&, bool, int);
	void DoShuffle(Blob&, Sequence&, ParseParams&, double&, double&, bool);
	void DoUnfold(Blob&, Sequence&, ParseParams&, double&, double&, bool, bool);
	void DoS(Blob&, ParseParams&);
	void Clear();
public:
	Parser() {
		Clear();
	}
	parse_exit ParseString(std::string);
	static bool messages() {
		return verbosity_ >= verbosity_type::messages;
	}
};

class ParseLaunch {
private:
	typedef std::vector<std::string> ArgVector;
	ArgVector args_;
public:
	ParseLaunch(int arg_count, char **arg_vector) {
		args_ = ArgVector(arg_vector, arg_vector + arg_count);
	}
	int Start();
};

}

#endif /* PARSER_H_ */
