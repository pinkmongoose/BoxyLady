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

#ifndef FOURIER_H_
#define FOURIER_H_

#include <complex>
#include <vector>

#include "Global.h"
#include "Waveform.h"

namespace BoxyLady {

class Fourier {
private:
	using Complex = std::complex<float_type>;
	using ComplexVector = std::vector<Complex>;
	ComplexVector buffer_;
	void FFT(bool);
	size_t size_ {0}, log_size_ {0}, rounded_size_ {0};
public:
	explicit Fourier() = default;
	explicit Fourier(MusicVector& music_data) {
		Transform(music_data);
	}
	void Transform(MusicVector&);
	void InverseTransform(MusicVector&);
	void GainFilter(float_type, float_type, float_type, float_type, size_t);
	void BandpassFilter(float_type, float_type, float_type, bool, size_t);
	void Shift(float_type, size_t);
	void Scale(float_type);
	float_type RMS(int);
	void Clean(float_type, int, bool, bool);
	void Power(float_type);
};

} //end namespace BoxyLady

#endif /* FOURIER_H_ */
