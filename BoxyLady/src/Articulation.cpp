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

#include "Articulation.h"
#include "Builders.h"
#include "Fraction.h"

namespace BoxyLady {

NoteDuration BuildNoteDuration(Blob &blob) {
	blob.AssertFunction();
	const int nn = blob.children_.size();
	if (nn == 1)
		return NoteDuration(blob[0].asDouble(0, INT_MAX) * Quarter);
	else
		return NoteDuration(
				blob[0].asDouble(0, INT_MAX) / blob[1].asDouble(1, INT_MAX));
}

//-----------------

NoteDuration::NoteDuration(std::string input) {
	int denominator = 0, dots = 0;
	bool number = true;
	for (char c : input) {
		if (number) {
			const char digit = c - '0';
			if ((digit < 10) && (digit >= 0)) {
				denominator = denominator * 10 + digit;
				continue;
			} else
				number = false;
		}
		if (c == '.')
			dots++;
		else
			throw EParser().msg(
					"Note duration [" + input + "] not recognised.");
	}
	const double reciprocal = 1.0 / static_cast<double>(denominator),
			multiplier = 2.0 - pow(2.0, -static_cast<double>(dots));
	d_ = reciprocal * multiplier;
}

//-----------------

void ArticulationGamut::StandardArticulations() {
	auto NewArticulation = [this](std::string key, std::string value) {
		Articulation(key) = NoteArticulation(value);
	};
	NewArticulation(".", "staccato=0.5");
	NewArticulation("p", "staccato=0.75");
	NewArticulation("'", "staccato=0.25");
	NewArticulation("l", "staccato=1");
	NewArticulation("-", "amp=1.3");
	NewArticulation("^", "amp=1.6");
	NewArticulation("v", "amp=0.5");
	NewArticulation(",", "amp=0.8 staccato=0.8");
	NewArticulation("*", "start_slur()");
	NewArticulation("!", "stop_slur()");
	NewArticulation("~", "glide(T)");
	NewArticulation("_", "rev(T)");
	NewArticulation("|", "bar(T)");
}

void ArticulationGamut::List1(NoteArticulation articulation, bool all) { // Those only printed with all=T don't relate to parser state
	ArticulationFlags &flags = articulation.flags_;
	if (flags[articulation_type::amp])
		std::cout << "amp = " << articulation.amp_ << " ";
	if (flags[articulation_type::staccato] || all)
		std::cout << "staccato = " << articulation.staccato_ << " ";
	if (flags[articulation_type::start_slur])
		std::cout << "start_slur() ";
	if (flags[articulation_type::stop_slur])
		std::cout << "stop_slur() ";
	if (flags[articulation_type::glide] || all)
		std::cout << "glide = " << BoolToString(articulation.glide_) << " ";
	if (flags[articulation_type::env] || all)
		std::cout << "envelope = " << articulation.envelope_.toString() << " ";
	if (flags[articulation_type::envelope_compress] || all)
		std::cout << "env_adjust = "
				<< BoolToString(articulation.envelope_compress_) << " ";
	if (flags[articulation_type::stereo] || all)
		std::cout << "stereo = (" << articulation.stereo_[0] << " "
				<< articulation.stereo_[1] << ") ";
	if (flags[articulation_type::phaser] || all)
		std::cout << "vib = (" << articulation.phaser_.freq() << " "
				<< articulation.phaser_.amp() << " "
				<< articulation.phaser_.offset() << " "
				<< articulation.phaser_.bend_factor() << " "
				<< articulation.phaser_.bend_time() << ") ";
	if (flags[articulation_type::scratcher] || all)
		std::cout << "scratch = " << articulation.scratcher_.toString() << " ";
	if (flags[articulation_type::tremolo] || all)
		std::cout << "tremolo = (" << articulation.tremolo_.freq() << " "
				<< articulation.tremolo_.amp() << ") ";
	if (flags[articulation_type::portamento] || all)
		std::cout << "port = " << articulation.portamento_time_ << " ";
	if (flags[articulation_type::reverb] || all)
		std::cout << "rev = " << BoolToString(articulation.reverb_) << " ";
	if (flags[articulation_type::d_play] || all)
		std::cout << "D_rev = " << (articulation.duration_.getDuration() * 4.0)
				<< " ";
	if (flags[articulation_type::bar])
		std::cout << "bar = " << BoolToString(articulation.bar_) << " ";
}

void ArticulationGamut::List() const {
	std::cout << "\n" << screen::Header << screen::Tab << "Articulations\n"
			<< screen::Header;
	for (auto index : articulations_) {
		std::cout << index.first << ": ";
		List1(index.second);
		std::cout << "\n";
	}
	std::cout << screen::Header;
}

NoteArticulation& ArticulationGamut::Articulation(std::string name) {
	if (articulations_.count(name))
		return articulations_[name];
	else {
		articulations_.insert(
				std::pair<std::string, NoteArticulation>(name,
						NoteArticulation()));
		return articulations_[name];
	}
}

NoteArticulation ArticulationGamut::Note(std::string input) {
	NoteArticulation articulation;
	std::string buffer = input;
	size_t index = buffer.find('-');
	if (index == std::string::npos)
		return articulation;
	buffer.erase(0, index + 1);
	for (index = 0; index < buffer.length(); index++) {
		const std::string key = buffer.substr(index, 1);
		if (articulations_.count(key) == 1)
			articulation.Overwrite(articulations_[key]);
		else
			throw EArtGamut().msg("Articulation [" + key + "] not recognised.");
	}
	return articulation;
}

ArticulationGamut& ArticulationGamut::ParseBlob(Blob &blob, bool makemusic) {
	for (auto &command : blob.children_) {
		command.AssertFunction();
		std::string key = command.key_;
		if (key == "new")
			Clear();
		else if (key == "standard_articulations")
			StandardArticulations();
		else if (key == "list") {
			if (makemusic)
				List();
		} else {
			if (key.length() > 1)
				throw EArtGamut().msg(
						"Articulations must only be one character long.");
			NoteArticulation &articulation = Articulation(key);
			articulation.Parse(command);
		}
	}
	return *this;
}

//-----------------

void NoteArticulation::Parse(Blob &blob) {
	Clear();
	for (auto &command : blob.children_) {
		const std::string key = command.key_;
		if (key == "amp") {
			amp_ = BuildAmplitude(command);
			flags_[articulation_type::amp] = true;
		} else if (key == "staccato") {
			staccato_ = command.asDouble(0, MaxStaccato);
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
					command["f"].asDouble(), command["bias"].asDouble(),
					command["loop"].asBool());
			flags_[articulation_type::scratcher] = true;
		} else if (key == "tremolo") {
			tremolo_ = BuildWave(command);
			flags_[articulation_type::tremolo] = true;
		} else if (key == "portamento") {
			portamento_time_ = command.asDouble(0, LongTime);
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
		} else if (key == "D_play") {
			duration_ = BuildNoteDuration(command);
			flags_[articulation_type::d_play] = true;
		}
	}
}

template<typename T>
void NoteArticulation::Overwrite1(T &this_var, T &source_var,
		NoteArticulation &source, articulation_type type) {
	if (source.flags_[type]) {
		this_var = source_var;
		flags_[type] = true;
	}
}

void NoteArticulation::Overwrite(NoteArticulation source) {
	Overwrite1(amp_, source.amp_, source, articulation_type::amp);
	Overwrite1(staccato_, source.staccato_, source,
			articulation_type::staccato);
	Overwrite1(start_slur_, source.start_slur_, source,
			articulation_type::start_slur);
	Overwrite1(stop_slur_, source.stop_slur_, source,
			articulation_type::stop_slur);
	Overwrite1(glide_, source.glide_, source, articulation_type::glide);
	Overwrite1(envelope_, source.envelope_, source, articulation_type::env);
	Overwrite1(stereo_, source.stereo_, source, articulation_type::stereo);
	Overwrite1(phaser_, source.phaser_, source, articulation_type::phaser);
	Overwrite1(scratcher_, source.scratcher_, source,
			articulation_type::scratcher);
	Overwrite1(tremolo_, source.tremolo_, source, articulation_type::tremolo);
	Overwrite1(portamento_time_, source.portamento_time_, source,
			articulation_type::portamento);
	Overwrite1(envelope_compress_, source.envelope_compress_, source,
			articulation_type::envelope_compress);
	Overwrite1(reverb_, source.reverb_, source, articulation_type::reverb);
	Overwrite1(duration_, source.duration_, source, articulation_type::d_play);
	Overwrite1(bar_, source.bar_, source, articulation_type::bar);
}

void NoteArticulation::Clear() {
	amp_ = staccato_ = 1.0;
	start_slur_ = stop_slur_ = glide_ = envelope_compress_ = reverb_ = bar_ =
			false;
	flags_.Clear();
	envelope_ = Envelope();
	stereo_ = Stereo();
	phaser_ = Phaser();
	scratcher_ = Scratcher();
	tremolo_ = Wave();
	portamento_time_ = 0;
	duration_ = NoteDuration(0.0);
}

//-----------------

Beat& BeatGamut::getBeat(std::string key) {
	if (beats_.count(key))
		return beats_[key];
	else {
		beats_.insert(std::pair<std::string, Beat>(key, Beat()));
		return beats_[key];
	}
}

void BeatGamut::List1(Beat beat) const {
	std::cout << "articulations(" << beat.articulations << ") ";
	std::cout << "D = "
			<< Fraction(beat.duration.getDuration(), 0.001).FractionString()
			<< " ";
	std::cout << "width = "
			<< Fraction(beat.width.getDuration(), 0.001).FractionString()
			<< " ";
	std::cout << "offset = "
			<< Fraction(beat.offset.getDuration(), 0.001).FractionString()
			<< " ";
}

void BeatGamut::List(double beat_time) const {
	std::cout << "\n" << screen::Header << screen::Tab << "Beats\n"
			<< screen::Header;
	for (auto index : beats_) {
		std::cout << index.first << ": ";
		List1(index.second);
		std::cout << "\n";
	}
	std::cout << "beat(" << Fraction(beat_time, 0.001).FractionString()
			<< ")\n";
	std::cout << screen::Header;
}

BeatGamut& BeatGamut::ParseBlob(Blob &blob, double &beat_time, bool makemusic) {
	for (auto &command : blob.children_) {
		command.AssertFunction();
		std::string key = command.key_;
		if (key == "new") {
			Clear();
			beat_time = 0;
		} else if (key == "list") {
			if (makemusic)
				List(beat_time);
		} else if (key == "beat")
			beat_time = BuildNoteDuration(command).getDuration();
		else {
			Beat &beat = getBeat(key);
			beat.articulations = command["articulations"].atom();
			if (beat.articulations == "off")
				beat.articulations = "";
			beat.duration = BuildNoteDuration(command["D"]);
			beat.offset =
					(command.hasKey("offset")) ?
							BuildNoteDuration(command["offset"]) : 0.0;
			beat.width =
					(command.hasKey("width")) ?
							BuildNoteDuration(command["width"]) : 0.01;
		}
	}
	return *this;
}

}

