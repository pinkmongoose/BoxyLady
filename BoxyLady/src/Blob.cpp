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

#include <set>
#include <map>
#include "Blob.h"

namespace BoxyLady {

bool Blob::isWhitespace(char c) const noexcept {
	if (c < 33) return true;
	else if (c == 127) return true;
	else return false;
}

char Blob::MatchingDelimiter(char c) const noexcept {
	static std::map<char, char> delimiters {{'(',')'}, {'<','>'}, {'{','}'}, {'[',']'}, {'"','"'}, {ascii::STX, ascii::ETX}};
	if (delimiters.contains(c)) return delimiters[c];
	else return 0;
}

int Blob::DelimiterSign(char c) const noexcept {
	const std::set<char> opener {'(','<','{','"',ascii::STX,'['}, closer {')','>','}','"',ascii::ETX,']'};
	if (opener.contains(c)) return 1;
	else if (closer.contains(c)) return -1;
	else return 0;
}

bool Blob::isAutoChar(char c) const noexcept {
	switch (c) {
	case '@': return true;
	default: return false;
	}
}

bool Blob::isTokenChar(char c) const noexcept {
	if (isWhitespace(c)) return false;
	else if (DelimiterSign(c)) return false;
	else return true;
}

std::string Blob::DelimiterName(char c) const {
	switch (c) {
	case ascii::STX: return "start of file";
	case ascii::ETX: return "end of file";
	default: return std::string(1, c);
	}
}
std::string Blob::DelimiterSymbol(char c) const {
	switch (c) {
	case ascii::STX: return "{";
	case ascii::ETX: return "}";
	default: return std::string(1, c);
	}
}

Blob Blob::Wrap(char delimiter) {
	Blob output(ascii::STX, "", ""), temp = *this;
	temp.delimiter_ = delimiter;
	output.children_.push_back(temp);
	return output;
}

bool Blob::isAtomic() const {
	if (val_.length())
		return true;
	if (delimiter_ != '(')
		return false;
	if (children_.size() != 1)
		return false;
	const Blob& first = children_.front();
	if (first.key_ != "")
		return false;
	return first.isAtomic();
}

std::string Blob::atom() const {
	if (!isAtomic())
		throw EError("Syntax error: single value expected.\n" + ErrorString());
	if (val_.length())
		return val_;
	return children_.front().atom();
}

bool Blob::isEmpty() const {
	if (val_.length() > 0)
		return false;
	if (children_.size())
		return false;
	return true;
}

bool Blob::isBlock(bool no_key) const {
	if (val_.length() > 0)
		return false;
	if (no_key && (key_.length() > 0))
		return false;
	if (delimiter_ == '"')
		return false;
	if (children_.size())
		return true;
	return false;
}

bool Blob::isToken() const {
	if (key_.length() > 0)
		return false;
	if (children_.size())
		return false;
	return true;
}

std::string Blob::Dump(std::string line_feed) const {
	std::string out {""};
	if (key_.length())
		out += (key_ + "=");
	if (isAtomic()) {
		out += ("'" + atom() + "' ");
		return out;
	}
	out += (val_ + DelimiterSymbol(delimiter_) + line_feed);
	for (auto& child : children_)
		out += child.Dump(line_feed);
	if (delimiter_)
		out += (DelimiterSymbol(MatchingDelimiter(delimiter_)) + line_feed);
	for (auto& c : out)
		if (c == 0) c = '?';
	return out;
}

std::string Blob::DumpChunk(size_t max_size, size_t small_size) const {
	std::string out {Dump("")};
	const size_t length {out.length()};
	const std::string fancy_out {(length < max_size) ?
		out : out.substr(0, small_size) + " ... " + out.substr(length - small_size, small_size)};
	return fancy_out;
}

std::string Blob::ErrorString() const {
	return std::string("Problem in '") + DumpChunk() + std::string("'.");
}

std::string GetABit(std::istream &File) {
	const std::streamsize n_chars {16};
	char buffer[n_chars];
	for (std::streamsize i {0}; i < n_chars; i++)
		buffer[i] = 0;
	File.read(buffer, n_chars - 1);
	return buffer;
}

void Blob::Parse(std::string input) {
	std::istringstream buffer(input, std::istringstream::in);
	Parse(buffer);
}

void Blob::Parse(std::istream &File) {
	enum class parse_mode {ready, scan1, scan2, literal};
	using enum parse_mode;
	parse_mode mode{ready};
	if (delimiter_ == '"')
		mode = literal;
	bool escape {false};
	char c;
	std::string buffer {""};
	Blob *child {nullptr};
	while (File.good()) {
		c = File.get();
		if (!File.good())
			c = ascii::ETX;
		switch (mode) {
		case ready: // gap between items
			if (c == MatchingDelimiter(delimiter_))
				return;
			if (isAutoChar(c)) {
				child = &(AddChild(0, "", ""));
				mode = scan2;
				child->key_ = std::string(1, c);
				continue;
			}
			if (DelimiterSign(c) < 0)
				throw EError("Syntax error: unexpected "
					+ DelimiterName(c) + " found before '"
					+ GetABit(File) + "'.");
			if (DelimiterSign(c) > 0) {
				Blob &C = AddChild(c, "", "");
				C.Parse(File);
				continue;
			}
			if (isWhitespace(c))
				continue;
			if (c == SplitChar)
				throw EError("Syntax error: unexpected " + DelimiterName(c) + " found before '" + GetABit(File) + "'.");
			child = &(AddChild(0, "", ""));
			mode = scan1;
			buffer += c;
			continue;
		case scan1: // prior to equals sign
			if (c == MatchingDelimiter(delimiter_)) {
				child->val_ = buffer;
				return;
			}
			if (DelimiterSign(c) < 0)
				throw EError("Syntax error: unexpected " + DelimiterName(c) + " found before '" + GetABit(File) + "'.");
			if (DelimiterSign(c) > 0) {
				child->key_ = buffer;
				buffer = "";
				child->delimiter_ = c;
				child->Parse(File);
				mode = ready;
				child = nullptr;
				continue;
			}
			if (isWhitespace(c)) {
				child->val_ = buffer;
				buffer = "";
				mode = ready;
				child = nullptr;
				continue;
			}
			if (c == SplitChar) {
				child->key_ = buffer;
				buffer = "";
				mode = scan2;
				continue;
			}
			buffer += c;
			continue;
		case scan2: // after equals sign
			if (c == MatchingDelimiter(delimiter_)) {
				child->val_ = buffer;
				return;
			}
			if (DelimiterSign(c) < 0)
				throw EError("Syntax error: unexpected " + DelimiterName(c) + " found before '" + GetABit(File) + "'.");
			if (DelimiterSign(c) > 0) {
				if (buffer.length())
					throw EError("Syntax error: unexpected " + DelimiterName(c) + " found before '" + GetABit(File) + "'.");
				child->delimiter_ = c;
				child->Parse(File);
				mode = ready;
				child = nullptr;
				continue;
			}
			if (isWhitespace(c)) {
				child->val_ = buffer;
				buffer = "";
				mode = ready;
				child = nullptr;
				continue;
			}
			if (c == SplitChar)
				throw EError("Syntax error: unexpected " + DelimiterName(c) + " found before '" + GetABit(File) + "'.");
			buffer += c;
			continue;
		case literal: // for string literals
			if (escape) {
				escape = false;
				if (c == MatchingDelimiter(delimiter_))
					buffer += c;
				else if (c == EscapeChar)
					buffer += c;
				else if (c == 'n')
					buffer += "\n";
				else
					throw EError("Unknown escape sequence: \\" + std::string(1, c) + ".");
			} else {
				if (c == MatchingDelimiter(delimiter_)) {
					val_ = buffer;
					return;
				} else if (c == EscapeChar)
					escape = true;
				else
					buffer += c;
			}
			continue;
		};
	}
}

} //end namespace BoxyLady
