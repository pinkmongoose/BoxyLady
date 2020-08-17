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

#ifndef GLOBAL_H_
#define GLOBAL_H_

#include <cstdlib>
#include <cmath>
#include <climits>
#include <cfloat>
#include <iostream>
#include <type_traits>
#include <bitset>
#include <vector>

namespace BoxyLady {

inline constexpr int PCMMax { 32767 }, PCMMin { -32768 }, PCMRange = PCMMax
		- PCMMin + 1, StereoChannels { 2 }, SingleChannel { 1 },
		MaxChannels { 2 }, Left { 0 }, Right { 1 };

enum class verbosity_type {
	none, errors, messages, verbose
};

namespace screen {
inline constexpr int Width { 80 }, TabWidth = { 8 };
inline std::string Header = "*" + std::string(Width - 2, '-') + "*\n";
inline std::string Tab = std::string(TabWidth, ' ');
}

namespace physics {
inline constexpr double CPitchRatio = pow(2.0, -9.0 / 12.0), // relationship of C to A=440.
CentsPerOctave = 1200.0;
}

class EBaseError: public std::exception {
protected:
	std::string description;
public:
	~EBaseError() throw () {
	}
	virtual EBaseError& msg(std::string s) {
		description = s;
		return *this;
	}
	const char* what() {
		return description.c_str();
	}
};

#define ChildErrorClass(EParent,EChild) class EChild :public EParent {public: EChild& msg(std::string s) {description=s; return *this;};}
ChildErrorClass(EBaseError, ESequence);
ChildErrorClass(ESequence, EClip);
ChildErrorClass(EBaseError, EParser);
ChildErrorClass(EBaseError, EFile);
ChildErrorClass(EBaseError, EBlob);
ChildErrorClass(EParser, EDictionary);
ChildErrorClass(EParser, EUserParse);
ChildErrorClass(EParser, EGamut);
ChildErrorClass(EParser, EArtGamut);
ChildErrorClass(EParser, EBeatGamut);
ChildErrorClass(EParser, ENoteArticulation);
ChildErrorClass(EBaseError, EFileFormat);
ChildErrorClass(EParser, EQuit);
#undef ChildErrorClass

inline std::string BoolToString(bool x) {
	return (x) ? "T" : "F";
}
;

template<typename enumeration>
constexpr auto EnumVal(enumeration value) noexcept {
	return static_cast<std::underlying_type_t<enumeration>>(value);
}

template<typename enumeration>
class Flags {
private:
	typedef std::bitset<EnumVal(enumeration::n)> FlagsType;
	FlagsType flags_;
public:
	bool operator[](enumeration flag) const {
		return flags_[EnumVal(flag)];
	}
	typename FlagsType::reference operator[](enumeration flag) {
		return flags_[EnumVal(flag)];
	}
	Flags(int value = 0) :
			flags_(value) {
	}
	void Clear() {
		flags_ = 0;
	}
	void Set(std::vector<enumeration> array, bool value = true) {
		for (auto item : array)
			flags_[item] = value;
	}
};

class TempFilename { //RAII for temp file names
private:
	std::string file_name_;
public:
	std::string file_name() {
		return file_name_;
	}
	TempFilename(std::string extension) {
		file_name_ = std::string(tmpnam(nullptr)) + "." + extension;
	}
	~TempFilename() {
		try {
			remove(file_name_.c_str());
		} catch (...) {
		};
	}
};

}

#endif /* GLOBAL_H_ */
