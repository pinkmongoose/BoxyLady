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

#ifndef BLOB_H_
#define BLOB_H_

#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <map>

#include "Global.h"

namespace BoxyLady {

namespace ascii {
	inline constexpr unsigned char STX {2}, ETX {3};
}

class Blob {
private:
	static constexpr char EscapeChar {'\\'}, QuoteChar {'"'}, SplitChar {'='};
	static constexpr std::string_view root {"?"};
	bool isWhitespace(char) const noexcept;
	char MatchingDelimiter(char) const noexcept;
	int DelimiterSign(char) const noexcept;
	bool isTokenChar(char) const noexcept;
	bool isAutoChar(char) const noexcept;
	std::string DelimiterName(char) const;
	std::string DelimiterSymbol(char) const;
public:
	std::vector<Blob> children_;
	std::string key_, val_;
	char delimiter_;
	void Parse(std::istream&);
	void Parse(std::string);
	explicit Blob(char delimiter = ascii::STX, std::string val = "", std::string key = std::string{root}) :
		key_{key}, val_{val}, delimiter_{delimiter} {}
	explicit Blob(std::string input, char delimiter = ascii::STX, std::string val = "", std::string key = std::string{root}) :
		key_{key}, val_{val}, delimiter_{delimiter} {
		Parse(input);
	}
	Blob& AddChild(char delimiter = 0, std::string val = "", std::string key = "") {
		children_.push_back(Blob(delimiter, val, key));
		return children_.back();
	}
	Blob Wrap(char);
	bool hasKey(std::string key) const {
		for (auto& child : children_)
			if (child.key_ == key)
				return true;
		return false;
	}
	Blob& operator[](std::string key) {
		for (auto& child : children_)
			if (child.key_ == key)
				return child;
		throw EError("Syntax error: missing value '" + key + "'.\n" + ErrorString());
	}
	Blob& operator[](size_t index) {
		if (index > children_.size() - 1)
			throw EError("Syntax error: missing value.\n" + ErrorString());
		return children_[index];
	}
	bool hasFlag(std::string key) const {
		for (auto& child : children_)
			if ((child.key_ == "") && (child.val_ == key))
				return true;
		return false;
	}
	std::string Dump(std::string = "\n") const;
	std::string DumpChunk(size_t = 25, size_t = 10) const;
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
			throw EError("Unknown command. () missing?\n" + ErrorString());
		return *this;
	}
	void AssertFunction() {
		ifFunction();
	}
	std::string atom() const;
	int asInt(int low = int_min, int high = int_max) const {
		int result {0};
		try {
        	result = std::stoi(atom());
		} catch (const std::exception& e) {
			throw EError("Syntax error: integer expected.\n" + ErrorString());
		}
		if ((result < low) || (result > high))
			throw EError("Integer out of range.\n" + ErrorString());
		else
			return result;
	}
	float_type asFloat(float_type low = -float_type_max, float_type high = float_type_max) const {
		float_type result {0};
		try {
        	result = std::stod(atom());
		} catch (const std::exception& e) {
			throw EError("Syntax error: float expected.\n" + ErrorString());
		}
		if ((result < low) || (result > high))
			throw EError("Floating point number out of range.\n" + ErrorString());
		else
			return result;
	}
	bool asBool() const {
		static const std::map<std::string_view, bool> bool_map {{"TRUE",true}, {"T",true}, {"true",true}, {"FALSE",false}, {"F",false}, {"false",false}};
		if (const auto string_value = atom(); bool_map.contains(string_value)) return bool_map.at(string_value);
		else throw EError("Syntax error: boolean expected.\n" + ErrorString());
	}
	bool tryWriteInt(std::string lookup_key, int& value, int low = int_min, int high = int_max) {
		if (hasKey(lookup_key)) {
			value = operator[](lookup_key).asInt(low, high);
			return true;
		} else
			return false;
	}
	bool tryWritefloat_type(std::string lookup_key, float_type& value, float_type low = -float_type_max, float_type high = float_type_max) {
		if (hasKey(lookup_key)) {
			value = operator[](lookup_key).asFloat(low, high);
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

} //end namespace BoxyLady

#endif /* BLOB_H_ */
