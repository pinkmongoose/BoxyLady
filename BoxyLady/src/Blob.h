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

#ifndef BLOB_H_
#define BLOB_H_

#include <cstdlib>
#include <cerrno>
#include <iostream>
#include <sstream>
#include <cstring>
#include <string>
#include <list>

#include "Global.h"

namespace BoxyLady {

namespace ascii {
enum {
	STX = 2, ETX = 3
};
}

class Blob {
	static constexpr char EscapeChar = '\\', QuoteChar = '"', SplitChar = '=';
	typedef std::list<Blob> BlobList;
	typedef BlobList::iterator BlobListIterator;
private:
	bool isWhitespace(char) const;
	char MatchingDelimiter(char) const;
	int DelimiterSign(char) const;
	bool isTokenChar(char) const;
	bool isAutoChar(char) const;
	std::string DelimiterName(char) const;
	std::string DelimiterSymbol(char) const;
public:
	BlobList children_;
	std::string key_, val_;
	char delimiter_;
	void Parse(std::istream&);
	void Parse(std::string);
	Blob(char delimiter = ascii::STX, std::string val = "", std::string key =
			"ROOT") :
			key_(key), val_(val), delimiter_(delimiter) {
	}
	Blob(std::string input, char delimiter = ascii::STX, std::string val = "",
			std::string key = "ROOT") :
			key_(key), val_(val), delimiter_(delimiter) {
		Parse(input);
	}
	Blob& AddChild(char delimiter = 0, std::string val = "", std::string key =
			"") {
		children_.push_back(Blob(delimiter, val, key));
		return children_.back();
	}
	Blob Wrap(char);
	bool hasKey(std::string key) const {
		for (auto &child : children_)
			if (child.key_ == key)
				return true;
		return false;
	}
	Blob& operator[](std::string key) {
		for (auto &child : children_)
			if (child.key_ == key)
				return child;
		throw EBlob().msg(
				"Syntax error: missing value '" + key + "'.\n" + ErrorString());
	}
	Blob& operator[](int index) {
		if (index > int(children_.size() - 1))
			throw EBlob().msg("Syntax error: missing value.\n" + ErrorString());
		BlobListIterator iterator = children_.begin();
		for (int counter = 0; counter < index; counter++)
			iterator++;
		return *iterator;
	}
	bool hasFlag(std::string key) const {
		for (auto &child : children_)
			if ((child.key_ == "") && (child.val_ == key))
				return true;
		return false;
	}
	std::string Dump(std::string = "\n") const;
	std::string DumpChunk(int = 25, int = 10) const;
	std::string ErrorString() const;
	bool isAtomic() const;
	bool isEmpty() const;
	bool isBlock(bool = true) const;
	bool isToken() const;
	bool isFunction() const {
		return delimiter_ == '(';
	}
	Blob& ifFunction() {
		if (!isFunction())
			throw EBlob().msg("Syntax error: ( expected.\n" + ErrorString());
		return *this;
	}
	void AssertFunction() {
		ifFunction();
	}
	std::string atom() const;
	int asInt(int low = INT_MIN, int high = INT_MAX) const {
		char *errc = NULL;
		errno = 0;
		const int result = strtol(atom().c_str(), &errc, 10);
		if (errno || strlen(errc))
			throw EBlob().msg(
					"Syntax error: integer expected.\n" + ErrorString());
		if ((result < low) || (result > high))
			throw EBlob().msg("Integer out of range.\n" + ErrorString());
		else
			return result;
	}
	double asDouble(double low = -DBL_MAX, double high = DBL_MAX) const {
		char *errc = NULL;
		errno = 0;
		const double result = strtod(atom().c_str(), &errc);
		if (errno || strlen(errc))
			throw EBlob().msg(
					"Syntax error: float expected.\n" + ErrorString());
		if ((result < low) || (result > high))
			throw EBlob().msg(
					"Floating point number out of range.\n" + ErrorString());
		else
			return result;
	}
	bool asBool() const {
		const std::string string_value = atom();
		if ((string_value == "TRUE") || (string_value == "T"))
			return true;
		if ((string_value == "FALSE") || (string_value == "F"))
			return false;
		throw EBlob().msg("Syntax error: boolean expected.\n" + ErrorString());
	}
	bool tryWriteInt(std::string lookup_key, int &value, int low = INT_MIN,
			int high = INT_MAX) {
		if (hasKey(lookup_key)) {
			value = operator[](lookup_key).asInt(low, high);
			return true;
		} else
			return false;
	}
	bool tryWriteDouble(std::string lookup_key, double &value, double low =
			-DBL_MAX, double high = DBL_MAX) {
		if (hasKey(lookup_key)) {
			value = operator[](lookup_key).asDouble(low, high);
			return true;
		} else
			return false;
	}
	bool tryWriteBool(std::string lookup_key, bool &value) {
		if (hasKey(lookup_key)) {
			value = operator[](lookup_key).asBool();
			return true;
		} else
			return false;
	}
	bool tryWriteString(std::string lookup_key, std::string &value) {
		if (hasKey(lookup_key)) {
			value = operator[](lookup_key).atom();
			return true;
		} else
			return false;
	}
};

}

#endif /* BLOB_H_ */
