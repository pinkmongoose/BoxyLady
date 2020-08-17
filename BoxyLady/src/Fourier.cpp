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

#include "Fourier.h"

#include <utility>
#include <cmath>

namespace BoxyLady {

inline int log2_int(int value) {
		int result=0;
		while (value >>= 1)
			++result;
		return result;
};

inline double Gaussian(double value, double mu, double sigma) {
	const double normalised = (value-mu)/sigma;
	return exp(-0.5*normalised*normalised);
}

void Fourier::Transform(MusicVector &music_data) {
	size_ = music_data.size();
	log_size_ = log2_int(size_);
	rounded_size_ = (1 << (log_size_));
	if (size_ - rounded_size_ >0) {
		rounded_size_*=2;
		log_size_++;
	}
	buffer_=ComplexVector(rounded_size_,Complex(0.0));
	for (int index = 0; index < size_; index++)
		buffer_[index] = music_data[index];
	FFT(false);
}

void Fourier::InverseTransform(MusicVector &music_data) {
	int music_size=music_data.size();
	FFT(true);
	for (int index = 0; (index < music_size) && (index < size_); index++)
		music_data[index] = static_cast<music_type>(std::real(buffer_[index]));
}

void Fourier::FFT(bool inverse) {
	const int size = rounded_size_, log_size=log_size_;
	auto reverse_bits = [log_size](int value) {
		int result = 0;
		for (int bit = 0; bit < log_size; bit++) {
			if (value & (1 << bit))
				result |= 1 << (log_size - 1 - bit);
		}
		return result;
	};
	for (int index = 0; index < size; index++) {
		if (index < reverse_bits(index))
			std::swap(buffer_[index], buffer_[reverse_bits(index)]);
	}
	for (int length = 2; length <= size; length <<= 1) {
		double theta = TwoPi / length * (inverse ? -1.0 : 1.0);
		Complex exp_i_theta(cos(theta), sin(theta));
		for (int block = 0; block < size; block += length) {
			Complex complex_w(1.0);
			for (int block_index = 0; block_index < length / 2; block_index++) {
				Complex complex_u = buffer_[block + block_index], complex_V =
						buffer_[block + block_index + length / 2] * complex_w;
				buffer_[block + block_index] = complex_u + complex_V;
				buffer_[block + block_index + length / 2] = complex_u - complex_V;
				complex_w *= exp_i_theta;
			}
		}
	}
	if (inverse) {
		for (Complex &index : buffer_)
			index /= size;
	}
}

void Fourier::GainFilter(double low_gain, double low_shoulder,
		double high_shoulder, double high_gain, int sample_rate) {
	const double frequency_multiplier = static_cast<double>(rounded_size_)
			/ static_cast<double>(sample_rate);
	for (int index = 0; index < rounded_size_ / 2; index++) {
		double gain;
		const double index_frequency = static_cast<double>(index)
				/ frequency_multiplier;
		if (index_frequency < low_shoulder)
			gain = low_gain;
		else if (index_frequency > high_shoulder)
			gain = high_gain;
		else {
			const double shoulder_fraction = (log(
					index_frequency / low_shoulder))
					/ log(high_shoulder / low_shoulder);
			gain = exp(
					(1.0 - shoulder_fraction) * log(low_gain)
							+ shoulder_fraction * log(high_gain));
		}
		buffer_[index] *= gain;
		buffer_[rounded_size_ - index - 1] *= gain;
	}
}

void Fourier::BandpassFilter(double frequency, double bandwidth,
		double filter_gain, bool comb, int sample_rate) {
	constexpr double eHalf=exp(0.5);
	const double frequency_multiplier = static_cast<double>(rounded_size_)
			/ static_cast<double>(sample_rate), log_gain = log(filter_gain),
			log_frequency = log(frequency), log_bandwidth = bandwidth
					* log(2.0);
	for (int index = 0; index < rounded_size_ / 2; index++) {
		double index_frequency = static_cast<double>(index)
				/ frequency_multiplier;
		if (comb) while (index_frequency > eHalf*frequency) index_frequency-=frequency;
		const double gaussian = Gaussian(
				log(index_frequency), log_frequency, log_bandwidth), gain = exp(
				gaussian * log_gain);
		buffer_[index] *= gain;
		buffer_[rounded_size_ - index - 1] *= gain;
	}
}

void Fourier::Shift(double shift_frequency, int sample_rate) {
	ComplexVector temp = buffer_;
	const double frequency_multiplier = static_cast<double>(rounded_size_)
			/ static_cast<double>(sample_rate);
	const int shift = static_cast<int>(shift_frequency * frequency_multiplier);
	for (int index = 0; index < rounded_size_ / 2; index++) {
		const int shifted_index = index - shift;
		if ((shifted_index >= 0) && (shifted_index < rounded_size_ / 2)) {
			temp[index] = buffer_[shifted_index];
			temp[rounded_size_ - index - 1] = buffer_[rounded_size_
					- shifted_index - 1];
		} else {
			temp[index] = Complex(0.0);
			temp[rounded_size_ - index - 1] = Complex(0.0);
		}
	}
	buffer_ = temp;
}

void Fourier::Scale(double factor) {
	ComplexVector temp = buffer_;
	for (int index = 0; index < rounded_size_ / 2; index++) {
		const double shifted = static_cast<double>(index) * factor, remainder =
				shifted - floor(shifted);
		const int shifted_index = static_cast<int>(shifted);
		if (shifted_index < rounded_size_ / 2) {
			int shifted_high = shifted_index + 1;
			if (shifted_high == rounded_size_ / 2)
				shifted_high--;
			temp[index] = (1.0 - remainder) * buffer_[shifted_index]
					+ remainder * buffer_[shifted_high];
			temp[rounded_size_ - index - 1] = (1.0 - remainder)
					* buffer_[rounded_size_ - shifted_index - 1]
					+ remainder * buffer_[rounded_size_ - shifted_high - 1];
		} else {
			temp[index] = Complex(0.0);
			temp[rounded_size_ - index - 1] = Complex(0.0);
		}
	}
	buffer_ = temp;
}

double Fourier::RMS(int scaling) {
	double sum_sq_gain = 0;
	for (int index = 0; index < rounded_size_; index++) {
		const int frequency =
				(index < rounded_size_ / 2) ? index : rounded_size_ - index - 1;
		const double gain = static_cast<double>(buffer_[index].real());
		double frequency_scale = 1.0;
		switch (scaling) {
		case 1:
			frequency_scale = static_cast<double>(frequency);
			break;
		case 2:
			frequency_scale = static_cast<double>(frequency);
			frequency_scale *= frequency_scale;
			break;
		}
		sum_sq_gain += gain * gain * frequency_scale;
	}
	const double mean_gain = sqrt(
			sum_sq_gain / static_cast<double>(rounded_size_));
	return mean_gain;
}

void Fourier::Clean(double min_gain, int scaling) {
	const double threshold = RMS(scaling) * min_gain;
	for (int index = 0; index < rounded_size_; index++) {
		const int frequency =
				(index < rounded_size_ / 2) ? index : rounded_size_ - index - 1;
		const double gain = abs(buffer_[index].real());
		double frequency_scale = 1.0;
		switch (scaling) {
		case 1:
			frequency_scale = static_cast<double>(frequency);
			break;
		case 2:
			frequency_scale = static_cast<double>(frequency);
			frequency_scale *= frequency_scale;
			break;
		}
		if (gain < threshold)
			buffer_[index] = Complex(0.0);
	}
}

} /* namespace BoxyLady */
