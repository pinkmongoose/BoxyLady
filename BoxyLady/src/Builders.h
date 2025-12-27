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

#ifndef BUILDERS_H_
#define BUILDERS_H_

#include "Stereo.h"
#include "Waveform.h"
#include "Envelope.h"
#include "Blob.h"

namespace BoxyLady {

float_type BuildAmplitude(Blob&);
float_type BuildFrequency(Blob&);
Stereo BuildStereo(Blob&);
Envelope BuildEnvelope(Blob&);
Wave BuildWave(Blob&);
Phaser BuildPhaser(Blob&, size_t = 5);

} //end namespace BoxyLady

#endif /* BUILDERS_H_ */
