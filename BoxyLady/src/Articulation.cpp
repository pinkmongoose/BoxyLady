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

#include "Articulation.h"
#include "Builders.h"
#include "Fraction.h"

namespace BoxyLady {

NoteDuration::NoteDuration(Blob& blob) {
	blob.AssertFunction();
	switch (blob.children_.size()) {
		case 1:
			*this = NoteDuration(blob[0].asFloat(0, int_max) * Quarter);
			return;
		case 2:
			*this = NoteDuration(blob[0].asFloat(0, int_max) / blob[1].asFloat(1, int_max));
			return;
		default:
			throw EError("Incorrect number of arguments for note length.\n" + blob.ErrorString());
	}	
}

NoteDuration::NoteDuration(std::string input) {
	int denominator {0}, dots {0}, colons {0};
	bool number {true};
	for (auto c : input) {
		if (number) {
			if (const int digit {c - '0'}; in_range(digit, 0, 9)) {
				denominator = denominator * 10 + digit;
				continue;
			} else
				number = false;
		}
		if (c == '.')
			dots++;
		else if (c == ':')
			colons++;
		else
			throw EError("Note duration [" + input + "] not recognised.");
	}
	const float_type reciprocal {1.0_flt / static_cast<float_type>(denominator)},
			multiplier {2.0_flt - pow(2.0_flt, -static_cast<float_type>(dots))};
	d_ = reciprocal * multiplier * pow(2.0_flt, colons);
}

//-----------------

void ArticulationGamut::StandardArticulations() {
	static const std::vector<std::pair<std::string, std::string>> standard_articulations {
		{".", "staccato=0.5"}, {"p", "staccato=0.75"}, {"'", "staccato=0.25"},
		{"l", "staccato=1"}, {"-", "amp=1.3"}, {"^", "amp=1.6"},
		{"v", "amp=0.5"}, {",", "amp=0.8 staccato=0.8"}, {"*", "start_slur()"},
		{"!", "stop_slur()"}, {"~", "glide(T)"}, {"_", "rev(T)"},
		{"|", "bar(T)"}
	};
	for (auto articulation : standard_articulations)
		articulations_[articulation.first] = NoteArticulation{articulation.second};
}

std::string ArticulationGamut::List1(NoteArticulation articulation, bool all) { // Those only printed with all=T don't relate to parser state
	std::ostringstream line; 
	ArticulationFlags& flags {articulation.flags_};
	if (flags[articulation_type::amp])
		std::print(line, "amp = {} ", articulation.amp_);
	if (flags[articulation_type::staccato] || all)
		std::print(line, "staccato = {} ", articulation.staccato_);
	if (flags[articulation_type::start_slur])
		std::print(line, "start_slur() ");
	if (flags[articulation_type::stop_slur])
		std::print(line, "stop_slur() ");
	if (flags[articulation_type::glide] || all)
		std::print(line, "glide = {} ", BoolToString(articulation.glide_));
	if (flags[articulation_type::env] || all)
		std::print(line, "envelope = {} ", articulation.envelope_.toString());
	if (flags[articulation_type::envelope_compress] || all)
		std::print(line, "env_adjust = {} ", BoolToString(articulation.envelope_compress_));
	if (flags[articulation_type::stereo] || all)
		std::print(line, "stereo = ({} {}) ", articulation.stereo_[0], articulation.stereo_[1]);
	if (flags[articulation_type::phaser] || all)
		std::print(line, "vib = ({} {} {} {}) ",
			articulation.phaser_.amp(), articulation.phaser_.offset(),
			articulation.phaser_.bend_factor(), articulation.phaser_.bend_time());
	if (flags[articulation_type::scratcher] || all)
		std::print(line, "scratch = {} ", articulation.scratcher_.toString());
	if (flags[articulation_type::tremolo] || all)
		std::print(line, "tremolo = ({} {}) ", articulation.tremolo_.freq(), articulation.tremolo_.amp());
	if (flags[articulation_type::portamento] || all)
		std::print(line, "port = {} ", articulation.portamento_time_);
	if (flags[articulation_type::reverb] || all)
		std::print(line, "rev = {} ", BoolToString(articulation.reverb_));
	if (flags[articulation_type::d_play] || all)
		std::print(line, "D_rev = {} ", articulation.duration_.getDuration()*4.0);
	if (flags[articulation_type::bar])
		std::print(line, "bar = {} ", BoolToString(articulation.bar_));
	return line.str();
}

void ArticulationGamut::List() const {
	screen.PrintHeader("Articulations");
	for (auto index : articulations_)
		screen.PrintWrap(index.first + ": " + List1(index.second));
	screen.PrintSeparatorBot();
}

NoteArticulation ArticulationGamut::Note(std::string input) const {
	NoteArticulation articulation;
	std::string buffer {input};
	if (size_t index {buffer.find('-')}; index == std::string::npos)
		return articulation;
	else {
		buffer.erase(0, index + 1);
		return FromString(buffer);
	}
}

NoteArticulation ArticulationGamut::FromString(std::string buffer) const {
	NoteArticulation articulation;
	for (size_t index = 0; index < buffer.length(); index++) {
	if (const auto key {buffer.substr(index, 1)}; articulations_.count(key) == 1)
		articulation.Overwrite(articulations_.at(key));
	else
		throw EError("Articulation [" + key + "] not recognised.");
	}
	return articulation;
}

ArticulationGamut& ArticulationGamut::ParseBlob(Blob& blob, bool makemusic) {
	for (auto& command : blob.children_) {
		command.AssertFunction();
		if (auto key {command.key_}; key == "new")
			articulations_ = {};
		else if (key == "standard_articulations")
			StandardArticulations();
		else if (key == "list") {
			if (makemusic)
				List();
		} else {
			if (key.length() > 1)
				throw EError("Articulations must only be one character long.");
			NoteArticulation& articulation {articulations_[key]};
			articulation.Parse(command);
		}
	}
	return *this;
}

//-----------------

void NoteArticulation::Parse(Blob& blob) {
	*this = NoteArticulation();
	for (auto& command : blob.children_) {
		if (const auto key {command.key_}; key == "amp") {
			amp_ = BuildAmplitude(command);
			flags_[articulation_type::amp] = true;
		} else if (key == "staccato") {
			staccato_ = command.asFloat(0, MaxStaccato);
			flags_[articulation_type::staccato] = true;
		} else if (key == "start_slur") {
			start_slur_ = true;
			flags_[articulation_type::start_slur] = true;
		} else if (key == "stop_slur") {
			stop_slur_ = true;
			flags_[articulation_type::stop_slur] = true;
		} else if (key == "glide") {
			glide_ = command.asBool();
			flags_[articulation_type::glide] = true;
		} else if ((key == "env") || (key == "envelope")) {
			envelope_ = BuildEnvelope(command);
			flags_[articulation_type::env] = true;
		} else if (key == "stereo") {
			stereo_ = BuildStereo(command);
			flags_[articulation_type::stereo] = true;
		} else if (key == "vib") {
			phaser_ = BuildPhaser(command);
			flags_[articulation_type::phaser] = true;
		} else if (key == "scratch") {
			scratcher_ = Scratcher(nullptr, command.ifFunction()["with"].atom(),
				command["f"].asFloat(), command["bias"].asFloat(),
				command["loop"].asBool());
			flags_[articulation_type::scratcher] = true;
		} else if (key == "tremolo") {
			tremolo_ = BuildWave(command);
			flags_[articulation_type::tremolo] = true;
		} else if (key == "portamento") {
			portamento_time_ = command.asFloat(0, MinuteLength);
			flags_[articulation_type::portamento] = true;
		} else if (key == "env_adjust") {
			envelope_compress_ = command.asBool();
			flags_[articulation_type::envelope_compress] = true;
		} else if (key == "rev") {
			reverb_ = command.asBool();
			flags_[articulation_type::reverb] = true;
		} else if (key == "bar") {
			bar_ = command.asBool();
			flags_[articulation_type::bar] = true;
		} else if (key == "D_rev") {
			duration_ = NoteDuration{command};
			flags_[articulation_type::d_play] = true;
		}
	}
}

void NoteArticulation::Overwrite(NoteArticulation source) {
	auto Overwrite1 {[this]<typename T>(T& this_var, T& source_var, NoteArticulation& source, articulation_type type) {
		if (source.flags_[type]) {
			this_var = source_var;
			flags_[type] = true;
		}
	}};
	Overwrite1(amp_, source.amp_, source, articulation_type::amp);
	Overwrite1(staccato_, source.staccato_, source, articulation_type::staccato);
	Overwrite1(start_slur_, source.start_slur_, source, articulation_type::start_slur);
	Overwrite1(stop_slur_, source.stop_slur_, source, articulation_type::stop_slur);
	Overwrite1(glide_, source.glide_, source, articulation_type::glide);
	Overwrite1(envelope_, source.envelope_, source, articulation_type::env);
	Overwrite1(stereo_, source.stereo_, source, articulation_type::stereo);
	Overwrite1(phaser_, source.phaser_, source, articulation_type::phaser);
	Overwrite1(scratcher_, source.scratcher_, source, articulation_type::scratcher);
	Overwrite1(tremolo_, source.tremolo_, source, articulation_type::tremolo);
	Overwrite1(portamento_time_, source.portamento_time_, source, articulation_type::portamento);
	Overwrite1(envelope_compress_, source.envelope_compress_, source, articulation_type::envelope_compress);
	Overwrite1(reverb_, source.reverb_, source, articulation_type::reverb);
	Overwrite1(duration_, source.duration_, source, articulation_type::d_play);
	Overwrite1(bar_, source.bar_, source, articulation_type::bar);
}

//-----------------

void BeatGamut::List(float_type beat_time) const {
	auto Print = [](std::string item) {screen.PrintWrap(item, Screen::PrintFlags({Screen::print_flag::frame, Screen::print_flag::wrap, Screen::print_flag::indent}));};
	screen.PrintHeader("Beats");
	for (auto index : beats_) {
		const Beat beat {index.second};
		std::string item {std::format("{}: articulations(", index.first)};
		item += beat.articulations + ") D = " + Fraction(beat.duration.getDuration(), 1.001).FractionString()
			+ " width = " + Fraction(beat.width.getDuration(), 1.001).FractionString()
			+ " offset = " + Fraction(beat.offset.getDuration(), 1.001).FractionString();
		Print(item);
	}
	Print("beat(" + Fraction(beat_time, 0.001).FractionString() + ")");
	screen.PrintSeparatorBot();
}

BeatGamut& BeatGamut::ParseBlob(Blob& blob, float_type& beat_time, bool makemusic) {
	for (auto& command : blob.children_) {
		command.AssertFunction();
		if (auto key {command.key_}; key == "new") {
			beats_ = {};
			beat_time = 0.0;
		} else if (key == "list") {
			if (makemusic)
				List(beat_time);
		} else if (key == "beat")
			beat_time = NoteDuration{command}.getDuration();
		else {
			Beat& beat {beats_[key]};
			if (beat.articulations = command["articulations"].atom(); beat.articulations == "off")
				beat.articulations = "";
			beat.duration = NoteDuration{command["D"]};
			beat.offset = (command.hasKey("offset")) ? NoteDuration{command["offset"]} : NoteDuration{0.0};
			beat.width = (command.hasKey("width")) ? NoteDuration{command["width"]} : NoteDuration{0.01};
		}
	}
	return *this;
}

std::string BeatGamut::BeatArticulations(float_type time) const {
	std::string beat_string {};
	for (const auto& item : beats_) {
		const Beat& beat {item.second};
		const float_type duration {beat.duration.getDuration()},
			offset {beat.offset.getDuration() / duration},
			width {beat.width.getDuration() / duration},
			fraction {time / duration},
			remainder {fraction - static_cast<float_type>(floor(fraction)) - offset},
			abs_remainder {(remainder < 0.0) ? -remainder : remainder};
		if (abs_remainder < width)
			beat_string += beat.articulations;
	}
	return beat_string;
}

//-----------------

AutoStereo& AutoStereo::ParseBlob(Blob& blob, [[maybe_unused]] bool make_music) {
	for (auto& command : blob.children_) {
		if (const auto key {command.key_}; key == "centre")
			centre_ = pow(2.0, command.asFloat(-10.0, 10.0));
		else if (key == "multiplier")
			multiplier_ = command.asFloat(0.0, 10.0);
		else if (key == "mode") {
			if (const auto mode {command.atom()}; mode == "ascending") mode_ = auto_stereo::ascending;
			else if (mode == "descending") mode_ = auto_stereo::descending;
			else if (mode == "organ") mode_ = auto_stereo::organ;
		} else if (key == "octave_bands")
			octave_bands_ = command.asFloat(1.0, 100.0);
		else if (command.isToken()) {
			auto token = command.val_;
			if (token == "off")
				mode_ = auto_stereo::none;
		}
	}
	return *this;
}

Stereo AutoStereo::Apply(float_type freq_mult) {
	if (mode_ == auto_stereo::none) return Stereo(1.0);
	freq_mult /= centre_;
	float_type octave {log2(freq_mult)}, position {octave * multiplier_}, band;
	switch(mode_) {
		case auto_stereo::descending:
		position = -position;
		break;
		case auto_stereo::organ:
		band = octave * octave_bands_ + 0.5;
		if ((std::lround(band) & 1) == 1) position = -position;
		default:;
	}
	return Stereo(1.0 - position, 1.0 + position);
}

std::string AutoStereo::toString() const {
	if (mode_ == auto_stereo::none) return "off";
	std::ostringstream buffer;
	std::print(buffer, "centre = {:.2f} multiplier = {:.2f} ", centre_, multiplier_);
	switch (mode_) {
		case auto_stereo::ascending: std::print(buffer, "ascending"); break;
		case auto_stereo::descending: std::print(buffer, "ascending"); break;
		case auto_stereo::organ: std::print(buffer, "organ octave_bands = {:.2f}", octave_bands_); break;
		default:;
	}
	return buffer.str();
}

} //end namespace BoxyLady
