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

#ifndef BUILDERS_H_
#define BUILDERS_H_

#include "Stereo.h"
#include "Waveform.h"
#include "Envelope.h"
#include "Blob.h"

namespace BoxyLady {

double BuildAmplitude(Blob&);
double BuildFrequency(Blob&);
Stereo BuildStereo(Blob&);
Envelope BuildEnvelope(Blob&);
Wave BuildWave(Blob&);
Phaser BuildPhaser(Blob&, int = 5);

}

#endif /* BUILDERS_H_ */
