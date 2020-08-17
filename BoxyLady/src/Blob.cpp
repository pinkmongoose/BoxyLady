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

#include "Blob.h"

namespace BoxyLady {

bool Blob::isWhitespace(char c) const {
	if (c < 33)
		return true;
	else if (c == 127)
		return true;
	else
		return false;
}

char Blob::MatchingDelimiter(char c) const {
	switch (c) {
	case '(':
		return ')';
	case '<':
		return '>';
	case '{':
		return '}';
	case '[':
		return ']';
	case '"':
		return '"';
	case ascii::STX:
		return ascii::ETX;
	default:
		return 0;
	}
}

bool Blob::isAutoChar(char c) const {
	switch (c) {
	case '@':
		return true;
	default:
		return false;
	}
}

int Blob::DelimiterSign(char c) const {
	switch (c) {
	case '(':
		;
		/*no break*/case '<':
		;
		/*no break*/case '{':
		;
		/*no break*/case '"':
		;
		/*no break*/case ascii::STX:
		;
		/*no break*/case '[':
		return 1;
	case ')':
		;
		/*no break*/case '>':
		;
		/*no break*/case '}':
		;
		/*no break*/case ']':
		;
		/*no break*/case ascii::ETX:
		return -1;
	default:
		return 0;
	}
}

bool Blob::isTokenChar(char c) const {
	if (isWhitespace(c))
		return false;
	if (DelimiterSign(c))
		return false;
	return true;
}

std::string Blob::DelimiterName(char c) const {
	switch (c) {
	case ascii::STX:
		return "start of file";
	case ascii::ETX:
		return "end of file";
	default:
		return std::string("") + c;
	}
}
std::string Blob::DelimiterSymbol(char c) const {
	switch (c) {
	case ascii::STX:
		return "<<";
	case ascii::ETX:
		return ">>";
	default:
		return std::string("") + c;
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
	const Blob &first = *(children_.begin());
	if (first.key_ != "")
		return false;
	return first.isAtomic();
}

std::string Blob::atom() const {
	if (!isAtomic())
		throw EBlob().msg(
				"Syntax error: single value expected.\n" + ErrorString());
	if (val_.length())
		return val_;
	return children_.begin()->atom();
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

std::string Blob::Dump(std::string lf) const {
	std::string out = "";
	if (key_.length())
		out += (key_ + "=");
	if (isAtomic()) {
		out += ("'" + atom() + "' ");
		return out;
	}
	out += (val_ + DelimiterSymbol(delimiter_) + lf);
	for (auto &child : children_)
		out += child.Dump(lf);
	if (delimiter_)
		out += (DelimiterSymbol(MatchingDelimiter(delimiter_)) + lf);
	for (auto &c : out)
		if (!c)
			c = '?';
	return out;
}

std::string Blob::DumpChunk(int max_size, int small_size) const {
	std::string out = Dump("");
	const int length = out.length();
	const std::string fancy_out =
			(length < max_size) ?
					out :
					out.substr(0, small_size) + " ... "
							+ out.substr(length - small_size, small_size);
	return fancy_out;
}

std::string Blob::ErrorString() const {
	return std::string("Problem in '") + DumpChunk() + std::string("'.");
}

std::string GetABit(std::istream &File) {
	const int n_chars = 16;
	char buffer[n_chars];
	for (int i = 0; i < n_chars; i++)
		buffer[i] = 0;
	File.read(buffer, n_chars - 1);
	return buffer;
}

void Blob::Parse(std::string input) {
	std::istringstream buffer(input, std::istringstream::in);
	Parse(buffer);
}

void Blob::Parse(std::istream &File) {
	enum {
		ready, scan1, scan2, literal
	} mode = ready;
	if (delimiter_ == '"')
		mode = literal;
	bool escape = false;
	char c;
	std::string buffer = "";
	Blob *child = nullptr;
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
				throw EBlob().msg(
						std::string("Syntax error: unexpected ")
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
				throw EBlob().msg(
						std::string("Syntax error: unexpected ")
								+ DelimiterName(c) + " found before '"
								+ GetABit(File) + "'.");
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
				throw EBlob().msg(
						std::string("Syntax error: unexpected ")
								+ DelimiterName(c) + " found before '"
								+ GetABit(File) + "'.");
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
				throw EBlob().msg(
						std::string("Syntax error: unexpected ")
								+ DelimiterName(c) + " found before '"
								+ GetABit(File) + "'.");
			if (DelimiterSign(c) > 0) {
				if (buffer.length())
					throw EBlob().msg(
							std::string("Syntax error: unexpected ")
									+ DelimiterName(c) + " found before '"
									+ GetABit(File) + "'.");
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
				throw EBlob().msg(
						std::string("Syntax error: unexpected ")
								+ DelimiterName(c) + " found before '"
								+ GetABit(File) + "'.");
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
					throw EBlob().msg(
							"Unknown escape sequence: \\" + std::string(1, c)
									+ ".");
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

}
