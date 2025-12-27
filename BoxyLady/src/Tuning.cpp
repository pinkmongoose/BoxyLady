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

#include <utility>
#include <numeric>

#include "Tuning.h"
#include "Fraction.h"
#include "Builders.h"

namespace BoxyLady {

constexpr size_t DiatonicNotes {7}, ChromaticNotes {12};
const std::array<std::string, DiatonicNotes> StandardNames { { "c", "d", "e", "f", "g", "a", "b" } };
const std::array<std::string, ChromaticNotes> ChromaticNames { { "c", "cis", "d", "ees", "e", "f", "fis", "g", "gis", "a", "bes", "b" } };
constexpr std::array<int, DiatonicNotes> StandardNotes { { 0, 2, 4, 5, 7, 9, 11 } };
constexpr float_type MaxAccidental {100.0};

PitchGamut& PitchGamut::Clear() {
	return *this = PitchGamut();
}

float_type PitchGamut::PitchIndex(NoteValue note) {
	float_type rank {note_values_[note.number_] + note.accidental_},
		max_rank {static_cast<float_type>(pitch_classes_n_)};
	if (rank < 0.0)
		return rank + max_rank;
	else if (rank > max_rank)
		return rank - max_rank;
	else
		return rank;
}

int PitchGamut::NearestOctave(const NoteValue note, const NoteValue relative_note) const {
	int number_difference {note.number_ - relative_note.number_};
	float_type fraction_of_octave {static_cast<float_type>(number_difference) / static_cast<float_type>(note_names_.size())};
	if (fraction_of_octave > 0.5)
		return -1;
	else if (fraction_of_octave <= -0.5)
		return 1;
	else
		return 0;
}

NoteValue PitchGamut::NoteAbsolute(const std::string input) {
	std::string buffer {input}, buffer2;
	NoteValue note;
	size_t number {std::string::npos}, name_length {0};
	for (size_t index {0}; index < note_names_.size(); index++) {
		const auto& note_name {note_names_[index]};
		if (buffer.find(note_name) == 0) {
			const size_t this_length {note_name.length()};
			if (this_length > name_length) {
				number = index;
				name_length = this_length;
			}
		}
	}
	buffer.erase(0, name_length);
	if (number == std::string::npos)
		throw EError("Note [" + buffer + "] not recognised in current gamut.");
	note.number_ = number;
	const size_t down_index {buffer.find(',')}, up_index {buffer.find('\'')};
	size_t articulation_index {buffer.find('-')}, note_end {down_index};
	if (up_index < note_end)
		note_end = up_index;
	if (articulation_index < note_end)
		note_end = articulation_index;
	if (note_end == 0)
		buffer2 = "";
	else if (note_end == std::string::npos) {
		buffer2 = buffer;
		buffer = "";
	} else {
		buffer2 = buffer;
		buffer2.erase(note_end, std::string::npos);
		buffer.erase(0, note_end);
	}
	if (buffer2.length() > 0) {
		size_t count {accidentals_.count(buffer2)};
		if (count != 1)
			throw EError("Accidental [" + buffer2 + "] not recognised in current gamut.");
		note.accidental_ = accidentals_[buffer2][note.number_];
	}
	articulation_index = buffer.find('-');
	if (articulation_index != std::string::npos)
		buffer.erase(articulation_index, std::string::npos);
	note.octave_ = 0;
	for (auto index : buffer) {
		if (index == '\'')
			note.octave_++;
		else if (index == ',')
			note.octave_--;
		else
			throw EError("Found something odd: [" + buffer + "] while building a note.");
	}
	return note;
}

NoteValue PitchGamut::NoteRelative(std::string input, NoteValue relative_note) {
	NoteValue note {NoteAbsolute(input)};
	note.octave_ = note.octave_ + relative_note.octave_ + NearestOctave(note, relative_note);
	return note;
}

NoteValue PitchGamut::Offset(NoteValue note, int number_offset, float_type accidental_offset, int octave_offset) const {
	note.number_ += number_offset;
	const size_t names_count {note_names_.size()};
	while (std::cmp_greater_equal(note.number_, names_count)) {
		note.number_ -= names_count;
		note.octave_++;
	}
	while (note.number_ < 0) {
		note.number_ += names_count;
		note.octave_--;
	}
	note.accidental_ += accidental_offset;
	note.octave_ += octave_offset;
	return note;
}

float_type PitchGamut::FreqMultStandard(NoteValue note) {
	const float_type note_ratio {FreqMultFromNote(note)};
	return note_ratio / standard_pitch_;
}

float_type PitchGamut::FreqMultFromNote(NoteValue note) {
	int& octave {note.octave_};
	float_type rank {note_values_[note.number_] + note.accidental_ + key_signature_[note.number_]},
		max_rank {static_cast<float_type>(pitch_classes_n_)};
	while (rank > max_rank) {
		rank -= max_rank;
		octave++;
	}
	while (rank < 0.0) {
		rank += max_rank;
		octave--;
	}
	const float_type rank_remainder {rank - floor(rank)};
	if (rank_remainder == 0.0)
		return FreqMultFromRank(octave, rank);
	else {
		const float_type ratio_below {FreqMultFromRank(octave, floor(rank))},
			ratio_above {FreqMultFromRank(octave, floor(rank) + 1.0)};
		return exp(log(ratio_below) * (1.0 - rank_remainder) + log(ratio_above) * rank_remainder);
	}
}

float_type PitchGamut::FreqMultFromRank(int octave, float_type rank) {
	const float_type max_rank {static_cast<float_type>(pitch_classes_n_)};
	while (rank >= max_rank) {
		rank -= max_rank;
		octave++;
	}
	while (rank < 0.0) {
		rank += max_rank;
		octave--;
	}
	return pow(repeat_ratio_, octave) * pitches_[static_cast<size_t>(rank)];
}

PitchGamut& PitchGamut::StandardPitch(std::string input, float_type ratio) {
	standard_pitch_ = FreqMultFromNote(NoteAbsolute(input));
	standard_pitch_ /= ratio;
	return *this;
}

void PitchGamut::StandardAccidentals(float_type multiplier) {
	static const std::vector<std::tuple<std::string, float_type>> standard_accidentals {
		{"es", -1.0}, {"is", 1.0}, {"eses", -2.0}, {"isis", 2.0}, {"eh", -0.5}, {"ih", 0.5},
		{"eseh", -1.5}, {"isih", 1.5}, {"et", -2.0 / 3.0}, {"it", 2.0 / 3.0},
		{"ets", -1.0 / 3.0}, {"its", 1.0 / 3.0}
	};
	for (auto accidental : standard_accidentals)
		Accidental(std::get<0>(accidental)).assign(note_names_.size(), multiplier * std::get<1>(accidental));
}

FloatVector& PitchGamut::Accidental(std::string name) {
	if (accidentals_.count(name))
		return accidentals_[name];
	else {
		accidentals_.insert(std::pair<std::string, FloatVector>(name, FloatVector()));
		return accidentals_[name];
	}
}

void PitchGamut::List(Blob& blob) {
	static const std::map<std::string, pitch_unit> pitch_units {
		{"cent", pitch_unit::cents}, {"m8ve", pitch_unit::millioctaves}, {"yu", pitch_unit::yu},
		{"12edo", pitch_unit::edo12}, {"19edo", pitch_unit::edo19},
		{"24edo", pitch_unit::edo24}, {"31edo", pitch_unit::edo31},
		{"Savart", pitch_unit::savart},
		{"meride", pitch_unit::meride}, {"heptameride", pitch_unit::heptameride}
	}; 
	constexpr float_type DefaultCents {20.0};
	float_type tol {pow(2.0, DefaultCents / physics::CentsPerOctave)};
	int limit {int_max};
	pitch_unit unit {pitch_unit::cents};
	if (blob.hasKey("pitch_unit")) {
		const std::string unit_str {blob["pitch_unit"].atom()};
		if (pitch_units.contains(unit_str)) unit = pitch_units.at(unit_str);
		else throw EError(unit_str + ": Unknown pitch unit.");
	}
	if (blob.hasKey("tol"))
		tol = pow(2.0, blob["tol"].asFloat(0, 100) / physics::CentsPerOctave);
	if (blob.hasKey("limit"))
		limit = blob["limit"].asInt(3, int_max);
	const bool list_factors {blob.hasFlag("factors")}, monzo_factors {blob.hasFlag("Monzo")};
	Screen::PrintFlags frame_flags;
	frame_flags[Screen::print_flag::frame] = !(list_factors || monzo_factors);
	auto ListFraction = [unit, list_factors, monzo_factors](Fraction fraction, Fraction remainder) {
		Screen::escape remainder_escape {(remainder.sgnLog() < 0.0)? Screen::escape::magenta :
			((remainder.sgnLog() > 0.0)? Screen::escape::cyan : Screen::escape::yellow)};
		return fraction.RatioString() + " " + screen.Tab(18)
			+ screen.Format({Screen::escape::yellow},fraction.PitchUnitString(unit))
			+ screen.Tab(27) + " ~ "
			+ fraction.FractionString(true)
			+ " (" + screen.Format({remainder_escape}, remainder.PitchUnitString(unit)) + ") "
			+ screen.Format({Screen::escape::green}, fraction.IntervalString()) + " "
			+ (list_factors? Factors(fraction).toString() + " " : "")
			+ (monzo_factors? Factors(fraction).toMonzo() : "");
	};
	auto DoList = [this, ListFraction, tol, limit](float_type ratio) {
		const Fraction fraction(ratio, tol, limit),
			remainder(ratio / fraction.ratio());
		return ListFraction(fraction, remainder);
	};
	enum class list_type {
		all, automatic, diatonic, chromatic
	};
	list_type type {list_type::all};
	if (blob.hasKey("type")) {
		const auto atom {blob["type"].atom()};
		if (atom == "diatonic")
			type = list_type::diatonic;
		else if (atom == "chromatic")
			type = list_type::chromatic;
		else if (atom == "auto")
			type = list_type::automatic;
	}
	screen.PrintHeader("Pitch table");
	switch (type) {
	case list_type::automatic:
		for (auto index : note_names_)
			screen.PrintFrame(index + " = " + DoList(pitches_[PitchIndex(index)]), frame_flags);
		break;
	case list_type::diatonic:
		for (auto index : StandardNames)
			screen.PrintFrame(index + " = " + DoList(pitches_[PitchIndex(index)]), frame_flags);
		break;
	case list_type::chromatic:
		for (auto index : ChromaticNames)
			screen.PrintFrame(index + " = " + DoList(pitches_[PitchIndex(index)]), frame_flags);
		break;
	default:
		for (size_t index {0}; index < pitch_classes_n_; index++)
			screen.PrintFrame(std::format("{}",index) + " = " + DoList(pitches_[index]), frame_flags);
		break;
	};
	screen.PrintFrame("RR = " + DoList(repeat_ratio_), frame_flags);
	screen.PrintSeparatorBot();
}

PitchGamut& PitchGamut::TuningBlob(Blob& blob, [[maybe_unused]] bool makemusic) {
	std::string tuning, key {"c"};
	if (blob.hasKey("type")) {
		tuning = blob["type"].atom();
		key = blob["key"].atom();
	} else
		tuning = blob.atom();
	if (tuning == "10edo")
		TET10();
	else if (tuning == "12edo")
		TET12();
	else if (tuning == "14edo")
		TET14();
	else if (tuning == "15edo")
		TwotoneNotes(3, 2, 1).EqualTemper(2.0).StandardPitch();
	else if (tuning == "19edo")
		MeantoneNotes(3, 2).EqualTemper(2.0).StandardPitch();
	else if (tuning == "22edo")
		TwotoneNotes(4, 3, 2).EqualTemper(2.0).StandardPitch();
	else if (tuning == "29edo")
		TwotoneNotes(5, 4, 3).EqualTemper(2.0).StandardPitch();
	else if (tuning == "31edo")
		MeantoneNotes(5, 3).EqualTemper(2.0).StandardPitch();
	else if (tuning == "43edo")
		MeantoneNotes(7, 4).EqualTemper(2.0).StandardPitch();
	else if (tuning == "48edo")
		TwotoneNotes(8, 7, 5).EqualTemper(2.0).StandardPitch();
	else if (tuning == "50edo")
		MeantoneNotes(8, 5).EqualTemper(2.0).StandardPitch();
	else if (tuning == "53edo")
		TwotoneNotes(9, 8, 5).EqualTemper(2.0).StandardPitch();
	else if (tuning == "81edo")
		MeantoneNotes(13, 8).EqualTemper(2.0).StandardPitch();
	else if (tuning == "pelog")
		Pelog();
	else if (tuning == "slendro")
		Slendro();
	else if (tuning == "Pythagorean")
		MeantoneNotes(2, 1).MeantoneRegular(3.0 / 2.0, key, 2.0);
	else if (tuning == "4cmt")
		MeantoneNotes(2, 1).MeantoneRegular(pow(5.0, 0.25), key, 2.0);
	else if (tuning == "harmonic")
		Harmonic12(key);
	else if (tuning == "Ptolemy")
		Ptolemy12(key);
	else if (tuning == "Harrison")
		MeantoneNotes(2, 1).MeantoneRegular(1.494412, key, 2.0);
	else if (tuning == "golden")
		MeantoneNotes(2, 1).MeantoneRegular(1.49503445, key, 2.0);
	else if (tuning == "BPLambda")
		BPLambda();
	else if (tuning == "WCAlpha")
		WCAlpha();
	else if (tuning == "WCBeta")
		WCBeta();
	else if (tuning == "WCGamma")
		WCGamma();
	else
		throw EError(tuning + ": Unknown tuning type.");
	return *this;
}

PitchGamut& PitchGamut::ParseBlob(Blob& blob, bool make_music) {
	for (auto command : blob.children_) {
		auto key {command.key_}, val {command.val_};
		if (key == "new")
			Clear();
		else if (key == "tuning")
			TuningBlob(command.ifFunction(), make_music);
		else if (key == "note_names") {
			size_t count {command.ifFunction().children_.size()};
			note_names_.resize(count);
			key_signature_.resize(count);
			note_values_.clear();
			accidentals_.clear();
			for (size_t index {0}; index < count; index++)
				note_names_[index] = command[index].atom();
		} else if (key == "notes_meantone")
			MeantoneNotes(command.ifFunction()["tone"].asInt(1, int_max), command["half"].asInt(1, int_max));
		else if (key == "notes_twotone")
			TwotoneNotes(command.ifFunction()["major"].asInt(1, int_max),
				command["minor"].asInt(1, int_max),
				command["half"].asInt(1, int_max));
		else if (key == "pitch_classes") {
			pitch_classes_n_ = command.asInt(1, int_max);
			pitches_.assign(pitch_classes_n_, 1.0);
			note_names_.clear();
			note_values_.clear();
			key_signature_.clear();
			accidentals_.clear();
		} else if (key == "notes") {
			if (note_names_.size() == 0)
				throw EError("Note names must be assigned before notes.");
			size_t count {command.ifFunction().children_.size()};
			if (count != note_names_.size())
				throw EError("Note offsets doesn't match note names.");
			note_values_.resize(count);
			for (size_t index {0}; index < count; index++)
				note_values_[index] = command[index].asFloat(0.0, pitch_classes_n_);
		} else if (key == "accidentals") {
			command.AssertFunction();
			if (note_names_.size() == 0)
				throw EError("Note names must be assigned before accidentals.");
			for (auto sub_command : command.children_) {
				const auto key {sub_command.key_};
				const size_t count {sub_command.children_.size()},
					names_count {note_names_.size()};
				if (count == 1)
					Accidental(key).assign(names_count, sub_command[0].asFloat(-MaxAccidental,MaxAccidental));
				else {
					sub_command.AssertFunction();
					if (count != names_count)
						throw EError("Note accidentals don't match note names.");
					FloatVector offsets;
					offsets.resize(count);
					for (size_t index {0}; index < count; index++)
						offsets[index] = sub_command[index].asFloat(-MaxAccidental, MaxAccidental);
					Accidental(key) = offsets;
				}
			}
		} else if (key == "standard_accidentals")
			StandardAccidentals(command.asFloat(-MaxAccidental, MaxAccidental));
		else if (key == "standard") {
			const auto name {command.ifFunction()["note"].atom()};
			const float_type ratio {command["r"].asFloat(0.0, 10000.0)};
			StandardPitch(name, ratio);
		} else if (key == "repeat_ratio")
			repeat_ratio_ = BuildFrequency(command);
		else if (key == "equal_tempered")
			EqualTemper(repeat_ratio_);
		else if (key == "normalise")
			NormalisePitches();
		else if (key == "list") {
			if (make_music)
				List(command.ifFunction());
		} else if (key == "generator") {
			const auto base_note {command.ifFunction()["note"].atom()};
			const int rank {static_cast<int>(PitchIndex(base_note))};
			const float_type base {BuildFrequency(command["r"])},
				generator {BuildFrequency(command["g"])};
			const int count {command["n"].asInt(1, int_max)},
				step {command["step"].asInt(-pitch_classes_n_, pitch_classes_n_)};
			Generator(base, generator, step, rank, count - 1);
		} else if (key == "pitches") {
			if (command.ifFunction().children_.size() != pitch_classes_n_)
				throw EError("Pitches don't match expected size.");
			for (size_t index {0}; index < pitch_classes_n_; index++)
				pitches_[index] = BuildFrequency(command[index]);
		} else if (key == "key_signature") {
			if (command.ifFunction().children_.size() != note_names_.size())
				throw EError("Key signature list dosn't match expected size.");
			for (size_t index {0}; index < note_names_.size(); index++)
				key_signature_[index] = command[index].asFloat(-10.0,10.0);
		} else if (key == "pitch") {
			size_t rank {static_cast<size_t>(PitchIndex(command.ifFunction()["note"].atom()))};
			pitches_[rank] = BuildFrequency(command["r"]);
		} else if (key == "move_pitch") {
			size_t rank {static_cast<size_t>(PitchIndex(command.ifFunction()["note"].atom()))};
			pitches_[rank] *= BuildFrequency(command["r"]);
		} else if (key == "rotate_pitches") {
			const auto base_note {command.ifFunction()["note"].atom()};
			const int rank {static_cast<int>(PitchIndex(base_note))};
			RotatePitches(rank);
		} else
			throw EError(key + "=" + val + ": Unknown command.");
	}
	if (pitch_classes_n_ < 1)
		throw EError("Gamut was not complete on finishing (no pitches).");
	if (note_values_.size() < 1)
		throw EError("Gamut was not complete on finishing (no note offsets).");
	if (note_names_.size() < 1)
		throw EError("Gamut was not complete on finishing (no note names).");
	return *this;
}

PitchGamut& PitchGamut::NormalisePitches() {
	const float_type ratio0 {pitches_[0]};
	for (auto& pitch : pitches_)
		pitch /= ratio0;
	return *this;
}

PitchGamut& PitchGamut::RotatePitches(int offset) {
	if (pitch_classes_n_ < 1)
		throw EError("Can't rotate pitches until they have been assigned.");
	FloatVector new_pitches {pitches_};
	for (size_t index {0}; index < pitch_classes_n_; index++) {
		int offset_index {static_cast<int>(index) + offset};
		if (offset_index < 0)
			new_pitches[offset_index + pitch_classes_n_] = pitches_[index] * repeat_ratio_;
		else if (std::cmp_greater_equal(offset_index, pitch_classes_n_))
			new_pitches[offset_index - pitch_classes_n_] = pitches_[index] / repeat_ratio_;
		else
			new_pitches[offset_index] = pitches_[index];
	}
	pitches_ = new_pitches;
	NormalisePitches();
	return *this;
}

PitchGamut& PitchGamut::EqualTemper(float_type repeat_ratio) {
	repeat_ratio_ = repeat_ratio;
	for (size_t index {0}; index < pitch_classes_n_; index++)
		pitches_[index] = pow(repeat_ratio_,
			static_cast<float_type>(index) / static_cast<float_type>(pitch_classes_n_));
	return *this;
}

PitchGamut& PitchGamut::TwotoneNotes(size_t major, size_t minor, size_t half) {
	Clear();
	if (major < minor)
		throw EError("Major tone must be at least as large as minor tone.");
	if (minor < half)
		throw EError("Minor tone must be at least as large as half tone.");
	if (major < 1)
		throw EError("Major tone must be at least one step.");
	if (minor < 1)
		throw EError("Minor tone must be at least one step.");
	if (half < 1)
		throw EError("Half tone must be at least one step.");
	const std::array<int, DiatonicNotes + 1> major_steps { { 0, 1, 1, 1, 2, 3, 3, 3 } },
		minor_steps { { 0, 0, 1, 1, 1, 1, 2, 2 } }, half_steps { {0, 0, 0, 1, 1, 1, 1, 2 } };
	std::array<int, DiatonicNotes + 1> steps;
	note_names_ = StringVector(StandardNames.begin(), StandardNames.end());
	key_signature_.resize(note_names_.size());
	for (size_t index {0}; index < DiatonicNotes + 1; index++)
		steps[index] = major_steps[index] * major + minor_steps[index] * minor + half_steps[index] * half;
	pitch_classes_n_ = steps[DiatonicNotes];
	note_values_ = FloatVector(steps.begin(), steps.end());
	StandardAccidentals(static_cast<float_type>(major) - static_cast<float_type>(half));
	pitches_.assign(pitch_classes_n_, 1.0);
	return *this;
}

PitchGamut& PitchGamut::MeantoneNotes(size_t whole, size_t half) {
	if (whole < half)
		throw EError("Whole tone must be at least as large as half tone.");
	if (whole < 1)
		throw EError("Whole tone must be at least one step.");
	if (half < 1)
		throw EError("Half tone must be at least one step.");
	return TwotoneNotes(whole, whole, half);
}

PitchGamut& PitchGamut::GeneralNotes(size_t pitch_classes,
		FloatVector note_values, StringVector names, float_type accidental_size) {
	Clear();
	if (note_values.size() != names.size())
		throw EError("Note offsets doesn't match note names.");
	note_names_ = names;
	key_signature_.resize(note_names_.size());
	note_values_ = note_values;
	pitch_classes_n_ = pitch_classes;
	if (accidental_size > 0)
		StandardAccidentals(accidental_size);
	return *this;
}

PitchGamut& PitchGamut::GeneralET(size_t pitch_classes, FloatVector note_values,
		float_type accidental_size, float_type repeat_ratio, StringVector names,
		std::string standard, float_type standard_ratio) {
	GeneralNotes(pitch_classes, note_values, names, accidental_size);
	pitches_.assign(pitch_classes_n_, 1.0);
	EqualTemper(repeat_ratio);
	StandardPitch(standard, standard_ratio);
	return *this;
}

PitchGamut& PitchGamut::ETWestern(size_t pitch_classes, FloatVector note_values,
		float_type accidental_size, float_type repeat_ratio) {
	if (note_values.size() != DiatonicNotes)
		throw EError("Gamut function requires 7 base notes (internal error).");
	return GeneralET(pitch_classes, note_values, accidental_size, repeat_ratio,
			StringVector(StandardNames.begin(), StandardNames.end()));
}

PitchGamut& PitchGamut::BPLambda() {
	static constexpr size_t BPDiatonicNotes {9}, BPChromaticNotes {13};
	static const std::array<std::string, BPDiatonicNotes> name_strings { { "c", "d", "e", "f", "g", "h", "j", "a", "b" } };
	static StringVector names {StringVector(name_strings.begin(), name_strings.end())};
	static const std::array<float_type, BPDiatonicNotes> note_values { { 0, 2, 3, 4, 6, 7, 9, 10, 12 } };
	return GeneralET(BPChromaticNotes,
			FloatVector(note_values.begin(), note_values.end()), 1, 3.0, names);
}

PitchGamut& PitchGamut::Pelog() {
	static constexpr size_t PelogDiatonicNotes {7}, PelogChromaticNotes {9};
	static const std::array<std::string, PelogDiatonicNotes> name_strings { { "ji", "ro", "lu", "pat", "ma", "nem", "pi" } };
	static StringVector names {StringVector(name_strings.begin(), name_strings.end())};
	static const std::array<float_type, PelogDiatonicNotes> note_values { { 0, 1, 2, 4, 5, 6, 8 } };
	return GeneralET(PelogChromaticNotes, FloatVector(note_values.begin(), note_values.end()), 0, 2.0, names, "ma''''");
}

PitchGamut& PitchGamut::Slendro() {
	static constexpr size_t SlendroNotes {5};
	static const std::array<std::string, SlendroNotes> name_strings { { "ji", "ro", "lu", "ma", "nem" } };
	static StringVector names = StringVector(name_strings.begin(), name_strings.end());
	static const std::array<float_type, SlendroNotes> note_values { { 0, 1, 2, 3, 4 } };
	return GeneralET(SlendroNotes, FloatVector(note_values.begin(), note_values.end()), 0, 2.0, names, "ma''''");
}

PitchGamut& PitchGamut::WCAlpha() {
	static float_type ScaleStep {((9.0 * log2(3.0 / 2.0) + 5.0 * log2(5.0 / 4.0)
			+ 4.0 * log2(6.0 / 5.0))) / (9.0 * 9.0 + 5.0 * 5.0 + 4.0 * 4.0)};
	static const float_type PseudoOctave {pow(2.0_flt, ScaleStep * 15.0_flt)};
	return TwotoneNotes(3, 2, 1).EqualTemper(PseudoOctave).StandardPitch();
}

PitchGamut& PitchGamut::WCBeta() {
	static const float_type ScaleStep {((11.0 * log2(3.0 / 2.0)
			+ 6.0 * log2(5.0 / 4.0) + 5.0 * log2(6.0 / 5.0)))
			/ (11.0 * 11.0 + 6.0 * 6.0 + 5.0 * 5.0)};
	static const float_type PseudoOctave {pow(2.0_flt, ScaleStep * 19.0_flt)};
	return MeantoneNotes(3, 2).EqualTemper(PseudoOctave).StandardPitch();
}

PitchGamut& PitchGamut::WCGamma() {
	static const float_type ScaleStep {((20.0 * log2(3.0 / 2.0)
			+ 11.0 * log2(5.0 / 4.0) + 9.0 * log2(6.0 / 5.0)))
			/ (20.0 * 20.0 + 11.0 * 11.0 + 9.0 * 9.0)};
	static float_type PseudoOctave {pow(2.0_flt, ScaleStep * 34.0_flt)};
	return TwotoneNotes(6, 5, 3).EqualTemper(PseudoOctave).StandardPitch();
}

PitchGamut& PitchGamut::TET14() {
	static constexpr std::array<float_type, 7> note_values { { 0, 2, 4, 6, 8, 10, 12 } };
	return ETWestern(14, FloatVector(note_values.begin(), note_values.end()), 1);
}

PitchGamut& PitchGamut::TET10() {
	static constexpr std::array<float_type, 7> note_values { { 0, 2, 4, 4, 6, 8, 8 } };
	return ETWestern(10, FloatVector(note_values.begin(), note_values.end()), 1);
}

PitchGamut& PitchGamut::Generator(float_type base, float_type generator, int step, int start, int count) {
	pitches_[start] = base;
	int rank {start}, n {static_cast<int>(pitch_classes_n_)};
	for (size_t index {0}; std::cmp_less(index, count); index++) {
		rank += step;
		base *= generator;
		if (rank >= n) {
			rank -= n;
			base /= 2.0;
		}
		if (rank < 0) {
			rank += n;
			base *= 2.0;
		}
		pitches_[rank] = base;
	}
	return *this;
}

PitchGamut& PitchGamut::Regular(float_type generator, int step, std::string base_note, float_type repeat_ratio) {
	repeat_ratio_ = repeat_ratio;
	const int start {static_cast<int>(PitchIndex(base_note))},
		anticlockwise_count {(static_cast<int>(pitch_classes_n_) - 1) / 2},
		clockwise_count {static_cast<int>(pitch_classes_n_) - 1 - anticlockwise_count};
	Generator(1.0, generator, step, start, clockwise_count);
	Generator(1.0, 1.0 / generator, -step, start, clockwise_count);
	NormalisePitches();
	return *this;
}

PitchGamut& PitchGamut::GeneralRegular(size_t pitch_classes,
		FloatVector note_values, float_type accidental_size, StringVector names,
		float_type generator, int step, std::string base_note, float_type repeat_ratio,
		std::string standard_note, float_type standard_ratio) {
	GeneralNotes(pitch_classes, note_values, names, accidental_size);
	Regular(generator, step, base_note, repeat_ratio);
	StandardPitch(standard_note, standard_ratio);
	return *this;
}

PitchGamut& PitchGamut::MeantoneRegular(float_type generator, std::string base_note, float_type repeat_ratio) {
	int step {static_cast<int>(PitchIndex("g") - PitchIndex("c"))};
	if (std::gcd(pitch_classes_n_, step) != 1)
		throw EError("The meantone regular function requires the octave/fifth steps counts to be coprime.");
	Regular(generator, step, base_note, repeat_ratio);
	StandardPitch();
	return *this;
}

PitchGamut& PitchGamut::RegularWestern(size_t pitch_classes,
		FloatVector note_values, float_type accidental_size, float_type generator,
		int step, std::string base_note) {
	if (note_values.size() != 7)
		throw EError("Gamut function requires 7 base notes (internal error).");
	return GeneralRegular(pitch_classes, note_values, accidental_size,
			StringVector(StandardNames.begin(), StandardNames.end()), generator,
			step, base_note, 2.0, "a''''", 1.0);
}

PitchGamut& PitchGamut::General12(FloatVector pitch_table, std::string base_note) {
	MeantoneNotes(2, 1);
	repeat_ratio_ = 2.0;
	pitches_ = pitch_table;
	RotatePitches(PitchIndex(base_note));
	StandardPitch();
	return *this;
}

PitchGamut& PitchGamut::Harmonic12(std::string base_note) {
	static constexpr std::array<float_type, ChromaticNotes> pitch_table { { 1.0, 17.0 / 16.0,
			9.0 / 8.0, 19.0 / 16.0, 5.0 / 4.0, 11.0 / 8.0, 23.0 / 16.0, 3.0
			/ 2.0, 13.0 / 8.0, 27.0 / 16.0, 7.0 / 4.0, 15.0 / 8.0 } };
	return General12(FloatVector(pitch_table.begin(), pitch_table.end()),
			base_note);
}

PitchGamut& PitchGamut::Ptolemy12(std::string base_note) {
	static constexpr std::array<float_type, ChromaticNotes> pitch_table { { 1.0, 16.0 / 15.0,
			9.0 / 8.0, 6.0 / 5.0, 5.0 / 4.0, 4.0 / 3.0, 45.0 / 32.0, 3.0 / 2.0,
			8.0 / 5.0, 5.0 / 3.0, 16.0 / 9.0, 15.0 / 8.0 } };
	return General12(FloatVector(pitch_table.begin(), pitch_table.end()),
			base_note);
}

} //end namespace BoxyLady
