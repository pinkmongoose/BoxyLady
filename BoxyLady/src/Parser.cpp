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

#include <fstream>
#include <tuple>
#include <filesystem>
#include <functional>
#include <utility>

#include "Parser.h"
#include "Sound.h"
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
//"0.2.3 'Honking Hum'"
//"0.2.4 'Luscious Least'"
//"0.2.5 'Mephitic Mathmo'"

Platform platform;

const std::string VersionNumber{"0.2.5"},
	VersionAlias{"Mephitic Mathmo"},
	Version{VersionNumber + " " + VersionAlias + "."},
	BootWelcome{"print(\"Welcome to BoxyLady. This is BoxyLady.\")\n"},
	BootHelp{"Usage: BoxyLady --help --version --noboot --portable --envshow --messages MESSAGELEVEL --interactive --outer 'SOURCE' --quick 'SOURCE' SOURCEFILE\n"},
	BootLicence{"Copyright (C) 2011-2025 Darren Green.\nLicense GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>\n\n\
This is free software; you are free to change and redistribute it.\n\
There is NO WARRANTY, to the extent permitted by law.\n\n"},
	BootContact{"Contact: darren.green@stir.ac.uk http://pinkmongoose.co.uk\n"},
	BootVersionInformation{"BoxyLady version " + Version + "\nCompiled for " + std::string(Platform::PlatformName) + " on "
		+ std::string(Platform::CompileDate) + " at " + std::string(Platform::CompileTime)
		+ " with " + std::string(Platform::Compiler) + ".\n"},
	BootInformation{BootVersionInformation + BootLicence + BootContact},
	Poem{"\n\nI wish I were an angler fish!\nI'd laze around all day,\nAnd wave my shiny dangly bit,\nTo lure in all my prey.\n\n\t--DG\n\n"};

void Slider::Build(Blob& blob, float_type now, bool do_amp) {
	blob.AssertFunction();
	if (blob.hasFlag("off")) {
		active_ = false;
		return;
	}
	float_type duration {blob["t"].asFloat(float_type_min, float_type_max)},
		rate {(do_amp) ?
			BuildAmplitude(blob["f"]) :
			blob["f"].asFloat(0.0001, 10000.0)};
	stop_ = now + duration;
	rate_ = log(rate) / duration;
	active_ = true;
}

void Slider::Update(float_type now, float_type duration, float_type& value) noexcept {
	if (!active_)
		return;
	if (now < stop_)
		value *= exp(rate_ * duration);
	else {
		value *= exp(rate_ * (duration - now + stop_));
		active_ = false;
	}
}

void Slider::Update(float_type now, float_type duration, Stereo& stereo) {
	float_type value {1.0};
	Update(now, duration, value);
	stereo[left] /= value;
	stereo[right] *= value;
}

std::string Slider::toString(float_type now) const {
	if (!active_)
		return "off";
	std::ostringstream stream;
	stream.precision(2);
	stream << "time = " << (stop_ - now) << "s rate = " << exp(rate_) << "/s";
	return stream.str();
}

float_type ParseParams::TimeDuration(NoteDuration note_duration, float_type now) const {
	constexpr float_type SecondsPerMinute {60.0}, QuartersPerSemibreve {4.0};
	const float_type duration {note_duration.getDuration()};
	if (duration == 0)
		return 0.0;
	switch (tempo_mode_) {
	case t_mode::tempo: {
		const float_type semibreve_tempo {tempo_ / (SecondsPerMinute * QuartersPerSemibreve)},
			raw_seconds {duration / semibreve_tempo},
			rall_rate {rall_.getRate()};
		if (rall_rate == 0)
			return raw_seconds;
		else {
			const float_type remaining_rall {rall_.getStop() - now},
				max_raw_rall {(exp(rall_rate * remaining_rall) - 1.0_flt) / rall_rate};
			if (raw_seconds <= max_raw_rall) {
				const float_type x {1.0_flt + rall_rate * raw_seconds};
				if (x <= 0)
					throw EError("Infinite note duration encountered.");
				else
					return log(x) / rall_rate;
			} else 
				return (raw_seconds - max_raw_rall) / exp(remaining_rall * rall_rate) + remaining_rall;	
		}
	}
	default:
		return duration * QuartersPerSemibreve;
	}
}

// Parser functions

void Parser::CheckSystem() {
	if (system(nullptr) == 0)
		throw EError("System calls are not available.");
}

verbosity_type Parser::verbosity_ {verbosity_type::none};

verbosity_type Parser::BuildVerbosity(std::string input) {
	static const std::map<std::string, verbosity_type> verbosity_types {
		{"none", verbosity_type::none}, {"errors", verbosity_type::errors},
		{"messages", verbosity_type::messages}, {"verbose", verbosity_type::verbose} 
	};
	if (verbosity_types.contains(input)) return verbosity_types.at(input);
	else throw EError(input + ": Unknown verbosity level.");
}

dic_item_protection Parser::BuildProtection(std::string input) {
	static const std::map<std::string, dic_item_protection> protection_levels {
		{"temp", dic_item_protection::temp}, {"unlocked", dic_item_protection::normal},
		{"locked", dic_item_protection::locked}, {"system", dic_item_protection::system}  
	};
	if (protection_levels.contains(input)) return protection_levels.at(input);
	else throw EError(input + ": Unknown slot protection level.");
}

SampleType Parser::BuildSampleType(Blob& blob, SampleType type) {
	blob.AssertFunction();
	static const std::map<std::string, std::pair<music_size, bool>> sample_types {
		{"CDDA", {Sound::CDSampleRate, false}},
		{"DVDA", {Sound::DVDSampleRate, false}},
		{"telephone", {Sound::TelephoneSampleRate, false}},
		{"wideband", {Sound::TelephoneSampleRate * 2, false}},
		{"32k", {Sound::TelephoneSampleRate * 4, false}},
		{"standard", {default_sample_rate_, false}},
		{"half", {default_sample_rate_ / 2, false}},
		{"quarter", {default_sample_rate_ / 4, false}},
		{"double", {default_sample_rate_ * 2, false}},
		{"quadruple", {default_sample_rate_ * 4, false}},
		{"octuple", {default_sample_rate_ * 8, false}},
		{"Amiga", {Sound::AmigaSampleRate, false}}
	};
	for (auto& child : blob.children_) {
		if (child.isToken()) {
			if (const std::string input {child.atom()}; sample_types.contains(input))
				std::tie(type.sample_rate, type.loop) = sample_types.at(input);
			else
				throw EError("Unknown sample type mode.\n" + blob.ErrorString());
		} else if (child.key_ == "loop")
			type.loop = child.asBool();
		else if (child.key_ == "loop_start") {
			type.loop_start = child.asFloat(0.0, float_type_max);
		}
		else if (child.key_ == "start_anywhere")
			type.start_anywhere = child.asBool();
		else if (child.key_ == "user")
			type.sample_rate = child.ifFunction()["Hz"].asInt(Sound::MinSampleRate, Sound::MaxSampleRate);
		else
			throw EError("Unknown sample type mode.\n" + blob.ErrorString());
	}
	return type;
}

Window Parser::BuildWindow(Blob& blob) const {
	blob.AssertFunction();
	Window window;
	if (blob.hasKey("start"))
		window.setStart(blob["start"].asFloat());
	if (blob.hasKey("end"))
		window.setEnd(blob["end"].asFloat());
	return window;
}

void Parser::Clear() {
	auto WaveformInstrument{[this](std::string instrument_name, std::string waveform){
		dictionary_.Insert(DictionaryItem(dic_item_type::macro),instrument_name)
			.Protect(dic_item_protection::system).getMacro()
			.Parse("instrument(type(loop=T) channels=1 shh!=verbose) synth(@instrument pitch_adjust(T) wave(440 0.9999 0) "+waveform+"())"
			);
	}};
	supervisor_ = false;
	portable_ = false;
	echo_shell_ = true;
	const std::string shell {platform.DefaultShellCommand()};
	mp3_encoder_ = shell + " no mp3 encoder command set : %source %dest %title %artist %album %track %year %genre %comment";
	file_play_ = shell + " no play command set : %file %arg";
	terminal_ = shell + " no terminal command set";
	ls_ = shell + " no ls command set";
	for (auto& i: std::initializer_list<std::pair<std::string, std::string>> {
		{":sine","sine"}, {":square","square"}, {":triangle","triangle"}, {":saw","saw"}
	}) WaveformInstrument(i.first, i.second);
	dictionary_.Insert(DictionaryItem(dic_item_type::macro), ":beep")
			.Protect(dic_item_protection::locked).getMacro().Parse("\\:triangle");
	verbosity_  = verbosity_type::messages;
	default_sample_rate_ = 44100;
	instrument_sample_rate_ = 0;
	instrument_duration_ = 1.0,
	max_instrument_duration_ = 16.0,
	instrument_frequency_multiplier_  = 1.0,
	standard_pitch_ = 440.0;
}

void Parser::TryMessage(std::string message, Blob& blob, std::initializer_list<Screen::escape> format) {
	if (blob.ifFunction().hasKey("shh!")) {
		const verbosity_type message_verbosity {BuildVerbosity(blob["shh!"].atom())};
		DoMessage(message, message_verbosity, format);
	} else
		DoMessage(message, verbosity_type::none, format);
}

void Parser::DoMessage(std::string message, verbosity_type verbosity, std::initializer_list<Screen::escape> format) const {
	if (verbosity <= verbosity_)
	screen.PrintMessage(message, format);
}

// Notes mode functions

void Parser::GlobalDefaults(Blob& blob) {
	params_.mode_ = context_mode::seq;
	Sound temp_sound;
	NotesModeBlob(blob, temp_sound, params_, 0.0, false);
}

void Parser::QuickMusic(Blob& blob) {
	const std::string name {"quick"};
	blob.AssertFunction();
	ParseParams params {params_};
	params.mode_ = context_mode::seq;
	params.instrument_ = ":beep";
	float_type start {0.0};
	if (dictionary_.contains(name) && (dictionary_.Find(name).ProtectionLevel() <= dic_item_protection::temp))
		dictionary_.Delete(name);
	DictionaryItem& slot {dictionary_.Insert(DictionaryItem(dic_item_type::sound), name)};
	slot.Protect(dic_item_protection::temp);
	DictionaryMutex mutex {slot};
	Sound& sound {slot.getSound()};
	sound.CreateSilenceSeconds(StereoChannels, default_sample_rate_, 0, 0);
	const float_type context_length {NotesModeBlob(blob, sound, params, start, true)};
	if (sound.p_samples() == 0) return;
	sound.set_tSeconds(context_length);
	std::string command {file_play_}, arg {""};
	CommandReplaceString(command, "%arg", arg, "play()");
	std::string file {TempFilename().file_name()};
	sound.SaveToFile(file, file_format::RIFF_wav, false);
	int return_code {PlayFile(file, command)};
	if (return_code != 0)
		DoMessage("\nFile play: error returned from external programme. [" + file_play_ + "]");
	remove(file.c_str());
}

void Parser::MakeMusic(Blob& blob) {
	const auto name {blob["@"].atom()};
	if (dictionary_.contains(name))
		throw EError(name + ": Object already exists.");
	const int channels {blob["channels"].asInt(1, MaxChannels)};
	if ((channels < 1) || (channels > MaxChannels))
		throw EError("Only 1 or 2 channels are currently supported.");
	SampleType sample_type {BuildSampleType(blob["type"])};
	ParseParams params {params_};
	params.mode_ = context_mode::nomode;
	float_type start {0.0};
	DictionaryItem& slot {dictionary_.Insert(DictionaryItem(dic_item_type::sound), name)};
	DictionaryMutex mutex {slot};
	Sound& sound {slot.getSound()};
	Blob& music_blob = blob["music"];
	if (!music_blob.isBlock(false))
		throw EError("No instruction block provided.\n" + blob.ErrorString());
	sound.CreateSilenceSeconds(channels, sample_type.sample_rate, 0, 0);
	sound.setType(sample_type);
	const float_type context_length {NotesModeBlob(music_blob, sound, params, start, true)};
	sound.set_tSeconds(context_length);
	DoMessage("Created patch [" + name + "]");
}

void Parser::UpdateSliders(float_type now, float_type duration, ParseParams& params) {
	params.cresc_.Update(now, duration, params.amp_);
	params.rall_.Update(now, duration, params.tempo_);
	params.salendo_.Update(now, duration, params.transpose_);
	params.pan_.Update(now, duration, params.articulation_.stereo_);
	params.staccando_.Update(now, duration, params.articulation_.staccato_);
	params.fidando_.Update(now, duration, params.fidato_);
}

float_type Parser::NotesModeBlob(Blob& blob, Sound& sound, ParseParams& params, float_type now, bool make_music) {
	auto AssertNoSlur {[params, blob]() {
		if (params.slur_)
			throw EError("Slurs must be contained entirely within contexts.\n" + blob.ErrorString());
	} };
	float_type len {0.0};
	bool inside_slur {true};
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
		throw EError("No context mode set. Use < or [ at start of notes mode.\n" + blob.ErrorString());
	for (auto& instruction : blob.children_) {
		std::string token {instruction.val_}, key {instruction.key_};
		if (instruction.isBlock())
			NestNotesModeBlob(instruction, sound, params, now, len, make_music);
		else if (instruction.isToken()) {
			if (token[0] == '\\')
				DoNotesMacro(token.erase(0, 1), sound, params, now, len, make_music);
			else if (token == "!")
				screen.PrintInline(std::string{make_music? "!":"?"}, {Screen::escape::cyan});
			else if (token == "|") {
				if (params.bar_check_)
					CheckBar(blob, params);
			} else if (token == "0")
				params.current_duration_ = NoteDuration(0.0);
			else if (token == "r") {
				const float_type duration {params.TimeDuration(params.current_duration_, now)};
				if (params.mode_ == context_mode::seq) {
					now += duration;
					len += duration;
					params.beat_time_ += params.current_duration_.getDuration();
				} else if (duration > len)
					len = duration;
				if (params.mode_ == context_mode::seq)
					UpdateSliders(now, duration, params);
			} else if (in_range(token[0], '0', '9')) {
				if (params.tempo_mode_ == t_mode::tempo)
					params.current_duration_ = NoteDuration(token);
				else
					params.current_duration_ = NoteDuration{Blob(token)[0].asFloat(0, float_type_max)};
			} else if (in_range(token[0], 'a', 'z')
					|| in_range(token[0], 'A', 'Z')
					|| (token[0] == '\'') || (token[0] == '+'))
				PlayNote(token, blob, sound, params, now, len, make_music);
			else if ((token == "") && (key == "")) {
			} else
				throw EError(token + ": Unrecognised music symbol.\n" + blob.ErrorString());
		} // end of bare tokens
		else if (key == "instrument") {
			std::string name {instruction.atom()};
			if (!dictionary_.contains(name)) throw EError(name + ": No such object.");
			params.instrument_ = name;
			params.slur_ = false;
		} else if (key == "silence") {
			const float_type duration {instruction.asFloat(0, HourLength)};
			if (params.mode_ == context_mode::seq) {
				now += duration;
				len += duration;
			} else if (duration > len)
				len = duration;
			if (params.mode_ == context_mode::seq)
				UpdateSliders(now, duration, params);
		} else if (key == "rel")
			params.last_note_ = params.gamut_.NoteAbsolute(instruction.atom());
		else if (key == "tuning")
			params.gamut_.TuningBlob(instruction.ifFunction(), make_music);
		else if (key == "gamut")
			params.gamut_.ParseBlob(instruction.ifFunction(), make_music);
		else if (key == "auto_stereo")
			params.auto_stereo_.ParseBlob(instruction.ifFunction(), make_music);
		else if (key == "articulations")
			params.articulation_gamut_.ParseBlob(instruction.ifFunction(), make_music);
		else if (key == "beats")
			params.beat_gamut_.ParseBlob(instruction.ifFunction(), params.beat_time_, make_music);
		else if (key == "show_state") {
			if (make_music)
				ShowState(instruction, params, now);
		} else if (key == "transpose")
			Transpose(instruction.ifFunction(), params);
		else if (key == "transpose_random")
			TransposeRandom(instruction.ifFunction(), params);
		else if (key == "intonal")
			Intonal(instruction.ifFunction(), params);
		else if (key == "tempo") {
			if (instruction.hasKey("rel")) {
				const NoteDuration note_duration {instruction.ifFunction()["rel"].atom()};
				params.tempo_ *= note_duration.getDuration() / params.current_duration_.getDuration();
			} else if (instruction.hasKey("f"))
				params.tempo_ *= instruction.ifFunction()["f"].asFloat(0.0001, 10000.0);
			else
				params.tempo_ = instruction.asFloat(1.0, 10000.0);
		} else if (key == "tempo_mode") {
			if (instruction.ifFunction().hasFlag("tempo"))
				params.tempo_mode_ = t_mode::tempo;
			else if (instruction.hasFlag("time"))
				params.tempo_mode_ = t_mode::time;
			else
				throw EError("Incorrect tempo mode set.\n" + blob.ErrorString());
		} else if (key == "offset")
			params.offset_time_ = instruction.asFloat(-MinuteLength, MinuteLength);
		else if (key == "-") params.current_duration_ -= NoteDuration(instruction.atom());
		else if ((key == "amp") || (key == "amp2")) {
			float_type& which_amp {(key == "amp") ? params.amp_ : params.amp2_};
			if (instruction.isFunction() && instruction.hasKey("f"))
				which_amp *= BuildAmplitude(instruction["f"]);
			else
				which_amp = BuildAmplitude(instruction);
		} else if (key == "amp_random")
			AmpRandom(instruction.ifFunction(), params);
		else if (key == "C") {
			const std::string buffer {"N(" + instruction.ifFunction()[0].atom() + ") \\" + instruction[1].atom()};
			Blob temp_blob {buffer};
			const float_type context_length {NotesModeBlob(temp_blob, sound, params, now, make_music)};
			if (params.mode_ == context_mode::seq) {
				now += context_length;
				len += context_length;
			} else if (context_length > len)
				len = context_length;
		} else if ((key == "env") || (key == "envelope"))
			params.articulation_.envelope_ = BuildEnvelope(instruction);
		else if (key == "gate")
			params.gate_ = instruction.asFloat(0.0, 0.02);
		else if (key == "vib")
			params.articulation_.phaser_ = BuildPhaser(instruction);
		else if (key == "tremolo")
			params.articulation_.tremolo_ = BuildWave(instruction);
		else if (key == "bend") {
			if (instruction.isFunction() && instruction.hasKey("t")) {
				params.articulation_.phaser_.set_bend_time(instruction["t"].asFloat(float_type_min, float_type_max));
				params.articulation_.phaser_.set_bend_factor(pow(
						instruction["f"].asFloat(0.0001, 10000.0),
						1.0 / params.articulation_.phaser_.bend_time()));
			} else
				params.articulation_.phaser_.set_bend_factor(instruction.asFloat(0.0001, 10000.0));
		} else if (key == "port")
			params.articulation_.portamento_time_ = instruction.asFloat(0, MinuteLength);
		else if (key == "scratch") {
			if (instruction.isFunction() && instruction.hasFlag("off"))
				params.articulation_.scratcher_ = Scratcher();
			else
				params.articulation_.scratcher_ = Scratcher(nullptr,
						instruction["with"].atom(), instruction["a"].asFloat(),
						instruction["bias"].asFloat(),
						instruction["loop"].asBool());
		} else if (key == "glide")
			params.articulation_.glide_ = instruction.asBool();
		else if (key == "octave")
			params.last_note_.setOctave(instruction.asInt(-256, 256));
		else if (key == "N") {
			params.last_note_ = params.gamut_.NoteRelative(instruction.atom(), params.last_note_);
//			lastFreqMult=P.G.FreqMult(P.LastNote)*P.transpose;
		} else if (key == "S")
			DoS(instruction.ifFunction(), params);
		else if (key == "print")
			DoMessage(instruction.atom(), verbosity_type::none, {Screen::escape::cyan});
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
			StereoRandom(instruction.ifFunction(), params);
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
			params.arpeggio_ = instruction.asFloat(0.0, MinuteLength);
		else if (key == "staccato")
			params.articulation_.staccato_ = instruction.asFloat(0, NoteArticulation::MaxStaccato);
		else if (key == "staccando")
			params.staccando_.Build(instruction, now);
		else if (key == "fidato")
			params.fidato_ = instruction.asFloat(0.0,1.0);
		else if (key == "fidando")
			params.fidando_.Build(instruction, now);
		else if (key == "D")
			params.current_duration_ = NoteDuration{instruction};
		else if (key == "D_rev")
			params.articulation_.duration_ = NoteDuration{instruction};
		else if (key == "D_random")
			params.current_duration_ = NoteDuration(
				1.0 / Rand.uniform(
					instruction.ifFunction()["max"].asFloat(float_type_min, float_type_max),
					instruction["min"].asFloat(float_type_min, float_type_max)));
		else if (key == "outer") {
			if (make_music)
				ParseBlobs(instruction.ifFunction());
		} else if (key == "def")
			MakeMacro(instruction, macro_type::macro, false);
		else if (key == "let")
			MakeMacro(instruction, macro_type::variable, true);
		else if (key == "condition")
			Condition(instruction, params);
		else if (key == "inc")
			Increment(instruction, 1);
		else if (key == "dec")
			Increment(instruction, -1);
		else if (key == "context_mode") {
			if (instruction.ifFunction().hasFlag("tune"))
				params.mode_ = context_mode::seq;
			else if (instruction.hasFlag("chords"))
				params.mode_ = context_mode::chord;
			else
				throw EError("Incorrect context mode set.\n" + blob.ErrorString());
		} else if (key == "oneof")
			OneOf(instruction.ifFunction(), sound, params, now, len, make_music, -1);
		else if (key == "arpeggiate")
			Arpeggiate(instruction.ifFunction(), sound, params, now, len, make_music);
		else if (key == "shuffle")
			Shuffle(instruction.ifFunction(), sound, params, now, len, make_music);
		else if (key == "scramble")
			Scramble(instruction.ifFunction());
		else if (key == "call_change")
			CallChange(instruction.ifFunction());
		else if (key == "mingle")
			Mingle(instruction.ifFunction());
		else if (key == "rotate")
			Rotate(instruction.ifFunction());
		else if (key == "replicate")
			Replicate(instruction.ifFunction());
		else if (key == "indirect")
			Indirect(instruction.ifFunction());
		else if (key == "unfold")
			Unfold(instruction.ifFunction(), sound, params, now, len, make_music, false);
		else if (key == "fill")
			Unfold(instruction.ifFunction(), sound, params, now, len, make_music, true);
		else if (key == "foreach")
			ForEach(instruction.ifFunction(), sound, params, now, len, make_music);
		else if (key == "switch")
			Switch(instruction.ifFunction(), sound, params, now, len, make_music, false);
		else if (key == "index")
			Switch(instruction.ifFunction(), sound, params, now, len, make_music, true);
		else if (key == "trill")
			Trill(instruction.ifFunction(), sound, params, now, len, make_music);
		else if (key == "precision") {
			instruction.ifFunction().tryWritefloat_type("amp", params.precision_amp_, 0, 1);
			instruction.tryWritefloat_type("pitch", params.precision_pitch_, 0, 1);
			instruction.tryWritefloat_type("time", params.precision_time_, 0, 1);
		} else if (key == "post_process") {
			if (const auto process {instruction.atom()}; process == "off")
				params.post_process_ = "";
			else {
				DictionaryItem& item {dictionary_.Find(process)};
				if (item.isNull())
					throw EError(process + ": No such object.");
				params.post_process_ = process;
			}
		} else
			throw EError(key + "=" + token + ": Unknown command.");
	}
	if (inside_slur)
		AssertNoSlur();
	return len;
}

void Parser::PlayNote(std::string token, Blob& blob, Sound& sound,
		ParseParams& params, float_type& now, float_type& len, bool make_music) {
	NoteValue note_value;
	if (token[0] == '\'')
		token.erase(0, 1);
	if (token[0] == '+') {
		if ((token.length() == 1) || (token[1] == '-'))
			note_value = params.last_note_;
		else
			throw EError("Problem in repeated note.\n" + blob.ErrorString());
	} else if (params.ignore_pitch_)
		note_value = params.last_note_;
	else
		note_value = params.gamut_.NoteRelative(token, params.last_note_);// n now contains the note to be played.
	NoteArticulation articulation {params.articulation_};
	CheckBeats(params, articulation);
	articulation.Overwrite(params.articulation_gamut_.Note(token));	// na contains the current articulations, overwritten by those of the beats, and then of the current note
	const float_type duration_rhythmic {params.TimeDuration(params.current_duration_, now)},
		duration_articulation {params.TimeDuration(articulation.duration_, now)};
	const float_type duration {(duration_articulation != 0.0) ? duration_articulation : duration_rhythmic * articulation.staccato_};
	float_type imprecision_pitch {1.0}, imprecision_time {0.0}, imprecision_amp {1.0}, offset_time {params.offset_time_};
	if (params.precision_pitch_)
		imprecision_pitch = 1.0 + Rand.uniform(-params.precision_pitch_, params.precision_pitch_);
	if (params.precision_time_)
		imprecision_time = Rand.uniform(0, params.precision_time_);
	if (params.precision_amp_)
		imprecision_amp = 1.0 + Rand.uniform(-params.precision_amp_, params.precision_amp_);
	if (make_music) {// omitted if music data isn't being generated - all timings calculated above
		std::string instrument {params.instrument_};
		if (instrument == "")
			throw EError("No instrument specified to use.\n" + blob.ErrorString());
		DictionaryItem& instrument_item {dictionary_.Find(instrument)};
		if (instrument_item.isNull())
			throw EError(instrument + ": No such object.");
		const float_type freq_mult_standard {params.gamut_.FreqMultStandard(note_value)},
			  freq_mult {freq_mult_standard * params.transpose_},
			  amp_mult {params.amp_adjust_.Amplitude(freq_mult)};
//			AmpMult=(P.AmpAdjust)? pow(P.AmpAdjustFreqMult/freq_mult,P.AmpAdjustExponent): 1.0;
		float_type freq_mult_imprecision, freq_mult_start;
		Sound *instrument_sound_ptr {0};
		if (instrument_item.isSound()) {	// assign sequence M as instrument
			instrument_sound_ptr =& (instrument_item.getSound());
			freq_mult_imprecision = freq_mult * imprecision_pitch;
		} else if (instrument_item.isMacro()) {
			const float_type synth_frequency_multiplier_old {instrument_frequency_multiplier_};
			instrument_frequency_multiplier_ *= (freq_mult * imprecision_pitch);
			instrument_duration_ = std::min(duration, max_instrument_duration_);
			instrument_sample_rate_ = sound.sample_rate();
			DictionaryMutex mutex {instrument_item};
			ParseBlobs(instrument_item.getMacro());
			DictionaryItem& created_item {dictionary_.Find("instrument")};
			if (created_item.isNull())
				throw EError("Failed to find 'instrument' slot." + blob.ErrorString());
			if (!created_item.isSound())
				throw EError("Slot 'instrument' must contain music data." + blob.ErrorString());
			instrument_sound_ptr = &(created_item.getSound());
			freq_mult_imprecision = 1.0;
			instrument_frequency_multiplier_ = synth_frequency_multiplier_old;// generate sequence M as instrument
		} else
			throw EError(instrument + ": Not suitable instrument.");
		Sound& instrument_sound {*instrument_sound_ptr};// refer below to the sequence to be overlaid as M
		SampleType instrument_sound_type {instrument_sound.getType()};
		Scratcher scratcher {articulation.scratcher_};
		if (scratcher.active()) {
			DictionaryItem& scratcher_item {dictionary_.Find(scratcher.name())};
			if (scratcher_item.isNull())
				throw EError("Failed to find 'scratch' slot." + blob.ErrorString());
			if (!scratcher_item.isSound())
				throw EError("Slot 'instrument' must contain music data." + blob.ErrorString());
			scratcher.set_sound(scratcher_item.getSound());
		}
		bool reverb {articulation.reverb_};
		Window window {
				(reverb) ?
						Window(now + imprecision_time + offset_time) :
						Window(now + imprecision_time + offset_time, now + imprecision_time + offset_time + duration)};
		OverlayFlags flags {{
			{overlay::resize, true},
			{overlay::loop, instrument_sound_type.loop},
			{overlay::random, instrument_sound_type.start_anywhere},
			{overlay::envelope_compress, articulation.envelope_compress_},
			{overlay::slur_on, params.slur_},
			{overlay::slur_off, params.slur_}
		}};
		if (articulation.start_slur_) {
			if (params.slur_)
				throw EError("Can't start slur twice. " + blob.ErrorString());
			if (params.mode_ != context_mode::seq)
				throw EError("Can only slur consecutive notes. " + blob.ErrorString());
			flags.Set({{overlay::slur_off, true}, {overlay::slur_on, false}});
			params.slur_ = true;
		};
		if (articulation.stop_slur_) {
			if (!params.slur_)
				throw EError("Can't stop a slur which isn't there. " + blob.ErrorString());
			flags.Set({{overlay::slur_off, false}, {overlay::slur_on, true}});
			params.slur_ = false;
		}; // Set up slurs
		if (reverb && params.slur_)
			throw EError("Ongoing slurs with reverb do not work. " + blob.ErrorString());
		Phaser phaser {articulation.phaser_};
		const float_type portamento_time {articulation.portamento_time_};
		if (articulation.glide_ || (flags[overlay::slur_on] && portamento_time)) {
			phaser.set_bend_time((articulation.glide_) ? duration : portamento_time);
			phaser.set_bend_factor(pow(
				freq_mult_imprecision / params.last_frequency_multiplier_,
				1.0 / phaser.bend_time()));
			freq_mult_start = params.last_frequency_multiplier_;
		} else {
			freq_mult_start = freq_mult_imprecision;
		} // Set up pitch bends
		const Stereo overlay_stereo {articulation.stereo_ * params.auto_stereo_.Apply(freq_mult_standard) *
			params.amp_ * params.amp2_ * (Rand.uniform()<=params.fidato_? 1.0:0.0) *
			articulation.amp_ * amp_mult * imprecision_amp};							
		if (params.post_process_ != "") {
			OverlayFlags process_flags {{overlay::resize}};
//			process_flags[overlay::resize] = true;
			if (reverb)
				throw EError("Post_process with reverb does not work. " + blob.ErrorString());
			DictionaryItem& process_item {dictionary_.Find(params.post_process_)};
			if (process_item.isNull())
				throw EError("Failed to find 'post_process' slot." + blob.ErrorString());
			Sound& process_sound {dictionary_.InsertSound("note")};
			process_sound.CreateSilenceSeconds(sound.channels(),
					sound.sample_rate(), window.length(), window.length());
			process_sound.DoOverlay(instrument_sound).stop(process_sound.p_samples()).pitch_factor(freq_mult_start)
				.flags(flags).stereo(overlay_stereo).phaser(phaser).envelope(articulation.envelope_)
				.scratcher(scratcher).tremolo(articulation.tremolo_)();
			DictionaryMutex mutex {process_item};
			ParseBlobs(process_item.getMacro());
			sound.DoOverlay(process_sound).window(window).flags(process_flags).gate(params.gate_)();
			dictionary_.Delete("note");
		} else
			sound.DoOverlay(instrument_sound).window(window).pitch_factor(freq_mult_start)
				.flags(flags).stereo(overlay_stereo).phaser(phaser).envelope(articulation.envelope_)
				.scratcher(scratcher).tremolo(articulation.tremolo_).gate(params.gate_)();
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

void Parser::CheckBeats(ParseParams& params, NoteArticulation& articulation) {
	NoteArticulation beat_articulation;
	beat_articulation.Overwrite(params.articulation_gamut_.FromString(params.beat_gamut_.BeatArticulations(params.beat_time_)));
	articulation.Overwrite(beat_articulation);
}
 
void Parser::CheckBar(Blob& blob, ParseParams& params) {
	NoteArticulation articulation;
	articulation.flags_[articulation_type::bar] = true;
	CheckBeats(params, articulation);
	if (!articulation.bar_)
		throw EError("Bar checky wecky failed at beat time " + std::to_string(params.beat_time_) + ".\n" + blob.ErrorString());
}

void Parser::ShowState([[maybe_unused]] Blob& blob, ParseParams params, float_type now) {
	auto Print = [](std::string item) {screen.PrintWrap(item, Screen::PrintFlags{Screen::print_flag::frame, Screen::print_flag::wrap, Screen::print_flag::indent});};
	screen.PrintSeparatorTop();
	Print("Current articulations:");
	Print(ArticulationGamut::List1(params.articulation_, true));
	Print("Other settings:");
	Print(std::format("gate({}) slur({}) staccando({}) arp = {}s",
		params.gate_, BoolToString(params.slur_), params.staccando_.toString(now), params.arpeggio_));
	Print(std::format("tempo = {}/min rall({}) tempo_mode = {}",
		params.tempo_, params.rall_.toString(now), (params.tempo_mode_ == t_mode::tempo) ? "tempo" : "time"));
	Print(std::format("offset = {}s D = {}",
		params.offset_time_, params.current_duration_.getDuration() * 4.0));
	std::string line {std::format("amp = {} amp2 = {} amp_adjust(",params.amp_, params.amp2_)};
	if (params.amp_adjust_.active_)
		line += std::format("power = {} standard_f = {})",
			params.amp_adjust_.exponent_, params.amp_adjust_.standard_);
	else
		line += "off)";
	Print(line);
	Print(std::format("cresc({}) pan({})",
		params.cresc_.toString(now), params.pan_.toString(now)));
	Print(std::format("auto_stereo({})", params.auto_stereo_.toString()));
	Print(std::format("transpose = {} ignore_pitch = {} salendo({})",
		params.transpose_, BoolToString(params.ignore_pitch_), params.salendo_.toString(now)));
	Print(std::format("fidato({}) fidando({})", params.fidato_, params.fidando_.toString(now)));
	Print(std::format("precision(amp = {} pitch = {} time = {})",
		params.precision_amp_, params.precision_pitch_, params.precision_time_));
	Print(std::format("instrument = {} post_process({})", params.instrument_, params.post_process_));
	Print(std::format("{} current_position = {}s",
		(params.mode_ == context_mode::seq) ? "[tune mode]" : "<chords mode>", now));
	Print(std::format("'last_f' = {} 'last_note'({})",
		params.last_frequency_multiplier_, params.last_note_.toString()));
	Print("Use list() in gamut() articulations() and beats() for more.");
	screen.PrintSeparatorBot();
}

void Parser::NestNotesModeBlob(Blob& blob, Sound& sound,
		ParseParams& params, float_type& now, float_type& len, bool make_music) {
	if (blob.delimiter_ == '(')
		throw EError("Delimeter ( not supported in music expressions. Use { instead.\n" + blob.ErrorString());
	ParseParams local_params {params};
	const float_type context_length {NotesModeBlob(blob, sound, local_params, now, make_music)};
	if (params.mode_ == context_mode::seq) {
		now += context_length;
		len += context_length;
		UpdateSliders(now, context_length, params);
	} else {
		if (context_length > len)
			len = context_length;
	}
}

void Parser::DoNotesMacro(std::string name, Sound& sound,
		ParseParams& params, float_type& now, float_type& len, bool make_music) {
	DictionaryItem& item {dictionary_.Find(name)};
	if (item.isNull())
		throw EError(name + ": No such object.");
	Sound& overlay_sound {item.getSound()};
	switch (item.getType()) {
	case dic_item_type::macro: {
		DictionaryMutex mutex {item};
		const float_type context_length {NotesModeBlob(item.getMacro(), sound, params, now, make_music)};
		if (params.mode_ == context_mode::seq) {
			now += context_length;
			len += context_length;
		} else if (context_length > len)
			len = context_length;
		return;
	}
	case dic_item_type::sound: {
		const float_type duration {item.getSound().get_tSeconds()};
		if (make_music) {
			const Window window {now, now + overlay_sound.get_pSeconds()};
			OverlayFlags flags {{overlay::resize}};
			sound.DoOverlay(overlay_sound).window(window).flags(flags)
				.stereo(params.articulation_.stereo_ * params.amp_ * params.amp2_ * (Rand.uniform() <= params.fidato_? 1.0 : 0.0))();
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
		throw EError(name + ": Unsuitable dictionary reference in notes mode.");
	};
}

void Parser::DoAmpAdjust(Blob& blob, ParseParams& params) {
	if (blob.hasFlag("off"))
		params.amp_adjust_ = AmpAdjust();
	else {
		float_type power {blob["power"].asFloat(0.0, 1.0)}, standard_pitch {1.0};
		if (blob.hasKey("standard")) {
			NoteValue standard {params.gamut_.NoteAbsolute(blob["standard"].atom())};
			standard_pitch = params.gamut_.FreqMultStandard(standard);
		}
		params.amp_adjust_ = AmpAdjust(power, standard_pitch);
	}
}

void Parser::Transpose(Blob& blob, ParseParams& params) {
	if (blob.hasKey("rel")) {
		const NoteValue last_note {params.last_note_},
			rel_note {params.gamut_.NoteAbsolute(blob["rel"].atom())};
		const float_type mult_last_note {params.gamut_.FreqMultStandard(last_note)},
			mult_rel_note {params.gamut_.FreqMultStandard(rel_note)};
		params.transpose_ *= mult_last_note / mult_rel_note;
		params.last_note_ = rel_note;
	} else if (blob.hasKey("Hz"))
		params.transpose_ = blob["Hz"].asFloat(0, float_type_max) / standard_pitch_;
	else if (blob.hasKey("f"))
		params.transpose_ *= blob["f"].asFloat(0, float_type_max);
	else {
		const NoteValue note {params.gamut_.NoteAbsolute(blob.atom())};
		params.transpose_ = params.gamut_.FreqMultStandard(note);
	}
}

void Parser::TransposeRandom(Blob& blob, ParseParams& params) {
	const float_type min {blob["min"].asFloat(0, float_type_max) / standard_pitch_},
		max {blob["max"].asFloat(0, float_type_max) / standard_pitch_};
	params.transpose_ = exp(Rand.uniform(log(min), log(max)));
}

void Parser::Intonal(Blob& blob, ParseParams& params) {
	NoteValue last_note {params.last_note_};
	if (blob.hasKey("rel"))
		last_note = params.gamut_.NoteAbsolute(blob["rel"].atom());
	const float_type mult_last_pre {params.gamut_.FreqMultStandard(last_note)};
	if (blob.hasKey("gamut"))
		params.gamut_.ParseBlob(blob["gamut"].ifFunction(), false);
	else if (blob.hasKey("tuning"))
		params.gamut_.TuningBlob(blob["tuning"].ifFunction(), false);
	else
		throw EError("Intonal needs tuning or gamut.\n" + blob.ErrorString());
	const float_type mult_last_post {params.gamut_.FreqMultStandard(last_note)};
	params.transpose_ *= mult_last_pre / mult_last_post;
}

void Parser::StereoRandom(Blob& blob, ParseParams& params) {
	const float_type left {blob["left"].asFloat(-1.0, 1.0)},
		right {blob["right"].asFloat(-1.0, 1.0)}, position {Rand.uniform(left, right)};
	params.articulation_.stereo_ = Stereo::Position(position);
}

void Parser::AmpRandom(Blob& blob, ParseParams& params) {
	const float_type min {BuildAmplitude(blob["min"])},
		max {BuildAmplitude(blob["max"])};
	if ((min <= 0.0) || (max <= 0.0))
		throw EError("Min/max amp must be positive.\n" + blob.ErrorString());
	params.amp_ = exp(Rand.uniform(log(min), log(max)));
}

void Parser::DoS(Blob& blob, ParseParams& params) {
	switch (blob.ifFunction().children_.size()) {
	case 0:
		params.last_note_ = params.gamut_.Offset(params.last_note_, 1, 0.0, 0);
		break;
	case 1:
		params.last_note_ = params.gamut_.Offset(params.last_note_, blob[0].asInt(), 0.0, 0);
		break;
	case 2:
		params.last_note_ = params.gamut_.Offset(params.last_note_, blob[0].asInt(), blob[1].asFloat(), 0);
		break;
	case 3:
		params.last_note_ = params.gamut_.Offset(params.last_note_, blob[0].asInt(), blob[1].asFloat(), blob[2].asInt());
		break;
	default:
		throw EError("Syntax error in S(...) function.\n" + blob.ErrorString());
	}
}

void Parser::OneOf(Blob& blob, Sound& sound, ParseParams& params, float_type& now, float_type& len, bool make_music, int which) {
	const size_t count {blob.children_.size()};
	if (count == 0) throw EError("oneof(...) needs options." + blob.ErrorString());
	Blob& choice {(which >= 0) ? blob[which] : blob[Rand.uniform(count)]};
	float_type context_length;
	if (!choice.isBlock())
		choice = choice.Wrap('(');
	if ((choice.delimiter_ == '[') || (choice.delimiter_ == '<') || (choice.delimiter_ == '{')) {
		ParseParams P1 {params};
		context_length = NotesModeBlob(choice, sound, P1, now, make_music);
	} else
		context_length = NotesModeBlob(choice, sound, params, now, make_music);
	if (params.mode_ == context_mode::seq) {
		now += context_length;
		len += context_length; /*???*/
		if (choice.delimiter_ != '(')
			UpdateSliders(now, context_length, params);
	} else if (context_length > len)
		len = context_length;
}

void Parser::Arpeggiate(Blob& blob, Sound& sound, ParseParams& params, float_type& now, float_type& len, bool make_music) {
	constexpr float_type ToleranceTime {0.0001};
	enum class arp_t {up, down, updown, downup, upanddown, downandup, random, random_different, driftup, driftdown, n};
	static const std::map<std::string, arp_t> arp_types {
		{"up", arp_t::up}, {"down", arp_t::down,}, {"up_down", arp_t::updown}, {"down_up", arp_t::downup},
		{"up_and_down", arp_t::upanddown}, {"down_and_up", arp_t::downandup},
		{"random", arp_t::random}, {"random_different", arp_t::random_different}, {"drift_up", arp_t::driftup}, {"drift_down", arp_t::driftdown}
	};
	static const BoxyLady::Flags<arp_t> start_top {{arp_t::down, arp_t::downup, arp_t::downandup, arp_t::driftdown}},
		rev_ends_once {{arp_t::updown, arp_t::downup, arp_t::driftup, arp_t::driftdown}},
		rev_ends_twice {{arp_t::upanddown, arp_t::downandup}},
		drifts {{arp_t::driftup, arp_t::driftdown}};
	if (params.mode_ != context_mode::seq) throw EError("Arpeggiate only works in tune mode\n" + blob.ErrorString());
	arp_t arp {arp_t::up};
	if (blob.hasKey("type")) {
		if (auto item {blob["type"].atom()}; arp_types.contains(item))
			arp = arp_types.at(item);
		else throw EError("Unknown arpeggiation type\n" + blob.ErrorString());
	}
	Blob& list {blob["from"]};
	if (!list.isFunction()) throw EError("Delimeters not relevant to 'from' block.\n" + blob.ErrorString());
	const size_t count {list.children_.size()};
	if (count < 3) throw EError("Arpeggiation data too short\n" + blob.ErrorString());
	int logical_count {static_cast<int>(count)};
	if (blob.hasKey("octaves"))
		logical_count = 1 + count * blob["octaves"].asInt(1, 10);
	const NoteDuration old_duration {params.current_duration_}; // store note length from before arpeggiation
	float_type duration {0.0}, drift_change_rate {0.25};
	if (blob.hasKey("drift_change_rate")) drift_change_rate = blob["drift_change_rate"].asFloat(0.0,1.0);
	if (blob.hasKey("t")) duration = blob["t"].asFloat(0.0, 100000.0);
	else duration = params.TimeDuration(params.current_duration_, now);
	if (blob.hasKey("d")) params.current_duration_ = NoteDuration(blob["d"].atom());
	int index {start_top[arp]? logical_count - 1 : 0},
		direction {start_top[arp]? -1 : 1},
		last_index {-1};
	const float_type tend {now + duration};
	for (bool more_repeats {true}; more_repeats;) {
		if (arp == arp_t::random)
			index = Rand.uniform(count);
		else if (arp == arp_t::random_different) {
			do {index = Rand.uniform(count);} while (index==last_index);
			last_index = index;
		}
		OneOf(list, sound, params, now, len, make_music, index % count);
		if (arp == arp_t::up) {
			if (++index == logical_count) index = 0;
		} else if (arp == arp_t::down) {
			if (--index < 0) index = logical_count - 1;
		} else if (rev_ends_once[arp]) {
			index += direction;
			if (drifts[arp]) {
				if (Rand.Bernoulli(drift_change_rate)) direction = -direction;
			}
			if (index == logical_count - 1) direction = -1;
			else if (index == 0) direction = 1;
		} else if (rev_ends_twice[arp]) {
			index += direction;
			if (index == logical_count - 1) direction = (direction == 0)? -1 : 0;
			if (index == 0) direction = (direction == 0)? 1 : 0;
		}
		if (now >= tend - ToleranceTime) more_repeats = false;
	}
	params.current_duration_ = old_duration;
}

void Parser::Shuffle(Blob& blob, Sound& sound, ParseParams& params, float_type& now, float_type& len, bool make_music) {
	bool replace {false};
	if (blob.hasKey("replace"))
		replace = blob["replace"].asBool();
	Blob& list {blob["from"]};
	const size_t count {list.children_.size()};
	int sample_size {0};
	if (blob.hasKey("n"))
		sample_size = blob["n"].asInt(0, int_max);
	else if (!replace)
		sample_size = count;
	else
		throw EError("shuffle() with 'replace' needs 'n'.\n" + blob.ErrorString());
	if (count == 0)
		throw EError("oneof(...) needs options." + blob.ErrorString());
	if ((!replace) && (sample_size > static_cast<int>(count)))
		throw EError("shuffle() needs a smaller n.\n" + blob.ErrorString());
	std::vector<int> use_count(sample_size, 0);
	for (int index {0}; index < sample_size; index++) {
		int choice {0};
		do {
			choice = Rand.uniform(count);
		} while (use_count[choice] && !replace);
		OneOf(list, sound, params, now, len, make_music, choice);
		use_count[choice]++;
	}
}

Blob& Parser::GetMutableBlob(std::string name, Blob &blob, bool allow_blank) {
	DictionaryItem& var_item {dictionary_.Find(name)};
	if (var_item.isNull()) {
		if (allow_blank) {
			DictionaryItem& new_item {dictionary_.Insert(DictionaryItem(dic_item_type::macro), name)};
			new_item.getMacroType() = macro_type::variable;
			return new_item.getMacro();
		} 
		else throw EError("Failed to find '"+name+"'.\n" + blob.ErrorString());
	}
	if (!var_item.isMacro()) throw EError("Switch variable '"+name+"' not a macro.\n" + blob.ErrorString());
	if (var_item.getMacroType()!=macro_type::variable) throw EError("Switch variable '"+name+"' not assigned using 'let'.\n" + blob.ErrorString());
	return var_item.getMacro();
}

void Parser::Scramble(Blob& blob) {
	Blob& change {GetMutableBlob(blob["@"].atom(), blob, false)};
	std::shuffle(change.children_.begin(), change.children_.end(), Rand.generator());
}

void Parser::CallChange(Blob& blob) {
	Blob& change {GetMutableBlob(blob["@"].atom(), blob, false)};
	const size_t change_size {change.children_.size()};
	const Blob& calls {blob["calls"]};
	for (const Blob& call : calls.children_) {
		//"odd", "even", "odd_outer", "even_outer", "reverse"
		if (call.children_.size() != 2) throw EError("Call needs two things to swap.\n" + call.ErrorString());
		//allow negative numbers from end
		const int first {call.children_.at(0).asInt(1,1000) - 1},
			second {call.children_.at(1).asInt(1,1000) - 1};
//		std::cout << first << " " << second << " " << change_size << "\n";
		if (std::cmp_greater_equal(first, change_size) || std::cmp_greater_equal(second, change_size))
			throw EError("Call parameter out of range.\n" + call.ErrorString());
		std::swap(change.children_.at(first), change.children_.at(second));
	}
}

void Parser::Mingle(Blob &blob) {
	std::vector<Blob> sources;
	int sample_size {0};
	Blob& source_list {blob["from"]};
	Blob& destination {GetMutableBlob(blob["@"].atom(),blob,true)};
	for (auto& source_item : source_list.children_) {
		const std::string source_name {source_item.atom()};
		DictionaryItem item {dictionary_.Find(source_name)};
		if (!item.isMacro()) throw EError("Source variable '"+source_name+"' not a macro.\n" + blob.ErrorString());
		Blob& macro {item.getMacro()};
		if (macro.children_.size() < 1) throw EError("Source variable '"+source_name+"' not long enough\n" + blob.ErrorString());
		sources.push_back(item.getMacro());
	}
	if (blob.hasKey("n"))
		sample_size = blob["n"].asInt(0, int_max);
	else
		sample_size = sources[0].children_.size();
	std::vector<size_t> position(sources.size(), 0);
	destination.children_.clear();
	for (int i=0; i<sample_size; i++)
		for (size_t j=0; j<sources.size(); j++) {
			destination.children_.push_back(sources[j].children_[position[j]]);
			position[j]++;
			if (position[j]>=sources[j].children_.size())
				position[j]=0;
		}
}

void Parser::Rotate(Blob &blob) {
	Blob& change {GetMutableBlob(blob["@"].atom(), blob, false)};
	const size_t size {change.children_.size()};
	size_t n {1};
	if (!size) EError("Source for rotate not long enough\n" + blob.ErrorString());
	if (blob.hasKey("n")) n = static_cast<size_t>(blob["n"].asInt(1, size));
	if (blob.hasFlag("drop_front"))
		change.children_.erase(change.children_.begin(), change.children_.begin() + n);
	else if (blob.hasFlag("drop_back"))
		change.children_.resize(change.children_.size() - n);
	else if (blob.hasFlag("rotate_front"))
		std::rotate(change.children_.begin(), change.children_.begin() + n, change.children_.end());
	else if (blob.hasFlag("rotate_back"))
		std::rotate(change.children_.rbegin(), change.children_.rbegin() + n, change.children_.rend());
}

void Parser::Replicate(Blob &blob) {
	Blob& change {GetMutableBlob(blob["@"].atom(), blob, false)};
	const int n {blob["n"].asInt(1, int_max)};
	const bool cycle {blob.hasFlag("cycle")};
	Blob temp;
	if (cycle) for (int i=0; i<n; i++)
		for (auto& item: change.children_) temp.children_.push_back(item);
	else for (auto& item : change.children_)
		for (int i=0; i<n; i++) temp.children_.push_back(item);
	change.children_ = temp.children_;
}

void Parser::Indirect(Blob &blob) {
	Blob& change {GetMutableBlob(blob["@"].atom(), blob, true)};
	change.children_.clear();
	DictionaryItem from_item {dictionary_.Find(blob["from"].atom())},
		index_item {dictionary_.Find(blob["indices"].atom())};
	if (!from_item.isMacro()) throw EError("From variable is not a macro." + blob.ErrorString());
	if (!index_item.isMacro()) throw EError("Index variable is not a macro." + blob.ErrorString());
	Blob& from {from_item.getMacro()};
	Blob& indices {index_item.getMacro()};
	const size_t max_item {from.children_.size()};
	for (auto& index_string : indices.children_) {
		const int index {index_string.asInt(1,max_item)};
		change.children_.push_back(from[index-1]);
	}
}

void Parser::Unfold(Blob& blob, Sound& sound, ParseParams& params,
		float_type& now, float_type& len, bool make_music, bool fill) {
	constexpr float_type ToleranceTime {0.001};
	float_type duration {0.0};
	const int repeats {(fill) ? 0 : blob["n"].asInt(0, int_max)};
	if (fill) {
		if (params.mode_ != context_mode::seq) throw EError("Unfold with fill must be used in tune mode.\n" + blob.ErrorString());
		if (blob.hasKey("t")) duration = blob["t"].asFloat(0.0, 100000.0);
		else duration = params.TimeDuration(params.current_duration_, now);
	}
	const float_type tend {now + duration};
	Blob& music_blob {blob["music"]};
	if (!music_blob.isBlock(false))
		throw EError("No instruction block provided.\n" + blob.ErrorString());
	int index {0};
	bool more_repeats {true};
	while (more_repeats) {
		float_type context_length;
		if (music_blob.delimiter_ != '(') {
			ParseParams local_params {params};
			context_length = NotesModeBlob(music_blob, sound, local_params, now, make_music);
		} else
			context_length = NotesModeBlob(music_blob, sound, params, now, make_music);
		if (params.mode_ == context_mode::seq) {
			now += context_length;
			len += context_length;
			if (music_blob.delimiter_ != '(')
				UpdateSliders(now, context_length, params);
		} else if (context_length > len)
			len = context_length;
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

void Parser::ForEach(Blob& blob, Sound& sound, ParseParams& params,
		float_type& now, float_type& len, bool makemusic) {
	const auto name {blob["var"].atom()};
	Blob do_blob {blob["do"]};
	Blob& in_blob {blob["in"].ifFunction()};
	if (!do_blob.isBlock(false))
		throw EError("No 'do' block provided.\n" + do_blob.ErrorString());
	for (auto& item : in_blob.children_) {
		if (!dictionary_.Find(name).isNull())
			throw EError(name + ": Object already exists.");
		if (item.isToken()) {
			item = item.Wrap(ascii::STX);
			item.delimiter_ = '(';
		};
		if (!item.isBlock(false))
			throw EError("No instruction block provided.\n" + item.ErrorString());
		if (item.delimiter_ != '(')
			throw EError("Definitions must include matching () delimeters.\n" + item.ErrorString());
		dictionary_.Insert(DictionaryItem(dic_item_type::macro), name).getMacro() = item;
		float_type context_length;
		if (do_blob.delimiter_ != '(') {
			ParseParams local_params {params};
			context_length = NotesModeBlob(do_blob, sound, local_params, now, makemusic);
		} else
			context_length = NotesModeBlob(do_blob, sound, params, now, makemusic);
		if (params.mode_ == context_mode::seq) {
			now += context_length;
			len += context_length;
			if (do_blob.delimiter_ != '(')
				UpdateSliders(now, context_length, params);
		} else if (context_length > len)
			len = context_length;
		dictionary_.Delete(name, false);
	}
}

void Parser::Increment(Blob& blob, int inc) {
	const auto var_name {blob.atom()};
	DictionaryItem& var_item {dictionary_.Find(var_name)};
	if (var_item.isNull()) throw EError("Failed to find '"+var_name+"'.\n" + blob.ErrorString());
	if (!var_item.isMacro()) throw EError("Switch variable '"+var_name+"' not a macro.\n" + blob.ErrorString());
	if (var_item.getMacroType()!=macro_type::variable) throw EError("Switch variable '"+var_name+"' not assigned using 'let'.\n" + blob.ErrorString());
	int value {var_item.getMacro().asInt()};
	std::ostringstream stream;
	stream << value+inc;
	var_item.getMacro()[0].val_ = stream.str();
}

void Parser::Condition(Blob& blob, ParseParams& params) {
	bool condition {false};
	const std::string command {blob[0].atom()}, var_name {":condition"};
	if ((command == "note_value_ceiling") || (command == "note_value_floor")) {
		const std::string val {blob[1].atom()};
		const NoteValue ref {params.gamut_.NoteAbsolute(val)};
		const float_type frequency_difference {params.gamut_.FreqMultFromNote(params.last_note_) / params.gamut_.FreqMultFromNote(ref)};
		if (command == "note_value_ceiling") condition = frequency_difference <= 1.0;
		else condition = frequency_difference >= 1.0;
	} else if (command == "random") condition = Rand.uniform() > blob[1].asFloat(0.0, 1.0);
	else throw EError("Condition not recognised\n" + blob.ErrorString());
	if (!dictionary_.contains(var_name)) ParseString("global(let(:condition(0)))");
	DictionaryItem& var_item {dictionary_.Find(var_name)};
	if (!var_item.isMacro()) throw EError("Switch variable '"+var_name+"' not a macro.\n" + blob.ErrorString());
	if (var_item.getMacroType()!=macro_type::variable) throw EError("Switch variable '"+var_name+"' not assigned using 'let'.\n" + blob.ErrorString());
	var_item.getMacro()[0].val_ = condition? "1" : "2";
}

void Parser::Switch(Blob& blob, Sound& sound, ParseParams& params,
		float_type& now, float_type& len, bool make_music, bool by_index) {
	const auto var_name {blob.hasFlag("condition")? ":condition" : blob["var"].atom()};
	DictionaryItem& var_item {dictionary_.Find(var_name)};
	if (var_item.isNull()) throw EError("Failed to find '"+var_name+"'." + blob.ErrorString());
	if (!var_item.isMacro()) throw EError("Switch variable '"+var_name+"' not a macro." + blob.ErrorString());
	Blob switches {blob["case"].ifFunction()}, music_blob;
	if (by_index) {
		const size_t var_i {static_cast<size_t>(var_item.getMacro().asInt())};
		if ((var_i < 1) || (var_i > switches.children_.size())) throw EError("Index "+var_name+" out of range." + blob.ErrorString());
		music_blob = switches[var_i - 1];
	} else {
		const auto var_value {var_item.getMacro().atom()};
		std::string matched;
		if (switches.hasKey(var_value)) matched = var_value; 
		else if (switches.hasKey("default")) matched = "default";
		else return;
		music_blob = switches[matched];
	}
	if (!music_blob.isBlock(false))
		throw EError("No instruction block provided.\n" + blob.ErrorString());
	float_type context_length;
	if (music_blob.delimiter_ != '(') {
		ParseParams local_params {params};
		context_length = NotesModeBlob(music_blob, sound, local_params, now, make_music);
	} else
		context_length = NotesModeBlob(music_blob, sound, params, now, make_music);
	if (params.mode_ == context_mode::seq) {
		now += context_length;
		len += context_length;
		if (music_blob.delimiter_ != '(')
			UpdateSliders(now, context_length, params);
	} else if (context_length > len)
		len = context_length;
}

void Parser::Trill(Blob& blob, Sound& sound, ParseParams& params, float_type& now, float_type& len, bool make_music) {
	if (params.mode_ != context_mode::seq)
		throw EError("Trill must be in tune mode\n" + blob.ErrorString());
	const size_t count_params {blob.children_.size()};
	bool do_turn {false};
	if (count_params < 3)
		throw EError("Malformed trill\n" + blob.ErrorString());
	int length {blob[0].asInt(4, 1000)};
	std::string first {blob[1].atom()}, second {blob[2].atom()}, turn;
	if (count_params == 4) {
		do_turn = true;
		turn = blob[3].atom();
	}
	ParseParams note_params {params};
	note_params.current_duration_ = NoteDuration{params.current_duration_.getDuration() / static_cast<float_type>(length)};
	for (int index {0}; index < length; index++) {
		if (do_turn && (index == length - 2))
			PlayNote(turn, blob, sound, note_params, now, len, make_music);
		else if (index %2 == 0)
			PlayNote(first, blob, sound, note_params, now, len, make_music);
		else
			PlayNote(second, blob, sound, note_params, now, len, make_music);
	}
}

// Sample mode functions

void Parser::LoadLibrary(std::string file_name, verbosity_type verbosity, bool internal) {
	VerbosityScope verbosity_scope;
	if (portable_) internal = false; 
	std::filesystem::path full_path = internal?
		std::filesystem::path {platform.AppConfigDir()} / file_name:
		std::filesystem::path {file_name};
	std::ifstream file(full_path.c_str(), std::ios::in);
	if (file.is_open()) {
		Blob blob;
		DoMessage("<parsing " + file_name + ">", verbosity);
		blob.Parse(file);
		file.close();
		ParseBlobs(blob);
		//Message("<finished " + file_name + ">");
	} else
		throw EError(file_name + ": Loading library failed");
}

Parser::parse_exit Parser::ParseString(std::string input) {
	Blob blob;
	std::istringstream stream(input, std::istringstream::in);
	blob.Parse(stream);
	return ParseBlobs(blob);
}

Parser::parse_exit Parser::ParseImmediate() {
	VerbosityScope verbosity_scope;
	std::string input;
	parse_exit exit_code {parse_exit::exit};
	do
		try {
			screen.PrintInline(screen.Prompt("BoxyLady$ "));
			getline(std::cin, input);
			if (!std::cin.good())
				exit_code = parse_exit::end;
			else if (input.length())
				exit_code = ParseString(input);
		} catch (EError& error) {
			if (error.is_terminate()) throw;
			screen.PrintError(error);
		} while (exit_code != parse_exit::end);
	return exit_code;
}

Parser::parse_exit Parser::ParseBlobs(Blob& blob) {
	parse_exit exit_code {parse_exit::exit};
	for (auto& instruction : blob.children_) {
		if (instruction.isToken()) {
			std::string token {instruction.val_};
			if (token[0] == '\\') {
				std::string name {token.erase(0, 1)};
				DictionaryItem& item {dictionary_.Find(name)};
				if (item.isNull())
					throw EError("\\" + name + ": No such object.");
				DictionaryMutex mutex {item};
				if (item.isMacro())
					ParseBlobs(item.getMacro());
				else
					throw EError("\\" + name + ": Is not a macro.");
				continue;
			}
			throw EError(token + ": Unknown command. () missing?\n" + blob.ErrorString());
		}
		instruction.AssertFunction();
		if (const auto token {instruction.key_}; token == "exit") {
			exit_code = parse_exit::end;
			break;
		} else if (token == "quit")
			throw EError("quit()", error_type::terminate);
		else if (token == "--version")
			screen.PrintMessage(BootInformation, {});
		else if (token == "BoxyLady") {
			if (VersionNumber != instruction.atom())
				DoMessage("This is not the BoxyLady version you are looking for.");
		} else if (token == "--help")
			screen.PrintWrap(BootHelp, Screen::PrintFlags({Screen::print_flag::wrap, Screen::print_flag::indent}));
		else if (token == "--poem")
			screen.PrintMessage(Poem, {});
		else if (token == "--interactive")
			ParseImmediate();
		else if (token == "--portable")
			portable_ = instruction.asBool();
		else if (token == "print")
			ShowPrint(instruction);
		else if (token == "rem") {
			//do nothing
		} else if (token == "source")
			LoadLibrary(instruction.atom(), verbosity_type::messages, false);
		else if (token == "library") {
			try {
				LoadLibrary(instruction.atom(), verbosity_type::errors, true);
			} catch (EError& error) {
				if (error.is_terminate()) throw;
				screen.PrintError(error);
			}
		} else if (token == "--messages")
			verbosity_ = BuildVerbosity(instruction.atom());
		else if (token == "config")
			ParseConfig(instruction);
		else if (token == "seed") {
			if (instruction.hasKey("val"))
				Rand.SetSeed(instruction["val"].asInt());
			else
				Rand.AutoSeed();
		} else if (token == "synth")
			Synth(instruction);
		else if (token == "def")
			MakeMacro(instruction, macro_type::macro, false);
		else if (token == "input")
			ReadCIN(instruction);
		else if ((token == "seq") || (token == "sequence"))
			MakeMusic(instruction);
		else if (token == "quick")
			QuickMusic(instruction);
		else if (token == "global")
			GlobalDefaults(instruction);
		else if (token == "list") {
			if (verbosity_ >= verbosity_type::messages)
				dictionary_.ListEntries(instruction);
		} else if (token == "defrag")
			Defrag();
		else if (token == "access")
			SetAccess(instruction);
		else if (token == "read")
			ReadSound(instruction);
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
		else if (token == "correl_plot")
			CorrelationPlot(instruction);
		else if (token == "delete")
			Delete(instruction);
		else if (token == "rename")
			Rename(instruction);
		else if (token == "write")
			WriteSound(instruction);
		else if (token == "play")
			PlayEntry(instruction);
		else if (token == "metadata")
			Metadata(instruction);
		else if (token == "external")
			ExternalProcessing(instruction);
		else if (token == "shell")
			ExternalCommand(instruction);
		else if (token == "terminal")
			ExternalTerminal(instruction);
		else if (token == "pwd")
			GetWD();
		else if (token == "cd")
			SetWD(instruction);
		else if (token == "ls")
			Ls(instruction);
		else if (token == "create")
			Create(instruction);
		else if (token == "instrument")
			Instrument(instruction);
		else if (token == "resize")
			Resize(instruction);
		else if (token == "crossfade")
			CrossFade(instruction);
		else if (token == "fade")
			Fade(instruction);
		else if (token == "amp")
			Balance(instruction);
		else if (token == "reverb")
			EchoEffect(instruction);
		else if (token == "karplus_strong")
			KarplusStrong(instruction);
		else if (token == "chowning")
			Chowning(instruction);
		else if (token == "modulator")
			Modulator(instruction, std::nullopt);
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
		else if (token == "fourier_cleanpass")
			FourierCleanPass(instruction);
		else if (token == "fourier_limiter")
			FourierLimit(instruction);
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
		else if (token == "fourier_scale")
			FourierScale(instruction);
		else if (token == "pitch_scale")
			PitchScale(instruction);
		else if (token == "fourier_power")
			FourierPower(instruction);
		else if (token == "repeat")
			Repeat(instruction);
		else if (token == "flags")
			Flags(instruction);
		else if (token == "envelope")
			ApplyEnvelope(instruction);
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
		else if (token == "filter_sweep")
			FilterSweep(instruction);
		else
			throw EError(token + ": Unknown command.\n" + blob.ErrorString());
	}
	return exit_code;
}

void Parser::ParseConfig(Blob& blob) {
	if (!blob.children_.size()) {
		ShowConfig(blob);
		return;	
	}
	for (auto& instruction : blob.children_) {
		const std::string key {instruction.key_};
		if (key == "default_sample_rate")
			default_sample_rate_ = instruction.asInt(Sound::MinSampleRate, Sound::MaxSampleRate);
		else if (key == "max_instrument_length")
			max_instrument_duration_ = instruction.asFloat(0.0, HourLength);
		else if (key == "standard_pitch")
			standard_pitch_ = instruction.asFloat(220.0, 880.0);
		else if (key == "interpolation")
			Sound::linear_interpolation = instruction.asBool();
		else if (key == "play_command")
			file_play_ = instruction.atom();
		else if (key == "terminal_command")
			terminal_ = instruction.atom();
		else if (key == "mp3_command")
			mp3_encoder_ = instruction.atom();
		else if (key == "ls_command")
			ls_ = instruction.atom();
		else if (key == "default_metadata")
			DefaultMetadata(instruction);
		else if (key == "echo_shell")
			echo_shell_ = instruction.asBool();
		else
			throw EError(key + ": Unknown config setting.\n" + blob.ErrorString());
	}
}

void Parser::ShowConfig([[maybe_unused]] Blob& blob) {
	auto Print = [](std::string item) {screen.PrintWrap(item, Screen::PrintFlags({Screen::print_flag::frame, Screen::print_flag::wrap, Screen::print_flag::indent}));};
	screen.PrintHeader("Configuration and global variables");
	Print("mp3_command = " + mp3_encoder_);
	Print("play_command = " + file_play_);
	Print("terminal_command = " + terminal_);
	Print("ls_command = " + ls_);
	Print(std::format("default_sample_rate = {}", default_sample_rate_));
	Print(std::format("max_instrument_duration = {}", max_instrument_duration_));
	Print(std::format("standard_pitch = {}", standard_pitch_));
	Print("interpolation(" + BoolToString(Sound::linear_interpolation) + ")");
	Print("echo_shell(" + BoolToString(echo_shell_) + ")");
	screen.PrintSeparatorSub();
	Print("--supervisor(" + BoolToString(supervisor_) + ")");
	Print("--portable(" + BoolToString(portable_) + ")");
	screen.PrintSeparatorBot();
	Sound::default_metadata_.Dump(true);
}

void Parser::ReadCIN(Blob& blob) {
	const std::string name {blob.atom()};
	std::string input;
	screen.PrintInline(screen.Prompt("\\" + name + "$ "));
	getline(std::cin, input);
	std::string command {"def(" + name + "(" + input + "))"};
	MakeMacro(Blob{command}[0], macro_type::macro);
}

void Parser::MakeMacro(Blob& blob, macro_type type, bool allow_replace) {
	std::string name;
	for (auto& macro_blob : blob.children_) {
		const auto name {macro_blob.key_};
		if (dictionary_.contains(name)) {
			if (!allow_replace) throw EError(name + ": Object already exists.");
			DictionaryItem item {dictionary_.Find(name)};
			if (item.getType()!=dic_item_type::macro) throw EError(name + ": Object must be a macro.");
			if (item.getMacroType()!=type) throw EError(name + ": Cannot replace different type of macro.");
			dictionary_.Delete(name);
		}
		if (!macro_blob.isBlock(false))
			throw EError(name + ": No instruction block provided.\n" + blob.ErrorString());
		if (macro_blob.delimiter_ != '(')
			throw EError("Definitions must include matching () delimeters.\n" + blob.ErrorString());
		DictionaryItem& new_item {dictionary_.Insert(DictionaryItem(dic_item_type::macro), name)};
		new_item.getMacro() = macro_blob;
		new_item.getMacroType() = type;
		DoMessage("Created macro [" + name + "]", verbosity_type::messages);
	}
}

void Parser::ReadSound(Blob& blob) {
	const auto sound_name {blob["@"].atom()},
		file_name {blob["file"].atom()};
	const bool debug {blob.hasFlag("debug")};
	file_format format {file_format::RIFF_wav};
	Sound& sound {dictionary_.InsertSound(sound_name)};
	sound.LoadFromFile(file_name, format, debug);
	SampleType sample_type {sound.getType()};
	if (blob.hasKey("type")) {
		sample_type = BuildSampleType(blob["type"]);
		sound.setType(sample_type);
	}
	TryMessage("Loaded patch `" + file_name + "` as [" + sound_name + "]", blob);
}

void Parser::Clone(Blob& blob) {
	verbosity_type verbosity {verbosity_type::none};
	size_t min_args {2};
	if (blob.hasKey("shh!")) {
		verbosity = BuildVerbosity(blob["shh!"].atom());
		min_args++;
	}
	if (blob.children_.size() < min_args) throw EError("Must have at least one source and destination to copy\n" + blob.ErrorString());
	const auto source_name {blob[0].atom()}; 
	Sound& source_sound {dictionary_.FindSound(source_name)};
	for (auto item {++blob.children_.begin()}; item != blob.children_.end(); item++) {
		if (item->key_ == "shh!") continue;
		const auto new_name {item->atom()};
		if (dictionary_.contains(new_name))
			throw EError(new_name + ": Object already exists.");
		Sound& new_sound {dictionary_.InsertSound(new_name)};
		new_sound = source_sound;
		DoMessage("Copied patch [" + source_name + "] as [" + new_name + "]", verbosity);
	}
}

void Parser::Combine(Blob& blob) {
	const auto new_name {blob["@"].atom()}, left {blob["l"].atom()},
			right {blob["r"].atom()};
	Sound& left_sound {dictionary_.FindSound(left)},
		&right_sound {dictionary_.FindSound(right)};
	if (dictionary_.contains(new_name))
		throw EError(new_name + ": Object already exists.");
	Sound& new_sound {dictionary_.InsertSound(new_name)};
	new_sound.Combine(left_sound, right_sound);
	TryMessage("Combined patches [" + left + "] and [" + right + "] as [" + new_name + "]", blob);
}

void Parser::Mix(Blob& blob) {
	const auto new_name {blob["@"].atom()}, a_name {blob["a_name"].atom()},
		b_name {blob["b_name"].atom()};
	const int channels {blob["channels"].asInt(1, MaxChannels)};
	Sound& a_sound {dictionary_.FindSound(a_name)},
		&b_sound {dictionary_.FindSound(b_name)};
	const Stereo a_stereo {BuildStereo(blob["stereo_a"])},
		b_stereo {BuildStereo(blob["stereo_b"])};
	if (dictionary_.contains(new_name))
		throw EError(new_name + ": Object already exists.");
	Sound& new_sound {dictionary_.InsertSound(new_name)};
	new_sound.Mix(a_sound, b_sound, a_stereo, b_stereo, channels);
	TryMessage("Mixed patches [" + a_name + "] and [" + b_name + "] as [" + new_name + "]", blob);
}

void Parser::Split(Blob& blob) {
	const auto nname {blob["@"].atom()}, left_name {blob["l"].atom()},
			right_name {blob["r"].atom()};
	Sound& source {dictionary_.FindSound(nname)};
	if (dictionary_.contains(left_name))
		throw EError(nname + ": Object already exists.");
	if (dictionary_.contains(right_name))
		throw EError(nname + ": Object already exists.");
	Sound& left_sound {dictionary_.InsertSound(left_name)},
		&right_sound {dictionary_.InsertSound(right_name)};
	source.Split(left_sound, right_sound);
	TryMessage("Split patches [" + left_name + "] and [" + right_name + "] from [" + nname + "]", blob);
}

void Parser::Rechannel(Blob& blob) {
	Sound& sound {dictionary_.FindSound(blob)};
	const int channels {blob["channels"].asInt(1, 2)};
	sound.Rechannel(channels);
}

void Parser::Cut(Blob& blob) {
	Sound& sound {dictionary_.FindSound(blob)};
	Window window {BuildWindow(blob)};
	sound.Cut(window);
}

void Parser::Paste(Blob& blob) {
	const auto new_name {blob["@"].atom()},
		source_name {blob["source"].atom()};
	Sound& source_sound {dictionary_.FindSound(source_name)};
	if (dictionary_.contains(new_name))
		throw EError(new_name + ": Object already exists.");
	Sound& new_sound {dictionary_.InsertSound(new_name)};
	Window window {BuildWindow(blob)};
	new_sound.Paste(source_sound, window);
	TryMessage("Pasted from patch [" + source_name + "] as [" + new_name + "]", blob);
}

void Parser::Histogram(Blob& blob) {
	const auto name {blob["@"].atom()};
	const bool plot {blob.hasFlag("plot")}, scale {blob.hasFlag("scale")};
	float_type clip {0.0};
	blob.tryWritefloat_type("clip", clip, 0.0, 1.0);
	Sound& sound {dictionary_.FindSound(name)};
	sound.AssertMusic();
	if (plot) screen.PrintHeader(std::string("Histogram of [") + name + "]");
	dictionary_.FindSound(blob).Histogram(scale, plot, clip);
	if (plot) screen.PrintSeparatorBot();
}

void Parser::CorrelationPlot(Blob& blob) {
	dictionary_.FindSound(blob).CorrelPlot();
}

void Parser::Rename(Blob& blob) {
	const auto old_name {blob[0].atom()}, new_name {blob[1].atom()}; //s
	DictionaryItem& item {dictionary_.Find(old_name)};
	if (item.isNull())
		throw EError(old_name + ": No such object.");
	if (dictionary_.contains(new_name))
		throw EError(new_name + ": Object already exists.");
	if (item.ProtectionLevel() > dic_item_protection::normal)
		throw EError(new_name + ": Object is protected and cannot be renamed.");
	if (!item.ValidName(new_name))
		throw EError(new_name + ": Illegal character in name.");
	dictionary_.Rename(old_name, new_name);
	TryMessage("Renamed [" + old_name + "] to [" + new_name + "]", blob);
}

void Parser::WriteSound(Blob& blob) {
	TempFilename unique_filename;
	std::string format_name {"boxy"}, temp_file_name;
	blob.tryWriteString("format", format_name);
	bool write_metadata {false};
	blob.tryWriteBool("metadata", write_metadata);
	const auto file_name {blob["file"].atom()},
		sound_name {blob["@"].atom()};
	file_format format {file_format::boxy};
	if (format_name == "boxy")
		format = file_format::boxy;
	else if (format_name == "wav")
		format = file_format::RIFF_wav;
	else if (format_name == "mp3")
		format = file_format::mp3;
	else
		throw EError("File type specified [" + format_name + "] not recognised.");
	Sound& sound {dictionary_.FindSound(sound_name)};
	switch (format) {
	case file_format::boxy:
		[[fallthrough]];
	case file_format::RIFF_wav:
		sound.SaveToFile(file_name, format, write_metadata);
		break;
	case file_format::mp3:
		temp_file_name = unique_filename.file_name();
		sound.SaveToFile(temp_file_name, file_format::RIFF_wav, false);
		Mp3Encode(temp_file_name, file_name, sound.metadata());
		remove(temp_file_name.c_str());
		break;
	}
	TryMessage("Saved patch [" + sound_name + "] to `" + file_name + "`", blob);
}

int Parser::PlayFile(std::string file_name, std::string command) {
	const static std::string file_token {"%file"};
	CheckSystem();
	command.replace(command.find(file_token), file_token.length(), file_name);
	if (echo_shell_) screen.PrintMessage(screen.Prompt() + command);
	return system(command.c_str());
}

void Parser::CommandReplaceString(std::string& command, std::string what,
		std::string with, std::string why) {
	const size_t found {command.find(what)};
	if (found == std::string::npos)
		throw EError("Token replacement: cannot find '" + what + "' in command for '" + why + "'.");
	command.replace(found, what.length(), with);
}

int Parser::ExternalCommand(Blob& blob) const {
	if (const auto token {blob.atom()}; system(token.c_str()) != 0)
		throw EError("External programme call: return code from system call indicates error.");
	else return 0;
}

int Parser::ExternalTerminal([[maybe_unused]] Blob& blob) const {
	if (system(terminal_.c_str()) != 0)
		throw EError("External programme call: return code from system call indicates error.");
	else return 0;
}

void Parser::GetWD() const {
    try {
        std::filesystem::path current_dir = std::filesystem::current_path();
		DoMessage(std::string{"Current working directory: "} + current_dir.string());
    } catch (const std::filesystem::filesystem_error& e) {
		throw EError("Can't get current working directory for some reason.");
	}
}

void Parser::SetWD(Blob& blob) const {
	std::string current_dir {blob.atom()};
    try {
        std::filesystem::current_path(current_dir);
    } catch (const std::filesystem::filesystem_error& e) {
		throw EError("Can't get current working directory to " + current_dir + " for some reason.");
	}
	GetWD();
}

int Parser::Ls([[maybe_unused]] Blob& blob) const {
	if (system(ls_.c_str()) != 0)
		throw EError("External programme call: return code from system call indicates error.");
	else return 0;
}

void Parser::ExternalProcessing(Blob& blob) {
	CheckSystem();
	std::string sound_name = {""}, command = {""}, argument = {""};
	if (blob.hasKey("@"))
		sound_name = blob["@"].atom();
	else
		throw EError("External operation: no sound_name specified.");
	if (blob.hasKey("command"))
		command = blob["command"].atom();
	else
		throw EError("External operation: no command.");
	Sound& sound {dictionary_.FindSound(sound_name)};
	TempFilename source_temp, destination_temp;
	std::string source_file_name {source_temp.file_name()},
		destination_file_name {destination_temp.file_name()};
	CommandReplaceString(command, "%source", source_file_name, "external()");
	CommandReplaceString(command, "%dest", destination_file_name, "external()");
	if (echo_shell_) TryMessage(screen.Prompt() + command, blob);
	sound.SaveToFile(source_file_name, file_format::RIFF_wav, false);
	Sound temp_sound;
	temp_sound.CopyType(sound);
	int return_code {system(command.c_str())};
	if (return_code != 0)
		throw EError("External programme call: return code from system call indicates error.");
	sound.Clear();
	sound.LoadFromFile(destination_file_name, file_format::RIFF_wav);
	sound.CopyType(temp_sound);
}

void Parser::PlayEntry(Blob& blob) {
	const bool mono {blob.hasFlag("mono")};
	if (file_play_.length() == 0)
		throw EError("File play: no command string set.");
	std::string file_name {""}, sound_name {""}, argument {""}, command {file_play_};
	blob.tryWriteString("file", file_name);
	blob.tryWriteString("arg", argument);
	blob.tryWriteString("@", sound_name);
	CommandReplaceString(command, "%arg", argument, "play()");
	if (file_name.length()) {
		PlayFile(file_name, command);
	} else if (sound_name.length()) {
		Sound& source_sound {dictionary_.FindSound(sound_name)};
		file_name = TempFilename().file_name();
		if (mono) {
			Sound temp_sound {source_sound};
			temp_sound.Rechannel(1);
			temp_sound.SaveToFile(file_name, file_format::RIFF_wav, false);
		}
		else source_sound.SaveToFile(file_name, file_format::RIFF_wav, false);
		int return_code {PlayFile(file_name, command)};
		if (return_code != 0)
			DoMessage("\nFile play: error returned from external programme. [" + file_play_ + "]");
		remove(file_name.c_str());
	} else
		throw EError("File play: nothing to play.");
}

void Parser::Metadata(Blob& blob) { 
	const auto name {blob["@"].atom()};
	DictionaryItem& item {dictionary_.Find(name)};
	if (item.getType() != dic_item_type::sound)
		throw EError("No sound '"+name+"' exists.");
	Sound& sound {item.getSound()};
	MetadataList& metadata {sound.metadata()};
	for (auto& child : blob.children_) {
		if (const auto key {child.key_}; key == "@")
			continue;
		else if (child.atom() == "print")
			metadata.Dump();
		else metadata[key] = child.val_;
	}
}

void Parser::DefaultMetadata(Blob& blob) {
	const auto key {blob["key"].atom()}, value {blob["value"].atom()},
		mp3 {blob.hasKey("mp3")? blob["mp3"].atom() : ""},
		RIFF {blob.hasKey("RIFF")? blob["RIFF"].atom() : ""};
	if (RIFF.length() > 0)
		if (RIFF.length() != 4) throw EError("RIFF tags must be four characters long (" + RIFF + ").");
	Sound::default_metadata_.EditListItem(key, mp3, RIFF, value);
}

void Parser::Mp3Encode(std::string source_file_name,
		std::string destination_file_name, MetadataList metadata) {
	CheckSystem();
	std::string command {mp3_encoder_};
	command = metadata.Mp3CommandUpdate(command);
	CommandReplaceString(command, "%source", source_file_name, "mp3encode()");
	CommandReplaceString(command, "%dest", destination_file_name, "mp3encode()");
	if (echo_shell_) screen.PrintMessage(screen.Prompt() + command);
	int return_code {system(command.c_str())};
	if (return_code != 0)
		throw EError("Conversion to MP3: return code from system call indicates error.");
}

void Parser::ShowPrint(Blob& blob) {
	if (blob.hasKey("@")) {
		if (blob.children_.size() > 1)
			throw EError("Print requires one argument.\n" + blob.ErrorString());
		const auto name {blob["@"].atom()};
		DictionaryItem& item {dictionary_.Find(name)};
		if (item.isNull())
			throw EError(name + ": No such object.");
		if (item.isMacro())
			DoMessage(item.getMacro().Dump(), verbosity_type::none, {Screen::escape::cyan});
		else if (item.isSound()) {
			screen.PrintHeader(std::string("Plot of [") + name + "]");
			item.getSound().Plot();
			screen.PrintSeparatorBot();
		}
	} else
		DoMessage(blob.atom(), verbosity_type::none, {Screen::escape::cyan});
}

void Parser::Create(Blob& blob) {
	const auto name {blob["@"].atom()};
	const int channels {blob["channels"].asInt(1, MaxChannels)};
	if ((channels < 1) || (channels > 2))
		throw EError("Only 1 or 2 channels are currently supported.");
	const float_type t_length {blob["len"].ifFunction()[0].asFloat(0, HourLength)},
		p_length {(blob["len"].children_.size() == 2) ?
			blob["len"][1].asFloat(0, HourLength) : t_length};
	SampleType type {BuildSampleType(blob["type"])};
	Sound& sound {dictionary_.InsertSound(name)};
	sound.CreateSilenceSeconds(channels, type.sample_rate, t_length, p_length);
	sound.setType(type);
	TryMessage("Created patch [" + name + "]", blob);
}

void Parser::Instrument(Blob& blob) {
	dictionary_.Delete("instrument");
	const int channels {blob["channels"].asInt(1, MaxChannels)};
	if ((channels < 1) || (channels > 2))
		throw EError("Only 1 or 2 channels are currently supported.");
	float_type t_length {instrument_duration_}, p_length {t_length};
	if (blob.hasKey("len")) {
		t_length = blob["len"].ifFunction()[0].asFloat(0.0, HourLength);
		p_length = (blob["len"].children_.size() == 2) ?
			blob["len"][1].asFloat(0.0, HourLength) : t_length;
	}
	SampleType type {BuildSampleType(blob["type"])};
	type.sample_rate = instrument_sample_rate_;
	Sound& sound {dictionary_.InsertSound("instrument")};
	sound.CreateSilenceSeconds(channels, type.sample_rate, t_length, p_length);
	sound.setType(type);
	TryMessage("Created patch [instrument]", blob);
}

void Parser::Resize(Blob& blob) {
	Sound& sound {dictionary_.FindSound(blob)};
	const auto mode {blob["mode"].atom()};
	if (mode == "auto") {
		float_type threshold {0.0};
		blob.tryWritefloat_type("threshold", threshold, 0.0, 1.0);
		sound.AutoResize(threshold);
		return;
	}
	const float_type t_length {blob["len"].ifFunction()[0].asFloat(0, HourLength)},
		p_length {(blob["len"].children_.size() == 2) ?
			blob["len"][1].asFloat(0, HourLength) : t_length};
	if (mode == "absolute")
		sound.Resize(t_length, p_length, false);
	else if (mode == "relative")
		sound.Resize(t_length, p_length, true);
	else
		throw EError(mode + ": Unknown resize mode.");
}

void Parser::Defrag() {
	dictionary_.Apply([](DictionaryItem& item) {
		if (item.isSound())
			item.getSound().Defrag();
	});
}

void Parser::SetAccess(Blob& blob) {
	verbosity_type verbosity {verbosity_type::messages};
	if (blob.hasKey("shh!"))
		verbosity = BuildVerbosity(blob["shh!"].atom());
	for (auto& child : blob.children_) {
		if (child.key_ == "shh!") continue;
		const auto name {child.key_}, atom {child.atom()};
		DictionaryItem& item {dictionary_.Find(name)};
		if (item.isNull())
			throw EError(name + ": No such object.");
		if ((!supervisor_) && (item.ProtectionLevel() == dic_item_protection::system))
			throw EError(name + ": Cannot change protection level.");
		dic_item_protection protection_level {BuildProtection(atom)};
		if ((!supervisor_) && (protection_level == dic_item_protection::system))
			throw EError(name + ": Cannot change to system level protection.");
		item.Protect(protection_level);
		DoMessage("Changed slot [" + name + "] protection level to '" + atom + "'.", verbosity);
	}
}

void Parser::Delete(Blob& blob) {
	verbosity_type verbosity {verbosity_type::none};
	if (blob.hasKey("shh!"))
		verbosity = BuildVerbosity(blob["shh!"].atom());
	for (auto& child : blob.children_) {
		if (child.key_ == "shh!") continue;
		else if (const auto name {child.atom()}; name == "*") {
			dictionary_.Clear(true);
			DoMessage("Cleared all dictionary entries");
			break;
		} else {
			if (dictionary_.Delete(name, true))
				DoMessage("Deleted [" + name + "]", verbosity);
			else
				DoMessage("Slot [" + name + "] doesn't exist, is protected, or in use.", verbosity);
		}
	}
}

void Parser::CrossFade(Blob& blob) {
	const auto start_name {blob["start"].atom()},
		end_name {blob["end"].atom()},
		new_name {blob["@"].atom()};
	Sound& start_sound {dictionary_.FindSound(start_name)},
			&end_sound {dictionary_.FindSound(end_name)};
	if (dictionary_.contains(new_name))
		throw EError(new_name + ": Object already exists.");
	Sound& new_sound {dictionary_.InsertSound(new_name)}, temp_sound {end_sound};
	new_sound = start_sound;
	new_sound.CrossFade(CrossFader::FadeOut().Linear());
	temp_sound.CrossFade(CrossFader::FadeIn().Linear());
//	new_sequence.Overlay(temp_sequence, 0);
	new_sound.DoOverlay(temp_sound)();
	TryMessage("Crossfaded patches [" + start_name + "] and [" + end_name + "] as [" + new_name + "]", blob);
}

void Parser::Fade(Blob& blob) {
	const auto mode {blob["mode"].atom()};
	CrossFader fader;
	Sound& sound {dictionary_.FindSound(blob)};
	if (mode == "fade_in")
		fader = CrossFader::FadeIn();
	else if (mode == "fade_out")
		fader = CrossFader::FadeOut();
	else if (mode == "linear_fade_in")
		fader = CrossFader::FadeIn().Linear();
	else if (mode == "linear_fade_out")
		fader = CrossFader::FadeOut().Linear();
	else if (mode == "pan_swap")
		fader = CrossFader::PanSwap();
	else if (mode == "pan_centre")
		fader = CrossFader::PanCentre();
	else if (mode == "pan_edge")
		fader = CrossFader::PanEdge();
	else if (mode == "manual") {
		const Stereo start_parallel {BuildStereo(blob["start_a"])},
				start_crossed {BuildStereo(blob["start_x"])},
				end_parallel {BuildStereo(blob["end_a"])},
				end_crossed {BuildStereo(blob["end_x"])};
		CrossFader temp_fader(MatrixMixer(start_parallel, start_crossed), MatrixMixer(end_parallel, end_crossed));
		if (blob.hasFlag("linear"))
			temp_fader.Linear();
		else if (blob.hasFlag("log"))
			temp_fader.Logarithmic();
		fader = temp_fader;
	} else
		throw EError(mode + ": Unknown fade mode.");
	if (blob.hasFlag("mirror"))
		fader.Mirror();
	sound.CrossFade(fader);
}

void Parser::Balance(Blob& blob) {
	std::string mode_string;
	if (!blob.tryWriteString("mode", mode_string))
		mode_string = "vol";
	Sound& sound {dictionary_.FindSound(blob)};
	if (mode_string == "vol")
		sound.CrossFade(CrossFader::Amp(BuildAmplitude(blob["a"])));
	else if (mode_string == "stereo")
		sound.CrossFade(CrossFader::AmpStereo(BuildStereo(blob["a"])));
	else if (mode_string == "cross")
		sound.CrossFade(CrossFader::AmpCross(BuildStereo(blob["a"]), BuildStereo(blob["x"])));
	else if (mode_string == "delay") {
		const Stereo parallel {BuildStereo(blob["a"])}, crossed {BuildStereo(blob["x"])};
		const float_type delay {blob["delay"].asFloat()};
		sound.DelayAmp(parallel, crossed, delay);
	} else if (mode_string == "inverse")
		sound.CrossFade(CrossFader::AmpInverse());
	else if (mode_string == "inverse_lr")
		sound.CrossFade(CrossFader::AmpInverseLR());
	else
		throw EError(mode_string + ": Unknown balance operation.");
}

void Parser::EchoEffect(Blob& blob) {
	const float_type delay {blob["delay"].asFloat()},
		amp {BuildAmplitude(blob["a"])};
	const int count {(blob.hasKey("n")) ? blob["n"].asInt(1, 1000) : 1};
	const bool resize {blob.hasFlag("resize")};
	FilterVector filters;
	if (blob.hasKey("filter"))
		filters = BuildFilters(blob["filter"]);
	dictionary_.FindSound(blob).EchoEffect(delay, amp, count, filters, resize);
}

void Parser::Tremolo(Blob& blob) {
	const Wave wave {BuildWave(blob["wave"])};
	dictionary_.FindSound(blob).Ring(std::nullopt, wave, false, 0.5, 1.0 - 0.5 * wave.amp());
}

void Parser::RingModulation(Blob& blob) {
	float_type amp {1.0}, bias {0.0};
	if (blob.hasKey("a"))
		amp = BuildAmplitude(blob["a"]);
	blob.tryWritefloat_type("bias", bias, -1000, 1000);
	Wave wave;
	if (blob.hasKey("wave")) {
		wave = BuildWave(blob["wave"]);
		dictionary_.FindSound(blob).Ring(std::nullopt, wave, false, amp, bias);
	} else if (blob.hasKey("with")) {
		Sound& with_sound {dictionary_.FindSound(blob["with"].atom())};
		dictionary_.FindSound(blob).Ring(with_sound, wave, false, amp, bias);
	} else
		throw EError(blob[0].Dump() + ": Unknown ring modulation operation.");
}

Filter Parser::BuildFilter(Blob& blob) const {
	blob.AssertFunction();
	if (const auto token {blob.key_}; token == "lowpass")
		return BuildLowPass(blob);
	else if (token == "highpass")
		return BuildHighPass(blob);
	else if (token == "bandpass")
		return BuildBandPass(blob);
	else if (token == "amp")
		return Filter::Amp(BuildAmplitude(blob));
	else if (token == "distort")
		return Filter::Distort(blob.asFloat(0.001, 1000.0));
	else if (token == "ks:blend")
		return Filter::KSBlend(blob.asFloat(0.0, 1.0));
	else if (token == "ks:reverse")
		return Filter::KSReverse(blob.asFloat(0.0, 1.0));
	else if (token == "fourier_gain")
		return BuildFourierGain(blob);
	else if (token == "fourier_bandpass")
		return BuildFourierBandpass(blob);
	else if (token == "fourier_clean")
		return Filter::FourierClean(BuildAmplitude(blob["a"]));
	else if (token == "fourier_cleanpass")
		return Filter::FourierCleanPass(BuildAmplitude(blob["a"]));
	else if (token == "fourier_limiter")
		return Filter::FourierLimit(BuildAmplitude(blob["a"]));
	else if (token == "narrow_stereo")
		return Filter::NarrowStereo(blob.asFloat(0.0, 1.0));
	else if (token == "pitch_scale")
		return BuildPitchScale(blob);
	else if (token == "inverse_lr")
		return Filter::InverseLR();
	else throw EError(token + ": Unknown filter type.");
}

FilterVector Parser::BuildFilters(Blob& blob) const {
	blob.AssertFunction();
	FilterVector filters;
	for (auto& child : blob.children_)
		filters.push_back(BuildFilter(child));
	return filters;
}

Filter Parser::BuildLowPass(Blob& blob) const {
	const float_type r {blob["r"].asFloat(1.0, 1000000.0)};
	const bool wrap {blob.hasFlag("wrap")};
	return Filter::LowPass(r, wrap);
}

Filter Parser::BuildHighPass(Blob& blob) const {
	const float_type r {blob["r"].asFloat(1.0, 1000000.0)};
	const bool wrap {blob.hasFlag("wrap")};
	return Filter::HighPass(r, wrap);
}

Filter Parser::BuildBandPass(Blob& blob) const {
	constexpr float_type dBperlog10 {20.0};
	const float_type frequency {blob["f"].asFloat(0.001, 100000.0)},
		bandwidth {blob["width"].asFloat(0.0, 100000.0)},
		gain {BuildAmplitude(blob["gain"])};
	if (gain <= 0.0)
		throw EError("Bandpass filter requires non-zero, positive gain (on amplitude scale).");
	const bool wrap {blob.hasFlag("wrap")};
	return Filter::BandPass(frequency, bandwidth, log10(gain) * dBperlog10, wrap);
}

Filter Parser::BuildFourierGain(Blob& blob) const {
	const float_type low_shoulder {blob["low"].asFloat(1.0, 1000000.0)},
		low_gain {BuildAmplitude(blob["low_gain"])},
		high_shoulder {blob["high"].asFloat(1.0, 1000000.0)},
		high_gain {BuildAmplitude(blob["high_gain"])};
	if (high_shoulder<low_shoulder)
		throw EError("Shoulders of Fourier gain filter in the wrong order.");
	if ((low_gain <= 0.0) || (high_gain <=0.0))
		throw EError("Fourier gain filter filter requires non-zero, positive gain (on amplitude scale).");
	return Filter::FourierGain(low_gain, low_shoulder, high_shoulder, high_gain);
}

Filter Parser::BuildFourierBandpass(Blob& blob) const {
	const float_type frequency {blob["f"].asFloat(1.0, 100000.0)},
		bandwidth {blob["width"].asFloat(0.0, 100000.0)},
		gain {BuildAmplitude(blob["gain"])};
	const bool comb {blob.hasFlag("comb")};
	if (gain <= 0.0)
		throw EError("Bandpass filter requires non-zero, positive gain (on amplitude scale).");
	return Filter::FourierBandpass(frequency, bandwidth, gain).SetFlag(filter_direction::comb, comb);
}

void Parser::FilterSweep(Blob& blob) {
	if (blob["start"].children_.size() != 1)
		throw EError("Filter sweep needs single filter.");
	if (blob["end"].children_.size() != 1)
		throw EError("Filter sweep needs single filter.");
	const Filter start_filter {BuildFilter(blob["start"][0])},
		end_filter {BuildFilter(blob["end"][0])};
	const int window_count {blob["windows"].asInt(2, int_max)};
	dictionary_.FindSound(blob).WindowedFilter(start_filter, end_filter, window_count);
}

void Parser::Integrate(Blob& blob) {
	const float_type factor {blob["f"].asFloat(0.0, 100000.0)},
		leak_per_second {blob["leak"].asFloat(0.0, 100000.0)},
		constant {blob["c"].asFloat(-1.0, 1.0)};
	dictionary_.FindSound(blob).Integrate(factor, leak_per_second, constant);
}

void Parser::Clip(Blob& blob) {
	float_type min {-1.0}, max {1.0};
	if (blob.hasKey("a"))
		min = -(max = BuildAmplitude(blob["a"]));
	else {
		min = blob["min"].asFloat(-1.0, 1.0);
		max = blob["max"].asFloat(-1.0, 1.0);
	}
	dictionary_.FindSound(blob).Clip(min, max);
}

void Parser::Abs(Blob& blob) {
	float_type amp {1.0};
	blob.tryWritefloat_type("a", amp, -1.0, 1.0);
	dictionary_.FindSound(blob).Abs(amp);
}

void Parser::Fold(Blob& blob) {
	const float_type amp {blob["a"].asFloat()};
	dictionary_.FindSound(blob).Fold(amp);
}

void Parser::OctaveEffect(Blob& blob) {
	float_type mix_proportion {1.0};
	blob.tryWritefloat_type("p", mix_proportion, 0.0, 1.0);
	dictionary_.FindSound(blob).Octave(mix_proportion);
}

void Parser::FourierShift(Blob& blob) {
	const float_type frequency {blob["f"].asFloat(-100000.0, 100000.0)};
	dictionary_.FindSound(blob).FourierShift(frequency);
}

void Parser::FourierClean(Blob& blob) {
	const float_type min_gain {BuildAmplitude(blob["a"])};
	dictionary_.FindSound(blob).FourierClean(min_gain, false, false);
}

void Parser::FourierCleanPass(Blob& blob) {
	const float_type min_gain {BuildAmplitude(blob["a"])};
	dictionary_.FindSound(blob).FourierClean(min_gain, true, false);
}

void Parser::FourierLimit(Blob& blob) {
	const float_type min_gain {BuildAmplitude(blob["a"])};
	dictionary_.FindSound(blob).FourierClean(min_gain, false, true);
}

void Parser::FourierScale(Blob& blob) {
	const float_type factor {blob["f"].asFloat(0.001, 1000.0)};
	dictionary_.FindSound(blob).FourierScale(factor);
}

void Parser::FourierPower(Blob& blob) {
	const float_type power {blob["power"].asFloat(-10.0, 10.0)};
	dictionary_.FindSound(blob).FourierPower(power);
}

void Parser::Repeat(Blob& blob) {
	const int count {blob["n"].asInt(1, int_max)};
	FilterVector filters;
	if (blob.hasKey("filter"))
		filters = BuildFilters(blob["filter"]);
	dictionary_.FindSound(blob).Repeat(count, filters);
}

void Parser::Flags(Blob& blob, Sound &sound) {
	SampleType type {sound.getType()};
	type = BuildSampleType(blob, type);
	sound.setType(type);
}

void Parser::Flags(Blob& blob) {
	Sound& sound{dictionary_.FindSound(blob)};
	SampleType type {sound.getType()};
	type = BuildSampleType(blob["type"], type);
	sound.setType(type);
}

void Parser::ApplyEnvelope(Blob& blob) {
	const Envelope envelope {BuildEnvelope(blob["e"])};
	dictionary_.FindSound(blob).ApplyEnvelope(envelope);
}

void Parser::Distort(Blob& blob) {
	const float_type power {blob["power"].asFloat(0.001, 1000.0)};
	dictionary_.FindSound(blob).Distort(power);
}

void Parser::Chorus(Blob& blob) {
	constexpr float_type DefaultVibFreq {5.0};
	const int count {blob["n"].asInt(1, 1000)};
	const float_type offset {blob["offset"].asFloat(0.0, 10.0)};
	Wave wave {(blob.hasKey("vib")) ? BuildWave(blob["vib"]) : Wave(DefaultVibFreq, 0.01, 0.0)};
	Sound& sound {dictionary_.FindSound(blob)};
	if (blob.hasFlag("stereo")) {
		if (sound.channels() == 1)
			sound.Rechannel(2);
		Sound left_sound, right_sound;
		sound.Split(left_sound, right_sound);
		left_sound.Chorus(count, offset, wave);
		right_sound.Chorus(count, offset, wave);
		sound.Combine(left_sound, right_sound);
	} else
		sound.Chorus(count, offset, wave);
}

void Parser::Offset(Blob& blob) {
	const float_type left_time {blob["l"].asFloat(-HourLength, HourLength)},
		right_time {blob["r"].asFloat(-HourLength, HourLength)};
	const bool wrap {blob.hasFlag("wrap")};
	Sound& sound {dictionary_.FindSound(blob)};
	sound.OffsetSeconds(left_time, right_time, wrap);
}

void Parser::Flange(Blob& blob) {
	const float_type freq {blob["f"].asFloat(0.0, 100000.0)},
		amp {blob["a"].asFloat(0.0, 1.0)};
	dictionary_.FindSound(blob).Flange(freq, amp);
}

void Parser::BitCrusher(Blob& blob) {
	const int bits {blob["bits"].asInt(1, 16)};
	dictionary_.FindSound(blob).BitCrusher(bits);
}

void Parser::Bias(Blob& blob) {
	const float_type level {blob["level"].asFloat(-1.0, 1.0)};
	dictionary_.FindSound(blob).Waveform(Wave(0.0, level, 0.0), Phaser(), Wave(),
		0.0, synth_type::constant, Stereo{0.0});
}

void Parser::Debias(Blob& blob) {
	static const std::map<std::string, Sound::debias_type> debias_types {
		{"start", Sound::debias_type::start}, {"end", Sound::debias_type::end}, {"mean", Sound::debias_type::mean}
	};
	if (const std::string input {blob["type"].atom()}; debias_types.contains(input))
		dictionary_.FindSound(blob).Debias(debias_types.at(input));
	else
		throw EError(blob.Dump() + ": Unknown debias operation.");
}

void Parser::KarplusStrong(Blob& blob) {
	const auto name {blob["@"].atom()};
	Sound& sound {dictionary_.InsertSound(name)};
	const int channels {blob["grain"].ifFunction()["channels"].asInt(1, MaxChannels)};
	SampleType grain_type {BuildSampleType(blob["grain"]["type"])};
	const float_type grain_frequency {1.0_flt / blob["grain"]["f"].asFloat(0.01,
			0.5_flt * float_type(grain_type.sample_rate))},
		len {blob["len"].asFloat(grain_frequency, HourLength)};
	sound.CreateSilenceSeconds(channels, grain_type.sample_rate,
		grain_frequency, grain_frequency);
	sound.setType(SampleType(false, false, grain_type.sample_rate, 0.0));
	if (blob.hasKey("synth"))
		Synth(blob["synth"].ifFunction(), sound);
	if (blob.hasKey("outer"))
		ParseBlobs(blob["outer"].ifFunction());
	FilterVector repeat_filters, mix_filters;
	if (blob.hasKey("filter"))
		repeat_filters = BuildFilters(blob["filter"]);
	if (blob.hasKey("mix_filter"))
		mix_filters = BuildFilters(blob["mix_filter"]);
	sound.Repeat(int(len / grain_frequency), repeat_filters);
	sound.ApplyFilters(mix_filters);
	sound.Debias(Sound::debias_type::end);
	sound.AutoResize(0.0);
	sound.AutoAmp();
	sound.setType(grain_type);
	TryMessage("Created patch [" + name + "]", blob);
}

void Parser::Chowning(Blob& blob) {
	const auto name {blob["@"].atom()};
	Sound& sound {dictionary_.InsertSound(name)};
	const int channels {blob["channels"].asInt(1, MaxChannels)};
	SampleType type {BuildSampleType(blob["type"])};
	const float_type length {blob["len"].asFloat(0, HourLength)};
	sound.CreateSilenceSeconds(channels, type.sample_rate, length, length);
	sound.setType(type);
	if (blob.hasKey("synth"))
		Synth(blob["synth"], sound);
	if (blob.hasKey("outer"))
		ParseBlobs(blob["outer"].ifFunction());
	Blob& modulator_blob {blob["modulators"]};
	for (auto& item : modulator_blob.children_)
		Modulator(item, sound);
	FilterVector mix_filters;
	if (blob.hasKey("filter"))
		mix_filters = BuildFilters(blob["filter"]);
	sound.ApplyFilters(mix_filters);
	sound.Debias(Sound::debias_type::mean);
	if (blob.hasKey("env"))
		sound.ApplyEnvelope(BuildEnvelope(blob["env"]));
	sound.AutoAmp();
	TryMessage("Created patch [" + name + "]", blob);
}

void Parser::Modulator(Blob &blob, OptRef<Sound> sound_ref) {
	blob.AssertFunction();
	Sound& sound {sound_ref.has_value()? sound_ref->get() : dictionary_.FindSound(blob)};
	const SampleType type {sound.getType()};
	OverlayFlags flags {{{overlay::loop, type.loop}}};
	enum class modulation {
		frequency, amplitude, distortion
	} mode {modulation::frequency};
	float_type amp {1.0}, bias {0.0};
	if (blob.hasKey("a"))
		amp = BuildAmplitude(blob["a"]);
	blob.tryWritefloat_type("bias", bias, -1000.0, 1000.0);
	Sound modulator_sound;
	modulator_sound.CreateSilenceSamples(SingleChannel,
		sound.sample_rate(), sound.p_samples(),
		sound.p_samples());
	Synth(blob["synth"].ifFunction(), modulator_sound);
	if (blob.hasKey("env"))
		modulator_sound.ApplyEnvelope(BuildEnvelope(blob["env"]));
	if (blob.hasKey("mode")) {
		if (blob["mode"].atom() == "am")
			mode = modulation::amplitude;
		else if (blob["mode"].atom() == "dm")
			mode = modulation::distortion;
	}
	if (mode == modulation::amplitude)
		sound.Ring(modulator_sound, Wave(), false, amp, bias);
	else if (mode == modulation::distortion)
		sound.Ring(modulator_sound, Wave(), true, amp, bias);
	else if (mode == modulation::frequency) {
		Sound temp_sound {sound};
		temp_sound.MakeSilent();
		Scratcher scratcher(&modulator_sound, "", amp, bias, true);
		temp_sound.DoOverlay(sound).stop(sound.p_samples()).flags(flags).scratcher(scratcher)();
		sound = temp_sound;
	}
	sound.setType(type);
}

// Synth mode functions

void Parser::Synth(Blob& blob, OptRef<Sound> sound_ref) {
	Wave wave {440.0, 1.0, 0.0}, tone;
	Phaser phaser;
	Stereo stereo;
	bool pitch_adjust {false};
	float_type power {1.0}, fundamental {standard_pitch_}, freq_offset {0.0};
	for (auto& item : blob.children_) {
		if (const auto token {item.key_}; token == "@") {
			if (sound_ref.has_value())
				throw EError("Can't change sample supplied to synth mode." + item.ErrorString());
			const auto name {item.atom()};
			sound_ref = dictionary_.FindSound(name);
		} else {
			if (!sound_ref.has_value())
				throw EError("Synth mode first needs a sound to work with." + item.ErrorString());
			if (token == "wave") {
				wave = BuildWave(item);
				wave.offset_freq(freq_offset);
				if (pitch_adjust)
					wave.scale_freq(instrument_frequency_multiplier_);
			} else if (token == "amp")
				wave = Wave(wave.freq(), BuildAmplitude(item), wave.offset());
			else if (token == "bias")
				wave = Wave(0.0, item.asFloat(), 0.0);
			else if (token == "vib")
				phaser = BuildPhaser(item, 4);
			else if (token == "bend")
				phaser.set_bend_factor(item.asFloat(0.0001, 10000.0));
			else if (token == "freq_offset")
				freq_offset = item.asFloat(-1000.0,1000.0);
			else if (token == "tone")
				tone = BuildWave(item);
			else if (token == "stereo")
				stereo = BuildStereo(item);
			else if (token == "power")
				power = item.asFloat(0.001, 1000);
			else if (token == "pitch_adjust")
				pitch_adjust = item.asBool();
			else if (token == "fundamental") {
				fundamental = item.asFloat(1.0, 30000.0);
				if (pitch_adjust)
					fundamental *= instrument_frequency_multiplier_;
			}  else {
				Sound& sound {sound_ref->get()};
				if (token == "sine")
					sound.Waveform(wave, phaser, tone, 0.0, synth_type::sine, stereo);
				else if (token == "c")
					sound.Waveform(wave, phaser, tone, 0.0, synth_type::constant, stereo);
				else if (token == "distort")
					sound.Waveform(wave, phaser, tone, power, synth_type::power, stereo);
				else if (token == "square")
					sound.Waveform(wave, phaser, tone, 0.0, synth_type::square, stereo);
				else if (token == "saw")
					sound.Waveform(wave, phaser, tone, 0.0, synth_type::saw, stereo);
				else if (token == "triangle")
					sound.Waveform(wave, phaser, tone, 0.0, synth_type::triangle, stereo);
				else if (token == "distort_triangle")
					sound.Waveform(wave, phaser, tone, power, synth_type::powertriangle, stereo);
				else if (token == "pulse")
					sound.Waveform(wave, phaser, tone, power, synth_type::pulse, stereo);
				else if (token == "white")
					sound.WhiteNoise(wave.amp(), stereo);
				else if (token == "red")
					sound.RedNoise(wave.amp(), stereo);
				else if (token == "velvet")
					sound.VelvetNoise(wave.freq(), wave.amp(), stereo);
				else if (token == "crackle")
					sound.CrackleNoise(wave.freq(), stereo);
				else if (token == "smatter")
					Smatter(sound, item.ifFunction());
				else if (token == "sines")
					Sines(sound, item.ifFunction(), fundamental);
				else if (token == "filter")
					sound.ApplyFilters(BuildFilters(item.ifFunction()));
				else
					throw EError(token + ": Unknown synth operation.");
			}
		}
	}
}

void Parser::Smatter(Sound& sound, Blob& blob) {
	Blob& pitch_blob {blob["pitch"].ifFunction()},
		&amp_blob {blob["amp"].ifFunction()},
		&stereo_blob {blob["stereo"].ifFunction()};
	const float_type freq {blob["f"].asFloat(0.0, float_type_max)};
	Sound& source {dictionary_.FindSound(blob["with"].atom())};
	const float_type high_pitch {pitch_blob["high"].asFloat(0.0, float_type_max)},
		low_pitch {pitch_blob["low"].asFloat(0.0, float_type_max)},
		amp_high {BuildAmplitude(amp_blob["high"])},
		amp_low {BuildAmplitude(amp_blob["low"])},
		stereo_left {stereo_blob["left"].asFloat(-1.0, 1.0)},
		stereo_right {stereo_blob["right"].asFloat(-1.0, 1.0)};
	const bool logarithmic_pitch {pitch_blob.hasFlag("log")},
		logarithmic_amp {amp_blob.hasFlag("log")},
		resize {blob.hasFlag("resize")},
		regular {blob.hasFlag("regular")};
	sound.Smatter(source, freq, low_pitch, high_pitch,
		logarithmic_pitch, amp_low, amp_high, logarithmic_amp, stereo_left,
		stereo_right, resize, regular);
}

void Parser::Sines(Sound& sound, Blob& blob, float_type fundamental) {
	for (int factor {1}; auto& item : blob.children_) {
		const float_type amp {BuildAmplitude(item)};
		const Wave wave(fundamental * factor, amp);
		sound.Waveform(wave, Phaser(), Wave(), 0, synth_type::sine, Stereo());
		factor++;
	}
}

// ParseLaunch functions

std::string ParseLaunch::BackSlashEscape(std::string input) {
	std::string output {""};
	for (char& c : input)
		output += (c == '\\')? "\\\\" : std::string(1,c);
	return output;
}

int ParseLaunch::Start() {
	int return_code {EXIT_SUCCESS};
	const size_t argument_count {args_.size()};
	try {
		bool do_bootstrap{true}, show_environment{false}, portable{false}, show_version{false}, show_help{false};
		std::string boot_instructions, args_instructions;
		if (argument_count < 2)
			boot_instructions += "print(\"BoxyLady: warning -- no arguments.\n" + BootHelp + "\")";
		else
			for (size_t index = 1; index < argument_count; index++) {
				auto next_arg {[this, argument_count, &index] () {
					if (++index == argument_count) throw EError("BoxyLady: bad arguments\n" + BootHelp);
					else return args_[index];
				}};
				auto test_arg = [](std::string arg, std::string test1, std::string test2) {
					return ((arg == test1) || (arg == test2));
				};
				if (std::string argument {args_[index]}; test_arg(argument, "--help", "-h"))
					show_help = true;
				else if (test_arg(argument, "--version", "-v"))
					show_version = true;
				else if (test_arg(argument, "--noboot", "-n"))
					do_bootstrap = false;
				else if (test_arg(argument, "--portable", "-p"))
					portable = true;
				else if (test_arg(argument, "--envshow", "-e"))
					show_environment = true;
				else if (test_arg(argument, "--messages", "-m"))
					boot_instructions += "--messages(" + next_arg() + ")\n";
				else if (test_arg(argument, "--outer", "-o"))
					args_instructions += next_arg() + "\n";
				else if (test_arg(argument, "--quick", "-q"))
					args_instructions += "quick(" + next_arg() + ")\n";
				else if (test_arg(argument, "--interactive", "-i"))
					args_instructions += "--interactive()\n";
				else if (argument[0]=='-')
					throw EError("BoxyLady: bad switch " + argument + "\n" + BootHelp);
				else args_instructions += "source(\"" + argument + "\")\n";
			}
		if (do_bootstrap) {
			std::string pre_boot = BootWelcome;
			if (show_version) pre_boot += "--version()\n";
			if (show_help) pre_boot += "--help()\n";
			if (portable) pre_boot += "--portable(T)\n";
			boot_instructions = pre_boot + boot_instructions + "library(\"" + std::string{platform.BootLibrary()} + "\")\n";
		}
		if (show_environment) {
			screen.PrintHeader("Environment");
			screen.PrintMessage("boot:\n" + boot_instructions);
			screen.PrintMessage("args:\n" + args_instructions);
			screen.PrintSeparatorBot();
		}
		Parser parser{};
		parser.Supervisor(true);
		parser.ParseString(boot_instructions);
		parser.Supervisor(false);
		parser.ParseString(args_instructions);
	} catch (EError& error) {
		if (!error.is_terminate()) return_code=1;
		screen.PrintError(error);
	} catch (std::exception& error) {
		screen.PrintError(error, "Internal error: ");
		return_code = EXIT_FAILURE;
	} catch (...) {
		screen.PrintError(EError{"Unspecified internal error: "});
		return_code = EXIT_FAILURE;
	};
	return return_code;
}

} // end of namespace BoxyLady

int main(int argc, char **argv) {
	return BoxyLady::ParseLaunch{argc, argv}.Start();
}
