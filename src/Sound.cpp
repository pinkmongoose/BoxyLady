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

#include "Sound.h"

#include <iomanip>
#include <fstream>
#include <sstream>
#include <utility>

namespace BoxyLady {

Darren::Random<float_type> Rand;

float_type Sound::linear_interpolation {true};
MetadataList Sound::default_metadata_;

//-----------------

template<typename T>
inline const char* AsBytes_const(const T& data) noexcept {
    return static_cast<const char*>(static_cast<const void*>(&data));
}

template<typename T>
inline char* AsBytes(T& data) noexcept {
    return static_cast<char*>(static_cast<void*>(&data));
}

inline void WriteFourBytes(std::ofstream& file, uint32_t data) {
	file.write(AsBytes_const(data), 4);
}

inline void WriteFourByteString(std::ofstream& file, const char *data) {
	file.write(data, 4);
}

inline void WriteTwoBytes(std::ofstream& file, uint16_t data) {
	file.write(AsBytes_const(data), 2);
}

inline void WriteByte(std::ofstream& file, uint8_t data) {
	file.write(AsBytes_const(data), 1);
}

void WriteInfoString(std::ofstream& file, std::string id, std::string value) {
	uint32_t value_length {static_cast<uint32_t>(value.length())}, chunk_length {value_length + 1};
	if (chunk_length % 1 == 1)
		chunk_length += 1;
	WriteFourByteString(file, id.c_str());
	WriteFourBytes(file, chunk_length);
	file.write(value.c_str(), value_length);
	file.put(0);
	if (value_length % 2 == 0)
		file.put(0);
}

uint32_t WriteInfoChunk(std::ofstream& file, MetadataList metadata) {
	std::streampos start_pos {file.tellp()};
	WriteFourByteString(file, "LIST");
	WriteFourBytes(file, 0);
	WriteFourByteString(file, "INFO");
	metadata.WriteWavInfo(file);
	std::streampos end_pos {file.tellp()};
	uint32_t chunk_length {static_cast<uint32_t>(end_pos - start_pos - 8)};
	file.seekp(start_pos + std::streamoff(4));
	WriteFourBytes(file, chunk_length);
	file.seekp(end_pos);
	return static_cast<int>(end_pos) - static_cast<int>(start_pos);
}

inline void CheckFileEnd(std::ifstream& file) {
	if (!file) throw EError("File: End of file reached.");
}

inline uint32_t ReadFourBytes(std::ifstream& file) {
	uint32_t data;
	file.read(AsBytes(data), 4);
	CheckFileEnd(file);
	return data;
}

inline std::string ReadFourByteString(std::ifstream& file) {
	uint32_t data {ReadFourBytes(file)};
	return std::string(AsBytes_const(data), 4);
}

inline uint16_t ReadTwoBytes(std::ifstream& file) {
	uint16_t data;
	file.read(AsBytes(data), 2);
	CheckFileEnd(file);
	return data;
}

inline uint8_t ReadByte(std::ifstream& file) {
	uint8_t data;
	file.read(AsBytes(data), 1);
	CheckFileEnd(file);
	return data;
}

//-----------------

MetadataList::MetadataList() {
	static const std::vector<std::tuple<std::string, std::string>> keys { {
		{"title", "INAM"}, {"artist", "IART"}, {"album", "IPRD"},
		{"year", "ICRD"}, {"track", "IPRT"}, {"genre", "IGNR"},
		{"comment", "ICMT"}, {"encoded_by", "ISFT"}
	} };
	for (size_t index {0}; index < keys.size(); index++) {
		std::string key {std::get<0>(keys[index])}, RIFF {std::get<1>(keys[index])};
		metadata_[key] = MetadataPoint {key, RIFF, ""};
	}
	metadata_["encoded_by"].value = "BoxyLady";
}

void MetadataList::EditListItem(std::string key, std::string mp3, std::string RIFF, std::string value) {
	auto& datapoint {metadata_[key]};
	if (mp3.length() > 0) datapoint.mp3_tag = mp3;
	if (RIFF.length() > 0) datapoint.RIFF_tag = RIFF;
	datapoint.value = value;
	if (datapoint.RIFF_tag.length() != 4) datapoint.RIFF_tag = "IXXX";
	if (!datapoint.mp3_tag.length()) datapoint.mp3_tag = "TXXX";
}

std::string& MetadataList::operator[](std::string key) {
	if (metadata_.contains(key))
		return metadata_.at(key).value;
	else throw EError("Metadata key not recognised. [" + key + "]");
}

void MetadataList::Dump(bool show_empty) const {
	screen.PrintHeader("Metadata table");
	for (auto& item : metadata_) {
		if ((item.second.value.length() > 0) || show_empty)
			screen.PrintWrap(std::format("{}: mp3 tag = {} RIFF tag = {} value = {}", item.first, item.second.mp3_tag, item.second.RIFF_tag, item.second.value));
	}
	screen.PrintSeparatorBot();
}

std::string MetadataList::Mp3CommandUpdate(std::string command) const {
	for (auto& item : metadata_) {
		const std::string what {std::string("%") + item.first},
			with {item.second.value};
		size_t found;
		do {
			found = command.find(what);
			if (found != std::string::npos)
				command.replace(found, what.length(), with);
		} while (found != std::string::npos);
	}
	return command;
}

void MetadataList::WriteWavInfo(std::ofstream& file) const {
	for (auto& item : metadata_) WriteInfoString(file, item.second.RIFF_tag, item.second.value);
}

//-----------------

inline void accumulate(music_type& sample, float_type source) noexcept {
	const int sum {static_cast<int>(static_cast<float_type>(sample) + source)};
	sample = static_cast<music_type>(std::clamp(sum, PCMMin, PCMMax));
/*
	if (sum > PCMMax) {
		[[unlikely]];
		sample = PCMMax;
	} else if (sum < PCMMin) {
		[[unlikely]];
		sample = PCMMin;
	} else {
		[[likely]];
		sample = sum;
	}
*/
}

//-----------------

std::string Scratcher::toString() {
	if (!active_) return "(off)";
	std::string buffer;
	std::ostringstream stream(buffer, std::istringstream::out);
	stream << "(with=" << name_ << " f=" << amp_ << " bias=" << offset_ << " loop=" << BoolToString(loop_) << ")";
	return stream.str();
}

//-----------------

Filter Filter::BalanceFilters(Filter filter_a, Filter filter_b, float_type balance) {
	auto GeoMean = [balance](float_type a, float_type b) {
		return exp(log(a) * (1.0 - balance) + log(b) * (balance));
	};
	auto ArithMean = [balance](float_type a, float_type b) {
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

void Sound::Clear() {
	music_data_.clear();
	channels_ = t_samples_ = p_samples_ = m_samples_ = sample_rate_ =
		overlay_position_ = phaser_position_ = envelope_position_ =
		scratcher_position_ = loop_start_samples_ = 0;
	loop_ = false;
	start_anywhere_ = false;
	metadata_ = default_metadata_;
}

void Sound::CopyType(const Sound& src) noexcept {
	loop_ = src.loop_;
	start_anywhere_ = src.start_anywhere_;
	loop_start_samples_ = src.loop_start_samples_;
}

void Sound::AssertMusic() const {
	if (!channels_) throw EError("Sound error: can't manipulate an empty or missing sample.");
	if (!sample_rate_) throw EError("Sound error: sample rate is 0 Hz!");
}

bool Sound::isSimilar(const Sound& sound_a, const Sound& sound_b) noexcept {
	if ((sound_a.p_samples_ != sound_b.p_samples_)
			|| (sound_a.t_samples_ != sound_b.t_samples_)
			|| (sound_a.sample_rate_ != sound_b.sample_rate_))
		return false;
	return true;
}

void Sound::Combine(const Sound& left, const Sound& right) {
	Mix(left, right, Stereo::Left(), Stereo::Right(), 2);
}

void Sound::Mix(const Sound& sound_a, const Sound& sound_b,
		Stereo stereo_a, Stereo stereo_b, int mix_channels) {
	sound_a.AssertMusic();
	sound_b.AssertMusic();
	if (!isSimilar(sound_a, sound_b))
		throw EError("Combine: Sources must be same length and sample rate.");
	Sound mix;
	mix.CreateSilenceSamples(mix_channels, sound_a.sample_rate_,
			sound_a.t_samples_, sound_a.p_samples_);
	mix.CopyType(sound_a);
	mix.DoOverlay(sound_a).stereo(stereo_a)();
	mix.DoOverlay(sound_b).stereo(stereo_b)();
	mix.metadata_ = sound_a.metadata_;
	*this = mix;
}

void Sound::Split(Sound& left, Sound& right) const {
	AssertMusic();
	if (channels_ != 2) throw EError("Split: Source must be 2-channel.");
	left.Clear();
	right.Clear();
	left.t_samples_ = right.t_samples_ = t_samples_;
	left.m_samples_ = right.m_samples_ = left.p_samples_ = right.p_samples_ = p_samples_;
	left.sample_rate_ = right.sample_rate_ = sample_rate_;
	left.loop_start_samples_ = right.loop_start_samples_ = loop_start_samples_;
	left.loop_ = right.loop_ = loop_;
	left.channels_ = right.channels_ = 1;
	left.music_data_.resize(left.m_samples_);
	right.music_data_.resize(right.m_samples_);
	for (size_t index {0}; index < p_samples_; index++) {
		left.music_data_[index] = music_data_[index * 2];
		right.music_data_[index] = music_data_[index * 2 + 1];
	}
	left.metadata_ = metadata_;
}

void Sound::Rechannel(int new_channels) {
	AssertMusic();
	Sound temp;
	temp.CreateSilenceSamples(new_channels, sample_rate_, t_samples_, p_samples_);
	temp.CopyType(*this);
	temp.DoOverlay(*this)();
	temp.metadata_ = metadata_;
	*this = temp;
}

void Sound::SaveToFile(std::string file_name, file_format format, bool write_metadata) const {
	AssertMusic();
	TempFilename unique_filename;
	std::string temp_file_name {unique_filename.file_name()};
	const uint16_t audio_format {1}, block_align {static_cast<uint16_t>(channels_ * 16 / 8)},
			bits_per_sample {16};
	const uint32_t byte_rate {static_cast<uint32_t>(sample_rate_ * channels_ * 16 / 8)},
		data_size {static_cast<uint32_t>(2 * channels_ * p_samples_)}, fmt_subchunk_size {16};
	uint32_t chunk_size {36 + data_size};
	if (chunk_size % 1 > 0)
		chunk_size++;
	if (format == file_format::boxy)
		chunk_size += 24;
	if (write_metadata) {
		std::ofstream temp_file(temp_file_name.c_str(), std::ios::out | std::ios::binary);
		const uint32_t metadata_size {WriteInfoChunk(temp_file, metadata_)};
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
		file.write(static_cast<const char*>(static_cast<const void*>(music_data_.data())), data_size);
		if (data_size % 1 > 0)
			WriteByte(file, 0);
		if (format == file_format::boxy) {
			WriteFourByteString(file, "boxy");
			WriteFourBytes(file, 32);
			WriteFourBytes(file, 0xDEADD0D1); //
			WriteFourBytes(file, loop_);
			WriteFourBytes(file, start_anywhere_);
			WriteFourBytes(file, loop_start_samples_);
			WriteFourBytes(file, t_samples_);
			WriteFourBytes(file, 0); //room for expansion
			WriteFourBytes(file, 0);
			WriteFourBytes(file, 0);
		}
		if (write_metadata) {
			std::ifstream temp_file(temp_file_name.c_str(), std::ios::in | std::ios::binary);
			file << temp_file.rdbuf();
			temp_file.close();
			remove(temp_file_name.c_str());
		}
		file.close();
	} else throw EError("Saving file: Open failed.");
}

void Sound::LoadFromFile(std::string file_name, [[maybe_unused]] file_format format, bool debug) {
	if (debug) screen.PrintMessage(file_name);
	Clear();
	std::ifstream file(file_name.c_str(), std::ios::in | std::ios::binary);
	uint32_t data_size {0}, file_size {0}, bit_width {0};
	if (file.is_open()) {
		if (debug) screen.PrintMessage(file_name + " opened");
		if (ReadFourByteString(file) != "RIFF")
			throw EError("File: This is not a RIFF file.");
		file_size = ReadFourBytes(file) + 8;
		if (ReadFourByteString(file) != "WAVE")
			throw EError("File: This is not a WAV file.");
		if (ReadFourByteString(file) != "fmt ")
			throw EError("File: Cannot find fmt block.");
		if (ReadFourBytes(file) != 16)
			throw EError("File: SubChunk1 size not 16 bytes.");
		if (ReadTwoBytes(file) != 1)
			throw EError("File: Can only process linear PCM files.");
		channels_ = ReadTwoBytes(file);
		sample_rate_ = ReadFourBytes(file);
		ReadFourBytes(file);
		ReadTwoBytes(file);
		bit_width = ReadTwoBytes(file);
		if ((bit_width != 16) && (bit_width != 8))
			throw EError("File: Can only process 8 or 16-bit files.");
		while (file.tellg() < file_size) {
			std::string tag {ReadFourByteString(file)};
			if (debug) {
				screen.PrintMessage("Encountered chunk: "+tag);
			}
			if (tag == "data") {
				data_size = ReadFourBytes(file);
				if (bit_width == 16) {
					music_data_.resize(data_size / 2);
					file.read(static_cast<char*>(static_cast<void*>(music_data_.data())),
						data_size);
				} else {
					music_data_.resize(data_size);
					for (size_t i=0; i<data_size; i++)
						music_data_[i] = (static_cast<music_type>(ReadByte(file)) - 128) << 8;
				}
			} else if (tag == "boxy") {
				ReadFourBytes(file);
				ReadFourBytes(file);	// spare four bytes
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
		m_samples_ = p_samples_ = data_size * 8 / (bit_width * channels_);
		if (!t_samples_)
			t_samples_ = p_samples_;
	} else
		throw EError("Opening file [" + file_name + "]: Opening failed. Is it there?");
	if ((channels_ < 1) || (channels_ > MaxChannels)) {
		Clear();
		throw EError("Only 1 or 2 channels are currently supported.");
	}
}

void Sound::CreateSilenceSeconds(int channels_val, music_size sample_rate_val, float_type t_time, float_type p_time) {
	sample_rate_ = sample_rate_val;
	CreateSilenceSamples(channels_val, sample_rate_val, Samples(t_time), Samples(p_time));
}

void Sound::CreateSilenceSamples(int channels_val, music_size sample_rate_val,
		music_size t_samples_val, music_size p_samples_val) {
	Clear();
	sample_rate_ = sample_rate_val;
	t_samples_ = t_samples_val;
	m_samples_ = p_samples_ = p_samples_val;
	channels_ = channels_val;
	if ((channels_ < 1) || (channels_ > MaxChannels))
		throw EError("Only 1 or 2 channels are currently supported.");
	const music_size size {channels_ * p_samples_};
	music_data_.resize(size);
}

std::pair<music_pos, music_pos> Sound::WindowPair(Window window) const noexcept {
	music_pos start {0}, stop {music_pos_max};
	if (window.hasStart())
		start = Samples(window.start());
	if (window.hasEnd())
		stop = Samples(window.end());
	return std::pair<music_pos, music_pos> { start, stop };
}

void Sound::WindowFrame(music_pos& start, music_pos& stop) const noexcept {
	if (start < 0)
		start = 0;
	else if (std::cmp_greater(start, p_samples_))
		start = p_samples_;
	if (stop < -1)
		stop = 0;
	else if (stop == -1)
		stop = p_samples_;
	else if (std::cmp_greater(stop, p_samples_))
		stop = p_samples_;
	if (stop < start)
		stop = start;
}

void Sound::Cut(Window window) {
	AssertMusic();
	const auto [start, stop] = WindowPair(window);
	Cut(start, stop);
}

void Sound::Cut(music_pos start, music_pos stop) {
	WindowFrame(start, stop);
	const music_size cut_length {static_cast<music_size>(stop - start)}, cut_size {cut_length * channels_};
	MusicVector::iterator start_iterator {music_data_.begin() + start * channels_},
		end_iterator {start_iterator + cut_size};
	music_data_.erase(start_iterator, end_iterator);
	p_samples_ -= cut_length;
	t_samples_ -= cut_length;
	m_samples_ -= cut_length;
	if (loop_start_samples_ > p_samples_)
		loop_start_samples_ = p_samples_;
}

void Sound::Paste(const Sound& source_sound, Window window) {
	source_sound.AssertMusic();
	auto [start, stop] = source_sound.WindowPair(window);
	source_sound.WindowFrame(start, stop);
	Clear();
	channels_ = source_sound.channels_;
	sample_rate_ = source_sound.sample_rate_;
	m_samples_ = p_samples_ = t_samples_ = stop - start;
	loop_start_samples_ = 0;
	const music_size size {channels_ * m_samples_}, offset {static_cast<music_size>(channels_ * start)};
	music_data_.resize(size);
	for (music_size index {0}; index < size; index++)
		music_data_[index] = source_sound.music_data_[offset + index];
}

/*void Sequence::Overlay(const Sequence& source_sequence, Window window,
		float_type pitch_factor, OverlayFlags flags, Stereo stereo, Phaser phaser,
		Envelope envelope, Scratcher scratcher, Wave tremolo) {
	const auto [start, stop] = WindowPair(window);
	Overlay(source_sequence, start, stop, pitch_factor, flags, stereo, phaser, envelope, scratcher, tremolo);
}*/

void Sound::Overlay(const Sound& overlay, music_pos start, music_pos stop,
		float_type pitch_factor, OverlayFlags flags, Stereo stereo, Phaser phaser,
		Envelope envelope, Scratcher scratcher, Wave tremolo, float_type gate_time) {
	// Check there's something to copy
	AssertMusic();
	if ((stop < start) || (stop < 0) || (start < 0))
		return;
	overlay.AssertMusic();
	// Set up scratcher
	music_size scratcher_length {0};
	float_type scratcher_amp {0.0}, scratcher_offset {0.0};
	bool scratcher_active {scratcher.active()},
		scratcher_loop {scratcher.loop()};
	if (scratcher_active) {
		scratcher.sound()->AssertMusic();
		if (sample_rate_ != scratcher.sound()->sample_rate_)
			throw EError("Sample overlay: Scratch sample must match sample for sample rate.");
		if (scratcher.sound()->channels_ != 1)
			throw EError("Sample overlay: Scratch sample must be one channel.");
		scratcher_length = scratcher.sound()->p_samples_;
		scratcher_amp = scratcher.amp();
		scratcher_offset = scratcher.offset();
		if (scratcher_length == 0)
			scratcher_active = false;
	}
	// Prevent infinite sample copying
	const bool source_loop {flags[overlay::loop]};
	if ((stop == music_pos_max) && (source_loop))
		throw EError("Sample overlay: Reverb on instrument sample?");
	// Set up parameters and variables
	const bool slur_on {flags[overlay::slur_on]},
		slur_off {flags[overlay::slur_off]};
	const float_type source_rate {static_cast<float_type>(overlay.sample_rate_)
			/ static_cast<float_type>(sample_rate_)},
		amp_left {stereo[left]}, amp_right {stereo[right]},
		amp_average {(amp_left + amp_right) * 0.5_flt},
		source_loop_start {static_cast<float_type>(overlay.loop_start_samples_)};
	float_type phaser_freq {phaser.freq()}, phaser_amp {phaser.amp()},
		tremolo_freq {tremolo.freq()}, tremolo_amp {tremolo.amp()};
	const bool phaser_active {phaser_freq != 0.0},
		tremolo_active {tremolo_freq != 0.0};
	music_pos bend_stop {start + static_cast<music_pos>(Samples(phaser.bend_time()))},
		position {start}, scratcher_position;
	float_type overlay_position, phaser_position, tremolo_position {0.0},
			envelope_position, scratcher_velocity {1.0};
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
	if (std::cmp_greater_equal(scratcher_position, scratcher_length))
		scratcher_position = 0;
	float_type overlay_velocity {pitch_factor * source_rate},
		phaser_velocity {static_cast<float_type>(phaser_freq)
			/ static_cast<float_type>(sample_rate_)},
		tremolo_velocity {static_cast<float_type>(tremolo_freq)
			/ static_cast<float_type>(sample_rate_)},
		bend_rate {pow(phaser.bend_factor(), 1.0_flt / static_cast<float_type>(Samples(1.0)))},
		bend {1.0};
	// Set up envelope
	const bool gate {flags[overlay::gate]};
	envelope.Prepare(sample_rate_, gate_time);
	if ((stop != music_pos_max) && (flags[overlay::envelope_compress])) {
		if (slur_off)
			envelope.Squish(music_pos_max);
		else
			envelope.Squish(stop - start + envelope_position);
	}
	const music_size env_length {envelope.activeLength()};
	// Main sample copying loop
	while (position < stop) {
		if ((env_length > 0) && (envelope_position > env_length))
			break;
		if (std::cmp_greater_equal(position, m_samples_)) {
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
					* (SinPhi(phaser_position) * phaser_amp + 1.0);
		}
		if (scratcher_active) {
			scratcher_velocity =
					static_cast<float_type>(scratcher.sound()->music_data_[scratcher_position])
							/ static_cast<float_type>(PCMMax) * scratcher_amp
							+ scratcher_offset;
		}
		if (position == bend_stop)
			bend_rate = 1.0;
		const music_pos overlay_index_1 {static_cast<music_pos>(overlay_position)},
			cend {(slur_off) ? music_pos_max : stop - position};
		music_pos overlay_index_2 {overlay_index_1 + 1};
		if (std::cmp_greater_equal(overlay_index_2, overlay.p_samples_)) {
			if (source_loop)
				overlay_index_2 = source_loop_start;
			else
				overlay_index_2 = overlay_index_1;
		}
		const float_type index_remainder {overlay_position - static_cast<float_type>(overlay_index_1)};
		float_type amp {
				(gate) ?
						envelope.Amp(envelope_position, cend) :
						envelope.Amp(envelope_position)};
		if (tremolo_active) {
			tremolo_position += tremolo_velocity;
			amp *= SinPhi(tremolo_position) * tremolo_amp + 1.0;
		}
		// Sample copying
		float_type float_type_overlay_sample, float_type_overlay_left, float_type_overlay_right;
		if (overlay.channels_ == 1) {
			if (linear_interpolation) {
				float_type_overlay_sample =
						(1.0 - index_remainder)
								* static_cast<float_type>(overlay.music_data_[overlay_index_1])
								+ index_remainder
										* static_cast<float_type>(overlay.music_data_[overlay_index_2]);
			} else
				float_type_overlay_sample = static_cast<float_type>(overlay.music_data_[overlay_index_1]);
			if (channels_ == 1)
				accumulate(music_data_[position], float_type_overlay_sample * amp * amp_average);
			else {
				accumulate(music_data_[position * 2], float_type_overlay_sample * amp * amp_left);
				accumulate(music_data_[position * 2 + 1], float_type_overlay_sample * amp * amp_right);
			}
		} else {
			if (linear_interpolation) {
				float_type_overlay_left =
						(1.0 - index_remainder)
								* static_cast<float_type>(overlay.music_data_[overlay_index_1 * 2])
								+ index_remainder
										* static_cast<float_type>(overlay.music_data_[overlay_index_2 * 2]);
				float_type_overlay_right =
						(1.0 - index_remainder)
								* static_cast<float_type>(overlay.music_data_[overlay_index_1 * 2 + 1])
								+ index_remainder
										* static_cast<float_type>(overlay.music_data_[overlay_index_2 * 2 + 1]);
			} else {
				float_type_overlay_left =
						static_cast<float_type>(overlay.music_data_[overlay_index_1 * 2]);
				float_type_overlay_right =
						static_cast<float_type>(overlay.music_data_[overlay_index_1 * 2 + 1]);
			}
			if (channels_ == 1)
				accumulate(music_data_[position], 0.5 * (float_type_overlay_left * amp_left + float_type_overlay_right * amp_right) * amp);
			else {
				accumulate(music_data_[position * 2],
						float_type_overlay_left * amp * amp_left);
				accumulate(music_data_[position * 2 + 1],
						float_type_overlay_right * amp * amp_right);
			};
		}
		// Updating positions
		position++;
		envelope_position++;
		if (scratcher_active) {
			scratcher_position++;
			if (std::cmp_greater_equal(scratcher_position, scratcher_length)) {
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
	if (flags[overlay::trim]) {
		if (std::cmp_greater(p_samples_, position))
			p_samples_ = position;
	}
	if (flags[overlay::resize]) {
		if (std::cmp_greater(position, p_samples_))
			p_samples_ = position;
	}
}

void Sound::Resize(music_size new_t_length, music_size new_p_length, bool rel) {
	if (rel) {
		new_t_length += t_samples_;
		new_p_length += p_samples_;
	};
	m_samples_ = new_p_length;
	const music_size new_size {channels_ * m_samples_};
	music_data_.resize(new_size);
	t_samples_ = new_t_length;
	p_samples_ = new_p_length;
	if (loop_start_samples_ > p_samples_)
		loop_start_samples_ = p_samples_;
}

void Sound::AutoResize(float_type threshold) {
	AssertMusic();
	const music_type int_threshold {music_type(PCMMax_f * threshold)};
	music_size start_position {0}, end_position {0};
	for (music_size index {0}; index < p_samples_; index++)
		for (int channel {0}; channel < channels_; channel++) {
			const bool sample = std::abs(music_data_[index * channels_ + channel]) > int_threshold;
			if ((!start_position) && sample)
				start_position = index;
			if (sample)
				end_position = index;
		}
	Cut(0, start_position);
	Cut(end_position + 1 - start_position, p_samples_); // first Cut alters the sample making it start shorter, plength updated automatically
}

void Sound::CrossFade(CrossFader fader) {
	AssertMusic();
	if (channels_ == 1) {
		for (music_size index {0}; index < p_samples_; index++) {
			const float_type time_rel {static_cast<float_type>(index)
				/ static_cast<float_type>(t_samples_)},
				progress {(time_rel > 1.0_flt) ? 1.0_flt : time_rel};
			music_type temp {0};
			accumulate(temp, fader.AmpTime(progress).Amp1(static_cast<float_type>(music_data_[index])));
			music_data_[index] = temp;
		}
	} else if (channels_ == 2) {
		for (music_size index {0}; index < p_samples_; index++) {
			const float_type time_rel {static_cast<float_type>(index)
				/ static_cast<float_type>(t_samples_)},
				progress {(time_rel > 1.0_flt) ? 1.0_flt : time_rel};
			std::array<music_type, 2> temp { { 0, 0 } };
			std::array<music_type*, 2> samples { { &music_data_[index * 2],
				&music_data_[index * 2 + 1] } };
			Stereo stereo = fader.AmpTime(progress).Amp2(
				Stereo(static_cast<float_type>(*(samples[0])), static_cast<float_type>(*(samples[1]))));
			for (int channel {0}; channel < 2; channel++) {
				accumulate(temp[channel], stereo[channel]);
				*(samples[channel]) = temp[channel];
			}
		}
	}
}

void Sound::DelayAmp(const Stereo parallel, const Stereo crossed, float_type delay) {
	AssertMusic();
	Sound temp {*this};
	CrossFade(CrossFader::AmpStereo(parallel));
	temp.CrossFade(CrossFader::AmpCross(Stereo(0, 0), crossed));
	DoOverlay(temp).window(Window(delay))();
}

void Sound::Reverse() {
	AssertMusic();
	for (music_size position {0}; position < p_samples_ / 2; position++)
		for (int channel {0}; channel < channels_; channel++) {
			const music_size early {position * channels_ + channel},
				late {(p_samples_ - 1 - position) * channels_ + channel};
			std::swap(music_data_[early], music_data_[late]);
		}
}

void Sound::Repeat(int count, FilterVector filters) {
	AssertMusic();
	Sound temp {*this};
	temp.Resize(t_samples_ * count, t_samples_ * (count - 1) + p_samples_,
			false);
	temp.MakeSilent();
	for (music_pos index {0}; index < count; index++) {
		temp.DoOverlay(*this).start(t_samples_ * index)();
		if (filters.size())
			ApplyFilters(filters);
	}
	*this = temp;
}

void Sound::EchoEffect(float_type offset, float_type amp, int count, FilterVector filters, bool resize) {
	AssertMusic();
	Sound source {*this};
	if (resize)
		Resize(0.0, offset * static_cast<float_type>(count), true);
	for (int index {1}; index <= count; index++) {
		source.ApplyFilters(filters);
		DoOverlay(source).start(Samples(offset * static_cast<float_type>(index))).stereo(Stereo{pow(amp, index)})();
	}
}

void Sound::Ring(OptRef<Sound> source_ref, Wave wave, bool distortion, float_type amp, float_type bias) {
	AssertMusic();
	if (source_ref.has_value()) {
		const Sound& source {source_ref->get()};
		source.AssertMusic();
		if (source.sample_rate_ != sample_rate_)
			throw EError("Ring modulation: Sources must be same sample rate.");
		if (source.channels_ != 1)
			throw EError("Ring modulation: Modulating sample must be single channel.");
	}
	const float_type wave_freq {wave.freq()}, wave_amp {wave.amp()}, wave_offset {wave.offset()};
	float_type wave_cycle {0.0}, wave_position {0.0}, value {0.0},
		wave_value {0.0}, factor {0.0};
	music_type buffer;
	music_pos source_position {0};
	for (music_size position {0}; position < p_samples_; position++) {
		if (source_ref.has_value()) {
			const Sound& source {source_ref->get()};
			source_position = position % source.p_samples_;
			const music_type source_sample {source.music_data_[source_position]};
			factor = bias + amp * static_cast<float_type>(source_sample) / PCMMax_f;
		} else {
			wave_cycle = position / static_cast<float_type>(sample_rate_);
			wave_position = wave_cycle * wave_freq - wave_offset;
			wave_value = SinPhi(wave_position) * wave_amp;
			factor = bias + amp * wave_value;
		}
		for (int channel {0}; channel < channels_; channel++) {
			buffer = 0.0;
			music_type& sample {music_data_[position * channels_ + channel]};
			if (distortion) {
				value = static_cast<float_type>(sample) / PCMMax_f;
				value = (value > 0) ? pow(value, factor) : -pow(-value, factor);
				value *= PCMMax_f;
			} else
				value = factor * static_cast<float_type>(sample);
			accumulate(buffer, value);
			sample = buffer;
		}
	}
}

void Sound::CrackleNoise(float_type amp, Stereo stereo) {
	AssertMusic();
	constexpr float_type MaxLogAmp {15.0};
	const float_type mean_stereo {0.5_flt * (stereo[0] + stereo[1])};
	const int count {static_cast<int>(amp * static_cast<float_type>(t_samples_)
		/ float_type(sample_rate_))};
	for (int channel {0}; channel < channels_; channel++)
		for (int index {0}; index < count; index++) {
			const music_pos position {static_cast<music_pos>(Rand.uniform(t_samples_))};
			music_type& sample {music_data_[position * channels_ + channel]};
			const float_type log_amp {Rand.uniform(MaxLogAmp)},
				value {pow(2.0_flt, log_amp)};
			float_type signed_value {(index % 2) ? value : -value};
			if (channels_ == 2) signed_value *= stereo[channel];
			else signed_value *= mean_stereo;
			accumulate(sample, signed_value);
		}
}

void Sound::WhiteNoise(float_type amp, Stereo stereo) {
	AssertMusic();
	for (music_size position {0}; position < t_samples_; position++)
		for (int channel {0}; channel < channels_; channel++) {
			const float_type rand {Rand.uniform()};
			float_type value {amp * (2.0_flt * PCMMax_f * (rand - 0.5_flt))};
			if (channels_ == 2)
				value *= stereo[channel];
			accumulate(music_data_[position * channels_ + channel], value);
		}
}

void Sound::RedNoise(float_type amp, Stereo stereo) {
	AssertMusic();
	Sound temp;
	temp.CreateSilenceSamples(channels_, sample_rate_, t_samples_, p_samples_);
	temp.WhiteNoise(0.1);
	temp.Integrate(1000.0, 0.000001, 0.0);
	temp.HighPass(25.0);
	temp.Debias(debias_type::mean);
	temp.Histogram(true, false, 0.0);
	DoOverlay(temp).stereo(stereo * amp)();
}

void Sound::VelvetNoise(float_type freq, float_type amp, Stereo stereo) {
	AssertMusic();
	const float_type mean_stereo {0.5_flt * (stereo[0] + stereo[1])};
	const int count {static_cast<int>(freq * static_cast<float_type>(t_samples_) / float_type(sample_rate_))};
	const music_size window {t_samples_ / count}; 
	for (int channel {0}; channel < channels_; channel++)
		for (int index {0}; index < count; index++) {
			const music_pos position {static_cast<music_pos>(Rand.uniform(window) + window * index)};
			music_type& sample {music_data_[position * channels_ + channel]};
			float_type signed_value {PCMMax_f * ((Rand.Bernoulli(0.5))? -amp : amp)};
			if (channels_ == 2) signed_value *= stereo[channel];
			else signed_value *= mean_stereo;
			accumulate(sample, signed_value);
		}
}

void Sound::Waveform(const Wave wave, const Phaser phaser,
		const Wave tremolo, float_type power, synth_type type, Stereo stereo) {
	AssertMusic();
	constexpr float_type BigPhi {1.0};
	if (channels_ > 2)
		throw EError("Wave synth only currently works with 1- or 2-channel sound.");
	const float_type& freq {wave.freq()}, &amp {wave.amp()},
		&offset {wave.offset()}, &phaser_freq {phaser.freq()},
		&phaser_amp {phaser.amp()}, stereo_mean_amp {(stereo[0] + stereo[1]) / 2.0_flt};
	float_type value {0}, wave_phi {offset}, phaser_phi {phaser.offset()},
			wave_velocity {freq / static_cast<float_type>(sample_rate_)},
			phaser_velocity {phaser_freq / static_cast<float_type>(sample_rate_)},
			bend_rate {pow(phaser.bend_factor(), 1.0_flt / static_cast<float_type>(Samples(1.0_flt)))},
			bend {1.0},
			tremolo_phi {tremolo.offset()},
			tremolo_velocity {tremolo.freq() / static_cast<float_type>(sample_rate_)},
			tremolo_amp {tremolo.amp()};
	for (music_size position {0}; position < p_samples_; position++) {
		phaser_phi += phaser_velocity;
		bend *= bend_rate;
		const float_type scaled_phaser_velocity {SinPhi(phaser_phi) * phaser_amp + 1.0_flt};
		wave_phi += wave_velocity * scaled_phaser_velocity * bend;
		//wave_phi += wave_velocity;
		while (wave_phi > BigPhi)
			wave_phi -= BigPhi;
		tremolo_phi += tremolo_velocity;
		if (tremolo_phi > BigPhi)
			tremolo_phi -= BigPhi;
		switch (type) {
		case synth_type::constant:
			value = amp * PCMMax_f;
			break;
		case synth_type::sine:
			value = SinPhi(wave_phi) * amp * PCMMax_f;
			break;
		case synth_type::power:
			value = SynthPower(wave_phi,
					(SinPhi(tremolo_phi) * tremolo_amp + 1.0) * power) * amp * PCMMax_f;
			break;
		case synth_type::saw:
			value = SynthSaw(wave_phi) * amp * PCMMax_f;
			break;
		case synth_type::triangle:
			value = SynthTriangle(wave_phi) * amp * PCMMax_f;
			break;
		case synth_type::powertriangle:
			value = SynthPowerTriangle(wave_phi,
					(SinPhi(tremolo_phi) * tremolo_amp + 1.0) * power) * amp * PCMMax_f;
			break;
		case synth_type::square:
			value = SynthSquare(wave_phi) * amp * PCMMax_f;
			break;
		case synth_type::pulse:
			value = SynthPulse(wave_phi,
					(SinPhi(tremolo_phi) * tremolo_amp + 1.0) * power) * amp * PCMMax_f;
			break;
		default:
			break;
		};
		if (channels_ == 2) {
			for (int channel {0}; channel < channels_; channel++)
				accumulate(music_data_[position * channels_ + channel], value * stereo[channel]);
		} else
			accumulate(music_data_[position], value * stereo_mean_amp);
	}
}

void Sound::Smatter(Sound& source, float_type frequency, float_type low_pitch,
		float_type high_pitch, bool logarithmic_pitch, float_type low_amp,
		float_type high_amp, bool logarithmic_amp, float_type stereo_left,
		float_type stereo_right, bool resize, bool regular) {
	AssertMusic();
	source.AssertMusic();
	if ((stereo_left < -1.0) || (stereo_right < -1.0) || (stereo_left > 1.0)
			|| (stereo_right > 1.0) || (stereo_left > stereo_right))
		throw EError("Incorrect stereo settings for 'smatter'.");
	if (frequency < 0.0)
		throw EError("Frequency must be positive for 'smatter'.");
	const int count {static_cast<int>(frequency * get_tSeconds())};
	const music_size old_t_samples {t_samples_};
	OverlayFlags flags {{{overlay::resize, resize}}};
	for (int index {0}; index < count; index++) {
		const float_type pitch {(logarithmic_pitch) ?
				exp(Rand.uniform(log(low_pitch), log(high_pitch))) :
				Rand.uniform(low_pitch, high_pitch)},
			amp {((logarithmic_amp) ?
				exp(Rand.uniform(log(low_amp), log(high_amp))) :
				Rand.uniform(low_amp, high_amp)) / sqrt(pitch)},
			stereo_position {Rand.uniform(stereo_left, stereo_right)},
			time {(regular)?
				get_tSeconds() * index / count :
				Rand.uniform(get_tSeconds())};
		const Stereo stereo {Stereo::Position(stereo_position)};
		const Window window(time);
		DoOverlay(source).window(window).pitch_factor(pitch).flags(flags).stereo(stereo * amp)();
	}
	t_samples_ = old_t_samples;
}

void Sound::ApplyEnvelope(Envelope envelope, bool gate, float_type gate_time) {
	if (!channels_)
		return;
	if (!(envelope.active()))
		return;
	envelope.Prepare(sample_rate_, gate_time);
	for (music_size position {0}; position < p_samples_; position++) {
		float_type amp {(gate) ?
				envelope.Amp(position, p_samples_ - 1 - position) :
				envelope.Amp(position)};
		for (int channel {0}; channel < channels_; channel++) {
			music_type& sample {music_data_[position * channels_ + channel]};
			music_type temp {0};
			accumulate(temp, sample * amp);
			sample = temp;
		}
	}
}

void Sound::Chorus(int count, float_type offset_time, const Wave wave) {
	AssertMusic();
	const float_type amp {1.0_flt / static_cast<float_type>(count + 1)};
	Sound temp {*this};
	for (int index {0}; index < count; index++) {
		const music_pos offset {static_cast<music_pos>((static_cast<float_type>(sample_rate_) * offset_time * index))};
		const Phaser phaser((Rand.uniform() + 1.0) * wave.freq(), wave.amp(), Rand.uniform(), 1.0);
		temp.DoOverlay(*this).start(offset).stereo(Stereo(amp)).phaser(phaser)();
	}
	*this = temp;
}

void Sound::Flange(float_type freq, float_type amp) {
	AssertMusic();
	Sound temp {*this};
	Phaser phaser(freq, amp);
	temp.Amp(0.5);
	Amp(0.5);
	DoOverlay(temp).phaser(phaser)();
}

void Sound::BitCrusher(int bits) {
	constexpr int MaxBits {16};
	AssertMusic();
	const int shift {MaxBits - bits};
	for (music_size position {0}; position < p_samples_; position++)
		for (int channel {0}; channel < channels_; channel++) {
			music_type& sample {music_data_[position * channels_ + channel]};
			sample = (sample >> shift) << shift;
		}
}

void Sound::Abs(float_type amp) {
	AssertMusic();
	for (music_size position {0}; position < p_samples_; position++)
		for (int channel {0}; channel < channels_; channel++) {
			music_type& sample {music_data_[position * channels_ + channel]};
			music_type temp {0};
			accumulate(temp, std::abs(sample) * amp);
			sample = temp;
		}
}

void Sound::Fold(float_type amp) {
	AssertMusic();
	for (music_size position {0}; position < p_samples_; position++)
		for (int channel {0}; channel < channels_; channel++) {
			music_type& sample {music_data_[position * channels_ + channel]};
			float_type value {static_cast<float_type>(sample) * amp / PCMMax_f};
			while ((value > 1.0) || (value < -1.0)) {
				if (value > 1.0)
					value = 2.0 - value;
				if (value < -1.0)
					value = -2.0 - value;
			}
			sample = value * PCMMax_f;
		}
}

void Sound::Octave(float_type mix_proportion) {
	AssertMusic();
	for (int channel {0}; channel < channels_; channel++) {
		bool sign {music_data_[channel] > 0}, even {false}, flip {false};
		for (music_size position {1}; position < p_samples_; position++) {
			music_type& sample {music_data_[position * channels_ + channel]};
			if (const bool current_sign {sample > 0}; current_sign != sign) {
				sign = current_sign;
				even = !even;
				if (even)
					flip = !flip;
			}
			const float_type value {(flip) ?
				static_cast<float_type>(sample) :
				-static_cast<float_type>(sample)};
			sample = (1.0 - mix_proportion) * static_cast<float_type>(sample)
				+ mix_proportion * value;
		}
	}
}

void Sound::OffsetSeconds(float_type left_time, float_type right_time, bool wrap) {
	AssertMusic();
	if (channels_ != 2)
		throw EError("Offset only currently works with 2-channel sound.");
	const std::array<music_pos, 2> offsets { { static_cast<music_pos>(Samples(left_time)), static_cast<music_pos>(Samples(right_time)) } };
	const music_size size {channels_ * p_samples_};
	MusicVector new_data(size);
	const music_pos max_position {static_cast<music_pos>(p_samples_)};
	for (music_pos position {0}; position < max_position; position++)
		for (int channel {0}; channel < channels_; channel++) {
			music_pos new_position {position - offsets[channel]};
			if (new_position < 0) {
				if (wrap)
					new_position += max_position;
				else
					continue;
			};
			if (new_position >= max_position) {
				if (wrap)
					new_position -= max_position;
				else
					continue;
			};
			new_data[position * channels_ + channel] = music_data_[new_position
					* channels_ + channel];
		}
	swap(music_data_, new_data);
}

void Sound::WindowedOverlay(const Sound& source, Window window) {
	AssertMusic();
	source.AssertMusic();
	if (!isSimilar(*this, source))
		throw EError("Windowed overlay: Sources must be same length and sample rate.");
	const auto [start, stop] = WindowPair(window);
	for (music_pos position {start}; position < stop; position++)
		for (int channel {0}; channel < channels_; channel++) {
			const music_pos index {position * channels_ + channel};
			const float_type value {static_cast<float_type>(source.music_data_[index])};
			accumulate(music_data_[index], value);
		}
}

void Sound::WindowedFilterLayer(Filter filter, Envelope env, Sound& output, Window window) {
	env.Prepare(sample_rate_);
	Sound temp {*this};
	temp.ApplyEnvelope(env, false);
	temp.ApplyFilter(filter);
	output.WindowedOverlay(temp, window);
}

void Sound::WindowedFilter(Filter start_filter, Filter end_filter, int window_count) {
	AssertMusic();
	if (start_filter.type() != end_filter.type())
		throw EError("Can only blend filters which are the same type.");
	Sound output {*this};
	output.Amp(0.0);
	const float_type sample_length {get_pSeconds()},
		window_length {sample_length / static_cast<float_type>(window_count)};
	Envelope env {Envelope::TriangularWindow(0.0, 0.0, window_length)};
	WindowedFilterLayer(start_filter, env, output, Window(0.0, window_length));
	for (int index {1}; index < window_count; index++) {
		const float_type window_peak {window_length * static_cast<float_type>(index)};
		env = Envelope::TriangularWindow(window_peak - window_length, window_peak,
			window_peak + window_length);
		Filter filter {Filter::BalanceFilters(start_filter, end_filter,
			static_cast<float_type>(index) / static_cast<float_type>(window_count))};
		WindowedFilterLayer(filter, env, output, Window(window_peak - window_length, window_peak + window_length));
	}
	env = Envelope::TriangularWindow(sample_length - window_length, sample_length, sample_length);
	WindowedFilterLayer(end_filter, env, output, Window(sample_length - window_length, sample_length));
	*this = output;
}

void Sound::ApplyFilter(Filter filter) {
	AssertMusic();
	const music_size length {p_samples_};
	if (filter.flags()[filter_direction::wrap])
		Repeat(3);
	if (filter.type() == filter_type::lo)
		LowPass(filter.omega());
	else if (filter.type() == filter_type::hi)
		HighPass(filter.omega());
	else if (filter.type() == filter_type::band)
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
	} else if (filter.type() == filter_type::narrow_stereo) {
		CrossFade(CrossFader::AmpCross(Stereo(1.0 - 0.5 * filter.gain()), Stereo(0.5 * filter.gain())));
	} else if (filter.type() == filter_type::fourier_gain) {
		FourierGain(filter.low_gain(),filter.low_shoulder(),filter.high_shoulder(),filter.high_gain());
	} else if (filter.type() == filter_type::fourier_bandpass) {
		FourierBandpass(filter.frequency(), filter.bandwidth(), filter.gain(), filter.GetFlag(filter_direction::comb));
	} else if (filter.type() == filter_type::fourier_clean) {
		FourierClean(filter.gain(), false, false);
	} else if (filter.type() == filter_type::fourier_cleanpass) {
		FourierClean(filter.gain(), true, false);
	} else if (filter.type() == filter_type::fourier_limit) {
		FourierClean(filter.gain(), false, true);
	} else if (filter.type() == filter_type::pitch_scale) {
		PitchScale(filter.gain());
	} else if (filter.type() == filter_type::inverse_lr) {
		CrossFade(CrossFader::AmpInverseLR());
	}
	if (filter.flags()[filter_direction::wrap]) {
		Cut(0, length);
		Cut(length, length * 2);
	}
}

void Sound::LowPass(float_type rRC) {
	AssertMusic();
	const float_type RC {1.0_flt / rRC}, dt {1.0_flt / static_cast<float_type>(sample_rate_)},
		a {dt / (dt + RC)};
	std::array<float_type, 2> previous = { 0, 0 };
	for (music_size position {0}; position < p_samples_; position++)
		for (int channel {0}; channel < channels_; channel++) {
			music_type& sample {music_data_[position * channels_ + channel]};
			const float_type value {a * static_cast<float_type>(sample)
				+ (1.0_flt - a) * previous[channel]};
			sample = value;
			previous[channel] = value;
		}
}

void Sound::HighPass(float_type rRC) {
	AssertMusic();
	const float_type RC {1.0_flt / rRC}, dt {1.0_flt / static_cast<float_type>(sample_rate_)},
			a {RC / (dt + RC)};
	std::array<float_type, 2> previous_value { { 0, 0 } };
	std::array<music_type, 2> previous_sample { { 0, 0 } };
	for (music_size position {0}; position < p_samples_; position++)
		for (int channel {0}; channel < channels_; channel++) {
			music_type& sample {music_data_[position * channels_ + channel]};
			const float_type value {a * static_cast<float_type>(sample - previous_sample[channel])
				+ a * previous_value[channel]};
			previous_sample[channel] = sample;
			sample = value;
			previous_value[channel] = value;
		}
}

void Sound::BandPass(float_type frequency, float_type bandwidth, float_type gain) {
	AssertMusic();
	const float_type A {pow(10.0_flt, gain / 40.0_flt)},
		w0 {physics::TwoPi * frequency / static_cast<float_type>(sample_rate_)},
		c {cos(w0)}, s {sin(w0)},
		alpha {s * sinh(log(2.0_flt) / 2.0_flt * bandwidth * w0 / s)};
	float_type b0 {1.0_flt + alpha * A}, b1 {-2.0_flt * c}, b2 {1.0_flt - alpha * A},
		a0 {1.0_flt + alpha / A}, a1 {-2.0_flt * c}, a2 {1.0_flt - alpha / A};
	b0 /= a0;
	b1 /= a0;
	b2 /= a0;
	a1 /= a0;
	a2 /= a0;
	for (int channel {0}; channel < channels_; channel++) {
		float_type xmem1 {0.0}, xmem2 {0.0}, ymem1 {0.0}, ymem2 {0.0};
		for (music_size position {0}; position < p_samples_; position++) {
			music_type& sample {music_data_[position * channels_ + channel]};
			const float_type x {static_cast<float_type>(sample) / PCMMax_f};
			const float_type y {b0 * x + b1 * xmem1 + b2 * xmem2 - a1 * ymem1 - a2 * ymem2};
			xmem2 = xmem1;
			xmem1 = x;
			ymem2 = ymem1;
			ymem1 = y;
			if (const int s {static_cast<int>(y * PCMMax_f)}; s > PCMMax)
				sample = PCMMax;
			else if (s < PCMMin)
				sample = PCMMin;
			else
				sample = s;
		}
	}
}

void Sound::FourierSplit(std::function<void (Fourier&)> Lambda) {
	AssertMusic();
	if (channels_ == 2) {
		Sound left, right;
		Split(left, right);
		left.FourierSplit(Lambda);
		right.FourierSplit(Lambda);
		Combine(left, right);
	} else if (channels_ == 1) {
		Fourier spectrum {music_data_};
		Lambda(spectrum);
		spectrum.InverseTransform(music_data_);
	} else throw EError("Fourier filters only work on 1-2 channels.");
}

void Sound::FourierGain(float_type low_gain, float_type low_shoulder,
		float_type high_shoulder, float_type high_gain) {
	FourierSplit([this, low_gain, low_shoulder, high_shoulder, high_gain] (Fourier& fourier) {
		fourier.GainFilter(low_gain, low_shoulder, high_shoulder, high_gain, sample_rate_);
	});
}

void Sound::FourierBandpass(float_type frequency, float_type bandwidth, float_type filter_gain, bool comb) {
	FourierSplit([this, frequency, bandwidth, filter_gain, comb] (Fourier& fourier) {
		fourier.BandpassFilter(frequency, bandwidth, filter_gain, comb, sample_rate_);
	});
}

void Sound::FourierShift(float_type frequency) {
	FourierSplit([this, frequency] (Fourier& fourier) {
		fourier.Shift(frequency, sample_rate_);
	});
}

void Sound::FourierClean(float_type min_gain, bool pass, bool limit) {
	FourierSplit([this, min_gain, pass, limit] (Fourier& fourier) {
		fourier.Clean(min_gain, 1, pass, limit);
	});
}

void Sound::FourierScale(float_type factor) {
	FourierSplit([this, factor] (Fourier& fourier) {
		fourier.Scale(factor);
	});
}

void Sound::FourierPower(float_type power) {
	FourierSplit([this, power] (Fourier& fourier) {
		fourier.Power(power);
	});
}

void Sound::PitchScale(float_type factor) {
	AssertMusic();
	if (channels_ == 2) {
		Sound left, right;
		Split(left, right);
		left.PitchScale(factor);
		right.PitchScale(factor);
		Combine(left, right);
	} else if (channels_ == 1) {
		MonoPitchScale(factor);
	} else throw EError("Pitch scale only works on 1-2 channels..");
}

inline float_type window_func(float_type x) noexcept {
	constexpr float_type halfpi {1.570796327};
	//if (x > 1.0) x = 2.0 - x;
	//return x;
	x = sin(x*halfpi);
	return x*x;
}

void Sound::MonoPitchScale(float_type factor) {
	const float_type min_f {40.0_flt / factor};
	const music_size window_size {1u << static_cast<int>(floor(log2(static_cast<float_type>(sample_rate_) / min_f) + 1.0))};
	const music_size src_length {(p_samples_/window_size + 2) * window_size},
		num_windows {src_length / window_size};
	//std::println("{} {} {} {} samples", window_size, p_samples_, src_length, num_windows);
	MusicVector src(src_length, 0),
		dest(src_length, 0);
	std::copy(music_data_.begin(), music_data_.end(), src.begin() + window_size);
	for (music_size index {0}; index < num_windows - 1; index ++) {
		//std::println("{} {} index", index, num_windows);
		MusicVector window(src.begin() + index * window_size, src.begin() + (index + 2) * window_size);
		for (music_size i {0}; i < window_size * 2; i++) {
			float_type frac = {window_func(static_cast<float_type>(i)/static_cast<float_type>(window_size))};
			window[i] *= frac;
		}
		Fourier spectrum {window};
		spectrum.Scale(1.0/factor);
		spectrum.InverseTransform(window);
		for (music_size i {0}; i < window_size * 2; i++) {
			//float_type frac = window_func(static_cast<float_type>(i)/static_cast<float_type>(window_size));
			dest[index * window_size + i] += window[i]; // * frac;
		}
	}
	std::copy(dest.begin() + window_size, dest.begin() + window_size + p_samples_, music_data_.begin());
}

void Sound::Integrate(float_type factor, float_type leak_per_second, float_type constant) {
	AssertMusic();
	const float_type leak_rate {exp(log(leak_per_second) / static_cast<float_type>(sample_rate_))},
		multiplier {physics::TwoPi * factor / static_cast<float_type>(sample_rate_)};
	for (int channel {0}; channel < channels_; channel++) {
		float_type value {constant};
		for (music_size position {0}; position < p_samples_; position++) {
			music_type& sample {music_data_[position * channels_ + channel]};
			value = leak_rate * (value + multiplier * static_cast<float_type>(sample) / PCMMax_f);
			sample = 0;
			accumulate(sample, value * PCMMax_f);
		}
	}
}

void Sound::Clip(float_type min, float_type max) {
	AssertMusic();
	const music_type int_min {static_cast<music_type>(min * PCMMin)},
		int_max {static_cast<music_type>(max * PCMMax)};
	for (music_size position {0}; position < p_samples_; position++)
		for (int channel {0}; channel < channels_; channel++) {
			music_type& sample {music_data_[position * channels_ + channel]};
			if (sample > int_max)
				sample = int_max;
			else if (sample < -int_min)
				sample = -int_min;
		}
}

void Sound::CorrelPlot() const {
	AssertMusic();
	if (channels_ != 2)
		throw EError("Offset only currently works with 2-channel sound.");
	const int /*levels {8},*/ bins {screen.Width},
		bin_width {static_cast<int>(static_cast<float_type>(p_samples_)
			/ static_cast<float_type>(bins) + 1)};
	const float_type fn {static_cast<float_type>(bin_width)};
	std::vector<float_type> sx(bins), sy(bins), ux(bins), uy(bins), sxy(bins), r(bins);
	for (int bin {0}; bin < bins; bin++) sx[bin] = sy[bin] = ux[bin] = uy[bin] =
	 sxy[bin] = r[bin] = 0;
	for (music_pos position {0}; std::cmp_less(position, p_samples_); position++) {
		int bin {static_cast<int>(static_cast<float_type>(position) / fn)};
		sx[bin] += static_cast<float_type>(music_data_[position * channels_]);
		sy[bin] += static_cast<float_type>(music_data_[position * channels_ + 1]);
	}
	for (int bin {0}; bin < bins; bin++) {
		ux[bin] = sx[bin] / bin_width;
		sx[bin] = 0;
		uy[bin] = sy[bin] / bin_width;
		sy[bin] = 0;
	}
	for (music_pos position {0}; std::cmp_less(position, p_samples_); position++) {
		int bin {static_cast<int>(static_cast<float_type>(position) / fn)};
		const float_type x {static_cast<float_type>(music_data_[position * channels_])},
			y {static_cast<float_type>(music_data_[position * channels_ + 1])};
		sx[bin] += (x - ux[bin]) * (x - ux[bin]);
		sy[bin] += (y - uy[bin]) * (y - uy[bin]);
		sxy[bin] += (x - ux[bin]) * (y - uy[bin]);
	}
	for (int bin {0}; bin < bins; bin++) {
		r[bin] = sxy[bin] / (sqrt(sx[bin]) * sqrt(sy[bin]));
	}
	screen.PrintHeader("Stereo correlation plot");
	for (int bin {0}; bin < bins; bin++) {
		screen.PrintFrame(std::format("{:8.3f}", r[bin]));
	}
	screen.PrintSeparatorBot();
}

void Sound::Plot() const {
	AssertMusic();
	constexpr int levels {8}, level_height {PCMRange / levels};
	const int bins {screen.Width - 2},
		bin_width {static_cast<int>(static_cast<float_type>(p_samples_)
					/ static_cast<float_type>(bins) + 1)};
	MusicVector bin_min(bins), bin_max(bins);
	for (int channel {0}; channel < channels_; channel++) {
		if (channel > 0) screen.PrintSeparatorSub();
		screen.PrintFrame(std::format("Channel {}", ChannelNames[channel]));
		for (int bin {0}; bin < bins; bin++)
			bin_min[bin] = bin_max[bin] = 0;
		for (music_size position {0}; position < p_samples_; position++) {
			int bin {static_cast<int>(static_cast<float_type>(position)
					/ static_cast<float_type>(bin_width))};
			music_type sample {music_data_[position * channels_ + channel]};
			if (sample > bin_max[bin])
				bin_max[bin] = sample;
			else if (sample < bin_min[bin])
				bin_min[bin] = sample;
		}
		for (int level {0}; level < levels; level++) {
			std::print("│");
			for (int bin {0}; bin < bins; bin++) {
				switch (level) {
				case 0:
					if (bin_max[bin] == PCMMax)
						std::print("{}",screen.Format({Screen::escape::bright_red},"█"));
					else
						std::print("{}",(bin_max[bin] > level_height * 3) ? "█" : "░");
					break;
				case 1:
					std::print("{}",(bin_max[bin] > level_height * 2) ? "█" : "░");
					break;
				case 2:
					std::print("{}", (bin_max[bin] > level_height) ? "█" : "░");
					break;
				case 3:
					if (bin_max[bin] > 0)
						std::print("█");
					else
						std::print("{}", (bin_max[bin] == 0) ? "▒" : "░");
					break;
				case 4:
					if (bin_min[bin] < 0)
						std::print("█");
					else
						std::print("{}", (bin_min[bin] == 0) ? "▒" : "░");
					break;
				case 5:
					std::print("{}", (bin_min[bin] < -level_height) ? "█" : "░");
					break;
				case 6:
					std::print("{}", (bin_min[bin] < -level_height * 2) ? "█" : "░");
					break;
				default:
					if (bin_min[bin] == PCMMin)
						std::print("{}",screen.Format({Screen::escape::bright_red},"█"));
					else
						std::print("{}", (bin_min[bin] < level_height * -3) ? "█" : "░");
					break;
				}
			}
			std::println("│");
		}
	}
}

void Sound::Histogram(bool scale, bool plot, float_type clip) {
	constexpr int steps {16};
	AssertMusic();
	std::vector<std::array<int, PCMRange>> histogram(channels_), cumulative(channels_);
	for (int channel {0}; channel < channels_; channel++)
		for (music_size position {0}; position < p_samples_; position++)
			histogram[channel][music_data_[position * channels_ + channel] - PCMMin]++;
	for (int channel {0}; channel < channels_; channel++)
		for (int position {0}; position < PCMRange; position++)
			cumulative[channel][position] = histogram[channel][position];
	for (int channel {0}; channel < channels_; channel++)
		for (int level {1}; level < PCMRange; level++)
			cumulative[channel][level] += cumulative[channel][level - 1];
	if (plot) {
		for (int channel {0}; channel < channels_; channel++) {
			if (channel > 0) screen.PrintSeparatorSub();
			screen.PrintFrame(std::format("Channel {}", ChannelNames[channel]));
			for (int level {0}; level < PCMRange; level += PCMRange / steps)
				screen.PrintFrame(std::format("{} {:.2f}",
					screen.StringN("█",static_cast<int>(static_cast<float_type>(cumulative[channel][level])
						* static_cast<float_type>(screen.Width - 10)
						/ static_cast<float_type>(p_samples_))),
					100.0 * static_cast<float_type>(cumulative[channel][level])
						/ static_cast<float_type>(p_samples_)
				));
			screen.PrintFrame(std::format("{} {:.2f}",
				screen.StringN("█",static_cast<int>(static_cast<float_type>(cumulative[channel][PCMRange - 1])
					* static_cast<float_type>(screen.Width - 10)
					/ static_cast<float_type>(p_samples_))),
				100.0 * static_cast<float_type>(cumulative[channel][PCMRange - 1])
					/ static_cast<float_type>(p_samples_)
			));
		}
	}
	if (scale) {
		int max_clip {static_cast<int>(clip * float_type(p_samples_))};
		std::vector<float_type> level_max(channels_);
		float_type max_level_max {0.0};
		for (int channel {0}; channel < channels_; channel++) {
			int clip_high {PCMRange}, clip_low {0};
			for (int level {0}; level < PCMRange / 2; level++) {
				if (cumulative[channel][level] > max_clip)
					break;
				clip_low = level;
			}
			for (int level {PCMRange - 1}; level > PCMRange / 2; level--) {
				if (std::cmp_less(cumulative[channel][level], p_samples_ - max_clip))
					break;
				clip_high = level;
			}
			const float_type level_low {static_cast<float_type>(clip_low + PCMMin) / PCMMax_f},
				level_high {static_cast<float_type>(clip_high + PCMMin) / PCMMax_f};
			level_max[channel] = std::max(-level_low, level_high);
			max_level_max = std::max(max_level_max, level_max[channel]);
		}
		Amp(1.0 / max_level_max);
	}
}

music_type Sound::Mean(int channel) const {
	float_type sum {0.0};
	for (music_size position {0}; position < p_samples_; position++)
		sum += static_cast<float_type>(music_data_[position * channels_ + channel]);
	return music_type(sum / static_cast<float_type>(p_samples_));
}

void Sound::Debias(debias_type type) {
	AssertMusic();
	for (int channel {0}; channel < channels_; channel++) {
		music_type offset {0};
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
		for (music_size position {0}; position < p_samples_; position++)
			accumulate(music_data_[position * channels_ + channel], static_cast<float_type>(-offset));
	}
}

} //end namespace BoxyLady
