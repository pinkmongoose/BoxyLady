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

#include "Tuning.h"

namespace BoxyLady {

constexpr int DiatonicNotes { 7 }, ChromaticNotes { 12 };
const std::array<std::string, DiatonicNotes> StandardNames { { "c", "d", "e",
		"f", "g", "a", "b" } };
const std::array<std::string, ChromaticNotes> ChromaticNames { { "c", "cis",
		"d", "ees", "e", "f", "fis", "g", "gis", "a", "bes", "b" } };
const std::array<int, DiatonicNotes> StandardNotes { { 0, 2, 4, 5, 7, 9, 11 } };
constexpr double MaxAccidental { 100.0 };

PitchGamut& PitchGamut::Clear() {
	note_names_.clear();
	pitch_classes_n_ = 0;
	repeat_ratio_ = 2.0;
	note_values_.clear();
	accidentals_.clear();
	pitches_.clear();
	standard_pitch_ = 1.0;
	return *this;
}

double PitchGamut::PitchIndex(NoteValue note) {
	double rank = note_values_[note.number_] + note.accidental_, max_rank =
			static_cast<double>(pitch_classes_n_);
	if (rank < 0)
		return rank + max_rank;
	else if (rank > max_rank)
		return rank - max_rank;
	else
		return rank;
}

int PitchGamut::NearestOctave(const NoteValue note,
		const NoteValue relative_note) const {
	int number_difference = note.number_ - relative_note.number_;
	double fraction_of_octave = static_cast<double>(number_difference)
			/ static_cast<double>(note_names_.size());
	if (fraction_of_octave > 0.5)
		return -1;
	else if (fraction_of_octave <= -0.5)
		return 1;
	else
		return 0;
}

NoteValue PitchGamut::Note(const std::string input) {
	std::string buffer = input, buffer2;
	NoteValue note;
	int number = -1, name_length = 0;
	for (size_t index = 0; index < note_names_.size(); index++) {
		const std::string &note_name = note_names_[index];
		if (buffer.find(note_name) == 0) {
			const int this_length = note_name.length();
			if (this_length > name_length) {
				number = index;
				name_length = this_length;
			}
		}
	}
	buffer.erase(0, name_length);
	if (number < 0)
		throw EGamut().msg(
				"Note [" + buffer + "] not recognised in current gamut.");
	note.number_ = number;
	const size_t down_index = buffer.find(','), up_index = buffer.find('\'');
	size_t articulation_index = buffer.find('-');
	size_t note_end = down_index;
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
		int count = accidentals_.count(buffer2);
		if (count != 1)
			throw EGamut().msg(
					"Accidental [" + buffer2
							+ "] not recognised in current gamut.");
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
			throw EGamut().msg(
					"Found something odd: [" + buffer
							+ "] while building a note.");
	}
	return note;
}

NoteValue PitchGamut::Note(std::string input, NoteValue relative_note) {
	NoteValue note = Note(input);
	note.octave_ = note.octave_ + relative_note.octave_
			+ NearestOctave(note, relative_note);
	return note;
}

NoteValue PitchGamut::Offset(NoteValue note, int number_offset,
		double accidental_offset, int octave_offset) const {
	note.number_ += number_offset;
	const int names_count = note_names_.size();
	while (note.number_ >= names_count) {
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

double PitchGamut::FreqMultStandard(NoteValue note) {
	const double note_ratio = FreqMultFromNote(note);
	return note_ratio / standard_pitch_;
}

double PitchGamut::FreqMultFromNote(NoteValue note) {
	int &octave = note.octave_;
	double rank = note_values_[note.number_] + note.accidental_, max_rank =
			static_cast<double>(pitch_classes_n_);
	while (rank > max_rank) {
		rank -= max_rank;
		octave++;
	}
	while (rank < 0) {
		rank += max_rank;
		octave--;
	}
	const double rank_remainder = rank - floor(rank);
	if (rank_remainder == 0)
		return FreqMultFromRank(octave, rank);
	else {
		const double ratio_below = FreqMultFromRank(octave, floor(rank)),
				ratio_above = FreqMultFromRank(octave, floor(rank) + 1);
		return exp(
				log(ratio_below) * (1.0 - rank_remainder)
						+ log(ratio_above) * rank_remainder);
	}
}

double PitchGamut::FreqMultFromRank(int octave, double rank) {
	const double max_rank = static_cast<double>(pitch_classes_n_);
	while (rank >= max_rank) {
		rank -= max_rank;
		octave++;
	}
	while (rank < 0) {
		rank += max_rank;
		octave--;
	}
	return pow(repeat_ratio_, octave) * pitches_[int(rank)];
}

PitchGamut& PitchGamut::StandardPitch(std::string input, double ratio) {
	standard_pitch_ = FreqMultFromNote(Note(input));
	standard_pitch_ /= ratio;
	return *this;
}

void PitchGamut::StandardAccidentals(double multiplier) {
	const int names_count = note_names_.size();
	auto NewAccidental = [this, names_count, multiplier](std::string key,
			double fraction) {
		Accidental(key).assign(names_count, fraction * multiplier);
	};
	NewAccidental("es", -1.0);
	NewAccidental("is", 1.0);
	NewAccidental("eses", -2.0);
	NewAccidental("isis", 2.0);
	NewAccidental("eh", -0.5);
	NewAccidental("ih", 0.5);
	NewAccidental("eseh", -1.5);
	NewAccidental("isih", 1.5);
	NewAccidental("et", -2.0 / 3.0);
	NewAccidental("it", 2.0 / 3.0);
	NewAccidental("ets", -1.0 / 3.0);
	NewAccidental("its", 1.0 / 3.0);
}

DoubleVector& PitchGamut::Accidental(std::string name) {
	if (accidentals_.count(name))
		return accidentals_[name];
	else {
		accidentals_.insert(
				std::pair<std::string, DoubleVector>(name, DoubleVector()));
		return accidentals_[name];
	}
}

void PitchGamut::List(Blob &blob) {
	constexpr double DefaultCents = 20.0;
	double tol = pow(2.0, DefaultCents / physics::CentsPerOctave);
	int limit = INT_MAX;
	if (blob.hasKey("tol"))
		tol = pow(2.0, blob["tol"].asDouble(0, 100) / physics::CentsPerOctave);
	if (blob.hasKey("limit"))
		limit = blob["limit"].asInt(0, INT_MAX);
	auto ListFraction = [](Fraction fraction, Fraction remainder) {
		return fraction.RatioString() + " " + fraction.CentsString() + " ~ "
				+ fraction.FractionString(true) + " (" + remainder.CentsString()
				+ ") " + fraction.IntervalString();
	};
	auto DoList = [this, ListFraction, tol, limit](double ratio) {
		const Fraction fraction(ratio, tol, limit), remainder(
				ratio / fraction.ratio());
		return ListFraction(fraction, remainder);
	};
	enum class list_type {
		all, automatic, diatonic, chromatic
	};
	list_type type = list_type::all;
	if (blob.hasKey("type")) {
		const std::string atom = blob["type"].atom();
		if (atom == "diatonic")
			type = list_type::diatonic;
		else if (atom == "chromatic")
			type = list_type::chromatic;
		else if (atom == "auto")
			type = list_type::automatic;
	}
	std::cout << "\n" << screen::Header << screen::Tab << "Pitch table\n"
			<< screen::Header;
	switch (type) {
	case list_type::automatic:
		for (auto index : note_names_)
			std::cout << index << " = " << DoList(pitches_[PitchIndex(index)])
					<< "\n";
		break;
	case list_type::diatonic:
		for (auto index : StandardNames)
			std::cout << index << " = " << DoList(pitches_[PitchIndex(index)])
					<< "\n";
		break;
	case list_type::chromatic:
		for (auto index : ChromaticNames)
			std::cout << index << " = " << DoList(pitches_[PitchIndex(index)])
					<< "\n";
		break;
	default:
		for (int index = 0; index < pitch_classes_n_; index++)
			std::cout << "pitch " << index << " = " << DoList(pitches_[index])
					<< "\n";
		break;
	};
	std::cout << "Repeat ratio = " << DoList(repeat_ratio_) << "\n";
	std::cout << screen::Header;
}

PitchGamut& PitchGamut::TuningBlob(Blob &blob, bool makemusic) {
	std::string tuning, key = "c";
	if (blob.hasKey("type")) {
		tuning = blob["type"].atom();
		key = blob["key"].atom();
	} else
		tuning = blob.atom();
	if (tuning == "10tet")
		TET10();
	else if (tuning == "12tet")
		TET12();
	else if (tuning == "14tet")
		TET14();
	else if (tuning == "15tet")
		TET15();
	else if (tuning == "19tet")
		TET19();
	else if (tuning == "22tet")
		TET22();
	else if (tuning == "31tet")
		TET31();
	else if (tuning == "pelog")
		Pelog();
	else if (tuning == "slendro")
		Slendro();
	else if (tuning == "Pythagorean")
		Pythagorean12(key);
	else if (tuning == "4cmt")
		QuarterComma12(key);
	else if (tuning == "harmonic")
		Harmonic12(key);
	else if (tuning == "Ptolemy")
		Ptolemy12(key);
	else if (tuning == "Harrison")
		Harrison12(key);
	else if (tuning == "BPLambda")
		BPLambda();
	else if (tuning == "WCAlpha")
		WCAlpha();
	else if (tuning == "WCBeta")
		WCBeta();
	else if (tuning == "WCGamma")
		WCGamma();
	else
		throw EGamut().msg(tuning + ": Unknown tuning type.");
	return *this;
}

PitchGamut& PitchGamut::ParseBlob(Blob &blob, bool make_music) {
	for (auto command : blob.children_) {
		std::string key = command.key_, val = command.val_;
		if (key == "new")
			Clear();
		else if (key == "tuning")
			TuningBlob(command.ifFunction(), make_music);
		else if (key == "note_names") {
			size_t count = command.ifFunction().children_.size();
			note_names_.resize(count);
			note_values_.clear();
			accidentals_.clear();
			for (size_t index = 0; index < count; index++)
				note_names_[index] = command[index].atom();
		} else if (key == "notes_meantone")
			MeantoneNotes(command.ifFunction()["tone"].asInt(1, INT_MAX),
					command["half"].asInt(1, INT_MAX));
		else if (key == "notes_twotone")
			TwotoneNotes(command.ifFunction()["major"].asInt(1, INT_MAX),
					command["minor"].asInt(1, INT_MAX),
					command["half"].asInt(1, INT_MAX));
		else if (key == "notes_porcupine")
			PorcupineNotes(command.ifFunction()["major"].asInt(2, INT_MAX),
					command["minor"].asInt(1, INT_MAX));
		else if (key == "pitch_classes") {
			pitch_classes_n_ = command.asInt(1, INT_MAX);
			pitches_.assign(pitch_classes_n_, 1.0);
			note_names_.clear();
			note_values_.clear();
			accidentals_.clear();
		} else if (key == "notes") {
			if (note_names_.size() == 0)
				throw EGamut().msg("Note names must be assigned before notes.");
			size_t count = command.ifFunction().children_.size();
			if (count != note_names_.size())
				throw EGamut().msg("Note offsets doesn't match note names.");
			note_values_.resize(count);
			for (size_t index = 0; index < count; index++)
				note_values_[index] = command[index].asDouble(0,
						pitch_classes_n_);
		} else if (key == "accidentals") {
			command.AssertFunction();
			if (note_names_.size() == 0)
				throw EGamut().msg(
						"Note names must be assigned before accidentals.");
			for (auto sub_command : command.children_) {
				std::string key = sub_command.key_;
				const size_t count = sub_command.children_.size(), names_count =
						note_names_.size();
				if (count == 1)
					Accidental(key).assign(names_count,
							sub_command[0].asDouble(-MaxAccidental,
									MaxAccidental));
				else {
					sub_command.AssertFunction();
					if (count != names_count)
						throw EGamut().msg(
								"Note accidentals don't match note names.");
					DoubleVector offsets;
					offsets.resize(count);
					for (size_t index = 0; index < count; index++)
						offsets[index] = sub_command[index].asDouble(
								-MaxAccidental, MaxAccidental);
					Accidental(key) = offsets;
				}
			}
		} else if (key == "standard_accidentals")
			StandardAccidentals(
					command.asDouble(-MaxAccidental, MaxAccidental));
		else if (key == "standard") {
			const std::string name = command.ifFunction()["note"].atom();
			const double ratio = command["r"].asDouble(0, 10000);
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
			std::string base_note = command.ifFunction()["note"].atom();
			const int rank = PitchIndex(base_note);
			const double base = BuildFrequency(command["r"]), generator =
					BuildFrequency(command["g"]);
			const int count = command["n"].asInt(1, INT_MAX), step =
					command["step"].asInt(-pitch_classes_n_, pitch_classes_n_);
			Generator(base, generator, step, rank, count - 1);
		} else if (key == "pitches") {
			if (int(command.ifFunction().children_.size()) != pitch_classes_n_)
				throw EGamut().msg("Pitches don't match expected size.");
			for (int index = 0; index < pitch_classes_n_; index++)
				pitches_[index] = BuildFrequency(command[index]);
		} else if (key == "pitch") {
			int rank = PitchIndex(command.ifFunction()["note"].atom());
			pitches_[rank] = BuildFrequency(command["r"]);
		} else if (key == "move_pitch") {
			int rank = PitchIndex(command.ifFunction()["note"].atom());
			pitches_[rank] *= BuildFrequency(command["r"]);
		} else if (key == "rotate_pitches") {
			std::string base_note = command.ifFunction()["note"].atom();
			const int rank = PitchIndex(base_note);
			RotatePitches(rank);
		} else
			throw EGamut().msg(key + "=" + val + ": Unknown command.");
	}
	if (pitch_classes_n_ < 1)
		throw EGamut().msg("Gamut was not complete on finishing (no pitches).");
	if (note_values_.size() < 1)
		throw EGamut().msg(
				"Gamut was not complete on finishing (no note offsets).");
	if (note_names_.size() < 1)
		throw EGamut().msg(
				"Gamut was not complete on finishing (no note names).");
	return *this;
}

PitchGamut& PitchGamut::NormalisePitches() {
	const double ratio0 = pitches_[0];
	for (auto &pitch : pitches_)
		pitch /= ratio0;
	return *this;
}

PitchGamut& PitchGamut::RotatePitches(int offset) {
	if (pitch_classes_n_ < 1)
		throw EGamut().msg(
				"Can't rotate pitches until they have been assigned.");
	DoubleVector new_pitches = pitches_;
	for (int index = 0; index < pitch_classes_n_; index++) {
		int offset_index = index + offset;
		if (offset_index < 0)
			new_pitches[offset_index + pitch_classes_n_] = pitches_[index]
					* repeat_ratio_;
		else if (offset_index >= pitch_classes_n_)
			new_pitches[offset_index - pitch_classes_n_] = pitches_[index]
					/ repeat_ratio_;
		else
			new_pitches[offset_index] = pitches_[index];
	}
	pitches_ = new_pitches;
	NormalisePitches();
	return *this;
}

PitchGamut& PitchGamut::EqualTemper(double repeat_ratio) {
	repeat_ratio_ = repeat_ratio;
	for (int index = 0; index < pitch_classes_n_; index++)
		pitches_[index] = pow(repeat_ratio_,
				static_cast<double>(index)
						/ static_cast<double>(pitch_classes_n_));
	return *this;
}

PitchGamut& PitchGamut::TwotoneNotes(int major, int minor, int half) {
	Clear();
	if (major < minor)
		throw EGamut().msg(
				"Major tone must be at least as large as minor tone.");
	if (minor < half)
		throw EGamut().msg(
				"Minor tone must be at least as large as half tone.");
	if (major < 1)
		EGamut().msg("Major tone must be at least one step.");
	if (minor < 1)
		EGamut().msg("Minor tone must be at least one step.");
	if (half < 1)
		EGamut().msg("Half tone must be at least one step.");
	const std::array<int, DiatonicNotes + 1> major_steps { { 0, 1, 1, 1, 2, 3,
			3, 3 } }, minor_steps { { 0, 0, 1, 1, 1, 1, 2, 2 } }, half_steps { {
			0, 0, 0, 1, 1, 1, 1, 2 } };
	std::array<int, DiatonicNotes + 1> steps;
	note_names_ = StringVector(StandardNames.begin(), StandardNames.end());
	for (int index = 0; index < DiatonicNotes + 1; index++)
		steps[index] = major_steps[index] * major + minor_steps[index] * minor
				+ half_steps[index] * half;
	pitch_classes_n_ = steps[DiatonicNotes];
	note_values_ = DoubleVector(steps.begin(), steps.end());
	StandardAccidentals(static_cast<double>(major) - static_cast<double>(half));
	pitches_.assign(pitch_classes_n_, 1.0);
	return *this;
}

PitchGamut& PitchGamut::PorcupineNotes(int major, int minor) {
	const int half = minor * 2 - major;
	return TwotoneNotes(major, minor, half);
}

PitchGamut& PitchGamut::MeantoneNotes(int whole, int half) {
	if (whole < half)
		throw EGamut().msg(
				"Whole tone must be at least as large as half tone.");
	if (whole < 1)
		throw EGamut().msg("Whole tone must be at least one step.");
	if (half < 1)
		throw EGamut().msg("Half tone must be at least one step.");
	return TwotoneNotes(whole, whole, half);
}

PitchGamut& PitchGamut::GeneralNotes(int pitch_classes,
		DoubleVector note_values, StringVector names, double accidental_size) {
	Clear();
	if (note_values.size() != names.size())
		throw EGamut().msg("Note offsets doesn't match note names.");
	note_names_ = names;
	note_values_ = note_values;
	pitch_classes_n_ = pitch_classes;
	if (accidental_size > 0)
		StandardAccidentals(accidental_size);
	return *this;
}

PitchGamut& PitchGamut::GeneralET(int pitch_classes, DoubleVector note_values,
		double accidental_size, double repeat_ratio, StringVector names,
		std::string standard, double standard_ratio) {
	GeneralNotes(pitch_classes, note_values, names, accidental_size);
	pitches_.assign(pitch_classes_n_, 1.0);
	EqualTemper(repeat_ratio);
	StandardPitch(standard, standard_ratio);
	return *this;
}

PitchGamut& PitchGamut::ETWestern(int pitch_classes, DoubleVector note_values,
		double accidental_size, double repeat_ratio) {
	if (note_values.size() != DiatonicNotes)
		throw EGamut().msg(
				"Gamut function requires 7 base notes (internal error).");
	return GeneralET(pitch_classes, note_values, accidental_size, repeat_ratio,
			StringVector(StandardNames.begin(), StandardNames.end()));
}

PitchGamut& PitchGamut::BPLambda() {
	constexpr int BPDiatonicNotes = 9, BPChromaticNotes = 13;
	const std::array<std::string, BPDiatonicNotes> name_strings { { "c", "d",
			"e", "f", "g", "h", "j", "a", "b" } };
	StringVector names = StringVector(name_strings.begin(), name_strings.end());
	const std::array<double, BPDiatonicNotes> note_values { { 0, 2, 3, 4, 6, 7,
			9, 10, 12 } };
	return GeneralET(BPChromaticNotes,
			DoubleVector(note_values.begin(), note_values.end()), 1, 3.0, names);
}

PitchGamut& PitchGamut::Pelog() {
	constexpr int PelogDiatonicNotes = 7, PelogChromaticNotes = 9;
	const std::array<std::string, PelogDiatonicNotes> name_strings { { "ji",
			"ro", "lu", "pat", "ma", "nem", "pi" } };
	StringVector names = StringVector(name_strings.begin(), name_strings.end());
	const std::array<double, PelogDiatonicNotes> note_values { { 0, 1, 2, 4, 5,
			6, 8 } };
	return GeneralET(PelogChromaticNotes,
			DoubleVector(note_values.begin(), note_values.end()), 0, 2.0, names,
			"ma''''");
}

PitchGamut& PitchGamut::Slendro() {
	constexpr int SlendroNotes = 5;
	const std::array<std::string, SlendroNotes> name_strings { { "ji", "ro",
			"lu", "ma", "nem" } };
	StringVector names = StringVector(name_strings.begin(), name_strings.end());
	const std::array<double, SlendroNotes> note_values { { 0, 1, 2, 3, 4 } };
	return GeneralET(SlendroNotes,
			DoubleVector(note_values.begin(), note_values.end()), 0, 2.0, names,
			"ma''''");
}

PitchGamut& PitchGamut::WCAlpha_old() {
	const std::array<double, 7> note_values { { 0, 3, 5, 6, 9, 12, 14 } };
	return GeneralET(15, DoubleVector(note_values.begin(), note_values.end()),
			1, pow(1.5, 15.0 / 9.0),
			StringVector(StandardNames.begin(), StandardNames.end()));
}

PitchGamut& PitchGamut::WCGamma_old() {
	const std::array<double, 7> note_values { { 0, 6, 11, 14, 20, 26, 31 } };
	return GeneralET(34, DoubleVector(note_values.begin(), note_values.end()),
			2, pow(1.5, 34.0 / 20.0),
			StringVector(StandardNames.begin(), StandardNames.end()));
}

PitchGamut& PitchGamut::WCAlpha() {
	constexpr double ScaleStep = ((9.0 * log2(3.0 / 2.0) + 5.0 * log2(5.0 / 4.0)
			+ 4.0 * log2(6.0 / 5.0))) / (9.0 * 9.0 + 5.0 * 5.0 + 4.0 * 4.0),
			PseudoOctave = pow(2.0, ScaleStep * 15.0);
	return TwotoneNotes(3, 2, 1).EqualTemper(PseudoOctave).StandardPitch();
}

PitchGamut& PitchGamut::WCBeta() {
	constexpr double ScaleStep = ((11.0 * log2(3.0 / 2.0)
			+ 6.0 * log2(5.0 / 4.0) + 5.0 * log2(6.0 / 5.0)))
			/ (11.0 * 11.0 + 6.0 * 6.0 + 5.0 * 5.0), PseudoOctave = pow(2.0,
			ScaleStep * 19.0);
	return MeantoneNotes(3, 2).EqualTemper(PseudoOctave).StandardPitch();
}

PitchGamut& PitchGamut::WCGamma() {
	constexpr double ScaleStep = ((20.0 * log2(3.0 / 2.0)
			+ 11.0 * log2(5.0 / 4.0) + 9.0 * log2(6.0 / 5.0)))
			/ (20.0 * 20.0 + 11.0 * 11.0 + 9.0 * 9.0), PseudoOctave = pow(2.0,
			ScaleStep * 34.0);
	return TwotoneNotes(6, 5, 3).EqualTemper(PseudoOctave).StandardPitch();
}

PitchGamut& PitchGamut::TET14() {
	const std::array<double, 7> note_values { { 0, 2, 4, 6, 8, 10, 12 } };
	return ETWestern(14, DoubleVector(note_values.begin(), note_values.end()),
			1);
}

PitchGamut& PitchGamut::TET10() {
	const std::array<double, 7> note_values { { 0, 2, 4, 4, 6, 8, 8 } };
	return ETWestern(10, DoubleVector(note_values.begin(), note_values.end()),
			1);
}

PitchGamut& PitchGamut::Generator(double base, double generator, int step,
		int start, int count) {
	pitches_[start] = base;
	for (int index = 0, rank = start; index < count; index++) {
		rank += step;
		base *= generator;
		if (rank >= pitch_classes_n_) {
			rank -= pitch_classes_n_;
			base /= 2.0;
		}
		if (rank < 0) {
			rank += pitch_classes_n_;
			base *= 2.0;
		}
		pitches_[rank] = base;
	}
	return *this;
}

PitchGamut& PitchGamut::Regular(double generator, int step,
		std::string base_note, double repeat_ratio) {
	repeat_ratio_ = repeat_ratio;
	const int start = PitchIndex(base_note), anticlockwise_count =
			(pitch_classes_n_ - 1) / 2, clockwise_count = pitch_classes_n_ - 1
			- anticlockwise_count;
	Generator(1.0, generator, step, start, clockwise_count);
	Generator(1.0, 1.0 / generator, -step, start, clockwise_count);
	NormalisePitches();
	return *this;
}

bool Coprime(int a, int b) {
	while (true) {
		if (!(a %= b))
			return b == 1;
		if (!(b %= a))
			return a == 1;
	}
	return false; // this never happens but silences the code parser
}

PitchGamut& PitchGamut::GeneralRegular(int pitch_classes,
		DoubleVector note_values, double accidental_size, StringVector names,
		double generator, int step, std::string base_note, double repeat_ratio,
		std::string standard_note, double standard_ratio) {
	GeneralNotes(pitch_classes, note_values, names, accidental_size);
	Regular(generator, step, base_note, repeat_ratio);
	StandardPitch(standard_note, standard_ratio);
	return *this;
}

PitchGamut& PitchGamut::MeantoneRegular(double generator, std::string base_note,
		double repeat_ratio) {
	int step = PitchIndex("g") - PitchIndex("c");
	if (!Coprime(pitch_classes_n_, step))
		throw EGamut().msg(
				"The meantone regular function requires the octave/fifth steps counts to be coprime.");
	Regular(generator, step, base_note, repeat_ratio);
	StandardPitch();
	return *this;
}

PitchGamut& PitchGamut::RegularWestern(int pitch_classes,
		DoubleVector note_values, double accidental_size, double generator,
		int step, std::string base_note) {
	if (note_values.size() != 7)
		throw EGamut().msg(
				"Gamut function requires 7 base notes (internal error).");
	return GeneralRegular(pitch_classes, note_values, accidental_size,
			StringVector(StandardNames.begin(), StandardNames.end()), generator,
			step, base_note, 2.0, "a''''", 1.0);
}

PitchGamut& PitchGamut::General12(DoubleVector pitch_table,
		std::string base_note) {
	MeantoneNotes(2, 1);
	repeat_ratio_ = 2.0;
	pitches_ = pitch_table;
	RotatePitches(PitchIndex(base_note));
	StandardPitch();
	return *this;
}

PitchGamut& PitchGamut::Harmonic12(std::string base_note) {
	const std::array<double, ChromaticNotes> pitch_table { { 1.0, 17.0 / 16.0,
			9.0 / 8.0, 19.0 / 16.0, 5.0 / 4.0, 11.0 / 8.0, 23.0 / 16.0, 3.0
					/ 2.0, 13.0 / 8.0, 27.0 / 16.0, 7.0 / 4.0, 15.0 / 8.0 } };
	return General12(DoubleVector(pitch_table.begin(), pitch_table.end()),
			base_note);
}

PitchGamut& PitchGamut::Ptolemy12(std::string base_note) {
	const std::array<double, ChromaticNotes> pitch_table { { 1.0, 16.0 / 15.0,
			9.0 / 8.0, 6.0 / 5.0, 5.0 / 4.0, 4.0 / 3.0, 45.0 / 32.0, 3.0 / 2.0,
			8.0 / 5.0, 5.0 / 3.0, 16.0 / 9.0, 15.0 / 8.0 } };
	return General12(DoubleVector(pitch_table.begin(), pitch_table.end()),
			base_note);
}

}
