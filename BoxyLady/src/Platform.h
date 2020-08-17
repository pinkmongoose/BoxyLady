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

#ifndef PLATFORM_H_
#define PLATFORM_H_

namespace Platform {
constexpr auto CompileDate = __DATE__, CompileTime = __TIME__, CompileVersion =
		__VERSION__, LinuxEnv = "HOME", LinuxLib = "/.BoxyLady.box", WinEnv =
		"USERPROFILE", WinLib = "\\\\BoxyLady.box";
}

#endif /* PLATFORM_H_ */
