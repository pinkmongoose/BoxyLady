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

#ifndef FOURIER_H_
#define FOURIER_H_

#include <complex>
#include <vector>

#include "Global.h"
#include "Waveform.h"

namespace BoxyLady {

typedef std::complex<double> Complex;
typedef std::vector<Complex> ComplexVector;

class Fourier {
private:
	ComplexVector buffer_;
	void FFT(bool);
	int size_, log_size_, rounded_size_;
public:
	Fourier() :
			size_(0), log_size_(0), rounded_size_(0) {
	}
	Fourier(MusicVector &music_data) {
		Transform(music_data);
	}
	void Transform(MusicVector&);
	void InverseTransform(MusicVector&);
	void GainFilter(double, double, double, double, int);
	void BandpassFilter(double, double, double, bool, int);
	void Shift(double, int);
	void Scale(double);
	double RMS(int);
	void Clean(double, int);
};

} /* namespace BoxyLady */

#endif /* FOURIER_H_ */
