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

#include <map>

#include "Dictionary.h"

namespace BoxyLady {

bool DictionaryItem::ValidName(std::string name) {
	constexpr std::string_view valid {":._-"};
	if (name.empty())
		return false;
	for (auto c : name) {
		if (in_range(c, 'A', 'Z'))
			continue;
		if (in_range(c, 'a', 'z'))
			continue;
		if (in_range(c, '0', '9'))
			continue;
		if (valid.find(c) != std::string::npos)
			continue;
		return false;
	}
	return true;
}

void Dictionary::SWrite(std::string& buffer, auto data, std::streampos tab) {
	std::ostringstream stream(buffer, std::ios::in | std::ios::out);
	stream.precision(3);
	stream.seekp(tab);
	stream << data;
	buffer = stream.str();
}

void Dictionary::WriteSlotProtection(std::string& block, dic_item_protection level) {
	static const std::map<dic_item_protection, std::string> levels {
		{dic_item_protection::temp, "t"}, {dic_item_protection::normal, ""},
		{dic_item_protection::locked, "L"}, {dic_item_protection::system, "S"},
		{dic_item_protection::active, "!"}
	};
	SWrite(block, levels.at(level), 67);
}

void Dictionary::ListEntries(Blob& Q) {
	const bool all {Q.hasFlag("*")};
	const std::array<std::string, 5> dictionary_types { { "unknown", "deleted", "sound", "macro", "instr." } };
	std::string display {std::string(screen.Width, ' ')}, title {display};
	SWrite(title, "Slot name", 1);
	SWrite(title, "Rate", 16);
	SWrite(title, "tLength", 25);
	SWrite(title, "pLength", 33);
	SWrite(title, "mLength", 41);
	SWrite(title, "Ch.", 50);
	SWrite(title, "Type", 54);
	SWrite(title, "Flags", 64);
	SWrite(title, "Mb/b", 72);
	title += screen.Tab(1) + "│" + screen.Tab(screen.Width) + "│";
	screen.PrintSeparatorTop();
	screen.Print(title + "\n");
	screen.PrintSeparatorMid();
	for (auto& map_item : dictionary_) {
		DictionaryItem& dictionary_item {map_item.second};
		if (!all)
			if (dictionary_item.protection_level_ == dic_item_protection::system)
				continue;
		Sound& sound {dictionary_item.sound_};
		std::string block {display};
		SWrite(block, map_item.first, 1);
		if (dictionary_item.isSound()) {
			const SampleType sample_type {sound.getType()};
			SWrite(block, float_type(sound.sample_rate()) / 1000, 16);
			SWrite(block, sound.get_tSeconds(), 25);
			SWrite(block, sound.get_pSeconds(), 33);
			SWrite(block, sound.get_mSeconds(), 41);
			SWrite(block, sound.channels(), 50);
			if (sample_type.loop)
				SWrite(block, "I", 65);
			if (sample_type.loop_start > 0.0)
				SWrite(block, "i", 65);
			if (sample_type.start_anywhere)
				SWrite(block, "A", 66);
			SWrite(block, sound.MusicDataSize(), 72);
		} else if (dictionary_item.isMacro()) {
			if (dictionary_item.getMacroType() == macro_type::variable)
				SWrite(block, "v", 64);
			if (dictionary_item.getMacro().children_.size() > 0)
				SWrite(block, dictionary_item.getMacro()[0].DumpChunk(32, 14), 16);
			SWrite(block, dictionary_item.getMacro().Dump().length(), 72);
		} else {
			SWrite(block, map_item.first, 4);
		}
		SWrite(block, dictionary_types[static_cast<size_t>(dictionary_item.getType())], 54);
		WriteSlotProtection(block, dictionary_item.ProtectionLevel());
		block += screen.Tab(1) + "│" + screen.Tab(screen.Width) + "│";
		screen.Print(block + "\n");
	}
	screen.PrintSeparatorBot();
}

} //end namespace BoxyLady
