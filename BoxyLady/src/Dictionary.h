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

#ifndef DICTIONARY_H_
#define DICTIONARY_H_

#include <unordered_map>

#include "Global.h"
#include "Sequence.h"
#include "Blob.h"

namespace BoxyLady {

enum class dic_item_protection {
	temp, normal, locked, system, active
};
enum class dic_item_type {
	null, patch, macro
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
	Blob macro_;
	Sequence sequence_;
public:
	DictionaryItem(dic_item_type type = dic_item_type::null) :
			semaphor_(0), type_(type), protection_level_(
					dic_item_protection::normal) {
	}
	dic_item_protection ProtectionLevel() const {
		return protection_level_;
	}
	void Protect(dic_item_protection level) {
		protection_level_ = level;
	}
	bool isMacro() const {
		return type_ == dic_item_type::macro;
	}
	bool isPatch() const {
		return type_ == dic_item_type::patch;
	}
	bool isNull() const {
		return type_ == dic_item_type::null;
	}
	dic_item_type getType() const {
		return type_;
	}
	Blob& getMacro() {
		return macro_;
	}
	Sequence& getSequence() {
		return sequence_;
	}
	static bool ValidName(std::string);
};

typedef std::unordered_map<std::string, DictionaryItem> DictionaryType;
typedef DictionaryType::iterator DictionaryIterator;
typedef std::list<DictionaryIterator> DictionaryIteratorList;

class DictionaryMutex {
private:
	DictionaryItem &item_;
public:
	DictionaryMutex(DictionaryItem &item) :
			item_(item) {
		item_.semaphor_++;
	}
	~DictionaryMutex() {
		item_.semaphor_--;
	}
};

class Dictionary {
private:
	DictionaryType dictionary_;
	DictionaryItem invalid_item_;
public:
	DictionaryIterator begin() {
		return dictionary_.begin();
	}
	DictionaryIterator end() {
		return dictionary_.end();
	}
	bool Exists(std::string name) const {
		auto item = dictionary_.find(name);
		return item != dictionary_.end();
	}
	DictionaryItem& Find(std::string name) {
		auto item = dictionary_.find(name);
		if (item != dictionary_.end())
			return item->second;
		else
			return invalid_item_;
	}
	Sequence& FindSequence(std::string name) {
		return Find(name).sequence_;
	}
	Sequence& FindSequence(Blob &Q) {
		const std::string name = Q["@"].atom();
		return FindSequence(name);
	}
	DictionaryItem& Insert(DictionaryItem item, std::string name) {
		if (!item.ValidName(name))
			throw EDictionary().msg(name + ": Illegal character in name.");
		if (Exists(name))
			throw EDictionary().msg(name + ": Name already used.");
		dictionary_.insert( { { name, item } });
		return Find(name);
	}
	Sequence& InsertSequence(std::string name) {
		DictionaryItem item;
		item.type_ = dic_item_type::patch;
		return Insert(item, name).sequence_;
	}
	bool Delete(std::string name, bool protect = false) {
		auto item = dictionary_.find(name);
		if (item != dictionary_.end()) {
			if (item->second.semaphor_)
				return false;
			if (protect
					&& (item->second.protection_level_
							> dic_item_protection::normal))
				return false;
			else {
				dictionary_.erase(item);
				return true;
			}
		} else
			return false;
	}
	void Clear(bool protect = false) {
		DictionaryIteratorList list;
		for (auto item = dictionary_.begin(); item != dictionary_.end();
				item++) {
			if (!item->second.semaphor_)
				if ((!protect)
						|| (item->second.protection_level_
								<= dic_item_protection::normal))
					list.push_back(item);
		}
		for (auto it : list)
			dictionary_.erase(it);
	}
	void Rename(std::string old_name, std::string new_name) {
		auto node = dictionary_.extract(old_name); //@suppress("Method cannot be resolved")
		node.key() = new_name; //@suppress("Method cannot be resolved")
		dictionary_.insert(move(node)); //@suppress("Invalid arguments") @suppress("Function cannot be resolved")
	}
	void ListEntries(Blob &Q);
	static void WriteSlotProtection(std::string&, dic_item_protection);
	template<class T>
	static void SWrite(std::string&, T, int);
};

}

#endif /* DICTIONARY_H_ */
