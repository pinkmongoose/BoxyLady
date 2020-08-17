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

#include "Sequence.h"

#include <iomanip>
#include <fstream>
#include <sstream>
#include <cstring>

namespace BoxyLady {

Darren::Random Rand;

double Sequence::linear_interpolation = true;

void WriteInfoString(std::ofstream&, std::string, std::string);

MetadataList::MetadataList() {
	const std::vector<std::string> keys = { "title", "artist", "album", "year",
			"track", "genre", "comment" }, RIFF_tags = { "INAM", "IART", "IPRD",
			"ICRD", "IPRT", "IGNR", "ICMT" };
	const int count = keys.size();
	metadata_.resize(count);
	for (int index = 0; index < count; index++) {
		MetadataPoint &item = metadata_[index];
		item.key_ = keys[index];
		item.mp3_tag_ = keys[index];
		item.RIFF_tag_ = RIFF_tags[index];
	}
}

std::string& MetadataList::operator[](std::string key) {
	for (std::size_t index = 0; index < metadata_.size(); index++)
		if (metadata_[index].key_ == key)
			return metadata_[index].value_;
	throw EParser().msg("Metadata key not recognised. [" + key + "]");
}

void MetadataList::Dump() const {
	for (std::size_t index = 0; index < metadata_.size(); index++) {
		MetadataPoint item = metadata_[index];
		if (item.value_.length() > 0)
			std::cout << item.key_ << " = " << item.value_ << "\n";
	}
}

std::string MetadataList::Mp3CommandUpdate(std::string command) const {
	for (std::size_t index = 0; index < metadata_.size(); index++) {
		MetadataPoint item = metadata_[index];
		const std::string what = std::string("%") + item.key_, with =
				item.value_;
		const size_t found = command.find(std::string("%") + item.key_);
		if (found != std::string::npos)
			command.replace(found, what.length(), with);
	}
	return command;
}

void MetadataList::WriteWavInfo(std::ofstream &file) const {
	for (std::size_t index = 0; index < metadata_.size(); index++) {
		MetadataPoint item = metadata_[index];
		WriteInfoString(file, item.RIFF_tag_, item.value_);
	}
}

inline void accumulate(music_type &sample, double source) {
	const int sum = static_cast<int>(static_cast<double>(sample) + source);
	if (sum > PCMMax)
		sample = PCMMax;
	else if (sum < PCMMin)
		sample = PCMMin;
	else
		sample = sum;
}

void Fader::Dump() const {
	std::cout << start_(0, 0) << " " << start_(0, 1) << " " << start_(1, 0)
			<< " " << start_(1, 1) << "\n";
	std::cout << end_(0, 0) << " " << end_(0, 1) << " " << end_(1, 0) << " "
			<< end_(1, 1) << "\n";
	std::cout << linear_ << " " << mirrored_ << "\n";
}

std::string Scratcher::toString() {
	if (!active_)
		return "(off)";
	std::string buffer;
	std::ostringstream stream(buffer, std::istringstream::out);
	stream << "(with=" << name_ << " f=" << amp_ << " bias=" << offset_
			<< " loop=" << BoolToString(loop_) << ")";
	return stream.str();
}

Filter Filter::BalanceFilters(Filter filter_a, Filter filter_b,
		double balance) {
	auto GeoMean = [balance](double a, double b) {
			return exp(log(a) * (1.0 - balance) + log(b) * (balance));
		};
	auto ArithMean = [balance](double a, double b) {
			return a * (1.0 - balance) + b * balance;
		};
	Filter temp(filter_a.type_);
	temp.bandwidth_ = GeoMean(filter_a.bandwidth_, filter_b.bandwidth_);
	temp.frequency_ = GeoMean(filter_a.frequency_, filter_b.frequency_);
	temp.low_gain_ = GeoMean(filter_a.low_gain_, filter_b.low_gain_);
	temp.high_gain_ = GeoMean(filter_a.high_gain_, filter_b.high_gain_);
	temp.low_shoulder_ = GeoMean(filter_a.low_shoulder_, filter_b.low_shoulder_);
	temp.high_shoulder_ = GeoMean(filter_a.high_shoulder_, filter_b.high_shoulder_);
	if (temp.type_ == filter_type::band)
		temp.gain_ = ArithMean(filter_a.gain_, filter_b.gain_);
	else
		temp.gain_ = GeoMean(filter_a.gain_, filter_b.gain_);
	temp.omega_ = GeoMean(filter_a.omega_, filter_b.omega_);
	return temp;
}

//-----------------

void Sequence::Clear() {
	music_data_.clear();
	channels_ = t_samples_ = p_samples_ = m_samples_ = sample_rate_ =
			overlay_position_ = phaser_position_ = envelope_position_ =
					scratcher_position_ = loop_start_samples_ = 0;
	gate_ = true;
	loop_ = false;
	start_anywhere_ = false;
}

void Sequence::CopyType(const Sequence &src) {
	loop_ = src.loop_;
	gate_ = src.gate_;
	start_anywhere_ = src.start_anywhere_;
	loop_start_samples_ = src.loop_start_samples_;
}

void Sequence::AssertMusic() const {
	if (!channels_)
		throw ESequence().msg(
				"Sequence error: can't manipulate an empty sample.");
	if (!sample_rate_)
		throw ESequence().msg("Sequence error: sample rate is 0 Hz!");
}
;

bool Sequence::isSimilar(const Sequence &sequence_a,
		const Sequence &sequence_b) {
	if ((sequence_a.p_samples_ != sequence_b.p_samples_)
			|| (sequence_a.t_samples_ != sequence_b.t_samples_))
		return false;
	else if (sequence_a.sample_rate_ != sequence_b.sample_rate_)
		return false;
	else
		return true;
}

void Sequence::Combine(const Sequence &left, const Sequence &right) {
	Mix(left, right, Stereo(1.0, 0.0), Stereo(0.0, 1.0), 2);
}

void Sequence::Mix(const Sequence &sequence_a, const Sequence &sequence_b,
		Stereo stereo_a, Stereo stereo_b, int mix_channels) {
	sequence_a.AssertMusic();
	sequence_b.AssertMusic();
	if (!isSimilar(sequence_a, sequence_b))
		throw ESequence().msg(
				"Combine: Sources must be same length and sample rate.");
	Sequence mix;
	mix.CreateSilenceSamples(mix_channels, sequence_a.sample_rate_,
			sequence_a.t_samples_, sequence_a.p_samples_);
	mix.CopyType(sequence_a);
	mix.Overlay(sequence_a, 0, INT_MAX, 1.0, OverlayFlags(0), stereo_a);
	mix.Overlay(sequence_b, 0, INT_MAX, 1.0, OverlayFlags(0), stereo_b);
	*this = mix;
}

void Sequence::Split(Sequence &left, Sequence &right) const {
	AssertMusic();
	if (channels_ != 2)
		throw ESequence().msg("Split: Source must be 2-channel.");
	left.Clear();
	right.Clear();
	left.t_samples_ = right.t_samples_ = t_samples_;
	left.m_samples_ = right.m_samples_ = left.p_samples_ = right.p_samples_ =
			p_samples_;
	left.sample_rate_ = right.sample_rate_ = sample_rate_;
	left.loop_start_samples_ = right.loop_start_samples_ = loop_start_samples_;
	left.loop_ = right.loop_ = loop_;
	left.gate_ = right.gate_ = gate_;
	left.channels_ = right.channels_ = 1;
	left.music_data_.resize(left.m_samples_);
	right.music_data_.resize(right.m_samples_);
	for (int index = 0; index < p_samples_; index++) {
		left.music_data_[index] = music_data_[index * 2];
		right.music_data_[index] = music_data_[index * 2 + 1];
	}
}

void Sequence::Rechannel(int new_channels) {
	AssertMusic();
	Sequence temp;
	temp.CreateSilenceSamples(new_channels, sample_rate_, t_samples_,
			p_samples_);
	temp.CopyType(*this);
	temp.Overlay(*this, 0);
	*this = temp;
}

inline void WriteFourBytes(std::ofstream &file, uint32_t data) {
	file.write(reinterpret_cast<char*>(&data), 4);
}

inline void WriteFourByteString(std::ofstream &file, const char *data) {
	file.write(data, 4);
}

inline void WriteTwoBytes(std::ofstream &file, uint16_t data) {
	file.write(reinterpret_cast<char*>(&data), 2);
}

inline void WriteByte(std::ofstream &file, uint8_t data) {
	file.write(reinterpret_cast<char*>(&data), 1);
}

void WriteInfoString(std::ofstream &file, std::string id, std::string value) {
	uint32_t value_length = value.length(), chunk_length = value_length + 1;
	if (chunk_length % 1 == 1)
		chunk_length += 1;
	WriteFourByteString(file, id.c_str());
	WriteFourBytes(file, chunk_length);
	file.write(value.c_str(), value_length);
	file.put(0);
	if (value_length % 2 == 0)
		file.put(0);
}

uint32_t WriteInfoChunk(std::ofstream &file, MetadataList metadata) {
	uint32_t start_pos = file.tellp();
	WriteFourByteString(file, "LIST");
	WriteFourBytes(file, 0);
	WriteFourByteString(file, "INFO");
	metadata.WriteWavInfo(file);
	std::streampos end_pos = file.tellp();
	uint32_t chunk_length = static_cast<int>(end_pos)
			- static_cast<int>(start_pos) - 8;
	file.seekp(start_pos + 4);
	WriteFourBytes(file, chunk_length);
	file.seekp(end_pos);
	return static_cast<int>(end_pos) - static_cast<int>(start_pos);
}

void Sequence::SaveToFile(std::string file_name, file_format format,
		bool write_metadata) const {
	AssertMusic();
	std::string temp_file_name = TempFilename("wav").file_name();
	const uint16_t audio_format = 1, block_align = channels_ * 16 / 8,
			bits_per_sample = 16;
	const uint32_t byte_rate = sample_rate_ * channels_ * 16 / 8, data_size = 2
			* channels_ * p_samples_, fmt_subchunk_size = 16;
	uint32_t chunk_size = 36 + data_size;
	if (chunk_size % 1 > 0)
		chunk_size++;
	if (format == file_format::boxy)
		chunk_size += 24;
	if (write_metadata) {
		std::ofstream temp_file(temp_file_name.c_str(),
				std::ios::out | std::ios::binary);
		const uint32_t metadata_size = WriteInfoChunk(temp_file, metadata_);
		temp_file.close();
		chunk_size += metadata_size;
	}
	std::ofstream file(file_name.c_str(), std::ios::out | std::ios::binary);
	if (file.is_open()) {
		WriteFourByteString(file, "RIFF");
		WriteFourBytes(file, chunk_size);
		WriteFourByteString(file, "WAVE");
		WriteFourByteString(file, "fmt ");
		WriteFourBytes(file, fmt_subchunk_size);
		WriteTwoBytes(file, audio_format);
		WriteTwoBytes(file, channels_);
		WriteFourBytes(file, sample_rate_);
		WriteFourBytes(file, byte_rate);
		WriteTwoBytes(file, block_align);
		WriteTwoBytes(file, bits_per_sample);
		WriteFourByteString(file, "data");
		WriteFourBytes(file, data_size);
		file.write(reinterpret_cast<const char*>(music_data_.data()),
				data_size);
		if (data_size % 1 > 0)
			WriteByte(file, 0);
		if (format == file_format::boxy) {
			WriteFourByteString(file, "boxy");
			WriteFourBytes(file, 32);
			WriteFourBytes(file, gate_);
			WriteFourBytes(file, loop_);
			WriteFourBytes(file, start_anywhere_);
			WriteFourBytes(file, loop_start_samples_);
			WriteFourBytes(file, t_samples_);
			WriteFourBytes(file, 0); //room for expansion
			WriteFourBytes(file, 0);
			WriteFourBytes(file, 0);
		}
		if (write_metadata) {
			std::ifstream temp_file(temp_file_name.c_str(),
					std::ios::in | std::ios::binary);
			file << temp_file.rdbuf();
			temp_file.close();
			try {
				remove(temp_file_name.c_str());
			} catch (...) {
			};
		}
		file.close();
	} else
		throw EFile().msg("Saving file: Open failed.");
}

inline void CheckFileEnd(std::ifstream &file) {
	if (!file)
		throw EFileFormat().msg("File: End of file reached.");
}

inline uint32_t ReadFourBytes(std::ifstream &file) {
	uint32_t data;
	file.read(reinterpret_cast<char*>(&data), 4);
	CheckFileEnd(file);
	return data;
}

inline std::string ReadFourByteString(std::ifstream &file) {
	uint32_t data = ReadFourBytes(file);
	return std::string(reinterpret_cast<char*>(&data), 4);
}

inline uint16_t ReadTwoBytes(std::ifstream &file) {
	uint16_t data;
	file.read(reinterpret_cast<char*>(&data), 2);
	CheckFileEnd(file);
	return data;
}

inline uint8_t ReadByte(std::ifstream &file) {
	uint8_t data;
	file.read(reinterpret_cast<char*>(&data), 1);
	CheckFileEnd(file);
	return data;
}

void Sequence::LoadFromFile(std::string file_name, file_format format) {
	Clear();
	gate_ = false;
	std::ifstream file(file_name.c_str(), std::ios::in | std::ios::binary);
	uint32_t data_size = 0, file_size = 0;
	if (file.is_open()) {
		if (ReadFourByteString(file) != "RIFF")
			throw EFileFormat().msg("File: This is not a RIFF file.");
		file_size = ReadFourBytes(file) + 8;
		if (ReadFourByteString(file) != "WAVE")
			throw EFileFormat().msg("File: This is not a WAV file.");
		if (ReadFourByteString(file) != "fmt ")
			throw EFileFormat().msg("File: Cannot find fmt block.");
		if (ReadFourBytes(file) != 16)
			throw EFileFormat().msg("File: SubChunk1 size not 16 bytes.");
		if (ReadTwoBytes(file) != 1)
			throw EFileFormat().msg("File: Can only process linear PCM files.");
		channels_ = ReadTwoBytes(file);
		sample_rate_ = ReadFourBytes(file);
		ReadFourBytes(file);
		ReadTwoBytes(file);
		if (ReadTwoBytes(file) != 16)
			throw EFileFormat().msg("File: Can only process 16-bit files.");
		while (file.tellg() < file_size) {
			std::string tag = ReadFourByteString(file);
			if (tag == "data") {
				data_size = ReadFourBytes(file);
				music_data_.resize(data_size / 2);
				file.read(reinterpret_cast<char*>(music_data_.data()),
						data_size);
			} else if (tag == "boxy") {
				ReadFourBytes(file);
				gate_ = ReadFourBytes(file);
				loop_ = ReadFourBytes(file);
				start_anywhere_ = ReadFourBytes(file);
				loop_start_samples_ = ReadFourBytes(file);
				t_samples_ = ReadFourBytes(file);
				ReadFourBytes(file);
				ReadFourBytes(file);
				ReadFourBytes(file);
			} else
				file.seekg(ReadFourBytes(file), std::ios::cur); // skip subchunk
			if (file.tellg() % 1)
				ReadByte(file); // 16-bit alignment
		}
		file.close();
		m_samples_ = p_samples_ = data_size / (2 * channels_);
		if (!t_samples_)
			t_samples_ = p_samples_;
	} else
		throw EFile().msg(
				"Opening file [" + file_name
						+ "]: Opening failed. Is it there?");
	if ((channels_ < 1) || (channels_ > MaxChannels)) {
		Clear();
		throw EFile().msg("Only 1 or 2 channels are currently supported.");
	}
}

void Sequence::CreateSilenceSeconds(int channels_val, int sample_rate_val,
		double t_time, double p_time) {
	sample_rate_ = sample_rate_val;
	CreateSilenceSamples(channels_val, sample_rate_val, Samples(t_time),
			Samples(p_time));
}

void Sequence::CreateSilenceSamples(int channels_val, int sample_rate_val,
		int t_samples_val, int p_samples_val) {
	Clear();
	sample_rate_ = sample_rate_val;
	t_samples_ = t_samples_val;
	m_samples_ = p_samples_ = p_samples_val;
	channels_ = channels_val;
	if ((channels_ < 1) || (channels_ > MaxChannels))
		throw ESequence().msg("Only 1 or 2 channels are currently supported.");
	const int size = channels_ * p_samples_;
	music_data_.resize(size);
}

std::pair<int, int> Sequence::WindowPair(Window window) const {
	int start = 0, stop = INT_MAX;
	if (window.hasStart())
		start = Samples(window.start());
	if (window.hasEnd())
		stop = Samples(window.end());
	return std::pair<int, int> { start, stop };
}

void Sequence::WindowFrame(int &start, int &stop) const {
	if (start < 0)
		start = 0;
	else if (start > p_samples_)
		start = p_samples_;
	if (stop < -1)
		stop = 0;
	else if (stop == -1)
		stop = p_samples_;
	else if (stop > p_samples_)
		stop = p_samples_;
	if (stop < start)
		stop = start;
}

void Sequence::Cut(Window window) {
	AssertMusic();
	const auto [start, stop] = WindowPair(window);
	Cut(start, stop);
}

void Sequence::Cut(int start, int stop) {
	WindowFrame(start, stop);
	const int cut_length = stop - start, cut_size = cut_length * channels_;
	MusicVector::iterator start_iterator = music_data_.begin()
			+ start * channels_, end_iterator = start_iterator + cut_size;
	music_data_.erase(start_iterator, end_iterator);
	p_samples_ -= cut_length;
	t_samples_ -= cut_length;
	m_samples_ -= cut_length;
	if (loop_start_samples_ > p_samples_)
		loop_start_samples_ = p_samples_;
}

void Sequence::Paste(const Sequence &source_sequence, Window window) {
	// source_sequence is the source sample
	source_sequence.AssertMusic();
	auto [start, stop] = source_sequence.WindowPair(window);
	source_sequence.WindowFrame(start, stop);
	Clear();
	channels_ = source_sequence.channels_;
	sample_rate_ = source_sequence.sample_rate_;
	m_samples_ = p_samples_ = t_samples_ = stop - start;
	loop_start_samples_ = 0;
	const int size = channels_ * m_samples_, offset = channels_ * start;
	music_data_.resize(size);
	for (int index = 0; index < size; index++)
		music_data_[index] = source_sequence.music_data_[offset + index];
}

void Sequence::Overlay(const Sequence &source_sequence, Window window,
		double pitch_factor, OverlayFlags flags, Stereo stereo, Phaser phaser,
		Envelope envelope, Scratcher scratcher, Wave tremolo) {
	const auto [start, stop] = WindowPair(window);
	Overlay(source_sequence, start, stop, pitch_factor, flags, stereo, phaser,
			envelope, scratcher, tremolo);
}

void Sequence::Overlay(const Sequence &overlay, int start, int stop,
		double pitch_factor, OverlayFlags flags, Stereo stereo, Phaser phaser,
		Envelope envelope, Scratcher scratcher, Wave tremolo) {
	// Check there's something to copy
	AssertMusic();
	if ((stop < start) || (stop < 0) || (start < 0))
		return;
	overlay.AssertMusic();
	// Set up scratcher
	int scratcher_length = 0;
	double scratcher_amp = 0.0, scratcher_offset = 0.0;
	bool scratcher_active = scratcher.active(), scratcher_loop =
			scratcher.loop();
	if (scratcher_active) {
		scratcher.sequence()->AssertMusic();
		if (sample_rate_ != scratcher.sequence()->sample_rate_)
			throw ESequence().msg(
					"Sample overlay: Scratch sample must match sample for sample rate.");
		if (scratcher.sequence()->channels_ != 1)
			throw ESequence().msg(
					"Sample overlay: Scatch sample must be one channel.");
		scratcher_length = scratcher.sequence()->p_samples_;
		scratcher_amp = scratcher.amp();
		scratcher_offset = scratcher.offset();
		if (scratcher_length == 0)
			scratcher_active = false;
	}
	// Prevent infinite sample copying
	const bool source_loop = flags[overlay::loop];
	if ((stop == INT_MAX) && (source_loop))
		throw ESequence().msg("Sample overlay: Reverb on instrument sample?");
	// Set up parameters and variables
	const bool slur_on = flags[overlay::slur_on], slur_off =
			flags[overlay::slur_off];
	const double source_rate = static_cast<double>(overlay.sample_rate_)
			/ static_cast<double>(sample_rate_), amp_left = stereo[Left],
			amp_right = stereo[Right], amp_average = (amp_left + amp_right)
					* 0.5, source_loop_start = overlay.loop_start_samples_;
	double &phaser_freq = phaser.freq(), &phaser_amp = phaser.amp(),
			&tremolo_freq = tremolo.freq(), &tremolo_amp = tremolo.amp();
	const bool phaser_active = (phaser_freq != 0.0), tremolo_active =
			(tremolo_freq != 0.0);
	int bend_stop = start + Samples(phaser.bend_time()), position = start,
			scratcher_position;
	double overlay_position, phaser_position, tremolo_position = 0,
			envelope_position, scratcher_velocity = 1.0;
	// Restore previous position counters, in the case of slurs, or reset
	if (slur_on) {
		overlay_position = overlay_position_;
		phaser_position = phaser_position_;
		envelope_position = envelope_position_;
		scratcher_position = scratcher_position_;
		tremolo_position = tremolo_position_;
	} else {
		if (flags[overlay::random])
			overlay_position = Rand.uniform(overlay.p_samples_);
		else
			overlay_position = 0.0;
		phaser_position = 0.0;
		envelope_position = 0.0;
		scratcher_position = 0.0;
	}
	if (scratcher_position >= scratcher_length)
		scratcher_position = 0;
	double overlay_velocity = pitch_factor * source_rate, phaser_velocity =
			static_cast<double>(phaser_freq)
					/ static_cast<double>(sample_rate_), tremolo_velocity =
			static_cast<double>(tremolo_freq)
					/ static_cast<double>(sample_rate_), bend_rate = pow(
			phaser.bend_factor(), 1.0 / static_cast<double>(Samples(1.0))),
			bend = 1.0;
	// Set up envelope
	envelope.Prepare(sample_rate_);
	if ((stop != INT_MAX) && (flags[overlay::envelope_compress])) {
		if (slur_off)
			envelope.Squish(INT_MAX);
		else
			envelope.Squish(stop - start + envelope_position);
	}
	// Main sample copying loop
	while (position < stop) {
		if (position >= m_samples_) {
			if (flags[overlay::resize]) {
				m_samples_ = (position + 1) * 2;
				music_data_.resize(m_samples_ * channels_);
			} else
				break;
		}
		if (overlay_position >= overlay.p_samples_) {
			if (source_loop)
				overlay_position += source_loop_start - overlay.p_samples_;
			else
				break;
		} else if (overlay_position < 0.0)
			overlay_position += overlay.p_samples_;
		if (phaser_active) {
			phaser_position += phaser_velocity;
			overlay_velocity = pitch_factor * source_rate
					* (CosTurn(phaser_position) * phaser_amp + 1.0);
		}
		if (scratcher_active) {
			scratcher_velocity =
					static_cast<double>(scratcher.sequence()->music_data_[scratcher_position])
							/ static_cast<double>(PCMMax) * scratcher_amp
							+ scratcher_offset;
		}
		if (position == bend_stop)
			bend_rate = 1.0;
		const int overlay_index = static_cast<int>(overlay_position), cend =
				(slur_off) ? INT_MAX : stop - position;
		int next_index = overlay_index + 1;
		if (next_index >= overlay.p_samples_) {
			if (source_loop)
				next_index = source_loop_start;
			else
				next_index = overlay_index;
		}
		const double index_remainder = overlay_position
				- static_cast<double>(overlay_index);
		double amp =
				(gate_) ?
						envelope.Amp(envelope_position, cend) :
						envelope.Amp(envelope_position);
		if (tremolo_active) {
			tremolo_position += tremolo_velocity;
			amp *= CosTurn(tremolo_position) * tremolo_amp + 1.0;
		}
		// Sample copying
		double double_overlay_sample, double_overlay_left, double_overlay_right;
		if (overlay.channels_ == 1) {
			if (linear_interpolation) {
				double_overlay_sample =
						(1.0 - index_remainder)
								* static_cast<double>(overlay.music_data_[overlay_index])
								+ index_remainder
										* static_cast<double>(overlay.music_data_[next_index]);
			} else
				double_overlay_sample =
						static_cast<double>(overlay.music_data_[overlay_index]);
			if (channels_ == 1)
				accumulate(music_data_[position],
						double_overlay_sample * amp * amp_average);
			else {
				accumulate(music_data_[position * 2],
						double_overlay_sample * amp * amp_left);
				accumulate(music_data_[position * 2 + 1],
						double_overlay_sample * amp * amp_right);
			}
		} else {
			if (linear_interpolation) {
				double_overlay_left =
						(1.0 - index_remainder)
								* static_cast<double>(overlay.music_data_[overlay_index
										* 2])
								+ index_remainder
										* static_cast<double>(overlay.music_data_[next_index
												* 2]);
				double_overlay_right =
						(1.0 - index_remainder)
								* static_cast<double>(overlay.music_data_[overlay_index
										* 2 + 1])
								+ index_remainder
										* static_cast<double>(overlay.music_data_[next_index
												* 2 + 1]);
			} else {
				double_overlay_left =
						static_cast<double>(overlay.music_data_[overlay_index
								* 2]);
				double_overlay_right =
						static_cast<double>(overlay.music_data_[overlay_index
								* 2 + 1]);
			}
			if (channels_ == 1)
				accumulate(music_data_[position],
						0.5
								* (double_overlay_left * amp_left
										+ double_overlay_right * amp_right)
								* amp);
			else {
				accumulate(music_data_[position * 2],
						double_overlay_left * amp * amp_left);
				accumulate(music_data_[position * 2 + 1],
						double_overlay_right * amp * amp_right);
			};
		}
		// Updating positions
		position++;
		envelope_position++;
		if (scratcher_active) {
			scratcher_position++;
			if (scratcher_position >= scratcher_length) {
				if (scratcher_loop)
					scratcher_position -= scratcher_length;
				else
					scratcher_active = false;
			}
		}
		bend *= bend_rate;
		overlay_position += overlay_velocity * bend * scratcher_velocity;
	}
	// Store position counters in case of slurring
	if (slur_off) {
		overlay_position_ = overlay_position;
		phaser_position_ = phaser_position;
		tremolo_position_ = tremolo_position;
		envelope_position_ = envelope_position;
		scratcher_position_ = scratcher_position;
	};
	if (flags[overlay::resize]) {
		if (position > p_samples_)
			p_samples_ = position;
	}
}

void Sequence::Resize(int new_t_length, int new_p_length, bool rel) {
	if (rel) {
		new_t_length += t_samples_;
		new_p_length += p_samples_;
	};
	m_samples_ = new_p_length;
	const int new_size = channels_ * m_samples_;
	music_data_.resize(new_size);
	t_samples_ = new_t_length;
	p_samples_ = new_p_length;// Old code here resized t length to p length if p length was shorter. No need.
	if (loop_start_samples_ > p_samples_)
		loop_start_samples_ = p_samples_;
}

void Sequence::AutoResize(double threshold) {
	AssertMusic();
	const music_type int_threshold = music_type(PCMMax_f * threshold);
	int start_position = 0, end_position = 0;
	for (int index = 0; index < p_samples_; index++)
		for (int channel = 0; channel < channels_; channel++) {
			const bool sample = abs(music_data_[index * channels_ + channel])
					> int_threshold;
			if ((!start_position) && sample)
				start_position = index;
			if (sample)
				end_position = index;
		}
	Cut(0, start_position);
	Cut(end_position + 1 - start_position, p_samples_); // first Cut alters the sample making it startt shorter, plength updated automatically
}

void Sequence::CrossFade(Fader fader) {
	AssertMusic();
	if (channels_ == 1) {
		for (int index = 0; index < p_samples_; index++) {
			const double time_rel = static_cast<double>(index)
					/ static_cast<double>(t_samples_), progress =
					(time_rel > 1.0) ? 1.0 : time_rel;
			music_type temp = 0;
			accumulate(temp,
					fader.AmpTime(progress).Amp1(
							static_cast<double>(music_data_[index])));
			music_data_[index] = temp;
		}
	} else if (channels_ == 2) {
		for (int index = 0; index < p_samples_; index++) {
			const double time_rel = static_cast<double>(index)
					/ static_cast<double>(t_samples_), progress =
					(time_rel > 1.0) ? 1.0 : time_rel;
			std::array<music_type, 2> temp { { 0, 0 } };
			std::array<music_type*, 2> samples { { &music_data_[index * 2],
					&music_data_[index * 2 + 1] } };
			Stereo stereo = fader.AmpTime(progress).Amp2(
					Stereo(static_cast<double>(*(samples[0])),
							static_cast<double>(*(samples[1]))));
			for (int channel = 0; channel < 2; channel++) {
				accumulate(temp[channel], stereo[channel]);
				*(samples[channel]) = temp[channel];
			}
		}
	}
}

void Sequence::DelayAmp(const Stereo parallel, const Stereo crossed,
		double delay) {
	AssertMusic();
	Sequence temp = *this;
	CrossFade(Fader::AmpLR(parallel));
	temp.CrossFade(Fader::AmpStereo(Stereo(0, 0), crossed));
	Overlay(temp, Window(delay));
}

void Sequence::Reverse() {
	AssertMusic();
	music_type buffer;
	for (int position = 0; position < p_samples_ / 2; position++)
		for (int channel = 0; channel < channels_; channel++) {
			const int early = position * channels_ + channel, late = (p_samples_
					- 1 - position) * channels_ + channel;
			buffer = music_data_[early];
			music_data_[early] = music_data_[late];
			music_data_[late] = buffer;
		}
}

void Sequence::Repeat(int count, FilterVector filters) {
	AssertMusic();
	Sequence temp = *this;
	temp.Resize(t_samples_ * count, t_samples_ * (count - 1) + p_samples_,
			false);
	temp.MakeSilent();
	for (int index = 0; index < count; index++) {
		temp.Overlay(*this, t_samples_ * index, INT_MAX);
		if (filters.size())
			ApplyFilters(filters);
	}
	*this = temp;
}

void Sequence::Echo(double offset, double amp, int count, FilterVector filters,
		bool resize) {
	AssertMusic();
	Sequence source = *this;
	if (resize)
		Resize(0.0, offset * static_cast<double>(count), true);
	for (int index = 1; index <= count; index++) {
		source.ApplyFilters(filters);
		Overlay(source, Samples(offset * static_cast<double>(index)), INT_MAX,
				1.0, OverlayFlags(0), Stereo(pow(amp, index)));
	}
}

void Sequence::Ring(Sequence *source, Wave wave, bool distortion, double amp,
		double bias) {
	AssertMusic();
	if (source) {
		source->AssertMusic();
		if (source->sample_rate_ != sample_rate_)
			throw ESequence().msg(
					"Ring modulation: Sources must be same sample rate.");
		if (source->channels_ != 1)
			throw ESequence().msg(
					"Ring modulation: Modulating sample must be single channel.");
	}
	const double wave_freq = wave.freq(), wave_amp = wave.amp(), wave_offset =
			wave.offset();
	double wave_cycle = 0, wave_position = 0, value = 0, wave_value = 0,
			factor = 0;
	music_type buffer;
	int source_position = 0;
	for (int position = 0; position < p_samples_; position++) {
		if (source) {
			source_position = position % source->p_samples_;
			music_type &source_sample = source->music_data_[source_position];
			factor = bias + amp * static_cast<double>(source_sample) / PCMMax_f;
		} else {
			wave_cycle = position / static_cast<double>(sample_rate_);
			wave_position = wave_cycle * wave_freq - wave_offset;
			wave_value = CosTurn(wave_position) * wave_amp;
			factor = bias + amp * wave_value;
		}
		for (int channel = 0; channel < channels_; channel++) {
			buffer = 0.0;
			music_type &sample = music_data_[position * channels_ + channel];
			if (distortion) {
				value = static_cast<double>(sample) / PCMMax_f;
				value = (value > 0) ? pow(value, factor) : -pow(-value, factor);
				value *= PCMMax_f;
			} else
				value = factor * static_cast<double>(sample);
			accumulate(buffer, value);
			sample = buffer;
		}
	}
}

inline double cycle(double theta) {
	return theta - floor(theta);
}

inline double SynthPower(double theta, double power) {
	const double theta_rounded = cycle(theta);
	const double value = CosTurn(theta_rounded);
	return (value > 0) ? pow(value, power) : -pow(-value, power);
}

inline double SynthSaw(double theta) {
	const double theta_rounded = cycle(theta);
	return theta_rounded * 2.0 - 1.0;
}

inline double SynthTriangle(double theta) {
	const double theta_rounded = cycle(theta);
	return (theta_rounded < 0.5) ?
			1.0 - 4.0 * theta_rounded : 4.0 * theta_rounded - 3.0;
}

inline double SynthPowerTriangle(double theta, double power) {
	const double theta_rounded = cycle(theta), one_less_theta = 1.0
			- theta_rounded, two_less_power = 2.0 - power;
	return (theta_rounded < (power / 2.0)) ?
			1.0 - 4.0 * theta_rounded / power :
			1.0 - 4.0 * one_less_theta / two_less_power;
}

inline double SynthSquare(double theta) {
	const double theta_rounded = cycle(theta - 0.25);
	return (theta_rounded < 0.5) ? -1.0 : 1.0;
}

inline double SynthPulse(double theta, double power) {
	const double theta_rounded = cycle(theta - 0.25);
	return (theta_rounded < (power / 2.0)) ? -1.0 : 1.0;
}

void Sequence::CrackleNoise(double amp, Stereo stereo) {
	constexpr double MaxLogAmp = 15.0;
	AssertMusic();
	const int count = amp * 0.5 * static_cast<double>(t_samples_)
			/ double(sample_rate_);
	for (int channel = 0; channel < channels_; channel++)
		for (int index = 0; index < count; index++) {
			const int position = Rand.uniform(t_samples_);
			music_type &sample = music_data_[position * channels_ + channel];
			const double log_amp = Rand.uniform(MaxLogAmp), value = pow(2.0,
					log_amp);
			double signed_value = (index % 2) ? value : -value;
			if (channels_ == 2)
				signed_value *= stereo[channel];
			accumulate(sample, signed_value);
		}
}

void Sequence::WhiteNoise(double amp, Stereo stereo) {
	AssertMusic();
	for (int position = 0; position < t_samples_; position++)
		for (int channel = 0; channel < channels_; channel++) {
			double rand = Rand.uniform();
			double value = amp * (2.0 * PCMMax_f * (rand - 0.5));
			if (channels_ == 2)
				value *= stereo[channel];
			accumulate(music_data_[position * channels_ + channel], value);
		}
}

void Sequence::Waveform(const Wave wave, const Phaser phaser,
		const Wave tremolo, double power, synth_type type, Stereo stereo) {
	AssertMusic();
	if (channels_ > 2)
		throw ESequence().msg(
				"Wave synth only currently works with 1- or 2-channel sound.");
	const double &freq = wave.freq(), &amp = wave.amp(), &offset =
			wave.offset(), &phaser_freq = phaser.freq(), &phaser_amp =
			phaser.amp(), stereo_mean_amp = (stereo[0] + stereo[1]) / 2.0;
	double value = 0, wave_theta = offset, phaser_theta = phaser.offset(),
			wave_velocity = freq / static_cast<double>(sample_rate_),
			phaser_velocity = phaser_freq / static_cast<double>(sample_rate_),
			bend_rate = pow(phaser.bend_factor(),
					1.0 / static_cast<double>(t_samples_)), bend = 1.0,
			tremolo_theta = tremolo.offset(), tremolo_velocity = tremolo.freq()
					/ static_cast<double>(sample_rate_), tremolo_amp =
					tremolo.amp();
	for (int position = 0; position < p_samples_; position++) {
		phaser_theta += phaser_velocity;
		bend *= bend_rate;
		const double scaled_phaser_velocity = CosTurn(phaser_theta) * phaser_amp
				+ 1.0;
		wave_theta += wave_velocity * scaled_phaser_velocity * bend;
		if (wave_theta > BigTheta)
			wave_theta -= BigTheta;
		tremolo_theta += tremolo_velocity;
		if (tremolo_theta > BigTheta)
			tremolo_theta -= BigTheta;
		switch (type) {
		case synth_type::constant:
			value = amp * PCMMax_f;
			break;
		case synth_type::sine:
			value = CosTurn(wave_theta) * amp * PCMMax_f;
			break;
		case synth_type::power:
			value = SynthPower(wave_theta,
					(CosTurn(tremolo_theta) * tremolo_amp + 1.0) * power) * amp
					* PCMMax_f;
			break;
		case synth_type::saw:
			value = SynthSaw(wave_theta) * amp * PCMMax_f;
			break;
		case synth_type::triangle:
			value = SynthTriangle(wave_theta) * amp * PCMMax_f;
			break;
		case synth_type::powertriangle:
			value = SynthPowerTriangle(wave_theta,
					(CosTurn(tremolo_theta) * tremolo_amp + 1.0) * power) * amp
					* PCMMax_f;
			break;
		case synth_type::square:
			value = SynthSquare(wave_theta) * amp * PCMMax_f;
			break;
		case synth_type::pulse:
			value = SynthPulse(wave_theta,
					(CosTurn(tremolo_theta) * tremolo_amp + 1.0) * power) * amp
					* PCMMax_f;
			break;
		default:
			break;
		};
		if (channels_ == 2) {
			for (int channel = 0; channel < channels_; channel++)
				accumulate(music_data_[position * channels_ + channel],
						value * stereo[channel]);
		} else
			accumulate(music_data_[position], value * stereo_mean_amp);
	}
}

void Sequence::Smatter(Sequence &source, double frequency, double low_pitch,
		double high_pitch, bool logarithmic_pitch, double low_amp,
		double high_amp, bool logarithmic_amp, double stereo_left,
		double stereo_right) {
	AssertMusic();
	source.AssertMusic();
	if ((stereo_left < -1) || (stereo_right < -1) || (stereo_left > 1)
			|| (stereo_right > 1) || (stereo_left > stereo_right))
		throw ESequence().msg("Incorrect stereo settings for 'smatter'.");
	if (frequency < 0)
		throw ESequence().msg("Frequency must be positive for 'smatter'.");
	const int count = frequency * get_tSeconds();
	for (int index = 0; index < count; index++) {
		const double pitch =
				(logarithmic_pitch) ?
						exp(Rand.uniform(log(low_pitch), log(high_pitch))) :
						Rand.uniform(low_pitch, high_pitch), amp = (
				(logarithmic_amp) ?
						exp(Rand.uniform(log(low_amp), log(high_amp))) :
						Rand.uniform(low_amp, high_amp)) / sqrt(pitch),
				stereo_position = Rand.uniform(stereo_left, stereo_right),
				time = Rand.uniform(get_tSeconds());
		const Stereo stereo = Stereo::Position(stereo_position);
		const Window window(time);
		Overlay(source, window, pitch, OverlayFlags(0), stereo * amp);
	}
}

void Sequence::ApplyEnvelope(Envelope envelope, bool gate) {
	if (!channels_)
		return;
	if (!(envelope.active()))
		return;
	envelope.Prepare(sample_rate_);
	for (int position = 0; position < p_samples_; position++) {
		double amp =
				(gate) ?
						envelope.Amp(position, p_samples_ - 1 - position) :
						envelope.Amp(position);
		for (int channel = 0; channel < channels_; channel++) {
			music_type &sample = music_data_[position * channels_ + channel],
					temp = 0.0;
			accumulate(temp, sample * amp);
			sample = temp;
		}
	}
}

void Sequence::Chorus(int count, double offset_time, const Wave wave) {
	AssertMusic();
	const double amp = 1.0 / static_cast<double>(count + 1);
	Sequence temp = *this;
	for (int index = 0; index < count; index++) {
		const int offset = int(
				static_cast<double>(sample_rate_) * offset_time * index);
		const Phaser phaser((Rand.uniform() + 1.0) * wave.freq(), wave.amp(),
				Rand.uniform(), 1.0);
		temp.Overlay(*this, offset, INT_MAX, 1.0, OverlayFlags(0), Stereo(amp),
				phaser);
	}
	*this = temp;
}

void Sequence::Flange(double freq, double amp) {
	AssertMusic();
	Sequence temp = *this;
	Phaser phaser(freq, amp);
	temp.Amp(0.5);
	Amp(0.5);
	Overlay(temp, 0, INT_MAX, 1.0, OverlayFlags(0), Stereo(), phaser);
}

void Sequence::BitCrusher(int bits) {
	constexpr int MaxBits = 16;
	AssertMusic();
	const int shift = MaxBits - bits;
	for (int position = 0; position < p_samples_; position++)
		for (int channel = 0; channel < channels_; channel++) {
			music_type &sample = music_data_[position * channels_ + channel];
			sample = (sample >> shift) << shift;
		}
}

void Sequence::Abs(double amp) {
	AssertMusic();
	for (int position = 0; position < p_samples_; position++)
		for (int channel = 0; channel < channels_; channel++) {
			music_type &sample = music_data_[position * channels_ + channel],
					temp = 0.0;
			accumulate(temp, abs(sample) * amp);
			sample = temp;
		}
}

void Sequence::Fold(double amp) {
	AssertMusic();
	for (int position = 0; position < p_samples_; position++)
		for (int channel = 0; channel < channels_; channel++) {
			music_type &sample = music_data_[position * channels_ + channel];
			double value = static_cast<double>(sample) * amp / PCMMax_f;
			while ((value > 1) || (value < -1)) {
				if (value > 1)
					value = 2 - value;
				if (value < -1)
					value = -2 - value;
			}
			sample = value * PCMMax_f;
		}
}

void Sequence::Octave(double mix_proportion) {
	AssertMusic();
	for (int channel = 0; channel < channels_; channel++) {
		bool sign = music_data_[channel] > 0, even = false, flip = false;
		for (int position = 1; position < p_samples_; position++) {
			music_type &sample = music_data_[position * channels_ + channel];
			const bool current_sign = sample > 0;
			if (current_sign != sign) {
				sign = current_sign;
				even = !even;
				if (even)
					flip = !flip;
			}
			const double value =
					(flip) ?
							static_cast<double>(sample) :
							-static_cast<double>(sample);
			sample = (1.0 - mix_proportion) * static_cast<double>(sample)
					+ mix_proportion * value;
		}
	}
}

void Sequence::OffsetSeconds(double left_time, double right_time, bool wrap) {
	AssertMusic();
	if (channels_ != 2)
		throw ESequence().msg(
				"Offset only currently works with 2-channel sound.");
	const std::array<int, 2> offsets {
			{ Samples(left_time), Samples(right_time) } };
	const int size = channels_ * p_samples_;
	MusicVector new_data(size);
	for (int position = 0; position < p_samples_; position++)
		for (int channel = 0; channel < channels_; channel++) {
			int new_position = position - offsets[channel];
			if (new_position < 0) {
				if (wrap)
					new_position += p_samples_;
				else
					continue;
			};
			if (new_position >= p_samples_) {
				if (wrap)
					new_position -= p_samples_;
				else
					continue;
			};
			new_data[position * channels_ + channel] = music_data_[new_position
					* channels_ + channel];
		}
	swap(music_data_, new_data);
}

void Sequence::WindowedOverlay(const Sequence &source, Window window) {
	AssertMusic();
	source.AssertMusic();
	if (!isSimilar(*this, source))
		throw ESequence().msg(
				"Windowed overlay: Sources must be same length and sample rate.");
	const auto [start, stop] = WindowPair(window);
	for (int position = start; position < stop; position++)
		for (int channel = 0; channel < channels_; channel++) {
			const int index = position * channels_ + channel;
			const double value = static_cast<double>(source.music_data_[index]);
			accumulate(music_data_[index], value);
		}
}

void Sequence::WindowedFilterLayer(Filter filter, Envelope env,
		Sequence &output, Window window) {
	env.Prepare(sample_rate_);
	Sequence temp = *this;
	temp.ApplyEnvelope(env, false);
	temp.ApplyFilter(filter);
	output.WindowedOverlay(temp, window);
}

void Sequence::WindowedFilter(Filter start_filter, Filter end_filter,
		int window_count) {
	AssertMusic();
	if (start_filter.type() != end_filter.type())
		throw ESequence().msg(
				"Can only blend filters which are the same type.");
	Sequence output = *this;
	output.Amp(0.0);
	const double sample_length = get_pSeconds(), window_length = sample_length
			/ static_cast<double>(window_count);
	Envelope env = Envelope::TriangularWindow(0.0, 0.0, window_length);
	WindowedFilterLayer(start_filter, env, output, Window(0.0, window_length));
	for (int index = 1; index < window_count; index++) {
		const double window_peak = window_length * static_cast<double>(index);
		env = Envelope::TriangularWindow(window_peak - window_length, window_peak,
				window_peak + window_length);
		Filter filter = Filter::BalanceFilters(start_filter, end_filter,
				static_cast<double>(index) / static_cast<double>(window_count));
		WindowedFilterLayer(filter, env, output,
				Window(window_peak - window_length, window_peak + window_length));
	}
	env = Envelope::TriangularWindow(sample_length - window_length, sample_length,
			sample_length);
	WindowedFilterLayer(end_filter, env, output,
			Window(sample_length - window_length, sample_length));
	*this = output;
}

void Sequence::ApplyFilter(Filter filter) {
	AssertMusic();
	const int length = p_samples_;
	if (filter.flags()[filter_direction::wrap])
		Repeat(3);
	if (filter.type() == filter_type::lo) {
		if (filter.flags()[filter_direction::mirror]) {
			Sequence temp = *this;
			temp.Reverse();
			temp.LowPass(filter.omega());
			temp.Reverse();
			temp.Amp(0.5);
			LowPass(filter.omega());
			Amp(0.5);
			Overlay(temp, 0);
		} else
			LowPass(filter.omega());
	} else if (filter.type() == filter_type::hi) {
		if (filter.flags()[filter_direction::mirror]) {
			Sequence temp = *this;
			temp.Reverse();
			temp.HighPass(filter.omega());
			temp.Reverse();
			temp.Amp(0.5);
			HighPass(filter.omega());
			Amp(0.5);
			Overlay(temp, 0);
		} else
			HighPass(filter.omega());
	} else if (filter.type() == filter_type::band)
		BandPass(filter.frequency(), filter.bandwidth(), filter.gain());
	else if (filter.type() == filter_type::amp)
		Amp(filter.gain());
	else if (filter.type() == filter_type::distort)
		Distort(filter.gain());
	else if (filter.type() == filter_type::ks_blend) {
		if (Rand.uniform() > filter.gain())
			Amp(-1.0);
	} else if (filter.type() == filter_type::ks_reverse) {
		if (Rand.uniform() > filter.gain())
			Reverse();
	} else if (filter.type() == filter_type::fourier_gain) {
		FourierGain(filter.low_gain(),filter.low_shoulder(),filter.high_shoulder(),filter.high_gain());
	} else if (filter.type() == filter_type::fourier_bandpass) {
		FourierBandpass(filter.frequency(), filter.bandwidth(), filter.gain(),
				filter.GetFlag(filter_direction::comb));
	}
	if (filter.flags()[filter_direction::wrap]) {
		Cut(0, length);
		Cut(length, length * 2);
	}
}

void Sequence::LowPass(double rRC) {
	AssertMusic();
	const double RC = 1.0 / rRC, dt = 1.0 / static_cast<double>(sample_rate_),
			a = dt / (dt + RC);
	std::array<double, 2> previous = { 0, 0 };
	for (int position = 0; position < p_samples_; position++)
		for (int channel = 0; channel < channels_; channel++) {
			music_type &sample = music_data_[position * channels_ + channel];
			const double value = a * static_cast<double>(sample)
					+ (1.0 - a) * previous[channel];
			sample = value;
			previous[channel] = value;
		}
}

void Sequence::HighPass(double rRC) {
	AssertMusic();
	const double RC = 1.0 / rRC, dt = 1.0 / static_cast<double>(sample_rate_),
			a = RC / (dt + RC);
	std::array<double, 2> previous_value = { 0, 0 };
	std::array<music_type, 2> previous_sample = { 0, 0 };
	for (int position = 0; position < p_samples_; position++)
		for (int channel = 0; channel < channels_; channel++) {
			music_type &sample = music_data_[position * channels_ + channel];
			const double value = a
					* static_cast<double>(sample - previous_sample[channel])
					+ a * previous_value[channel];
			previous_sample[channel] = sample;
			sample = value;
			previous_value[channel] = value;
		}
}

void Sequence::BandPass(double frequency, double bandwidth, double gain) {
	AssertMusic();
	const double A = pow(10.0, gain / 40.0), w0 = TwoPi * frequency
			/ static_cast<double>(sample_rate_), c = cos(w0), s = sin(w0),
			alpha = s * sinh(log(2.0) / 2.0 * bandwidth * w0 / s);
	double b0 = 1.0 + alpha * A, b1 = -2.0 * c, b2 = 1.0 - alpha * A, a0 = 1.0
			+ alpha / A, a1 = -2.0 * c, a2 = 1.0 - alpha / A;
	b0 /= a0;
	b1 /= a0;
	b2 /= a0;
	a1 /= a0;
	a2 /= a0;
	for (int channel = 0; channel < channels_; channel++) {
		double xmem1 = 0, xmem2 = 0, ymem1 = 0, ymem2 = 0;
		for (int position = 0; position < p_samples_; position++) {
			music_type &sample = music_data_[position * channels_ + channel];
			const double x = static_cast<double>(sample) / PCMMax_f;
			const double y = b0 * x + b1 * xmem1 + b2 * xmem2 - a1 * ymem1
					- a2 * ymem2;
			xmem2 = xmem1;
			xmem1 = x;
			ymem2 = ymem1;
			ymem1 = y;
			const int s = int(y * PCMMax_f);
			if (s > PCMMax)
				sample = PCMMax;
			else if (s < PCMMin)
				sample = PCMMin;
			else
				sample = s;
		}
	}
}

void Sequence::FourierGain(double low_gain, double low_shoulder,
		double high_shoulder, double high_gain) {
	AssertMusic();
	if (channels_ == 2) {
		Sequence left, right;
		Split(left, right);
		left.FourierGain(low_gain, low_shoulder, high_shoulder, high_gain);
		right.FourierGain(low_gain, low_shoulder, high_shoulder, high_gain);
		Combine(left, right);
	} else {
		Fourier spectrum(music_data_);
		spectrum.GainFilter(low_gain, low_shoulder, high_shoulder, high_gain,
				sample_rate_);
		spectrum.InverseTransform(music_data_);
	}
}

void Sequence::FourierBandpass(double frequency, double bandwidth, double filter_gain, bool comb) {
	AssertMusic();
	if (channels_ == 2) {
		Sequence left, right;
		Split(left, right);
		left.FourierBandpass(frequency, bandwidth, filter_gain, comb);
		right.FourierBandpass(frequency, bandwidth, filter_gain, comb);
		Combine(left, right);
	} else {
		Fourier spectrum(music_data_);
		spectrum.BandpassFilter(frequency, bandwidth, filter_gain, comb, sample_rate_);
		spectrum.InverseTransform(music_data_);
	}
}

void Sequence::FourierShift(double frequency) {
	AssertMusic();
	if (channels_ == 2) {
		Sequence left, right;
		Split(left, right);
		left.FourierShift(frequency);
		right.FourierShift(frequency);
		Combine(left, right);
	} else {
		Fourier spectrum(music_data_);
		spectrum.Shift(frequency, sample_rate_);
		spectrum.InverseTransform(music_data_);
	}
}

void Sequence::FourierClean(double min_gain) {
	AssertMusic();
	if (channels_ == 2) {
		Sequence left, right;
		Split(left, right);
		left.FourierClean(min_gain);
		right.FourierClean(min_gain);
		Combine(left, right);
	} else {
		Fourier spectrum(music_data_);
		spectrum.Clean(min_gain,1);
		spectrum.InverseTransform(music_data_);
	}
}

void Sequence::Integrate(double factor, double leak_per_second,
		double constant) {
	AssertMusic();
	const double leak_rate = exp(
			log(leak_per_second) / static_cast<double>(sample_rate_)),
			multiplier = TwoPi * factor / static_cast<double>(sample_rate_);
	for (int channel = 0; channel < channels_; channel++) {
		double value = constant;
		for (int position = 0; position < p_samples_; position++) {
			music_type &sample = music_data_[position * channels_ + channel];
			value = leak_rate
					* (value
							+ multiplier * static_cast<double>(sample)
									/ PCMMax_f);
			sample = 0;
			accumulate(sample, value * PCMMax_f);
		}
	}
}

void Sequence::Clip(double min, double max) {
	AssertMusic();
	const music_type int_min = min * PCMMin, int_max = max * PCMMax;
	for (int position = 0; position < p_samples_; position++)
		for (int channel = 0; channel < channels_; channel++) {
			music_type &sample = music_data_[position * channels_ + channel];
			if (sample > int_max)
				sample = int_max;
			else if (sample < -int_min)
				sample = -int_min;
		}
}

void Sequence::Plot() const {
	AssertMusic();
	constexpr int levels = 8, level_height = PCMRange / levels;
	const int bins = screen::Width, bin_width =
			static_cast<int>(static_cast<double>(p_samples_)
					/ static_cast<double>(bins) + 1);
	MusicVector bin_min(bins), bin_max(bins);
	for (int channel = 0; channel < channels_; channel++) {
		std::cout << "Channel " << channel << "\n";
		for (int bin = 0; bin < bins; bin++)
			bin_min[bin] = bin_max[bin] = 0;
		for (int position = 0; position < p_samples_; position++) {
			int bin = static_cast<int>(static_cast<double>(position)
					/ static_cast<double>(bin_width));
			music_type sample = music_data_[position * channels_ + channel];
			if (sample > bin_max[bin])
				bin_max[bin] = sample;
			else if (sample < bin_min[bin])
				bin_min[bin] = sample;
		}
		for (int level = 0; level < levels; level++) {
			for (int bin = 0; bin < bins; bin++) {
				switch (level) {
				case 0:
					if (bin_max[bin] == PCMMax)
						std::cout << "X";
					else
						std::cout
								<< ((bin_max[bin] > level_height * 3) ?
										"@" : " ");
					break;
				case 1:
					std::cout
							<< ((bin_max[bin] > level_height * 2) ? "@" : " ");
					break;
				case 2:
					std::cout << ((bin_max[bin] > level_height) ? "@" : " ");
					break;
				case 3:
					if (bin_max[bin] > 0)
						std::cout << "@";
					else
						std::cout << ((bin_max[bin] == 0) ? "." : " ");
					break;
				case 4:
					if (bin_min[bin] < 0)
						std::cout << "@";
					else
						std::cout << ((bin_min[bin] == 0) ? "." : " ");
					break;
				case 5:
					std::cout << ((bin_min[bin] < -level_height) ? "@" : " ");
					break;
				case 6:
					std::cout
							<< ((bin_min[bin] < -level_height * 2) ? "@" : " ");
					break;
				default:
					if (bin_min[bin] == PCMMin)
						std::cout << "X";
					else
						std::cout
								<< ((bin_min[bin] < level_height * -3) ?
										"@" : " ");
					break;
				}
			}
			std::cout << "\n";
		}
	}
}

void Sequence::Histogram(bool scale, bool plot, double clip) {
	constexpr int steps = 16;
	AssertMusic();
	std::vector<std::array<int, PCMRange>> histogram(channels_), cumulative(
			channels_);
	for (int channel = 0; channel < channels_; channel++)
		for (int position = 0; position < p_samples_; position++)
			histogram[channel][music_data_[position * channels_ + channel]
					- PCMMin]++;
	for (int channel = 0; channel < channels_; channel++)
		for (int position = 0; position < PCMRange; position++)
			cumulative[channel][position] = histogram[channel][position];
	for (int channel = 0; channel < channels_; channel++)
		for (int level = 1; level < PCMRange; level++)
			cumulative[channel][level] += cumulative[channel][level - 1];
	if (plot) {
		std::cout << screen::Header << "\n";
		for (int channel = 0; channel < channels_; channel++) {
			std::cout << "Channel " << channel << "\n";
			for (int level = 0; level < PCMRange; level += PCMRange / steps)
				std::cout
						<< std::string(
								static_cast<int>(static_cast<double>(cumulative[channel][level])
										* static_cast<double>(screen::Width - 10)
										/ static_cast<double>(p_samples_)), '*')
						<< " " << std::fixed << std::setprecision(2)
						<< 100.0
								* static_cast<double>(cumulative[channel][level])
								/ static_cast<double>(p_samples_) << "\n";
			std::cout
					<< std::string(
							static_cast<int>(static_cast<double>(cumulative[channel][PCMRange
									- 1])
									* static_cast<double>(screen::Width - 10)
									/ static_cast<double>(p_samples_)), '*')
					<< " " << std::fixed << std::setprecision(2)
					<< 100.0
							* static_cast<double>(cumulative[channel][PCMRange
									- 1]) / static_cast<double>(p_samples_)
					<< "\n";
			std::cout << screen::Header << "\n";
		}
	}
	if (scale) {
		int max_clip = int(clip * double(p_samples_));
		std::vector<double> level_max(channels_);
		double max_level_max = 0.0;
		for (int channel = 0; channel < channels_; channel++) {
			int clip_high = PCMRange, clip_low = 0;
			for (int level = 0; level < PCMRange / 2; level++) {
				if (cumulative[channel][level] > max_clip)
					break;
				clip_low = level;
			}
			for (int level = PCMRange - 1; level > PCMRange / 2; level--) {
				if (cumulative[channel][level] < p_samples_ - max_clip)
					break;
				clip_high = level;
			}
			const double level_low = static_cast<double>(clip_low + PCMMin)
					/ PCMMax_f, level_high = static_cast<double>(clip_high
					+ PCMMin) / PCMMax_f;
			level_max[channel] = std::max(-level_low, level_high);
			max_level_max = std::max(max_level_max, level_max[channel]);
		}
		Amp(1.0 / max_level_max);
	}
}

music_type Sequence::Mean(int channel) const {
	double sum = 0.0;
	for (int position = 0; position < p_samples_; position++)
		sum += static_cast<double>(music_data_[position * channels_ + channel]);
	return music_type(sum / static_cast<double>(p_samples_));
}

void Sequence::Debias(debias_type type) {
	AssertMusic();
	for (int channel = 0; channel < channels_; channel++) {
		music_type offset = 0;
		switch (type) {
		case debias_type::start:
			offset = music_data_[channel];
			break;
		case debias_type::end:
			offset = music_data_[(p_samples_ - 1) * channels_ + channel];
			break;
		default:
			offset = Mean(channel);
			break;
		}
		for (int position = 0; position < p_samples_; position++)
			accumulate(music_data_[position * channels_ + channel],
					static_cast<double>(-offset));
	}
}

} //end of namespace
