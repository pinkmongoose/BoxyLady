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

#ifndef DICTIONARY_H_
#define DICTIONARY_H_

#include <map>
#include <functional>

#include "Global.h"
#include "Sound.h"
#include "Blob.h"

namespace BoxyLady {

enum class dic_item_protection {
	temp, normal, locked, system, active
};
enum class dic_item_type {
	null, deleted, sound, macro
};
enum class macro_type {
	null, macro, variable
};

class DictionaryItem;
class Dictionary;
class DictionaryMutex;

class DictionaryItem {
	friend class DictionaryMutex;
	friend class Dictionary;
private:
	int semaphor_;
	dic_item_type type_;
	dic_item_protection protection_level_;
	macro_type macro_type_;
	Blob macro_;
	Sound sound_;
public:
	explicit DictionaryItem(dic_item_type type = dic_item_type::null) noexcept :
			semaphor_{0}, type_{type},
			protection_level_{dic_item_protection::normal},
			macro_type_{macro_type::null} {}
	dic_item_protection ProtectionLevel() const noexcept {
		return protection_level_;
	}
	DictionaryItem& Protect(dic_item_protection level) noexcept {
		protection_level_ = level;
		return *this;
	}
	bool isMacro() const noexcept {
		return type_ == dic_item_type::macro;
	}
	bool isSound() const noexcept {
		return type_ == dic_item_type::sound;
	}
	bool isNull() const noexcept {
		return type_ == dic_item_type::null;
	}
	void markDeleted() noexcept {type_ = dic_item_type::deleted;}
	bool isDeleted() const noexcept {return type_ == dic_item_type::deleted;}
	dic_item_type getType() const noexcept {return type_;}
	macro_type& getMacroType() noexcept {return macro_type_;}
	Blob& getMacro() {return macro_;}
	Sound& getSound() {return sound_;}
	static bool ValidName(std::string);
};

class DictionaryMutex {
private:
	DictionaryItem& item_;
public:
	explicit DictionaryMutex(DictionaryItem& item) noexcept :
			item_{item} {
		item_.semaphor_++;
	}
	~DictionaryMutex() noexcept {
		item_.semaphor_--;
	}
};

class Dictionary {
private:
	using DictionaryMap = std::map<std::string, DictionaryItem>;
	using DictionaryIterator = DictionaryMap::iterator;
	DictionaryMap dictionary_;
	DictionaryItem invalid_item_;
	DictionaryIterator begin() {return dictionary_.begin();}
	DictionaryIterator end() {return dictionary_.end();}
public:
	bool contains(std::string name) const {
		return dictionary_.contains(name);
	}
	DictionaryItem& Find(std::string name) {
		if (dictionary_.contains(name))
			return dictionary_[name];
		else
			return invalid_item_;
	}
	Sound& FindSound(std::string name) {
		return Find(name).sound_;
	}
	Sound& FindSound(Blob& Q) {
		const std::string name {Q["@"].atom()};
		return FindSound(name);
	}
	DictionaryItem& Insert(DictionaryItem item, std::string name) {
		if (!item.ValidName(name))
			throw EError(name + ": Illegal character in name.");
		if (contains(name))
			throw EError(name + ": Name already used.");
		dictionary_.insert( { { name, item } });
		return Find(name);
	}
	Sound& InsertSound(std::string name) {
		DictionaryItem item;
		item.type_ = dic_item_type::sound;
		return Insert(item, name).sound_;
	}
	bool Delete(std::string name, bool protect = false) {
		if (auto item {dictionary_.find(name)}; item != dictionary_.end()) {
			if (item->second.semaphor_)
				return false;
			if (protect && (item->second.protection_level_ > dic_item_protection::normal))
				return false;
			else {
				dictionary_.erase(item);
				return true;
			}
		} else
			return false;
	}
	void Clear(bool protect = false) {
		for (auto& item : dictionary_) {
			if (item.second.semaphor_ == 0)
				if ((!protect) || (item.second.protection_level_ <= dic_item_protection::normal))
					item.second.markDeleted();
		}
		std::erase_if(dictionary_, [](const auto& item) {return item.second.isDeleted();});
	}
	void Rename(std::string old_name, std::string new_name) {
		auto node {dictionary_.extract(old_name)};
		node.key() = new_name;
		dictionary_.insert(move(node));
	}
	void ListEntries(Blob& Q);
	static void WriteSlotProtection(std::string&, dic_item_protection);
	static void SWrite(std::string&, auto, std::streampos);
	template <typename Operation>
	void Apply(Operation op) {
		for (auto& item : dictionary_) op(item.second);
	}
};

} //end namespace BoxyLady

#endif /* DICTIONARY_H_ */
