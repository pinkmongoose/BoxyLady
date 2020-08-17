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

#include <fstream>

#include "Parser.h"
#include "Sequence.h"
#include "Platform.h"

namespace BoxyLady {

//"0.1.1 'Fragrant Frag-Queen'"
//"0.1.2 'Disagreeable Deva'"
//"0.1.3 'Chuffing Chimney'"
//"0.1.4 'Balmy Bunty'"
//"0.1.5 'Pungent Pad'"
//"0.1.6 'Resplendent Rav'"
//"0.1.7 'Beefy Botch'"
//"0.1.8 'Ravishing Rod'"
//"0.2.1 'Groovy Great'"
//"0.2.2 'Mopping Mags'"

const std::string VersionNumber("0.2.3"), VersionAlias("Honking Hum"), Version(
		VersionNumber + " " + VersionAlias + "."), BootWelcome(
		"echo(\"\nWelcome to BoxyLady. This is BoxyLady.\")\n"),
		BootHelp(
				"Usage: BoxyLady --help --version --noboot --envshow --messages MESSAGELEVEL --i --e 'SOURCE' SOURCEFILE\n"),
		BootLicence(
				"Copyright (C) 2011-2020 Darren Green.\nLicense GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>\n\n\
This is free software; you are free to change and redistribute it.\n\
There is NO WARRANTY, to the extent permitted by law.\n\n"),
		BootContact(
				"Contact: darren.green@stir.ac.uk http://www.pinkmongoose.co.uk\n"),
		BootVersionInformation(
				"BoxyLady version " + Version + "\nCompiled on "
						+ Platform::CompileDate + " at " + Platform::CompileTime
						+ " with GCC " + Platform::CompileVersion + ".\n"),
		BootInformation(BootVersionInformation + BootLicence + BootContact),
		Poem(
				"\n\nI wish I were an angler fish!\nI'd laze around all day,\nAnd wave my shiny dangly bit,\nTo lure in all my prey.\n\n\t--DG\n\n");

verbosity_type Parser::verbosity_ = verbosity_type::messages;
int Parser::default_sample_rate_ = 44100;
double Parser::instrument_duration_ = 1.0;
double Parser::max_instrument_duration_ = 16.0;
double Parser::synth_frequency_multiplier_ = 1.0;
double Parser::standard_pitch_ = 440.0;

void CheckSystem() {
	if (system(nullptr) == 0)
		throw EParser().msg("System calls are not available.");
}

void Slider::Build(Blob &blob, double now, bool do_amp) {
	blob.AssertFunction();
	if (blob.hasFlag("off")) {
		Clear();
		return;
	}
	;
	double duration = blob["t"].asDouble(DBL_MIN, DBL_MAX), rate =
			(do_amp) ?
					BuildAmplitude(blob["f"]) :
					blob["f"].asDouble(0.0001, 10000.0);
	stop_ = now + duration;
	rate_ = log(rate) / duration;
	active_ = true;
}

void Slider::Update(double now, double duration, double &value) {
	if (!active_)
		return;
	if (now < stop_)
		value *= exp(rate_ * duration);
	else {
		value *= exp(rate_ * (duration - now + stop_));
		Clear();
	}
}

void Slider::Update(double now, double duration, Stereo &stereo) {
	double value = 1.0;
	Update(now, duration, value);
	stereo[Left] /= value;
	stereo[Right] *= value;
}

std::string Slider::toString(double now) const {
	if (!active_)
		return "off";
	std::ostringstream buffer;
	buffer.precision(2);
	buffer << "time = " << (stop_ - now) << "s rate = " << exp(rate_) << "/s";
	return buffer.str();
}

double ParseParams::TimeDuration(NoteDuration note_duration) const {
	constexpr double SecondsPerMinute = 60.0, QuartersPerSemibreve = 4.0;
	const double duration = note_duration.getDuration();
	if (duration == 0)
		return 0.0;
	switch (tempo_mode_) {
	case t_mode::tempo: {
		const double semibreve_tempo = tempo_
				/ (SecondsPerMinute * QuartersPerSemibreve), rall_rate =
				rall_.getRate();
		if (rall_rate == 0)
			return duration / semibreve_tempo;
		const double x = (semibreve_tempo + rall_rate * duration)
				/ semibreve_tempo;
		if (x <= 0)
			throw EParser().msg("Infinite note duration encountered.");
		return log(x) / rall_rate;
	}
	default: {
		return duration;
	}
	}
}

// Parser functions

verbosity_type Parser::BuildVerbosity(std::string input) {
	verbosity_type type;
	if (input == "none")
		type = verbosity_type::none;
	else if (input == "errors")
		type = verbosity_type::errors;
	else if (input == "messages")
		type = verbosity_type::messages;
	else if (input == "verbose")
		type = verbosity_type::verbose;
	else {
		type = verbosity_type::errors;
		throw EParser().msg(input + ": Unknown verbosity level.");
	}
	return type;
}

dic_item_protection Parser::BuildProtection(std::string input) {
	dic_item_protection protection;
	if (input == "temp")
		protection = dic_item_protection::temp;
	else if (input == "unlocked")
		protection = dic_item_protection::normal;
	else if (input == "locked")
		protection = dic_item_protection::locked;
	else if (input == "system")
		protection = dic_item_protection::system;
	else {
		protection = dic_item_protection::temp;
		throw EParser().msg(input + ": Unknown slot protection level.");
	}
	return protection;
}

SampleType Parser::BuildSampleType(Blob &blob, SampleType type) {
	blob.AssertFunction();
	for (auto &child : blob.children_) {
		if (child.isToken()) {
			const std::string atom = child.atom();
			if (atom == "rb")
				type.sample_rate_ = CDSampleRate;
			else if (atom == "telephone")
				type.sample_rate_ = TelephoneSampleRate;
			else if (atom == "sr")
				type.sample_rate_ = default_sample_rate_;
			else if (atom == "lo")
				type.sample_rate_ = default_sample_rate_ / 2;
			else if (atom == "LO")
				type.sample_rate_ = default_sample_rate_ / 4;
			else if (atom == "hi")
				type.sample_rate_ = default_sample_rate_ * 2;
			else if (atom == "HI")
				type.sample_rate_ = default_sample_rate_ * 4;
			else if (atom == "*HI*")
				type.sample_rate_ = default_sample_rate_ * 8;
			else if (atom == "instrument") {
				type.sample_rate_ = default_sample_rate_ * 4;
				type.loop_ = true;
			} else
				throw EParser().msg(
						"Unknown sample type mode.\n" + blob.ErrorString());
		} else if (child.key_ == "gate")
			type.gate_ = child.asBool();
		else if (child.key_ == "loop")
			type.loop_ = child.asBool();
		else if (child.key_ == "loop_start")
			type.loop_start_ = child.asDouble(0, DBL_MAX);
		else if (child.key_ == "start_anywhere")
			type.start_anywhere_ = child.asBool();
		else if (child.key_ == "user")
			type.sample_rate_ = child.ifFunction()["Hz"].asInt(MinSampleRate,
					MaxSampleRate);
		else
			throw EParser().msg(
					"Unknown sample type mode.\n" + blob.ErrorString());
	}
	return type;
}

Window Parser::BuildWindow(Blob &blob) const {
	blob.AssertFunction();
	Window window;
	if (blob.hasKey("start"))
		window.setStart(blob["start"].asDouble());
	if (blob.hasKey("end"))
		window.setEnd(blob["end"].asDouble());
	return window;
}

void Parser::Clear() {
	supervisor_ = false;
	mp3_encoder_ =
			"echo %source %dest %title %artist %album %track %year %genre %comment";
	file_play_ = "echo %file %arg";
}

void Parser::ShowMessage(std::string message, Blob &blob) const {
	if (blob.ifFunction().hasKey("shh!")) {
		const verbosity_type verbosity = BuildVerbosity(blob["shh!"].atom());
		Message(message, verbosity);
	} else
		Message(message, verbosity_type::none);
}

void Parser::Message(std::string message, verbosity_type verbosity) const {
	if (int(verbosity) <= messages())
		std::cout << message << "\n";
}

// Notes mode functions

void Parser::GlobalDefaults(Blob &blob) {
	params_.mode_ = context_mode::seq;
	Sequence temp_sequence;
	NotesModeBlob(blob, temp_sequence, params_, 0, false);
}

void Parser::QuickMusic(Blob &blob) {
	const std::string name = "quick";
	blob.AssertFunction();
	ParseParams params = params_;
	params.mode_ = context_mode::seq;
	params.instrument_ = "beep";
	double start = 0;
	DictionaryItem &slot = dictionary_.Insert(
			DictionaryItem(dic_item_type::patch), name);
	DictionaryMutex mutex(slot);
	Sequence &sequence = slot.getSequence();
	sequence.CreateSilenceSeconds(StereoChannels, default_sample_rate_, 0, 0);
	Window window = NotesModeBlob(blob, sequence, params, start, true);
	sequence.set_tSeconds(window.end());
	std::string command = file_play_, arg = "";
	CommandReplaceString(command, "%arg", arg, "play()");
	std::string file = TempFilename("wav").file_name();
	sequence.SaveToFile(file, file_format::RIFF_wav, false);
	int return_code = DoPlay(file, command);
	if (return_code != 0)
		Message(
				"\nFile play: error returned from external programme. ["
						+ file_play_ + "]");
	remove(file.c_str());
}

void Parser::MakeMusic(Blob &blob) {
	const std::string name = blob["@"].atom();
	if (dictionary_.Exists(name))
		throw EParser().msg(name + ": Object already exists.");
	const int channels = blob["channels"].asInt(1, MaxChannels);
	if ((channels < 1) || (channels > MaxChannels))
		throw EParser().msg("Only 1 or 2 channels are currently supported.");
	SampleType sample_type = BuildSampleType(blob["type"]);
	ParseParams params = params_;
	params.mode_ = context_mode::nomode;
	double start = 0;
	DictionaryItem &slot = dictionary_.Insert(
			DictionaryItem(dic_item_type::patch), name);
	DictionaryMutex mutex(slot);
	Sequence &sequence = slot.getSequence();
	Blob &music_blob = blob["music"];
	if (!music_blob.isBlock(false))
		throw EParser().msg(
				"No instruction block provided.\n" + blob.ErrorString());
	sequence.CreateSilenceSeconds(channels, sample_type.sample_rate_, 0, 0);
	sequence.setType(sample_type);
	Window window = NotesModeBlob(music_blob, sequence, params, start, true);
	sequence.set_tSeconds(window.end());
	Message("Created patch [" + name + "]");
}

void Parser::UpdateSliders(double now, double duration, ParseParams &params) {
	params.cresc_.Update(now, duration, params.amp_);
	params.rall_.Update(now, duration, params.tempo_);
	params.salendo_.Update(now, duration, params.transpose_);
	params.pan_.Update(now, duration, params.articulation_.stereo_);
	params.staccando_.Update(now, duration, params.articulation_.staccato_);
}

Window Parser::NotesModeBlob(Blob &blob, Sequence &sequence,
		ParseParams &params, double now, bool make_music) {
	auto AssertNoSlur = [params, blob]() {
		if (params.slur_)
			throw EParser().msg(
					"Slurs must be contained entirely within contexts.\n"
							+ blob.ErrorString());
	};
	double len = 0, tlen = 0;
	bool inside_slur = true;
	switch (blob.delimiter_) {
	case '[':
		params.mode_ = context_mode::seq;
		break;
	case '<':
		params.mode_ = context_mode::chord;
		break;
	case '{':
		break;
	default:
		inside_slur = false;
		break;
	}
	if (inside_slur)
		AssertNoSlur();
	if (params.mode_ == context_mode::nomode)
		throw EParser().msg(
				"No context mode set. Use < or [ at start of notes mode.\n"
						+ blob.ErrorString());
	for (auto &instruction : blob.children_) {
		std::string token = instruction.val_, key = instruction.key_;
		if (instruction.isBlock())
			NestNotesModeBlob(instruction, sequence, params, now, len,
					make_music);
		else if (instruction.isToken()) {
			if (token[0] == '\\')
				DoNotesMacro(token.erase(0, 1), sequence, params, now, len,
						make_music);
			else if (token[0] == '#') {
				std::string name = token.erase(0, 1);
				if (!dictionary_.Exists(name))
					throw EParser().msg(name + ": No such object.");
				params.instrument_ = name;
				params.slur_ = false;
			} else if (token == "!")
				std::cout << ((make_music) ? "!" : "?");
			else if (token == "|") {
				if (params.bar_check_)
					CheckBar(blob, params);
			} else if (token == "0")
				params.current_duration_ = NoteDuration(0.0);
			else if (token == "r") {
				const double duration = params.TimeDuration(
						params.current_duration_);
				if (params.mode_ == context_mode::seq) {
					now += duration;
					len += duration;
					params.beat_time_ += params.current_duration_.getDuration();
				} else if (duration > len)
					len = duration;
				if (params.mode_ == context_mode::seq)
					UpdateSliders(now, duration, params);
			} else if ((token[0] >= '0') && (token[0] <= '9')) {
				if (params.tempo_mode_ == t_mode::tempo)
					params.current_duration_ = NoteDuration(token);
				else
					params.current_duration_ = Blob(token)[0].asDouble(0,
							DBL_MAX);
			} else if (((token[0] >= 'a') && (token[0] <= 'z'))
					|| ((token[0] >= 'A') && (token[0] <= 'Z'))
					|| (token[0] == '\'') || (token[0] == '+'))
				DoNote(token, blob, sequence, params, now, len, make_music);
			else if ((token == "") && (key == "")) {
			} else
				throw EParser().msg(
						token + ": Unrecognised music symbol.\n"
								+ blob.ErrorString());
		} // end of bare tokens
		else if (key == "silence") {
			const double duration = instruction.asDouble(0, MaxSequenceLength);
			if (params.mode_ == context_mode::seq) {
				now += duration;
				len += duration;
			} else if (duration > len)
				len = duration;
			if (params.mode_ == context_mode::seq)
				UpdateSliders(now, duration, params);
		} else if (key == "rel")
			params.last_note_ = params.gamut_.Note(instruction.atom());
		else if (key == "tuning")
			params.gamut_.TuningBlob(instruction.ifFunction(), make_music);
		else if (key == "gamut")
			params.gamut_.ParseBlob(instruction.ifFunction(), make_music);
		else if (key == "articulations")
			params.articulation_gamut_.ParseBlob(instruction.ifFunction(),
					make_music);
		else if (key == "beats")
			params.beat_gamut_.ParseBlob(instruction.ifFunction(),
					params.beat_time_, make_music);
		else if (key == "show_state") {
			if (make_music)
				ShowState(instruction, params, now);
		} else if (key == "flags")
			Flags(instruction, &sequence);
		else if (key == "transpose")
			DoTranspose(instruction.ifFunction(), params);
		else if (key == "transpose_random")
			DoTransposeRandom(instruction.ifFunction(), params);
		else if (key == "intonal")
			DoIntonal(instruction.ifFunction(), params);
		else if (key == "tempo") {
			if (instruction.hasKey("rel")) {
				const NoteDuration note_duration(
						instruction.ifFunction()["rel"].atom());
				params.tempo_ *= note_duration.getDuration()
						/ params.current_duration_.getDuration();
			} else if (instruction.hasKey("f"))
				params.tempo_ *= instruction.ifFunction()["f"].asDouble(0.0001,
						10000.0);
			else
				params.tempo_ = instruction.asDouble(1.0, 10000.0);
		} else if (key == "tempo_mode") {
			if (instruction.ifFunction().hasFlag("tempo"))
				params.tempo_mode_ = t_mode::tempo;
			else if (instruction.hasFlag("time"))
				params.tempo_mode_ = t_mode::time;
			else
				throw EParser().msg(
						"Incorrect tempo mode set.\n" + blob.ErrorString());
		} else if (key == "offset")
			params.offset_time_ = instruction.asDouble(-LongTime, LongTime);
		else if ((key == "amp") || (key == "amp2")) {
			double &which_amp = (key == "amp") ? params.amp_ : params.amp2_;
			if (instruction.isFunction() && instruction.hasKey("f"))
				which_amp *= BuildAmplitude(instruction["f"]);
			else
				which_amp = BuildAmplitude(instruction);
		} else if (key == "amp_random")
			DoAmpRandom(instruction.ifFunction(), params);
		else if (key == "C") {
			const std::string buffer = "N(" + instruction.ifFunction()[0].atom()
					+ ") \\" + instruction[1].atom();
			Blob temp_blob(buffer);
			const Window window = NotesModeBlob(temp_blob, sequence, params,
					now, make_music);
			if (params.mode_ == context_mode::seq) {
				now += window.start();
				len += window.start();
			} else if (window.start() > len)
				len = window.start();
		} else if ((key == "env") || (key == "envelope"))
			params.articulation_.envelope_ = BuildEnvelope(instruction);
		else if (key == "vib")
			params.articulation_.phaser_ = BuildPhaser(instruction);
		else if (key == "tremolo")
			params.articulation_.tremolo_ = BuildWave(instruction);
		else if (key == "bend") {
			if (instruction.isFunction() && instruction.hasKey("t")) {
				params.articulation_.phaser_.bend_time() =
						instruction["t"].asDouble(DBL_MIN, DBL_MAX);
				params.articulation_.phaser_.bend_factor() = pow(
						instruction["f"].asDouble(0.0001, 10000.0),
						1.0 / params.articulation_.phaser_.bend_time());
			} else
				params.articulation_.phaser_.bend_factor() =
						instruction.asDouble(0.0001, 10000.0);
		} else if (key == "port")
			params.articulation_.portamento_time_ = instruction.asDouble(0,
					LongTime);
		else if (key == "scratch") {
			if (instruction.isFunction() && instruction.hasFlag("off"))
				params.articulation_.scratcher_ = Scratcher();
			else
				params.articulation_.scratcher_ = Scratcher(nullptr,
						instruction["with"].atom(), instruction["a"].asDouble(),
						instruction["bias"].asDouble(),
						instruction["loop"].asBool());
		} else if (key == "glide")
			params.articulation_.glide_ = instruction.asBool();
		else if (key == "octave")
			params.last_note_.setOctave(instruction.asInt(-256, 256));
		else if (key == "N") {
			params.last_note_ = params.gamut_.Note(instruction.atom(),
					params.last_note_);
//			lastFreqMult=P.G.FreqMult(P.LastNote)*P.transpose;
		} else if (key == "S")
			DoS(instruction.ifFunction(), params);
		else if (key == "echo")
			Message(instruction.atom());
		else if (key == "rem") {
		} else if (key == "rall")
			params.rall_.Build(instruction, now);
		else if (key == "cresc")
			params.cresc_.Build(instruction, now, true);
		else if (key == "salendo")
			params.salendo_.Build(instruction, now);
		else if (key == "pan")
			params.pan_.Build(instruction, now, true);
		else if (key == "stereo") {
			if (instruction.ifFunction().hasFlag("swap"))
				params.articulation_.stereo_.Swap();
			else if (instruction.hasFlag("off"))
				params.articulation_.stereo_ = Stereo(1.0);
			else
				params.articulation_.stereo_ = BuildStereo(instruction);
		} else if (key == "stereo_random")
			DoStereoRandom(instruction.ifFunction(), params);
		else if (key == "amp_adjust")
			DoAmpAdjust(instruction.ifFunction(), params);
		else if (key == "ignore_pitch")
			params.ignore_pitch_ = instruction.asBool();
		else if (key == "env_adjust")
			params.articulation_.envelope_compress_ = instruction.asBool();
		else if (key == "rev")
			params.articulation_.reverb_ = instruction.asBool();
		else if (key == "bar_check")
			params.bar_check_ = instruction.asBool();
		else if (key == "arp")
			params.arpeggio_ = instruction.asDouble(0, LongTime);
		else if (key == "staccato")
			params.articulation_.staccato_ = instruction.asDouble(0,
					MaxStaccato);
		else if (key == "staccando")
			params.staccando_.Build(instruction, now);
		else if (key == "D")
			params.current_duration_ = BuildNoteDuration(instruction);
		else if (key == "D_rev")
			params.articulation_.duration_ = BuildNoteDuration(instruction);
		else if (key == "D_random")
			params.current_duration_ = NoteDuration(
					1.0
							/ Rand.uniform(
									instruction.ifFunction()["max"].asDouble(
											DBL_MIN, DBL_MAX),
									instruction["min"].asDouble(DBL_MIN,
											DBL_MAX)));
		else if (key == "outer") {
			if (make_music)
				ParseBlobs(instruction.ifFunction());
		} else if (key == "def")
			MakeMacro(instruction, macro_type::macro);
		else if (key == "context_mode") {
			if (instruction.ifFunction().hasFlag("tune"))
				params.mode_ = context_mode::seq;
			else if (instruction.hasFlag("chords"))
				params.mode_ = context_mode::chord;
			else
				throw EParser().msg(
						"Incorrect context mode set.\n" + blob.ErrorString());
		} else if (key == "oneof")
			DoOneOf(instruction.ifFunction(), sequence, params, now, len,
					make_music, -1);
		else if (key == "shuffle")
			DoShuffle(instruction.ifFunction(), sequence, params, now, len,
					make_music);
		else if (key == "unfold")
			DoUnfold(instruction.ifFunction(), sequence, params, now, len,
					make_music, false);
		else if (key == "fill")
			DoUnfold(instruction.ifFunction(), sequence, params, now, len,
					make_music, true);
		else if (key == "foreach")
			DoForEach(instruction.ifFunction(), sequence, params, now, len,
					make_music);
		else if (key == "precision") {
			instruction.ifFunction().tryWriteDouble("amp",
					params.precision_amp_, 0, 1);
			instruction.tryWriteDouble("pitch", params.precision_pitch_, 0, 1);
			instruction.tryWriteDouble("time", params.precision_time_, 0, 1);
		} else if (key == "post_process") {
			const std::string process = instruction.atom();
			if (process == "off")
				params.post_process_ = "";
			else {
				DictionaryItem &item = dictionary_.Find(process);
				if (item.isNull())
					throw EParser().msg(process + ": No such object.");
				params.post_process_ = process;
			}
		} else
			throw EParser().msg(key + "=" + token + ": Unknown command.");
	}
	if (inside_slur)
		AssertNoSlur();
	return Window(len, len + tlen);
}

void Parser::DoNote(std::string token, Blob &blob, Sequence &sequence,
		ParseParams &params, double &now, double &len, bool make_music) {
	NoteValue note_value;
	if (token[0] == '\'')
		token.erase(0, 1);
	if (token[0] == '+') {
		if ((token.length() == 1) || (token[1] == '-'))
			note_value = params.last_note_;
		else
			throw EParser().msg(
					"Problem in repeated note.\n" + blob.ErrorString());
	} else if (params.ignore_pitch_)
		note_value = params.last_note_;
	else
		note_value = params.gamut_.Note(token, params.last_note_);// n now contains the note to be played.
	NoteArticulation articulation = params.articulation_;
	CheckBeats(params, articulation);
	articulation.Overwrite(params.articulation_gamut_.Note(token));	// na contains the current articulations, overwritten by those of the beats, and then of the current note
	const double duration_rhythmic = params.TimeDuration(
			params.current_duration_), duration_articulation =
			params.TimeDuration(articulation.duration_);
	const double duration =
			(duration_articulation) ?
					duration_articulation :
					duration_rhythmic * articulation.staccato_;
	double imprecision_pitch = 1, imprecision_time = 0, imprecision_amp = 1,
			offset_time = params.offset_time_;
	if (params.precision_pitch_)
		imprecision_pitch = 1.0
				+ Rand.uniform(-params.precision_pitch_,
						params.precision_pitch_);
	if (params.precision_time_)
		imprecision_time = Rand.uniform(0, params.precision_time_);
	if (params.precision_amp_)
		imprecision_amp = 1.0
				+ Rand.uniform(-params.precision_amp_, params.precision_amp_);
	if (make_music) {// omitted if music data isn't being generated - all timings calculated above
		std::string instrument = params.instrument_;
		if (instrument == "")
			throw EParser().msg(
					"No instrument specified to use.\n" + blob.ErrorString());
		DictionaryItem &instrument_item = dictionary_.Find(instrument);
		if (instrument_item.isNull())
			throw EParser().msg(instrument + ": No such object.");
		const double freq_mult = params.gamut_.FreqMultStandard(note_value)
				* params.transpose_, amp_mult = params.amp_adjust_.Amplitude(
				freq_mult);
//			AmpMult=(P.AmpAdjust)? pow(P.AmpAdjustFreqMult/freq_mult,P.AmpAdjustExponent): 1.0;
		double freq_mult_imprecision, freq_mult_start;
		Sequence *instrument_sequence_ptr = 0;
		if (instrument_item.isPatch()) {	// assign sequence M as instrument
			instrument_sequence_ptr = &(instrument_item.getSequence());
			freq_mult_imprecision = freq_mult * imprecision_pitch;
		} else if (instrument_item.isMacro()) {
			const double synth_frequency_multiplier_old =
					synth_frequency_multiplier_;
			synth_frequency_multiplier_ *= (freq_mult * imprecision_pitch);
			instrument_duration_ = std::min(duration, max_instrument_duration_);
			DictionaryMutex mutex(instrument_item);
			ParseBlobs(instrument_item.getMacro());
			DictionaryItem &created_item = dictionary_.Find("instrument");
			if (created_item.isNull())
				throw EParser().msg(
						"Failed to find 'instrument' slot."
								+ blob.ErrorString());
			if (!created_item.isPatch())
				throw EParser().msg(
						"Slot 'instrument' must contain music data."
								+ blob.ErrorString());
			instrument_sequence_ptr = &(created_item.getSequence());
			freq_mult_imprecision = 1.0;
			synth_frequency_multiplier_ = synth_frequency_multiplier_old;// generate sequence M as instrument
		} else
			throw EParser().msg(instrument + ": Not suitable instrument.");
		Sequence &instrument_sequence = *instrument_sequence_ptr;// refer below to the sequence to be overlaid as M
		SampleType instrument_sequence_type = instrument_sequence.getType();
		Scratcher scratcher = articulation.scratcher_;
		if (scratcher.active()) {
			DictionaryItem &scratcher_item = dictionary_.Find(scratcher.name());
			if (scratcher_item.isNull())
				throw EParser().msg(
						"Failed to find 'scratch' slot." + blob.ErrorString());
			if (!scratcher_item.isPatch())
				throw EParser().msg(
						"Slot 'instrument' must contain music data."
								+ blob.ErrorString());
			scratcher.sequence() = &(scratcher_item.getSequence());
		}
		bool reverb = articulation.reverb_;
		Window window =
				(reverb) ?
						Window(now + imprecision_time + offset_time) :
						Window(now + imprecision_time + offset_time,
								now + imprecision_time + offset_time
										+ duration);
		OverlayFlags flags;
		flags[overlay::resize] = true;
		flags[overlay::loop] = instrument_sequence_type.loop_;
		flags[overlay::random] = instrument_sequence_type.start_anywhere_;
		flags[overlay::envelope_compress] = articulation.envelope_compress_;
		flags[overlay::slur_on] = params.slur_;
		flags[overlay::slur_off] = params.slur_;
		if (articulation.start_slur_) {
			if (params.slur_)
				throw EParser().msg(
						"Can't start slur twice. " + blob.ErrorString());
			if (params.mode_ != context_mode::seq)
				throw EParser().msg(
						"Can only slur consecutive notes. "
								+ blob.ErrorString());
			flags[overlay::slur_off] = true;
			flags[overlay::slur_on] = false;
			params.slur_ = true;
		};
		if (articulation.stop_slur_) {
			if (!params.slur_)
				throw EParser().msg(
						"Can't stop a slur which isn't there. "
								+ blob.ErrorString());
			flags[overlay::slur_off] = false;
			flags[overlay::slur_on] = true;
			params.slur_ = false;
		}; // Set up slurs
		if (reverb && params.slur_)
			throw EParser().msg(
					"Ongoing slurs with reverb do not work. "
							+ blob.ErrorString());
		Phaser phaser = articulation.phaser_;
		const double portamento_time = articulation.portamento_time_;
		if (articulation.glide_
				|| (flags[overlay::slur_on] && portamento_time)) {
			phaser.bend_time() =
					(articulation.glide_) ? duration : portamento_time;
			phaser.bend_factor() = pow(
					freq_mult_imprecision / params.last_frequency_multiplier_,
					1.0 / phaser.bend_time());
			freq_mult_start = params.last_frequency_multiplier_;
		} else {
			freq_mult_start = freq_mult_imprecision;
		} // Set up pitch bends
		if (params.post_process_ != "") {
			OverlayFlags process_flags;
			process_flags[overlay::resize] = true;
			if (reverb)
				throw EParser().msg(
						"Post_process with reverb does not work. "
								+ blob.ErrorString());
			DictionaryItem &process_item = dictionary_.Find(
					params.post_process_);
			if (process_item.isNull())
				throw EParser().msg(
						"Failed to find 'post_process' slot."
								+ blob.ErrorString());
			Sequence &process_sequence = dictionary_.InsertSequence("note");
			process_sequence.CreateSilenceSeconds(sequence.channels(),
					sequence.sample_rate(), window.length(), window.length());
			process_sequence.Overlay(instrument_sequence, 0,
					process_sequence.p_samples(), freq_mult_start, flags,
					articulation.stereo_ * params.amp_ * params.amp2_
							* articulation.amp_ * amp_mult * imprecision_amp,
					phaser, articulation.envelope_, scratcher,
					articulation.tremolo_);
			DictionaryMutex mutex(process_item);
			ParseBlobs(process_item.getMacro());
			sequence.Overlay(process_sequence, window, 1.0, process_flags);
			dictionary_.Delete("note");
		} else
			sequence.Overlay(instrument_sequence, window, freq_mult_start,
					flags,
					articulation.stereo_ * params.amp_ * params.amp2_
							* articulation.amp_ * amp_mult * imprecision_amp,
					phaser, articulation.envelope_, scratcher,
					articulation.tremolo_);
		params.last_frequency_multiplier_ = freq_mult_imprecision;
	}
	if (duration_rhythmic) {
		if (params.mode_ == context_mode::seq) {
			now += duration_rhythmic;
			len += duration_rhythmic;
			params.beat_time_ += params.current_duration_.getDuration();
		} else {
			now += params.arpeggio_;
			if (duration_rhythmic > len)
				len = duration_rhythmic;
		}
		if (params.mode_ == context_mode::seq)
			UpdateSliders(now, duration_rhythmic, params);
	}
	params.last_note_ = note_value;
}

void Parser::CheckBeat(ParseParams &params, NoteArticulation &articulation,
		Beat beat) {
	const double time = params.beat_time_, duration =
			beat.duration.getDuration(), offset = beat.offset.getDuration()
			/ duration, width = beat.width.getDuration() / duration, fraction =
			time / duration, remainder = fraction
			- static_cast<double>(floor(fraction)) - offset, abs_remainder =
			(remainder < 0) ? -remainder : remainder;
	if (abs_remainder < width) {
		NoteArticulation beat_articulation;
		beat_articulation.Overwrite(
				params.articulation_gamut_.Note("-" + beat.articulations));
		articulation.Overwrite(beat_articulation);
	}
}

void Parser::CheckBar(Blob &blob, ParseParams &params) {
	NoteArticulation articulation;
	articulation.flags_[articulation_type::bar] = true;
	CheckBeats(params, articulation);
	if (!articulation.bar_)
		throw EParser().msg("Bar check failed. " + blob.ErrorString());
}

void Parser::ShowState(Blob &blob, ParseParams params, double now) {
	std::cout << "\n" << screen::Header;
	std::cout << "Current articulations:\n";
	ArticulationGamut::List1(params.articulation_, true);
	std::cout << "\nOther settings:\n" << "slur(" << BoolToString(params.slur_)
			<< ") staccando(" << params.staccando_.toString(now) << ") "
			<< ") arp = " << params.arpeggio_ << "s\n" << "tempo = "
			<< params.tempo_ << "/min rall(" << params.rall_.toString(now)
			<< ") tempo_mode = "
			<< ((params.tempo_mode_ == t_mode::tempo) ? "tempo" : "time")
			<< " \n" << "offset = " << params.offset_time_ << "s D = "
			<< (params.current_duration_.getDuration() * 4.0) << "\n"
			<< "amp = " << params.amp_ << " amp2 = " << params.amp2_
			<< " amp_adjust(";
	if (params.amp_adjust_.active_) {
		std::cout << "power = " << params.amp_adjust_.exponent_
				<< " 'standard_f' = " << params.amp_adjust_.standard_;
	} else
		(std::cout << "off");
	std::cout << ") cresc(" << params.cresc_.toString(now) << ") " << "pan("
			<< params.pan_.toString(now) << ")\n" << "transpose = "
			<< params.transpose_ << " ignore_pitch = "
			<< BoolToString(params.ignore_pitch_) << " salendo("
			<< params.salendo_.toString(now) << ")\n" << "precision(amp = "
			<< params.precision_amp_ << " pitch = " << params.precision_pitch_
			<< " time = " << params.precision_time_ << ")\n" << "#"
			<< params.instrument_ << " post_process(" << params.post_process_
			<< ")\n"
			<< ((params.mode_ == context_mode::seq) ?
					"[tune mode]" : "<chords mode>") << " current_position = "
			<< now << "s\n" << "'last_f' = "
			<< params.last_frequency_multiplier_ << " 'last_note'("
			<< params.last_note_.toString() << ")" << "\n" << screen::Header
			<< "Use list() in gamut() articulations() and beats() for more.\n";
}

void Parser::NestNotesModeBlob(Blob &blob, Sequence &sequence,
		ParseParams &params, double &now, double &len, bool make_music) {
	if (blob.delimiter_ == '(')
		throw EParser().msg(
				"Delimeter ( not supported in music expressions. Use { instead.\n"
						+ blob.ErrorString());
	ParseParams local_params = params;
	const Window window = NotesModeBlob(blob, sequence, local_params, now,
			make_music);
	if (params.mode_ == context_mode::seq) {
		now += window.start();
		len += window.start();
		UpdateSliders(now, window.start(), params);
	} else {
		if (window.start() > len)
			len = window.start();
	}
}

void Parser::DoNotesMacro(std::string name, Sequence &sequence,
		ParseParams &params, double &now, double &len, bool make_music) {
	DictionaryItem &item = dictionary_.Find(name);
	if (item.isNull())
		throw EParser().msg(name + ": No such object.");
	Sequence &overlay_sequence = item.getSequence();
	switch (item.getType()) {
	case dic_item_type::macro: {
		DictionaryMutex mutex(item);
		const Window window = NotesModeBlob(item.getMacro(), sequence, params,
				now, make_music);
		if (params.mode_ == context_mode::seq) {
			now += window.start();
			len += window.start();
		} else if (window.start() > len)
			len = window.start();
		return;
	}
	case dic_item_type::patch: {
		const double duration = item.getSequence().get_tSeconds();
		if (make_music) {
			const Window window(now, now + overlay_sequence.get_pSeconds());
			OverlayFlags flags;
			flags[overlay::resize] = true;
			sequence.Overlay(overlay_sequence, window, 1.0, flags,
					params.articulation_.stereo_ * params.amp_ * params.amp2_);
		}
		if (params.mode_ == context_mode::seq) {
			now += duration;
			len += duration;
		} else if (duration > len)
			len = duration;
		if (params.mode_ == context_mode::seq)
			UpdateSliders(now, duration, params);
		return;
	}
	default:
		throw EParser().msg(name + ": Bad sequence type.");
	};
}

void Parser::DoAmpAdjust(Blob &blob, ParseParams &params) {
	if (blob.hasFlag("off"))
		params.amp_adjust_ = AmpAdjust();
	else {
		double e = blob["power"].asDouble(0.0, 1.0), f = 1;
		if (blob.hasKey("standard")) {
			NoteValue standard = params.gamut_.Note(blob["standard"].atom());
			f = params.gamut_.FreqMultStandard(standard);
		}
		params.amp_adjust_ = AmpAdjust(e, f);
	}
}

void Parser::DoTranspose(Blob &blob, ParseParams &params) {
	if (blob.hasKey("rel")) {
		const NoteValue last_note = params.last_note_, rel_note =
				params.gamut_.Note(blob["rel"].atom());
		const double mult_last_note = params.gamut_.FreqMultStandard(last_note),
				mult_rel_note = params.gamut_.FreqMultStandard(rel_note);
		params.transpose_ *= mult_last_note / mult_rel_note;
		params.last_note_ = rel_note;
	} else if (blob.hasKey("Hz"))
		params.transpose_ = blob["Hz"].asDouble(0, DBL_MAX) / standard_pitch_;
	else if (blob.hasKey("f"))
		params.transpose_ *= blob["f"].asDouble(0, DBL_MAX);
	else {
		const NoteValue note = params.gamut_.Note(blob.atom());
		params.transpose_ = params.gamut_.FreqMultStandard(note);
	}
}

void Parser::DoTransposeRandom(Blob &blob, ParseParams &params) {
	const double min = blob["min"].asDouble(0, DBL_MAX) / standard_pitch_, max =
			blob["max"].asDouble(0, DBL_MAX) / standard_pitch_;
	params.transpose_ = exp(Rand.uniform(log(min), log(max)));
}

void Parser::DoIntonal(Blob &blob, ParseParams &params) {
	NoteValue last_note = params.last_note_;
	if (blob.hasKey("rel"))
		last_note = params.gamut_.Note(blob["rel"].atom());
	const double mult_last_pre = params.gamut_.FreqMultStandard(last_note);
	if (blob.hasKey("gamut"))
		params.gamut_.ParseBlob(blob["gamut"].ifFunction(), false);
	else if (blob.hasKey("tuning"))
		params.gamut_.TuningBlob(blob["tuning"].ifFunction(), false);
	else
		throw EParser().msg(
				"Intonal needs tuning or gamut.\n" + blob.ErrorString());
	const double mult_last_post = params.gamut_.FreqMultStandard(last_note);
	params.transpose_ *= mult_last_pre / mult_last_post;
}

void Parser::DoStereoRandom(Blob &blob, ParseParams &params) {
	const double left = blob["left"].asDouble(-1, 1), right =
			blob["right"].asDouble(-1, 1), position = Rand.uniform(left, right);
	params.articulation_.stereo_ = Stereo::Position(position);
}

void Parser::DoAmpRandom(Blob &blob, ParseParams &params) {
	const double min = BuildAmplitude(blob["min"]), max = BuildAmplitude(
			blob["max"]);
	if ((min <= 0) || (max <= 0))
		throw EParser().msg(
				"Min/max amp must be positive.\n" + blob.ErrorString());
	params.amp_ = exp(Rand.uniform(log(min), log(max)));
}

void Parser::DoS(Blob &blob, ParseParams &params) {
	switch (blob.ifFunction().children_.size()) {
	case 0:
		params.last_note_ = params.gamut_.Offset(params.last_note_, 1, 0.0, 0);
		break;
	case 1:
		params.last_note_ = params.gamut_.Offset(params.last_note_,
				blob[0].asInt(), 0.0, 0);
		break;
	case 2:
		params.last_note_ = params.gamut_.Offset(params.last_note_,
				blob[0].asInt(), blob[1].asDouble(), 0);
		break;
	case 3:
		params.last_note_ = params.gamut_.Offset(params.last_note_,
				blob[0].asInt(), blob[1].asDouble(), blob[2].asInt());
		break;
	default:
		throw EParser().msg(
				"Syntax error in S(...) function.\n" + blob.ErrorString());
	}
}

void Parser::DoOneOf(Blob &blob, Sequence &sequence, ParseParams &params,
		double &now, double &len, bool make_music, int which) {
	const int count = blob.children_.size();
	if (count == 0)
		throw EParser().msg("oneof(...) needs options." + blob.ErrorString());
	Blob &choice = (which >= 0) ? blob[which] : blob[Rand.uniform(count)];
	Window window;
	if (!choice.isBlock())
		choice = choice.Wrap('(');
	if ((choice.delimiter_ == '[') || (choice.delimiter_ == '<')
			|| (choice.delimiter_ == '{')) {
		ParseParams P1 = params;
		window = NotesModeBlob(choice, sequence, P1, now, make_music);
	} else
		window = NotesModeBlob(choice, sequence, params, now, make_music);
	if (params.mode_ == context_mode::seq) {
		now += window.start();
		len += window.start();
		if (choice.delimiter_ != '(')
			UpdateSliders(now, window.start(), params);
	} else if (window.start() > len)
		len = window.start();
}

void Parser::DoShuffle(Blob &blob, Sequence &sequence, ParseParams &params,
		double &now, double &len, bool make_music) {
	bool replace = false;
	if (blob.hasKey("replace"))
		replace = blob["replace"].asBool();
	Blob &list = blob["from"];
	const int count = list.children_.size();
	int sample_size = 0;
	if (blob.hasKey("n"))
		sample_size = blob["n"].asInt(0, INT_MAX);
	else if (!replace)
		sample_size = count;
	else
		throw EParser().msg(
				"shuffle() with 'replace' needs 'n'.\n" + blob.ErrorString());
	if (count == 0)
		throw EParser().msg("oneof(...) needs options." + blob.ErrorString());
	if ((!replace) && (sample_size > count))
		throw EParser().msg(
				"shuffle() needs a smaller n.\n" + blob.ErrorString());
	std::vector<int> use_count(sample_size, 0);
	for (int index = 0; index < sample_size; index++) {
		int choice = 0;
		do {
			choice = Rand.uniform(count);
		} while (use_count[choice] && !replace);
		DoOneOf(list, sequence, params, now, len, make_music, choice);
		use_count[choice]++;
	}

}

void Parser::DoUnfold(Blob &blob, Sequence &sequence, ParseParams &params,
		double &now, double &len, bool make_music, bool fill) {
	const int repeats = (fill) ? 0 : blob["n"].asInt(0, INT_MAX);
	const double duration = (fill) ? blob["t"].asDouble(0.0, LongTime) : 0.0,
			tend = now + duration;
	Blob &music_blob = blob["music"];
	if (!music_blob.isBlock(false))
		throw EParser().msg(
				"No instruction block provided.\n" + blob.ErrorString());
	int index = 0;
	bool more_repeats = true;
	while (more_repeats) {
		Window window;
		if (music_blob.delimiter_ != '(') {
			ParseParams local_params = params;
			window = NotesModeBlob(music_blob, sequence, local_params, now,
					make_music);
		} else
			window = NotesModeBlob(music_blob, sequence, params, now,
					make_music);
		if (params.mode_ == context_mode::seq) {
			now += window.start();
			len += window.start();
			if (music_blob.delimiter_ != '(')
				UpdateSliders(now, window.start(), params);
		} else if (window.start() > len)
			len = window.start();
		++index;
		if (fill) {
			if (now >= tend - ToleranceTime)
				more_repeats = false;
		} else {
			if (index == repeats)
				more_repeats = false;
		}
	}
}

void Parser::DoForEach(Blob &blob, Sequence &sequence, ParseParams &params,
		double &now, double &len, bool makemusic) {
	const std::string name = blob["var"].atom();
	Blob do_blob = blob["do"], &in_blob = blob["in"].ifFunction();
	if (!do_blob.isBlock(false))
		throw EParser().msg(
				"No 'do' block provided.\n" + do_blob.ErrorString());
	for (auto &item : in_blob.children_) {
		if (dictionary_.Find(name).isNull())
			EParser().msg(name + ": Object already exists.");
		if (item.isToken()) {
			item = item.Wrap(ascii::STX);
			item.delimiter_ = '(';
		};
		if (!item.isBlock(false))
			throw EParser().msg(
					"No instruction block provided.\n" + item.ErrorString());
		if (item.delimiter_ != '(')
			throw EParser().msg(
					"Definitions must include matching () delimeters.\n"
							+ item.ErrorString());
		dictionary_.Insert(DictionaryItem(dic_item_type::macro), name).getMacro() =
				item;
		Window window;
		if (do_blob.delimiter_ != '(') {
			ParseParams local_params = params;
			window = NotesModeBlob(do_blob, sequence, local_params, now,
					makemusic);
		} else
			window = NotesModeBlob(do_blob, sequence, params, now, makemusic);
		if (params.mode_ == context_mode::seq) {
			now += window.start();
			len += window.start();
			if (do_blob.delimiter_ != '(')
				UpdateSliders(now, window.start(), params);
		} else if (window.start() > len)
			len = window.start();
		dictionary_.Delete(name, false);
	}
}

// Sample mode functions

void Parser::LoadLibrary(std::string file_name) {
	std::ifstream file(file_name.c_str(), std::ios::in);
	if (file.is_open()) {
		Blob blob;
		Message("<parsing  " + file_name + ">");
		blob.Parse(file);
		ParseBlobs(blob);
		file.close();
		Message("<finished " + file_name + ">");
	} else
		throw EFile().msg(file_name + ": Loading library failed");
}

parse_exit Parser::ParseString(std::string input) {
	Blob blob;
	std::istringstream stream(input, std::istringstream::in);
	blob.Parse(stream);
	return ParseBlobs(blob);
}

parse_exit Parser::ParseImmediate() {
	std::string input;
	parse_exit exit_code = parse_exit::exit;
	do
		try {
			std::cout << "BoxyLady$ ";
			getline(std::cin, input);
			if (!std::cin.good())
				exit_code = parse_exit::end;
			else if (input.length())
				exit_code = ParseString(input);
		} catch (EQuit&) {
			throw;
		} catch (EBaseError &error) {
			std::cerr << error.what() << "\n";
		} while (exit_code != parse_exit::end);
	return exit_code;
}

parse_exit Parser::ParseBlobs(Blob &blob) {
	parse_exit exit_code = parse_exit::exit;
	for (auto &instruction : blob.children_) {
		if (instruction.isToken()) {
			std::string token = instruction.val_;
			if (token[0] == '\\') {
				std::string name = token.erase(0, 1);
				DictionaryItem &item = dictionary_.Find(name);
				if (item.isNull())
					throw EParser().msg(name + ": No such object.");
				DictionaryMutex mutex(item);
				if (item.isMacro())
					ParseBlobs(item.getMacro());
				else
					throw EParser().msg(name + ": Is not a macro.");
				continue;
			}
			EParser().msg(
					token + ":" + instruction.val_
							+ ": Unknown command. () missing?");
		}
		instruction.AssertFunction();
		const std::string token = instruction.key_;
		if (token == "end") {
			exit_code = parse_exit::end;
			break;
		} else if (token == "quit")
			throw EQuit().msg("quit()");
		else if (token == "--version")
			std::cout << BootInformation;
		else if (token == "BoxyLady") {
			if (VersionNumber != instruction.atom())
				Message(
						"This is not the BoxyLady version you are looking for.");
		} else if (token == "--help")
			std::cout << BootHelp;
		else if (token == "--user")
			supervisor_ = (instruction.asInt() == 1);
		else if (token == "--poem")
			std::cout << Poem;
		else if (token == "--i")
			ParseImmediate();
		else if (token == "echo")
			DoEcho(instruction);
		else if (token == "rem") {
		} else if (token == "library")
			LoadLibrary(instruction.atom());
		else if (token == "--library") {
			try {
				LoadLibrary(instruction.atom());
			} catch (EFile &error) {
				std::cerr << error.what() << "\n";
			}
		} else if (token == "--messages")
			verbosity_ = BuildVerbosity(instruction.atom());
		else if (token == "default:sample_rate")
			default_sample_rate_ = instruction.asDouble(MinSampleRate,
					MaxSampleRate);
		else if (token == "default:max_instrument_length")
			max_instrument_duration_ = instruction.asDouble(0,
					MaxSequenceLength);
		else if (token == "default:standard_pitch")
			standard_pitch_ = instruction.asDouble(220, 880);
		else if (token == "default:click_time")
			Envelope::gate_time = instruction.asDouble(0, 0.02);
		else if (token == "default:interpolation")
			Sequence::linear_interpolation = instruction.asBool();
		else if (token == "seed") {
			if (instruction.hasKey("val"))
				Rand.SetSeed(instruction["val"].asInt());
			else
				Rand.AutoSeed();
		} else if (token == "synth")
			Synth(instruction);
		else if (token == "def")
			MakeMacro(instruction, macro_type::macro);
		else if (token == "seq")
			MakeMusic(instruction);
		else if (token == "quick")
			QuickMusic(instruction);
		else if (token == "global")
			GlobalDefaults(instruction);
		else if (token == "list") {
			if (Parser::messages())
				dictionary_.ListEntries(instruction);
		} else if (token == "defrag")
			DoDefrag();
		else if (token == "access")
			DoAccess(instruction);
		else if (token == "patch")
			LoadPatch(instruction);
		else if (token == "copy")
			Clone(instruction);
		else if (token == "combine")
			Combine(instruction);
		else if (token == "mix")
			Mix(instruction);
		else if (token == "split")
			Split(instruction);
		else if (token == "rechannel")
			Rechannel(instruction);
		else if (token == "cut")
			Cut(instruction);
		else if (token == "paste")
			Paste(instruction);
		else if (token == "histogram")
			Histogram(instruction);
		else if (token == "delete")
			DoDelete(instruction);
		else if (token == "rename")
			RenameEntry(instruction);
		else if (token == "output")
			SavePatch(instruction);
		else if (token == "command:mp3")
			mp3_encoder_ = instruction.atom();
		else if (token == "play")
			Play(instruction);
		else if (token == "command:play")
			file_play_ = instruction.atom();
		else if (token == "metadata")
			Metadata(instruction);
		else if (token == "external")
			ExternalProcessing(instruction);
		else if (token == "create")
			DoCreate(instruction);
		else if (token == "instrument")
			DoInstrument(instruction);
		else if (token == "resize")
			Resize(instruction);
		else if (token == "crossfade")
			CrossFade(instruction);
		else if (token == "fade")
			Fade(instruction);
		else if (token == "amp")
			Balance(instruction);
		else if (token == "reverb")
			Echo(instruction);
		else if (token == "karplus_strong")
			KarplusStrong(instruction);
		else if (token == "chowning")
			Chowning(instruction);
		else if (token == "modulator")
			Modulator(instruction);
		else if (token == "reverse")
			Reverse(instruction);
		else if (token == "tremolo")
			Tremolo(instruction);
		else if (token == "lowpass")
			LowPass(instruction);
		else if (token == "highpass")
			HighPass(instruction);
		else if (token == "bandpass")
			BandPass(instruction);
		else if (token == "fourier_gain")
			FourierGain(instruction);
		else if (token == "fourier_bandpass")
			FourierBandpass(instruction);
		else if (token == "fourier_clean")
			FourierClean(instruction);
		else if (token == "integrate")
			Integrate(instruction);
		else if (token == "clip")
			Clip(instruction);
		else if (token == "abs")
			Abs(instruction);
		else if (token == "fold")
			Fold(instruction);
		else if (token == "octave")
			OctaveEffect(instruction);
		else if (token == "fourier_shift")
			FourierShift(instruction);
		else if (token == "repeat")
			Repeat(instruction);
		else if (token == "flags")
			Flags(instruction);
		else if (token == "envelope")
			DoEnvelope(instruction);
		else if (token == "distort")
			Distort(instruction);
		else if (token == "chorus")
			Chorus(instruction);
		else if (token == "offset")
			Offset(instruction);
		else if (token == "ringmod")
			RingModulation(instruction);
		else if (token == "flange")
			Flange(instruction);
		else if (token == "bitcrusher")
			BitCrusher(instruction);
		else if (token == "bias")
			Bias(instruction);
		else if (token == "debias")
			Debias(instruction);
		else if (token == "ext")
			ExternalCommand(instruction);
		else if (token == "filter_sweep")
			FilterSweep(instruction);
		else
			throw EParser().msg(
					token + ":" + instruction.val_
							+ ": Unknown command. () missing?");
	}
	return exit_code;
}

void Parser::MakeMacro(Blob &blob, macro_type type) {
	std::string name;
	for (auto &macro_blob : blob.children_) {
		const std::string name = macro_blob.key_;
		if (dictionary_.Exists(name))
			EParser().msg(name + ": Object already exists.");
		if (!macro_blob.isBlock(false))
			throw EParser().msg(
					name + ": No instruction block provided.\n"
							+ blob.ErrorString());
		if (macro_blob.delimiter_ != '(')
			throw EParser().msg(
					"Definitions must include matching () delimeters.\n"
							+ blob.ErrorString());
		dictionary_.Insert(DictionaryItem(dic_item_type::macro), name).getMacro() =
				macro_blob;
		Message("Created macro [" + name + "]", verbosity_type::messages);
	}
}

void Parser::LoadPatch(Blob &blob) {
	const std::string sequence_name = blob["@"].atom(), file_name =
			blob["file"].atom();
	file_format format = file_format::RIFF_wav;
	Sequence &sequence = dictionary_.InsertSequence(sequence_name);
	sequence.LoadFromFile(file_name, format);
	SampleType sample_type = sequence.getType();
	if (blob.hasKey("type")) {
		sample_type = BuildSampleType(blob["type"]);
		sequence.setType(sample_type);
	}
	ShowMessage("Loaded patch `" + file_name + "` as [" + sequence_name + "]",
			blob);
}

void Parser::Clone(Blob &blob) {
	verbosity_type verbosity = verbosity_type::none;
	if (blob.hasKey("shh!"))
		verbosity = BuildVerbosity(blob["shh!"].atom());
	const std::string source_name = blob[0].atom(); //s
	Sequence &source_sequence = dictionary_.FindSequence(source_name);
	bool first = true;
	for (auto &item : blob.children_) {
		if (item.key_ == "shh!")
			continue;
		if (first) {
			first = false;
			continue;
		};
		const std::string new_name = item.atom(); //s
		if (dictionary_.Exists(new_name))
			throw EParser().msg(new_name + ": Object already exists.");
		Sequence &new_sequence = dictionary_.InsertSequence(new_name);
		new_sequence = source_sequence;
		Message("Copied patch [" + source_name + "] as [" + new_name + "]",
				verbosity);
	}
}

void Parser::Combine(Blob &blob) {
	const std::string new_name = blob["@"].atom(), left = blob["l"].atom(),
			right = blob["r"].atom();
	Sequence &left_sequence = dictionary_.FindSequence(left), &right_sequence =
			dictionary_.FindSequence(right);
	if (dictionary_.Exists(new_name))
		throw EParser().msg(new_name + ": Object already exists.");
	Sequence &new_sequence = dictionary_.InsertSequence(new_name);
	new_sequence.Combine(left_sequence, right_sequence);
	ShowMessage(
			"Combined patches [" + left + "] and [" + right + "] as ["
					+ new_name + "]", blob);
}

void Parser::Mix(Blob &blob) {
	const std::string new_name = blob["@"].atom(), a_name =
			blob["a_name"].atom(), b_name = blob["b_name"].atom();
	const int channels = blob["channels"].asInt(1, MaxChannels);
	Sequence &a_sequence = dictionary_.FindSequence(a_name), &b_sequence =
			dictionary_.FindSequence(b_name);
	const Stereo a_stereo = BuildStereo(blob["stereo_a"]), b_stereo =
			BuildStereo(blob["stereo_b"]);
	if (dictionary_.Exists(new_name))
		throw EParser().msg(new_name + ": Object already exists.");
	Sequence &new_sequence = dictionary_.InsertSequence(new_name);
	new_sequence.Mix(a_sequence, b_sequence, a_stereo, b_stereo, channels);
	ShowMessage(
			"Mixed patches [" + a_name + "] and [" + b_name + "] as ["
					+ new_name + "]", blob);
}

void Parser::Split(Blob &blob) {
	const std::string nname = blob["@"].atom(), left_name = blob["l"].atom(),
			right_name = blob["r"].atom();
	Sequence &source = dictionary_.FindSequence(nname);
	if (dictionary_.Exists(left_name))
		throw EParser().msg(nname + ": Object already exists.");
	if (dictionary_.Exists(right_name))
		throw EParser().msg(nname + ": Object already exists.");
	Sequence &left_sequence = dictionary_.InsertSequence(left_name);
	Sequence &right_sequence = dictionary_.InsertSequence(right_name);
	source.Split(left_sequence, right_sequence);
	ShowMessage(
			"Split patches [" + left_name + "] and [" + right_name + "] from ["
					+ nname + "]", blob);
}

void Parser::Rechannel(Blob &blob) {
	Sequence &sequence = dictionary_.FindSequence(blob);
	const int channels = blob["channels"].asInt(1, 2);
	sequence.Rechannel(channels);
}

void Parser::Cut(Blob &blob) {
	Sequence &sequence = dictionary_.FindSequence(blob);
	Window window = BuildWindow(blob);
	sequence.Cut(window);
}

void Parser::Paste(Blob &blob) {
	const std::string new_name = blob["@"].atom(), source_name =
			blob["source"].atom();
	Sequence &source_sequence = dictionary_.FindSequence(source_name);
	if (dictionary_.Exists(new_name))
		throw EParser().msg(new_name + ": Object already exists.");
	Sequence &new_sequence = dictionary_.InsertSequence(new_name);
	Window window = BuildWindow(blob);
	new_sequence.Paste(source_sequence, window);
	ShowMessage("Pasted from patch [" + source_name + "] as [" + new_name + "]",
			blob);
}

void Parser::Histogram(Blob &blob) {
	const bool plot = (blob.hasFlag("plot")), scale = (blob.hasFlag("scale"));
	double clip = 0.0;
	blob.tryWriteDouble("clip", clip, 0, 1);
	dictionary_.FindSequence(blob).Histogram(scale, plot, clip);
}

void Parser::RenameEntry(Blob &blob) {
	const std::string old_name = blob[0].atom(), new_name = blob[1].atom(); //s
	DictionaryItem &item = dictionary_.Find(old_name);
	if (item.isNull())
		throw EParser().msg(old_name + ": No such object.");
	if (dictionary_.Exists(new_name))
		throw EParser().msg(new_name + ": Object already exists.");
	if (item.ProtectionLevel() > dic_item_protection::normal)
		throw EParser().msg(
				new_name + ": Object is protected and cannot be renamed.");
	if (!item.ValidName(new_name))
		throw EParser().msg(new_name + ": Illegal character in name.");
	dictionary_.Rename(old_name, new_name);
	ShowMessage("Renamed [" + old_name + "] to [" + new_name + "]", blob);
}

void Parser::SavePatch(Blob &blob) {
	std::string format_name = "boxy", temp_file_name;
	blob.tryWriteString("format", format_name);
	bool write_metadata = false;
	blob.tryWriteBool("metadata", write_metadata);
	const std::string file_name = blob["file"].atom(), sequence_name =
			blob["@"].atom();
	file_format format = file_format::boxy;
	if (format_name == "boxy")
		format = file_format::boxy;
	else if (format_name == "wav")
		format = file_format::RIFF_wav;
	else if (format_name == "mp3")
		format = file_format::mp3;
	else
		throw EParser().msg(
				"File type specified [" + format_name + "] not recognised.");
	Sequence &sequence = dictionary_.FindSequence(sequence_name);
	switch (format) {
	case file_format::boxy: //no break
	case file_format::RIFF_wav:
		sequence.SaveToFile(file_name, format, write_metadata);
		break;
	case file_format::mp3:
		temp_file_name = TempFilename("wav").file_name();
		sequence.SaveToFile(temp_file_name, file_format::RIFF_wav, false);
		Mp3Encode(temp_file_name, file_name, sequence.metadata());
		remove(temp_file_name.c_str());
		break;
	default:
		;
	}
	ShowMessage("Saved patch [" + sequence_name + "] to `" + file_name + "`",
			blob);
}

int Parser::DoPlay(std::string file_name, std::string command) {
	CheckSystem();
	command.replace(command.find("%file"), 5, file_name);
	std::cout << "$ " << command << "\n";
	int return_code = system(command.c_str());
	return return_code;
}

void Parser::CommandReplaceString(std::string &command, std::string what,
		std::string with, std::string why) {
	const size_t found = command.find(what);
	if (found == std::string::npos)
		throw EUserParse().msg(
				"Token replacement: cannot find '" + what + "' in command for '"
						+ why + "'.");
	command.replace(found, what.length(), with);
}

int Parser::ExternalCommand(Blob &blob) const {
	const std::string token = blob.atom();
	int return_code = system(token.c_str());
	if (return_code != 0)
		throw EParser().msg(
				"External programme call: return code from system call indicates error.");
	return return_code;
}

void Parser::ExternalProcessing(Blob &blob) {
	CheckSystem();
	std::string source_file_name = "", destination_file_name = "",
			sequence_name = "", command = "", argument = "";
	if (blob.hasKey("@"))
		sequence_name = blob["@"].atom();
	else
		throw EParser().msg("External operation: no sequence_name specified.");
	if (blob.hasKey("command"))
		command = blob["command"].atom();
	else
		throw EParser().msg("External operation: no command.");
	Sequence &sequence = dictionary_.FindSequence(sequence_name);
	source_file_name = TempFilename("wav").file_name();
	destination_file_name = TempFilename("wav").file_name();
	CommandReplaceString(command, "%source", source_file_name, "external()");
	CommandReplaceString(command, "%dest", destination_file_name, "external()");
	ShowMessage("$ " + command, blob);
	sequence.SaveToFile(source_file_name, file_format::RIFF_wav, false);
	Sequence temp_sequence;
	temp_sequence.CopyType(sequence);
	sequence.Clear();
	int return_code = system(command.c_str());
	if (return_code != 0)
		throw EParser().msg(
				"External programme call: return code from system call indicates error.");
	remove(source_file_name.c_str());
	sequence.LoadFromFile(destination_file_name, file_format::RIFF_wav);
	sequence.CopyType(temp_sequence);
	remove(destination_file_name.c_str());
}

void Parser::Play(Blob &blob) {
	if (file_play_.length() == 0)
		throw EParser().msg("File play: no command string set.");
	std::string file_name = "", sequence_name = "", argument = "", command =
			file_play_;
	blob.tryWriteString("file", file_name);
	blob.tryWriteString("arg", argument);
	blob.tryWriteString("@", sequence_name);
	CommandReplaceString(command, "%arg", argument, "play()");
	if (file_name.length()) {
		DoPlay(command, file_play_);
	} else if (sequence_name.length()) {
		Sequence &sequence = dictionary_.FindSequence(sequence_name);
		file_name = TempFilename("wav").file_name();
		sequence.SaveToFile(file_name, file_format::RIFF_wav, false);
		int return_code = DoPlay(file_name, command);
		if (return_code != 0)
			Message(
					"\nFile play: error returned from external programme. ["
							+ file_play_ + "]");
		remove(file_name.c_str());
	} else
		EParser().msg("File play: nothing to play.");
}

void Parser::Metadata(Blob &blob) {
	Sequence &sequence = dictionary_.FindSequence(blob);
	for (auto &child : blob.children_) {
		const std::string key = child.key_;
		if (child.key_ == "@")
			continue;
		sequence.metadata()[key] = child.val_;
	}
}

void Parser::Mp3Encode(std::string source_file_name,
		std::string destination_file_name, MetadataList metadata) {
	CheckSystem();
	std::string command = mp3_encoder_;
	command = metadata.Mp3CommandUpdate(command);
	CommandReplaceString(command, "%source", source_file_name, "mp3encode()");
	CommandReplaceString(command, "%dest", destination_file_name,
			"mp3encode()");
	int return_code = system(command.c_str());
	if (return_code != 0)
		throw EParser().msg(
				"Conversion to MP3: return code from system call indicates error.");
}

void Parser::DoEcho(Blob &blob) {
	if (blob.hasKey("@")) {
		if (blob.children_.size() > 1)
			throw EParser().msg(
					"Echo requires one argument.\n" + blob.ErrorString());
		const std::string name = blob["@"].atom();
		DictionaryItem &item = dictionary_.Find(name);
		if (item.isNull())
			throw EParser().msg(name + ": No such object.");
		if (item.isMacro())
			Message(item.getMacro().Dump());
		else if (item.isPatch()) {
			std::cout << "\nPlot of [" << name << "]\n";
			item.getSequence().Plot();
		}
	} else
		Message(blob.atom());
}

void Parser::DoCreate(Blob &blob) {
	const std::string name = blob["@"].atom();
	const int channels = blob["channels"].asInt(1, MaxChannels);
	if ((channels < 1) || (channels > 2))
		throw EParser().msg("Only 1 or 2 channels are currently supported.");
	const double t_length = blob["len"].ifFunction()[0].asDouble(0,
			MaxSequenceLength), p_length =
			(blob["len"].children_.size() == 2) ?
					blob["len"][1].asDouble(0, MaxSequenceLength) : t_length;
	SampleType type = BuildSampleType(blob["type"]);
	Sequence &sequence = dictionary_.InsertSequence(name);
	sequence.CreateSilenceSeconds(channels, type.sample_rate_, t_length,
			p_length);
	sequence.setType(type);
	ShowMessage("Created patch [" + name + "]", blob);
}

void Parser::DoInstrument(Blob &blob) {
	dictionary_.Delete("instrument");
	const int channels = blob["channels"].asInt(1, MaxChannels);
	if ((channels < 1) || (channels > 2))
		throw EParser().msg("Only 1 or 2 channels are currently supported.");
	double t_length = instrument_duration_, p_length = t_length;
	if (blob.hasKey("len"))
		t_length = blob["len"].ifFunction()[0].asDouble(0, MaxSequenceLength), p_length =
				(blob["len"].children_.size() == 2) ?
						blob["len"][1].asDouble(0, MaxSequenceLength) : t_length;
	SampleType type = BuildSampleType(blob["type"]);
	Sequence &sequence = dictionary_.InsertSequence("instrument");
	sequence.CreateSilenceSeconds(channels, type.sample_rate_, t_length,
			p_length);
	sequence.setType(type);
	ShowMessage("Created patch [instrument]", blob);
}

void Parser::Resize(Blob &blob) {
	Sequence &sequence = dictionary_.FindSequence(blob);
	const std::string mode = blob["mode"].atom();
	if (mode == "auto") {
		double threshold = 0.0;
		blob.tryWriteDouble("threshold", threshold, 0.0, 1.0);
		sequence.AutoResize(threshold);
		return;
	}
	const double t_length = blob["len"].ifFunction()[0].asDouble(0,
			MaxSequenceLength), p_length =
			(blob["len"].children_.size() == 2) ?
					blob["len"][1].asDouble(0, MaxSequenceLength) : t_length;
	if (mode == "absolute")
		sequence.Resize(t_length, p_length, false);
	else if (mode == "relative")
		sequence.Resize(t_length, p_length, true);
	else
		throw EParser().msg(mode + ": Unknown resize mode.");
}

void Parser::DoDefrag() {
	for (auto &item : dictionary_) {
		if (item.second.isPatch())
			item.second.getSequence().Defrag();
	}
}

void Parser::DoAccess(Blob &blob) {
	verbosity_type verbosity = verbosity_type::messages;
	if (blob.hasKey("shh!"))
		verbosity = BuildVerbosity(blob["shh!"].atom());
	for (auto &child : blob.children_) {
		if (child.key_ == "shh!")
			continue;
		const std::string name = child.key_, atom = child.atom();
		DictionaryItem &item = dictionary_.Find(name);
		if (item.isNull())
			throw EDictionary().msg(name + ": No such object.");
		if ((!supervisor_)
				&& (item.ProtectionLevel() == dic_item_protection::system))
			throw EParser().msg(name + ": Cannot change protection level.");
		dic_item_protection protection_level = BuildProtection(atom);
		if ((!supervisor_) && (protection_level == dic_item_protection::system))
			throw EParser().msg(
					name + ": Cannot change to system level protection.");
		item.Protect(protection_level);
		Message(
				"Changed slot [" + name + "] protection level to '" + atom
						+ "'.", verbosity);
	}
}

void Parser::DoDelete(Blob &blob) {
	verbosity_type verbosity = verbosity_type::none;
	if (blob.hasKey("shh!"))
		verbosity = BuildVerbosity(blob["shh!"].atom());
	for (auto &child : blob.children_) {
		if (child.key_ == "shh!")
			continue;
		const std::string name = child.atom();
		if (name == "*") {
			dictionary_.Clear(true);
			Message("Cleared all dictionary entries");
		} else {
			if (dictionary_.Delete(name, true))
				Message("Deleted [" + name + "]", verbosity);
			else
				Message(
						"Slot [" + name
								+ "] doesn't exist, is protected, or in use.",
						verbosity);
		}
	}
}

void Parser::CrossFade(Blob &blob) {
	const std::string start_name = blob["start"].atom(), end_name =
			blob["end"].atom(), new_name = blob["@"].atom();
	Sequence &start_sequence = dictionary_.FindSequence(start_name),
			&end_sequence = dictionary_.FindSequence(end_name);
	if (dictionary_.Exists(new_name))
		throw EParser().msg(new_name + ": Object already exists.");
	Sequence &new_sequence = dictionary_.InsertSequence(new_name),
			temp_sequence = end_sequence;
	new_sequence = start_sequence;
	new_sequence.CrossFade(Fader::FadeOut().Linear());
	temp_sequence.CrossFade(Fader::FadeIn().Linear());
	new_sequence.Overlay(temp_sequence, 0);
	ShowMessage(
			"Crossfaded patches [" + start_name + "] and [" + end_name
					+ "] as [" + new_name + "]", blob);
}

void Parser::Fade(Blob &blob) {
	const std::string mode = blob["mode"].atom();
	Fader fader;
	Sequence &sequence = dictionary_.FindSequence(blob);
	if (mode == "fade_in")
		fader = Fader::FadeIn();
	else if (mode == "fade_out")
		fader = Fader::FadeOut();
	else if (mode == "linear_fade_in")
		fader = Fader::FadeIn().Linear();
	else if (mode == "linear_fade_out")
		fader = Fader::FadeOut().Linear();
	else if (mode == "pan_swap")
		fader = Fader::PanSwap();
	else if (mode == "pan_centre")
		fader = Fader::PanCentre();
	else if (mode == "pan_edge")
		fader = Fader::PanEdge();
	else if (mode == "manual") {
		const Stereo start_parallel = BuildStereo(blob["start_a"]),
				start_crossed = BuildStereo(blob["start_x"]), end_parallel =
						BuildStereo(blob["end_a"]), end_crossed = BuildStereo(
						blob["end_x"]);
		Fader temp_fader(Crosser(start_parallel, start_crossed),
				Crosser(end_parallel, end_crossed));
		if (blob.hasFlag("linear"))
			temp_fader.Linear();
		else if (blob.hasFlag("log"))
			temp_fader.Logarithmic();
		fader = Fader(temp_fader);
	} else
		throw EParser().msg(mode + ": Unknown fade mode.");
	if (blob.hasFlag("mirror"))
		fader.Mirror();
	sequence.CrossFade(fader);
}

void Parser::Balance(Blob &blob) {
	std::string mode_string;
	if (!blob.tryWriteString("mode", mode_string))
		mode_string = "vol";
	Sequence &sequence = dictionary_.FindSequence(blob);
	if (mode_string == "vol")
		sequence.CrossFade(Fader::Amp(BuildAmplitude(blob["a"])));
	else if (mode_string == "lr")
		sequence.CrossFade(Fader::AmpLR(BuildStereo(blob["a"])));
	else if (mode_string == "stereo")
		sequence.CrossFade(
				Fader::AmpStereo(BuildStereo(blob["a"]),
						BuildStereo(blob["x"])));
	else if (mode_string == "delay") {
		const Stereo parallel = BuildStereo(blob["a"]), crossed = BuildStereo(
				blob["x"]);
		const double delay = blob["delay"].asDouble();
		sequence.DelayAmp(parallel, crossed, delay);
	} else if (mode_string == "inverse")
		sequence.CrossFade(Fader::AmpInverse());
	else if (mode_string == "inverse_lr")
		sequence.CrossFade(Fader::AmpInverseLR());
	else
		throw EParser().msg(mode_string + ": Unknown balance operation.");
}

void Parser::Echo(Blob &blob) {
	const double delay = blob["delay"].asDouble(), amp = BuildAmplitude(
			blob["a"]);
	const int count = (blob.hasKey("n")) ? blob["n"].asInt(1, 1000) : 1;
	const bool resize = blob.hasFlag("resize");
	if (blob.hasKey("bandpass"))
		throw EParser().msg("'Bandpass' deprecated: use 'filter'");
	FilterVector filters;
	if (blob.hasKey("filter"))
		filters = BuildFilters(blob["filter"]);
	dictionary_.FindSequence(blob).Echo(delay, amp, count, filters, resize);
}

void Parser::Tremolo(Blob &blob) {
	const Wave wave = BuildWave(blob["wave"]);
	dictionary_.FindSequence(blob).Ring(nullptr, wave, false, 0.5,
			1.0 - 0.5 * wave.amp());
}

void Parser::RingModulation(Blob &blob) {
	double amp = 1.0, bias = 0.0;
	if (blob.hasKey("a"))
		amp = BuildAmplitude(blob["a"]);
	blob.tryWriteDouble("bias", bias, -1000, 1000);
	Wave wave;
	if (blob.hasKey("wave")) {
		wave = BuildWave(blob["wave"]);
		dictionary_.FindSequence(blob).Ring(nullptr, wave, false, amp, bias);
	} else if (blob.hasKey("with")) {
		Sequence &with_sequence = dictionary_.FindSequence(blob["with"].atom());
		dictionary_.FindSequence(blob).Ring(&with_sequence, wave, false, amp,
				bias);
	} else
		throw EParser().msg(
				blob[0].Dump() + ": Unknown ring modulation operation.");
}

Filter Parser::BuildFilter(Blob &blob) const {
	blob.AssertFunction();
	const std::string token = blob.key_;
	if (token == "lowpass")
		return BuildLowPass(blob);
	else if (token == "highpass")
		return BuildHighPass(blob);
	else if (token == "bandpass")
		return BuildBandPass(blob);
	else if (token == "amp")
		return BUildAmpFilter(blob);
	else if (token == "distort")
		return BuildDistort(blob);
	else if (token == "ks:blend")
		return BuildKSBlend(blob);
	else if (token == "ks:reverse")
		return BuildKSReverse(blob);
	else if (token == "fourier_gain")
		return BuildFourierGain(blob);
	else if (token == "fourier_bandpass")
		return BuildFourierBandpass(blob);
	throw EParser().msg(token + ": Unknown filter type.");
}

FilterVector Parser::BuildFilters(Blob &blob) const {
	blob.AssertFunction();
	FilterVector filters;
	for (auto &child : blob.children_)
		filters.push_back(BuildFilter(child));
	return filters;
}

Filter Parser::BuildLowPass(Blob &blob) const {
	const double r = blob["r"].asDouble(1, 1000000);
	const bool mirror = blob.hasFlag("mirror"), wrap = blob.hasFlag("wrap");
	return Filter::LowPass(r, mirror, wrap);
}

Filter Parser::BuildHighPass(Blob &blob) const {
	const double r = blob["r"].asDouble(1, 1000000);
	const bool mirror = blob.hasFlag("mirror"), wrap = blob.hasFlag("wrap");
	return Filter::HighPass(r, mirror, wrap);
}

Filter Parser::BuildBandPass(Blob &blob) const {
	const double frequency = blob["f"].asDouble(1, 100000), bandwidth =
			blob["width"].asDouble(0, 100000), gain = BuildAmplitude(
			blob["gain"]);
	if (gain <= 0.0)
		throw EParser().msg(
				"Bandpass filter requires non-zero, positive gain (on amplitude scale).");
	const bool wrap = blob.hasFlag("wrap");
	return Filter::BandPass(frequency, bandwidth, log10(gain) * 20.0, wrap);
}

Filter Parser::BuildFourierGain(Blob &blob) const {
	const double low_shoulder = blob["low"].asDouble(1, 1000000), low_gain =
			BuildAmplitude(blob["low_gain"]), high_shoulder =
			blob["high"].asDouble(1, 1000000), high_gain = BuildAmplitude(
			blob["high_gain"]);
	if (high_shoulder<low_shoulder)
		throw EParser().msg(
				"Shoulders of Fourier gain filter in the wrong order.");
	if ((low_gain <= 0.0) || (high_gain <=0.0))
		throw EParser().msg(
				"Fourier gain filter filter requires non-zero, positive gain (on amplitude scale).");
	return Filter::FourierGain(low_gain, low_shoulder, high_shoulder, high_gain);
}

Filter Parser::BuildFourierBandpass(Blob &blob) const {
	const double frequency = blob["f"].asDouble(1, 100000), bandwidth =
			blob["width"].asDouble(0, 100000), gain = BuildAmplitude(
			blob["gain"]);
	const bool comb = blob.hasFlag("comb");
	if (gain <= 0.0)
		throw EParser().msg(
				"Bandpass filter requires non-zero, positive gain (on amplitude scale).");
	return Filter::FourierBandpass(frequency, bandwidth, gain).SetFlag(
			filter_direction::comb, comb);
}

void Parser::FilterSweep(Blob &blob) {
	if (blob["start"].children_.size() != 1)
		throw EParser().msg("Filter sweep needs single filter.");
	if (blob["end"].children_.size() != 1)
		throw EParser().msg("Filter sweep needs single filter.");
	const Filter start_filter = BuildFilter(blob["start"][0]), end_filter =
			BuildFilter(blob["end"][0]);
	const int window_count = blob["windows"].asInt(2, INT_MAX);
	dictionary_.FindSequence(blob).WindowedFilter(start_filter, end_filter,
			window_count);
}

void Parser::Integrate(Blob &blob) {
	const double factor = blob["f"].asDouble(0, 100000), leak_per_second =
			blob["leak"].asDouble(0, 100000), constant = blob["c"].asDouble(
			-1.0, 1.0);
	dictionary_.FindSequence(blob).Integrate(factor, leak_per_second, constant);
}

void Parser::Clip(Blob &blob) {
	double min = -1, max = 1;
	if (blob.hasKey("a"))
		min = -(max = BuildAmplitude(blob["a"]));
	else {
		min = blob["min"].asDouble(-1, 1);
		max = blob["max"].asDouble(-1, 1);
	}
	Sequence &sequence = dictionary_.FindSequence(blob);
	sequence.Clip(min, max);
}

void Parser::Abs(Blob &blob) {
	double amp = 1.0;
	blob.tryWriteDouble("a", amp, -1, 1);
	dictionary_.FindSequence(blob).Abs(amp);
}

void Parser::Fold(Blob &blob) {
	const double amp = blob["a"].asDouble();
	dictionary_.FindSequence(blob).Fold(amp);
}

void Parser::OctaveEffect(Blob &blob) {
	double mix_proportion = 1.0;
	blob.tryWriteDouble("p", mix_proportion, 0, 1);
	dictionary_.FindSequence(blob).Octave(mix_proportion);
}

void Parser::FourierShift(Blob &blob) {
	double frequency = blob["f"].asDouble(-100000.0, 100000.0);
	dictionary_.FindSequence(blob).FourierShift(frequency);
}

void Parser::FourierClean(Blob &blob) {
	double min_gain = BuildAmplitude(blob["a"]);
	dictionary_.FindSequence(blob).FourierClean(min_gain);
}

void Parser::Repeat(Blob &blob) {
	const int count = blob["n"].asInt(1, INT_MAX);
	FilterVector filters;
	if (blob.hasKey("filter"))
		filters = BuildFilters(blob["filter"]);
	dictionary_.FindSequence(blob).Repeat(count, filters);
}

void Parser::Flags(Blob &blob, Sequence *sequence) {
	bool sequence_active = sequence;
	if (!sequence_active)
		sequence = &dictionary_.FindSequence(blob);
	SampleType type = sequence->getType();
	type = (sequence_active) ?
			BuildSampleType(blob, type) : BuildSampleType(blob["type"], type);
	sequence->setType(type);
}

void Parser::DoEnvelope(Blob &blob) {
	const Envelope envelope = BuildEnvelope(blob["e"]);
	dictionary_.FindSequence(blob).ApplyEnvelope(envelope);
}

void Parser::Distort(Blob &blob) {
	const double power = blob["power"].asDouble(0.001, 1000);
	dictionary_.FindSequence(blob).Distort(power);
}

void Parser::Chorus(Blob &blob) {
	constexpr double DefaultVibFreq = 5.0;
	const int count = blob["n"].asInt(1, 1000);
	const double offset = blob["offset"].asDouble(0, 10);
	Wave wave =
			(blob.hasKey("vib")) ?
					BuildWave(blob["vib"]) : Wave(DefaultVibFreq, 0.01, 0);
	Sequence &sequence = dictionary_.FindSequence(blob);
	if (blob.hasFlag("stereo")) {
		if (sequence.channels() < 2)
			sequence.Rechannel(2);
		Sequence left_sequence, right_sequence;
		sequence.Split(left_sequence, right_sequence);
		left_sequence.Chorus(count, offset, wave);
		right_sequence.Chorus(count, offset, wave);
		sequence.Combine(left_sequence, right_sequence);
	} else
		sequence.Chorus(count, offset, wave);
}

void Parser::Offset(Blob &blob) {
	const double left_time = blob["l"].asDouble(-MaxSequenceLength,
			MaxSequenceLength), right_time = blob["r"].asDouble(
			-MaxSequenceLength, MaxSequenceLength);
	const bool wrap = blob.hasFlag("wrap");
	Sequence &sequence = dictionary_.FindSequence(blob);
	sequence.OffsetSeconds(left_time, right_time, wrap);
}

void Parser::Flange(Blob &blob) {
	const double freq = blob["f"].asDouble(0, 100000), amp = blob["a"].asDouble(
			0, 1);
	dictionary_.FindSequence(blob).Flange(freq, amp);
}

void Parser::BitCrusher(Blob &blob) {
	const int bits = blob["bits"].asInt(1, 16);
	dictionary_.FindSequence(blob).BitCrusher(bits);
}

void Parser::Bias(Blob &blob) {
	const double level = blob["level"].asDouble(-1, 1);
	dictionary_.FindSequence(blob).Waveform(Wave(0, level, 0), Phaser(), Wave(),
			0, synth_type::constant, 0);
}

void Parser::Debias(Blob &blob) {
	debias_type type = debias_type::mean;
	const std::string type_string = blob["type"].atom();
	if (type_string == "start")
		type = debias_type::start;
	else if (type_string == "end")
		type = debias_type::end;
	else if (type_string == "mean")
		type = debias_type::mean;
	else
		throw EParser().msg(blob.Dump() + ": Unknown debias operation.");
	dictionary_.FindSequence(blob).Debias(type);
}

void Parser::KarplusStrong(Blob &blob) {
	const std::string name = blob["@"].atom();
	Sequence &sequence = dictionary_.InsertSequence(name);
	const int channels = blob["grain"].ifFunction()["channels"].asInt(1,
			MaxChannels);
	SampleType grain_type = BuildSampleType(blob["grain"]["type"]);
	const double grain_frequency = 1.0
			/ blob["grain"]["f"].asDouble(0.01,
					0.5 * double(grain_type.sample_rate_)), len =
			blob["len"].asDouble(grain_frequency, MaxSequenceLength);
	sequence.CreateSilenceSeconds(channels, grain_type.sample_rate_,
			grain_frequency, grain_frequency);
	sequence.setType(
			SampleType(false, false, false, grain_type.sample_rate_, 0.0));
	if (blob.hasKey("synth"))
		Synth(blob["synth"].ifFunction(), &sequence);
	if (blob.hasKey("outer"))
		ParseBlobs(blob["outer"].ifFunction());
	FilterVector repeat_filters, mix_filters;
	if (blob.hasKey("filter"))
		repeat_filters = BuildFilters(blob["filter"]);
	if (blob.hasKey("mix_filter"))
		mix_filters = BuildFilters(blob["mix_filter"]);
	sequence.Repeat(int(len / grain_frequency), repeat_filters);
	sequence.ApplyFilters(mix_filters);
	sequence.Debias(debias_type::end);
	sequence.AutoResize(0.0);
	sequence.AutoAmp();
	sequence.setType(grain_type);
	ShowMessage("Created patch [" + name + "]", blob);
}

void Parser::Chowning(Blob &blob) {
	const std::string name = blob["@"].atom();
	Sequence &sequence = dictionary_.InsertSequence(name);
	const int channels = blob["channels"].asInt(1, MaxChannels);
	SampleType type = BuildSampleType(blob["type"]);
	const double length = blob["len"].asDouble(0, MaxSequenceLength);
	sequence.CreateSilenceSeconds(channels, type.sample_rate_, length, length);
	sequence.setType(type);
	if (blob.hasKey("synth"))
		Synth(blob["synth"], &sequence);
	if (blob.hasKey("outer"))
		ParseBlobs(blob["outer"].ifFunction());
	Blob &modulator_blob = blob["modulators"];
	for (auto &item : modulator_blob.children_)
		Modulator(item, &sequence);
	sequence.Debias(debias_type::mean);
	if (blob.hasKey("env"))
		sequence.ApplyEnvelope(BuildEnvelope(blob["env"]));
	sequence.AutoAmp();
	ShowMessage("Created patch [" + name + "]", blob);
}

void Parser::Modulator(Blob &blob, Sequence *sequence_pointer) {
	blob.AssertFunction();
	if (!sequence_pointer)
		sequence_pointer = &dictionary_.FindSequence(blob);
	const SampleType type = sequence_pointer->getType();
	const OverlayFlags flags = (type.loop_) ? (1 << EnumVal(overlay::loop)) : 0;
	enum class modulation {
		frequency, amplitude, distortion
	} mode = modulation::frequency;
	double amp = 1, bias = 0;
	if (blob.hasKey("a"))
		amp = BuildAmplitude(blob["a"]);
	blob.tryWriteDouble("bias", bias, -1000, 1000);
	Sequence modulator_sequence;
	modulator_sequence.CreateSilenceSamples(SingleChannel,
			sequence_pointer->sample_rate(), sequence_pointer->p_samples(),
			sequence_pointer->p_samples());
	Synth(blob["synth"].ifFunction(), &modulator_sequence);
	if (blob.hasKey("env"))
		modulator_sequence.ApplyEnvelope(BuildEnvelope(blob["env"]));
	if (blob.hasKey("mode")) {
		if (blob["mode"].atom() == "am")
			mode = modulation::amplitude;
		else if (blob["mode"].atom() == "dm")
			mode = modulation::distortion;
	}
	if (mode == modulation::amplitude)
		sequence_pointer->Ring(&modulator_sequence, Wave(), false, amp, bias);
	else if (mode == modulation::distortion)
		sequence_pointer->Ring(&modulator_sequence, Wave(), true, amp, bias);
	else if (mode == modulation::frequency) {
		Sequence temp_sequence = *sequence_pointer;
		temp_sequence.MakeSilent();
		temp_sequence.setGate(false);
		Scratcher scratcher = Scratcher(&modulator_sequence, "", amp, bias,
				true);
		temp_sequence.Overlay(*sequence_pointer, 0,
				sequence_pointer->p_samples(), 1.0, flags, Stereo(), Phaser(),
				Envelope(), scratcher);
		*sequence_pointer = temp_sequence;
	}
	sequence_pointer->setType(type);
}

// Synth mode functions

void Parser::Synth(Blob &blob, Sequence *sequence_pointer) {
	Wave wave, tone;
	Phaser phaser;
	Stereo stereo;
	const bool active_sequence = sequence_pointer;
	bool pitch_adjust = false;
	double power = 1.0, fundamental = standard_pitch_;
	for (auto &item : blob.children_) {
		const std::string token = item.key_;
		if (token == "@") {
			if (active_sequence)
				throw EParser().msg(
						"Can't change sample supplied to synth mode."
								+ item.ErrorString());
			const std::string name = item.atom();
			sequence_pointer = &(dictionary_.FindSequence(name));
		} else {
			if (!sequence_pointer)
				throw EParser().msg(
						"Synth mode first needs a sequence to work with."
								+ item.ErrorString());
			if (token == "wave") {
				wave = BuildWave(item);
				if (pitch_adjust)
					wave.freq() *= synth_frequency_multiplier_;
			} else if (token == "amp")
				wave.amp() = BuildAmplitude(item);
			else if (token == "bias")
				wave = Wave(0, item.asDouble(), 0);
			else if (token == "vib")
				phaser = BuildPhaser(item, 4);
			else if (token == "bend")
				phaser.bend_factor() = item.asDouble(0.0001, 10000.0);
			else if (token == "tone")
				tone = BuildWave(item);
			else if (token == "stereo")
				stereo = BuildStereo(item);
			else if (token == "power")
				power = item.asDouble(0.001, 1000);
			else if (token == "pitch_adjust")
				pitch_adjust = item.asBool();
			else if (token == "sine")
				sequence_pointer->Waveform(wave, phaser, tone, 0,
						synth_type::sine, stereo);
			else if (token == "c")
				sequence_pointer->Waveform(wave, phaser, tone, 0,
						synth_type::constant, stereo);
			else if (token == "distort")
				sequence_pointer->Waveform(wave, phaser, tone, power,
						synth_type::power, stereo);
			else if (token == "square")
				sequence_pointer->Waveform(wave, phaser, tone, 0,
						synth_type::square, stereo);
			else if (token == "saw")
				sequence_pointer->Waveform(wave, phaser, tone, 0,
						synth_type::saw, stereo);
			else if (token == "triangle")
				sequence_pointer->Waveform(wave, phaser, tone, 0,
						synth_type::triangle, stereo);
			else if (token == "distort_triangle")
				sequence_pointer->Waveform(wave, phaser, tone, power,
						synth_type::powertriangle, stereo);
			else if (token == "pulse")
				sequence_pointer->Waveform(wave, phaser, tone, power,
						synth_type::pulse, stereo);
			else if (token == "white")
				sequence_pointer->WhiteNoise(wave.amp(), stereo);
			else if (token == "crackle")
				sequence_pointer->CrackleNoise(wave.freq(), stereo);
			else if (token == "smatter")
				DoSmatter(sequence_pointer, item.ifFunction());
			else if (token == "fundamental") {
				fundamental = item.asDouble(1, 30000);
				if (pitch_adjust)
					fundamental *= synth_frequency_multiplier_;
			} else if (token == "sines")
				DoSines(sequence_pointer, item.ifFunction(), fundamental);
			else if (token == "filter")
				sequence_pointer->ApplyFilters(BuildFilters(item.ifFunction()));
			else
				throw EParser().msg(token + ": Unknown synth operation.");
		}
	}
}

void Parser::DoSmatter(Sequence *sequence_pointer, Blob &blob) {
	Blob &pitch_blob = blob["pitch"].ifFunction(), &amp_blob =
			blob["amp"].ifFunction(), &stereo_blob =
			blob["stereo"].ifFunction();
	const double freq = blob["f"].asDouble(0, DBL_MAX);
	Sequence &sequence = dictionary_.FindSequence(blob["with"].atom());
	const double high_pitch = pitch_blob["high"].asDouble(0, DBL_MAX),
			low_pitch = pitch_blob["low"].asDouble(0, DBL_MAX), amp_high =
					BuildAmplitude(amp_blob["high"]), amp_low = BuildAmplitude(
					amp_blob["low"]), stereo_left =
					stereo_blob["left"].asDouble(-1, 1), stereo_right =
					stereo_blob["right"].asDouble(-1, 1);
	const bool logarithmic_pitch = pitch_blob.hasFlag("log"), logarithmic_amp =
			amp_blob.hasFlag("log");
	sequence_pointer->Smatter(sequence, freq, low_pitch, high_pitch,
			logarithmic_pitch, amp_low, amp_high, logarithmic_amp, stereo_left,
			stereo_right);
}

void Parser::DoSines(Sequence *sequence_pointer, Blob &blob,
		double fundamental) {
	int factor = 1;
	for (auto &item : blob.children_) {
		const double amp = BuildAmplitude(item);
		const Wave wave(fundamental * factor, amp);
		sequence_pointer->Waveform(wave, Phaser(), Wave(), 0, synth_type::sine,
				Stereo());
		factor++;
	}
}

// ParseLaunch functions

std::string BackSlashEscape(std::string input) {
	std::string output = "";
	for (char &c : input)
		if (c == '\\')
			output += "\\\\";
		else
			output += c;
	return output;
}

int ParseLaunch::Start(void) {
	int return_code = 0, argument_count = args_.size();
	try {
		bool do_bootstrap = true, show_environment = false;
		std::string args_instructions, boot_instructions, instructions;
		if (argument_count < 2)
			args_instructions +=
					"echo(\"BoxyLady: warning -- no arguments.\nFor help, type BoxyLady --help.\")";
		else
			for (int index = 1; index < argument_count; index++) {
				std::string argument(args_[index]);
				if (argument == "--help")
					args_instructions += "--help()\n";
				else if (argument == "--version")
					args_instructions += "--version()\n";
				else if (argument == "--noboot")
					do_bootstrap = false;
				else if (argument == "--envshow")
					show_environment = true;
				else if (argument == "--messages") {
					if (++index == argument_count) {
						std::cout << "BoxyLady: bad arguments.\n";
						return FailStart;
					};
					args_instructions += "--messages(" + args_[index] + ")\n";
				} else if (argument == "--e") {
					if (++index == argument_count) {
						std::cout << "BoxyLady: bad arguments.\n";
						return FailStart;
					};
					boot_instructions += args_[index] + "\n";
				} else if (argument == "--i")
					boot_instructions += "--i()\n";
				else
					boot_instructions += "library(\"" + argument + "\")\n";
			}
		if (do_bootstrap) {
			args_instructions += BootWelcome + "--user(1)\n";
			char *working_path = getenv(Platform::LinuxEnv);
			if ((working_path) && (std::string() != working_path))
				args_instructions += "--library(\""
						+ BackSlashEscape(working_path) + Platform::LinuxLib
						+ "\")\n";
			working_path = getenv(Platform::WinEnv);
			if ((working_path) && (std::string() != working_path))
				args_instructions += "--library(\""
						+ BackSlashEscape(working_path) + Platform::WinLib
						+ "\")\n";
			args_instructions += "--user(0)\n";
		}
		instructions = args_instructions + boot_instructions;
		if (show_environment)
			std::cout << "%--Environment--%\n" << instructions
					<< "%---------------%\n";
		Parser parser;
		parser.ParseString(instructions);
	} catch (EQuit &error) {
		std::cerr << error.what() << "\n";
	} catch (EBaseError &error) {
		std::cerr << error.what() << "\n";
		return_code = 1;
	} catch (std::exception &error) {
		std::cerr << "Internal error: " << error.what() << "\n";
		return_code = 1;
	} catch (...) {
		std::cerr << "Internal error.\n";
		return_code = 1;
	};
	return return_code;
}

} // end of namespace BoxyLady

int main(int argc, char **argv) {
	return BoxyLady::ParseLaunch(argc, argv).Start();
}

