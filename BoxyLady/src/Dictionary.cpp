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

#include "Dictionary.h"

namespace BoxyLady {

bool DictionaryItem::ValidName(std::string name) {
	const std::string valid = ":._-";
	if (name.empty())
		return false;
	for (auto c : name) {
		if ((c >= 'A') && (c <= 'Z'))
			continue;
		if ((c >= 'a') && (c <= 'z'))
			continue;
		if ((c >= '0') && (c <= '9'))
			continue;
		if (valid.find(c) != std::string::npos)
			continue;
		return false;
	}
	return true;
}

template<class T>
void Dictionary::SWrite(std::string &buffer, T data, int tab) {
	std::ostringstream File(buffer, std::istringstream::out);
	File.precision(3);
	File.seekp(tab);
	File << data;
	buffer = File.str();
}

void Dictionary::WriteSlotProtection(std::string &block,
		dic_item_protection level) {
	switch (level) {
	case dic_item_protection::locked:
		SWrite(block, "L", 67);
		break;
	case dic_item_protection::system:
		SWrite(block, "S", 67);
		break;
	case dic_item_protection::active:
		SWrite(block, "!", 67);
		break;
	default:
		break;
	}
}

void Dictionary::ListEntries(Blob &Q) {
	const bool all = Q.hasFlag("*");
	const std::array<std::string, 4> dictionary_types { { "unknown", "sequence",
			"macro", "instr." } };
	std::string display = std::string(screen::Width, ' ');
	std::string title = display;
	SWrite(title, "Slot name", 4);
	SWrite(title, "Rate", 16);
	SWrite(title, "tLength", 25);
	SWrite(title, "pLength", 33);
	SWrite(title, "mLength", 41);
	SWrite(title, "Ch.", 50);
	SWrite(title, "Type", 54);
	SWrite(title, "Flags", 64);
	SWrite(title, "Mb/b", 72);
	std::cout << "\n" << screen::Header << title << "\n" << screen::Header;
	int index = 1;
	for (auto &map_item : dictionary_) {
		DictionaryItem &dictionary_item = map_item.second;
		if (!all)
			if (dictionary_item.protection_level_
					== dic_item_protection::system)
				continue;
		Sequence &sequence = dictionary_item.sequence_;
		std::string block = display;
		SWrite(block, index, 0);
		SWrite(block, map_item.first, 4);
		if (dictionary_item.isPatch()) {
			const SampleType sample_type = sequence.getType();
			SWrite(block, double(sequence.sample_rate()) / 1000, 16);
			SWrite(block, sequence.get_tSeconds(), 25);
			SWrite(block, sequence.get_pSeconds(), 33);
			SWrite(block, sequence.get_mSeconds(), 41);
			SWrite(block, sequence.channels(), 50);
			if (sample_type.gate_)
				SWrite(block, "G", 64);
			if (sample_type.loop_)
				SWrite(block, "I", 65);
			if (sample_type.loop_start_)
				SWrite(block, "i", 65);
			if (sample_type.start_anywhere_)
				SWrite(block, "A", 66);
			SWrite(block, sequence.MusicDataSize(), 72);
		} else if (dictionary_item.isMacro()) {
			SWrite(block, dictionary_item.getMacro()[0].DumpChunk(32, 14), 16);
			SWrite(block, dictionary_item.getMacro().Dump().length(), 72);
		} else {
			SWrite(block, map_item.first, 4);
		}
		SWrite(block, dictionary_types[int(dictionary_item.getType())], 54);
		WriteSlotProtection(block, dictionary_item.ProtectionLevel());
		std::cout << block << "\n";
		index++;
	}
	std::cout << screen::Header;
}

}
