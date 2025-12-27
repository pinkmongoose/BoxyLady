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

#ifndef GLOBAL_H_
#define GLOBAL_H_

#define FLT_FLOAT
//#define FLT_DOUBLE

#include <cmath>
#include <limits>
#include <sstream>
#include <iostream>
#include <print>
#include <type_traits>
#include <bitset>
#include <vector>
#include <map>
#include <numbers>
#include <optional>

namespace BoxyLady {

#ifdef FLT_FLOAT
inline constexpr float operator ""_flt(long double value) {
	return static_cast<float>(value);
}
using float_type = float;
inline constexpr float_type exp(float_type val) noexcept {return expf(val);}
inline constexpr float_type log(float_type val) noexcept {return std::logf(val);}
inline constexpr float_type log2(float_type val) noexcept {return std::log2f(val);}
inline constexpr float_type log10(float_type val) noexcept {return std::log10f(val);}
inline constexpr float_type pow(float_type base, float_type exp_val) noexcept {return std::powf(base, exp_val);}
inline constexpr float_type sqrt(float_type val) noexcept {return std::sqrtf(val);}
inline constexpr float_type sin(float_type val) noexcept {return std::sinf(val);}
inline constexpr float_type cos(float_type val) noexcept {return std::cosf(val);}
inline constexpr float_type sinh(float_type val) noexcept {return std::sinhf(val);}
inline constexpr float_type floor(float_type val) noexcept {return std::floorf(val);}
#endif

#ifdef FLT_DOUBLE
inline constexpr double operator ""_flt(long double value) {
	return static_cast<double>(value);
}
using float_type = double;
inline constexpr float_type exp(float_type val) noexcept {return std::exp(val);}
inline constexpr float_type log(float_type val) noexcept {return std::log(val);}
inline constexpr float_type log2(float_type val) noexcept {return std::log2(val);}
inline constexpr float_type log10(float_type val) noexcept {return std::log10(val);}
inline constexpr float_type pow(float_type base, float_type exp_val) noexcept {return std::pow(base, exp_val);}
inline constexpr float_type sqrt(float_type val) noexcept {return std::sqrt(val);}
inline constexpr float_type sin(float_type val) noexcept {return std::sin(val);}
inline constexpr float_type cos(float_type val) noexcept {return std::cos(val);}
inline constexpr float_type sinh(float_type val) noexcept {return std::sinh(val);}
inline constexpr float_type floor(float_type val) noexcept {return std::floor(val);}
#endif

inline constexpr float_type float_type_max = std::numeric_limits<float_type>::max(),
 	float_type_min = std::numeric_limits<float_type>::min();

inline constexpr int int_min = std::numeric_limits<int>::min(),
	int_max = std::numeric_limits<int>::max();
inline constexpr long long int longlong_max = std::numeric_limits<long long int>::max();

template <typename T>
concept Numeric = std::is_arithmetic_v<T>;

template <typename T>
concept FlagEnum = std::is_enum_v<T> && requires{T::n;};

template<FlagEnum T>
inline constexpr auto EnumVal(T value) noexcept {
	return static_cast<std::underlying_type_t<T>>(value);
};

template<FlagEnum T>
class Flags {
private:
	using FlagsType = std::bitset<EnumVal(T::n)>; // n must be after the last member of the scoped enum.
	using FlagsMap = std::map<T, bool>;
	FlagsType flags_;
public:
	void Set(FlagsMap flags_map) {
		for (const auto& item : flags_map)
			flags_.set(EnumVal(item.first), item.second);
	}
	explicit Flags(int value = 0) : flags_{static_cast<FlagsType>(value)} {}
	explicit Flags(FlagsMap flags_map) : flags_{0} {
		Set(flags_map);
	}
	explicit Flags(std::initializer_list<T> list) {
		for (const auto& item : list)
			flags_.set(EnumVal(item), true);
	}
	bool operator[](T flag) const {
		return flags_[EnumVal(flag)];
	}
	typename FlagsType::reference operator[](T flag) {
		return flags_[EnumVal(flag)];
	}
};

enum class error_type {
	error, terminate
};

class EError: public std::exception {
protected:
	std::string description_;
	error_type error_type_;
public:
	explicit EError(std::string description, error_type type = error_type::error)
		:std::exception(), description_{std::move(description)}, error_type_{type} {};
	~EError() noexcept override = default;
	const char* what() const noexcept override {
		return description_.c_str();
	}
	bool is_terminate() const noexcept {
		return error_type_ == error_type::terminate;
	}
};

class Screen {
private:
public:
	enum class escape {
		red, yellow, green, blue, magenta, cyan, white,
		bold, reset,
		bright_red,
		no_auto_wrap, auto_wrap, clear_screen, cursor_home
	};
	enum class print_flag {
		wrap, frame, indent, no_newline, n
	};
	using PrintFlags = Flags<print_flag>;
	static constexpr size_t Width {80}, TabWidth {8};
	static const std::string Separator, SeparatorTop, SeparatorMid, SeparatorBot, SeparatorSub;
	static std::string EscapeString(escape);
	explicit Screen();
	~Screen();
	static std::string StringN(const std::string&, size_t n);
	static std::string Format(const std::initializer_list<escape>&, const std::string&);
	static std::string Prompt(const std::string& text = "$") {
		return Format({{escape::green}}, text);
	};
	static std::string Tab(int pos = TabWidth) {
		return (std::ostringstream{} << CSI << pos << "G").str();
	};
	static void PrintHeader(const std::string&, bool =true);
	static void PrintSeparatorTop() {std::print("{}", SeparatorTop);};
	static void PrintSeparatorMid() {std::print("{}", SeparatorMid);};
	static void PrintSeparatorBot() {std::print("{}", SeparatorBot);};
	static void PrintSeparatorSub() {std::print("{}", SeparatorSub);};
	static void PrintFrame(const std::string&, PrintFlags =PrintFlags({print_flag::frame}));
	static void PrintWrap(const std::string&, PrintFlags =PrintFlags({print_flag::frame, print_flag::wrap}));
	static void PrintError(const std::exception&, std::string = std::string{});
	static void PrintMessage(const std::string&, std::initializer_list<Screen::escape> ={});
	static void PrintInline(const std::string&, std::initializer_list<Screen::escape> ={});
	static void Print(const std::string&);
private:
	static constexpr std::string CSI {"\033["};
	using EscapeSequenceMap = std::map<escape, std::string>;
	static const EscapeSequenceMap EscapeSequences;
};

namespace physics {
inline constexpr float_type CPitchRatio {0.594603558},  // relationship of middle C to A=440
	CentsPerOctave {1200.0}, MillioctavesPerOctave {1000.0}, YuPerOctave {1024.0},
	SavartsPerOctave {300}, MeridesPerOctave {43}, HeptameridesPerOctave {301},
	eHalf {1.6487212707001}; // exponent of 0.5
//	eHalf {ex(0.5)};
} //end namespace physics

inline std::string BoolToString(bool x) {
	return (x) ? "T" : "F";
}

class Glossary {
private:
	std::map<std::string, std::string> map_;
	//#HERE
};

class TempFilename { //RAII for temp file names
private:
	std::string file_name_;
	static int counter;
public:
	std::string file_name();
	explicit TempFilename();
	~TempFilename();
};

template <typename T>
using OptRef = std::optional<std::reference_wrapper<T>>;

template <Numeric T> constexpr int sgn(T val) {
    return (T{0} < val) - (val < T{0});
}

template <std::totally_ordered T> constexpr bool in_range(T val, T lo, T hi) {
	return (val >= lo) && (val <= hi);
}

extern Screen screen;

} //end namespace BoxyLady

#endif /* GLOBAL_H_ */
