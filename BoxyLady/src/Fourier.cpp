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
#include <cmath>
#include <bit>

#include "Fourier.h"
#include "Envelope.h"

namespace BoxyLady {

inline float_type Gaussian(float_type value) noexcept {
	return exp(-0.5_flt*value*value);
}

inline float_type Gaussian(float_type value, float_type mu, float_type sigma) noexcept {
	const float_type normalised {(value-mu)/sigma};
	return exp(-0.5_flt*normalised*normalised);
}

void Fourier::Transform(MusicVector& music_data) {
	size_ = music_data.size();
	log_size_ = std::bit_width(size_);
	rounded_size_ = std::bit_ceil(size_);
	buffer_ = ComplexVector(rounded_size_,Complex(0.0_flt));
	for (size_t index {0}; index < size_; index++)
		buffer_[index] = music_data[index];
	FFT(false);
}

void Fourier::InverseTransform(MusicVector& music_data) {
	size_t music_size {music_data.size()};
	FFT(true);
	for (size_t index {0}; (index < music_size) && (index < size_); index++)
		music_data[index] = static_cast<music_type>(std::real(buffer_[index]));
}

void Fourier::FFT(bool inverse) {
	const size_t size {rounded_size_}, log_size {log_size_};
	auto reverse_bits = [log_size](size_t value) {
		size_t result = 0;
		for (size_t bit {0}; bit < log_size; bit++) {
			if (value & (1 << bit))
				result |= 1 << (log_size - 1 - bit);
		}
		return result;
	};
	for (size_t index {0}; index < size; index++) {
		if (index < reverse_bits(index))
			std::swap(buffer_[index], buffer_[reverse_bits(index)]);
	}
	for (size_t length {2}; length <= size; length <<= 1) {
		float_type theta {physics::TwoPi / length * (inverse ? -1.0_flt : 1.0_flt)};
		Complex exp_i_theta(cos(theta), sin(theta));
		for (size_t block {0}; block < size; block += length) {
			Complex complex_w {1.0_flt};
			for (size_t block_index {0}; block_index < length / 2; block_index++) {
				Complex complex_u {buffer_[block + block_index]},
					complex_V {buffer_[block + block_index + length / 2] * complex_w};
				buffer_[block + block_index] = complex_u + complex_V;
				buffer_[block + block_index + length / 2] = complex_u - complex_V;
				complex_w *= exp_i_theta;
			}
		}
	}
	if (inverse) {
		for (Complex& index : buffer_) index /= size;
	}
}

void Fourier::GainFilter(float_type low_gain, float_type low_shoulder,
		float_type high_shoulder, float_type high_gain, size_t sample_rate) {
	const float_type frequency_multiplier {static_cast<float_type>(rounded_size_)
		/ static_cast<float_type>(sample_rate)};
	for (size_t index {0}; index < rounded_size_ / 2; index++) {
		float_type gain;
		const float_type index_frequency {static_cast<float_type>(index)
			/ frequency_multiplier};
		if (index_frequency < low_shoulder)
			gain = low_gain;
		else if (index_frequency > high_shoulder)
			gain = high_gain;
		else {
			const float_type shoulder_fraction {(log(
				index_frequency / low_shoulder))
				/ log(high_shoulder / low_shoulder)};
			gain = exp(
				(1.0_flt - shoulder_fraction) * log(low_gain)
				+ shoulder_fraction * log(high_gain));
		}
		buffer_[index] *= gain;
		buffer_[rounded_size_ - index - 1] *= gain;
	}
}

void Fourier::BandpassFilter(float_type frequency, float_type bandwidth,
		float_type filter_gain, bool comb, size_t sample_rate) {
	using BoxyLady::physics::eHalf;
	constexpr float_type log_2 {0.69314718_flt};
	const float_type frequency_multiplier {static_cast<float_type>(rounded_size_)
			/ static_cast<float_type>(sample_rate)},
		log_gain {log(filter_gain)},
		log_frequency {log(frequency)},
		log_bandwidth {bandwidth * log_2};
	for (size_t index {0}; index < rounded_size_ / 2; index++) {
		float_type index_frequency {static_cast<float_type>(index) / frequency_multiplier};
		if (comb) while (index_frequency > eHalf*frequency) index_frequency -= frequency;
		const float_type gaussian {Gaussian(log(index_frequency), log_frequency, log_bandwidth)},
			gain {exp(gaussian * log_gain)};
		buffer_[index] *= gain;
		buffer_[rounded_size_ - index - 1] *= gain;
	}
}

void Fourier::Shift(float_type shift_frequency, size_t sample_rate) {
	ComplexVector temp {buffer_};
	const float_type frequency_multiplier {static_cast<float_type>(rounded_size_)
		/ static_cast<float_type>(sample_rate)};
	const music_pos shift {static_cast<music_pos>(shift_frequency * frequency_multiplier)};
	for (music_pos index {0}; std::cmp_less(index, rounded_size_ / 2); index++) {
		const music_pos shifted_index {index - shift};
		if ((shifted_index >= 0) && (std::cmp_less(shifted_index, rounded_size_ / 2))) {
			temp[index] = buffer_[shifted_index];
			temp[rounded_size_ - index - 1] = buffer_[rounded_size_ - shifted_index - 1];
		} else {
			temp[index] = Complex(0.0_flt);
			temp[rounded_size_ - index - 1] = Complex(0.0_flt);
		}
	}
	buffer_ = temp;
}

void Fourier::Scale(float_type factor) {
	ComplexVector temp {buffer_};
	for (music_pos index {0}; std::cmp_less(index, rounded_size_ / 2); index++) {
		const float_type shifted {static_cast<float_type>(index) / factor},
			remainder {shifted - floor(shifted)};
		const music_pos shifted_index {static_cast<music_pos>(shifted)};
		if (std::cmp_less(shifted_index, rounded_size_ / 2)) {
			music_pos shifted_high {shifted_index + 1};
			if (std::cmp_equal(shifted_high, rounded_size_ / 2))
				shifted_high--;
			temp[index] = (1.0_flt - remainder) * buffer_[shifted_index]
					+ remainder * buffer_[shifted_high];
			temp[rounded_size_ - index - 1] = (1.0_flt - remainder)
					* buffer_[rounded_size_ - shifted_index - 1]
					+ remainder * buffer_[rounded_size_ - shifted_high - 1];
		} else {
			temp[index] = Complex(0.0_flt);
			temp[rounded_size_ - index - 1] = Complex(0.0_flt);
		}
	}
	buffer_ = temp;
}

float_type Fourier::RMS(int scaling) {
	float_type sum_sq_gain {0.0_flt};
	for (size_t index {0}; index < rounded_size_; index++) {
		const size_t frequency {(index < rounded_size_ / 2) ? index : rounded_size_ - index - 1};
		const float_type gain {static_cast<float_type>(std::abs(buffer_[index]))};
		float_type frequency_scale {1.0_flt};
		switch (scaling) {
		case 1:
			frequency_scale = static_cast<float_type>(frequency);
			break;
		case 2:
			frequency_scale = static_cast<float_type>(frequency);
			frequency_scale *= frequency_scale;
			break;
		}
		sum_sq_gain += gain * gain * frequency_scale;
	}
	const float_type mean_gain {sqrt(sum_sq_gain / static_cast<float_type>(rounded_size_))};
	return mean_gain;
}

void Fourier::Clean(float_type min_gain, int scaling, bool pass, bool limit) {
	const float_type threshold {RMS(scaling) * min_gain};
	for (size_t index {0}; index < rounded_size_; index++) {
		const size_t frequency {(index < rounded_size_ / 2) ? index : rounded_size_ - index - 1};
		const float_type gain {std::abs(buffer_[index])};
		float_type frequency_scale {1.0_flt};
		switch (scaling) {
		case 1:
			frequency_scale = static_cast<float_type>(frequency);
			break;
		case 2:
			frequency_scale = static_cast<float_type>(frequency);
			frequency_scale *= frequency_scale;
			break;
		}
		const bool below {gain < threshold},
			cut {pass? !below : below};
		if (cut) buffer_[index] = limit? (buffer_[index] * min_gain) : Complex(0.0_flt);
	}
}

void Fourier::Power(float_type power) {
	constexpr float_type max_frequency {0.5_flt};
	const float_type min_frequency {1.0_flt / static_cast<float_type>(rounded_size_)};
	for (size_t index {0}; index < rounded_size_; index++) {
		const size_t bin {(index < rounded_size_ / 2) ? index : rounded_size_ - index - 1};
		const float_type frequency {static_cast<float_type>(bin) / static_cast<float_type>(rounded_size_)};
		if (power < 0.0_flt) {
			if (frequency == 0.0) continue;
			const float_type frequency_multiplier {min_frequency / frequency};
			buffer_[index] *= pow(frequency_multiplier, -power);
		} else {
			const float_type frequency_multiplier = frequency / max_frequency;
			buffer_[index] *= pow(frequency_multiplier, power);
		}
	}
}

} //end namespace BoxyLady
