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

#include <filesystem>

#include "Global.h"

namespace BoxyLady {

const Screen::EscapeSequenceMap Screen::EscapeSequences = EscapeSequenceMap {
		{escape::red, "31m"}, {escape::yellow, "33m"}, {escape::green, "32m"}, {escape::blue, "34m"},
		{escape::magenta, "95m"}, {escape::cyan, "36m"}, {escape::white, "37m"},
		{escape::bold, "1m"}, {escape::reset, "0m"},
		{escape::bright_red, "91m"},
		{escape::no_auto_wrap, "?7l"}, {escape::auto_wrap, "?7h"}, {escape::clear_screen, "2J"}, {escape::cursor_home, "H"}
};

const std::string Screen::Separator = StringN("═", Width) + "\n",
	Screen::SeparatorTop = "╒" + StringN("═", Width - 2) + "╕\n",
	Screen::SeparatorMid = "╞" + StringN("═", Width - 2) + "╡\n",
	Screen::SeparatorSub = "├" + StringN("─", Width - 2) + "┤\n",
	Screen::SeparatorBot = "╘" + StringN("═", Width - 2) + "╛\n";

std::string Screen::EscapeString(escape code) {
	std::ostringstream stream;
	stream << CSI << EscapeSequences.at(code);
	return stream.str();
};

Screen::Screen() {
	std::println("{}", EscapeString(escape::no_auto_wrap));
	std::println("{}", EscapeString(escape::clear_screen));
	std::println("{}", EscapeString(escape::cursor_home));
}

Screen::~Screen() {
	std::println("{}", EscapeString(escape::auto_wrap));
}

std::string Screen::StringN(const std::string& source, size_t n) {
	std::ostringstream stream;
	for (size_t i=0; i<n; i++) stream << source;
	return stream.str();
}

std::string Screen::Format(const std::initializer_list<escape>& escapes, const std::string& text) {
	std::ostringstream stream;
	for (auto& code : escapes) {
		if (EscapeSequences.contains(code))
			stream << CSI << EscapeSequences.at(code);
	}
	stream << text << CSI << EscapeSequences.at(escape::reset);
	return stream.str();
}

void Screen::PrintHeader(const std::string& header, bool more) {
	std::print("\n{}│{}{}{}│\n{}",
		SeparatorTop, Tab(), header, Tab(Width), more? SeparatorMid : SeparatorBot);
}

void Screen::PrintFrame(const std::string& line, PrintFlags flags) {
	if (flags[print_flag::frame]) std::println("│{}{}│", line, Tab(Width));
	else std::println("{}", line);
}

void Screen::PrintWrap(const std::string& message, PrintFlags flags) {
	std::string line {message};
	constexpr size_t standard_indent {4};
	size_t effective_width {flags[print_flag::frame]? Width-2 : Width},
		indent {0},
		frame_indent {flags[print_flag::frame]? size_t{1} : 0};
	while (line.length() > effective_width) {
		std::string top_line {line.substr(0, effective_width)};
		line.erase(0, effective_width);
		if (flags[print_flag::frame]) std::println("│{}{}{}│", Tab(indent + frame_indent + 1), top_line, Tab(Width));
		else std::println("{}{}", Tab(indent), top_line);
		if (!flags[print_flag::wrap]) return;
		if (flags[print_flag::indent] && (!indent)) {
			indent = standard_indent;
			effective_width -= standard_indent;
		}
	}
	if (flags[print_flag::frame]) std::println("│{}{}{}│", Tab(indent + frame_indent + 1), line, Tab(Width));
	else std::println("{}{}", Tab(indent), line);
}

void Screen::PrintError(const std::exception& error, std::string prefix) {
	std::print("{}", EscapeString(escape::auto_wrap));
	std::println(std::cerr, "{}", screen.Format({escape::bright_red}, prefix + error.what()));
	std::print("{}", EscapeString(escape::no_auto_wrap));
}

void Screen::PrintMessage(const std::string& message, std::initializer_list<Screen::escape> format) {
	std::print("{}", EscapeString(escape::auto_wrap));
	std::println("{}", Format(format, message));
	std::print("{}", EscapeString(escape::no_auto_wrap));
}

void Screen::PrintInline(const std::string& message, std::initializer_list<Screen::escape> format) {
	std::print("{}", EscapeString(escape::auto_wrap));
	std::print("{}", Format(format, message));
	std::print("{}", EscapeString(escape::no_auto_wrap));
}

void Screen::Print(const std::string& message) {
	std::print("{}", message);	
}

Screen screen;

int TempFilename::counter = 0;

std::string TempFilename::file_name() {
	return file_name_;
}

TempFilename::TempFilename() {
	counter++;
	std::string leafname {std::string("boxy") + std::to_string(counter) + ".tmp"};
	file_name_ = (std::filesystem::temp_directory_path() / leafname).generic_string();
}

TempFilename::~TempFilename() {
	try {
		remove(file_name_.c_str());
	} catch (...) {
	};
}

} //end namespace BoxyLady
