#include <iostream>
#include <fstream>
#include <string>
#include <optional>
#include <vector>
#include <unordered_set>
#include <functional>
#include <queue>
#include <cassert>
#include <cstring>

namespace {

int getBufferSize(int dictionary_size) {
	// We can optimize the size returned this function empirically
	// through load-tests. There is a tradeoff between a high-value
	// (which lowers probability of collisions) and lower-value
	// (increases collision probability, but decreases memory consumption)
	//
	// We can even use a function that returns a number power of two,
	// Which enables implementing '%' operation below more efficient
	// (using bit operations). 
	static int kScalingFactor = 2;
	return dictionary_size * kScalingFactor;
}

// Constant for total number of words, based on the given dictionary.
constexpr int kWordCount = 19878;
}

// Unused, but could be used in an alternative implementation, see below.
constexpr int kMaxWordLength = 17;

class HashTable {

private:
	struct Entry {
		// We use the knowledge of the input dictionary, with a word having a max length of
		// kMaxWordLength.
		// We use an array of chars instead of std::string to improve cache locality
		// of memory accesses, as all bytes representing an Entry are now contigous in memory.
		// Would we have used a std::string, that underlying string data could possibly be in
		// a different memory region and incurr another more expensive memory lookup.
		//
		// Alternative: given that we know all the possible set of words, we could, in our entire
		// application refer the words by their indices in a sorted list, instead of the full
		// string representation.
		// This would have allowed us here to store an int, instead of a char[], which would
		// have reduced memory per Entry, enabling more entries to be read in one cache-line read.
		// It would improve efficiency if we have collisions, as likely a single cache-line read
		// would have been necessary.

		char key[kMaxWordLength + 1];

		// We place here the two bool variables to avoid unnecessary padding. Otherwise
		// the value would have come just after the key (which is now at the end).

		// Marks an element as deleted.
		// 
		// We need a tombstone, instead of just marking the element as free
		// because if we do a lookup, we might find a key stored after a
		// deleted element. We need to differentiate between a deleted and
		// never-written element.
		bool tombstone = false;
		// Mark an element as occupied, i.e. holds a valid (key, value).
		bool occupied = false;

		int value;
	};

public:

	using HashFunction = std::function<std::uint64_t(const std::string_view&)>;

	HashTable(int dictionary_size = kWordCount,
		 HashFunction hash_function = std::hash<std::string_view>()):
			size_(getBufferSize(dictionary_size)), buffer_(size_), hash_function_(std::move(hash_function)) {}

	
	// Inserts.
	//
	// Pass key by value because we need to create a copy anyway.
	void insert(std::string key, int value) {
		// We use the two helper functions findKey and findFirstEmptyOrTombstone
		// for better clarity.
		// For improving efficiency, these can be combined such that we do a single pass
		// over the buffer_.

		// First see if there is a key present.
		std::optional<size_t> key_idx = findKey(key);
		if (key_idx) {
			// Yes, the key exists then just update the value.
			buffer_[*key_idx].value = value;
			// Record the key as being updated.
			inserts_.push_back(std::move(key));
			return;
		}
		// Find first empty slot (or tombstoned) to insert.
		std::optional<size_t> idx = findFirstEmptyOrTombstone(key);
		if (!idx) {
			throw std::overflow_error("Buffer capacity exceeded");
		}

		buffer_[*idx] = {
			.occupied = true,
			.value = value,
		};
		// Copy the key.
		strcpy(buffer_[*idx].key, key.c_str());

		// Record the key as being updated.
		inserts_.push_back(std::move(key));

	}

	// Get
	//
	// Accept a std::string_view type to be more generic
	// than const std::string&.
	std::optional<int> get(std::string_view key) {
		std::optional<size_t> key_idx = findKey(key);
		if (key_idx) {
			return buffer_[*key_idx].value;
		}
		return std::nullopt;
	}

	// Remove
	//
	// Accept a std::string_view type to be more generic
	// than const std::string&.
	void remove(std::string_view key) {
		std::optional<size_t> key_idx = findKey(key);
		if (key_idx) {
			buffer_[*key_idx].tombstone = true;
			buffer_[*key_idx].occupied = false;
			// Reset the string.
			buffer_[*key_idx].key[0] = '\0';
		}
	}

	// Get_first.
	//
	// In order to implement get_first, we rely on insert()
	// method to build a "history" of key updates.
	// We then find the first key that still exists and return this
	// key and its value. On the way, keys that do not exist are
	// discarded.
	//
	// The complexity is O(1) amortized - each update inserts 
	// one element in `updates_`, hence for each update operation
	// we can pop only once.
	std::pair<std::string_view, int> get_first() {
		// Use the "history" of inserts and find the first
		// key that still has a value and return them.
		while (!inserts_.empty()) {
			const std::string key = inserts_.front();
			std::optional<int> value = get(key);

			if (value) {
				return std::make_pair(key, *value);
			} else {
				// If the key does no longer exist we drop it.
				inserts_.pop_front();
			}
		}
		throw std::invalid_argument("Container is empty");
	}

	// Get_last.
	//
	// Similar to get_first, but looking from the "back".
	std::pair<std::string_view, int> get_last() {
		while (!inserts_.empty()) {
			const std::string key = inserts_.back();
			std::optional<int> value = get(key);

			if (value) {
				return std::make_pair(key, *value);
			} else {
				// If the key does no longer exist we drop it.
				inserts_.pop_back();
			}
		}
		throw std::invalid_argument("Container is empty");
	}


private:

	// Finds the index in the buffer of the key, if it exists.
	// Returns std::nullopt otherwise.
	std::optional<size_t> findKey(std::string_view key) {
		int idx = hash_function_(key) % size_;
		for (int i = idx, cnt = 0; cnt != size_; i = (i + 1) % size_, cnt++) {
			// Neither containing a value, neither a tombstone, the element
			// does not exist.
			if (buffer_[i].occupied == false && buffer_[i].tombstone == false) {
				return std::nullopt;
			}
			if (buffer_[i].occupied && buffer_[i].key == key) {
				// The key exists.
				return i;
			}
			// Tombstone case: we just move forward.
		}
		return std::nullopt;
	}

	// Finds first index of an empty or tombstoned cell.
	std::optional<size_t> findFirstEmptyOrTombstone(std::string_view key) {
		int idx = hash_function_(key) % size_;
		
		for (int i = idx, cnt = 0; cnt != size_; i = (i + 1) % size_, cnt++) {
			if (buffer_[i].occupied == false || buffer_[i].tombstone) {
				return i;
			}
		}
		// Buffer is full.
		return std::nullopt;
	}


	int size_;
	std::vector<Entry> buffer_;

	HashFunction hash_function_;

	// We use a deque to build a "history" of updates. The deque in C++ is implemented
	// using a linked-list of arrays, which helps with cache locality.
	std::deque<std::string> inserts_;

};

void test_easy() {
	HashTable h(kWordCount);

	h.insert("a", 1);

	assert((*h.get("a") == 1));
	assert((h.get("b").has_value() == false));

	h.insert("a", 2);
	assert((*h.get("a") == 2));

	std::cout << "test_easy[DONE]" << std::endl;
}

void test_remove_in_the_middle() {
	// Inject poor hash function for testing purposes.
	HashTable h(kWordCount, [](const auto& s) { return 1; });

	for (int i = 0; i < 7; i++) {
		h.insert(std::to_string(i), i);
	}

	h.remove(std::to_string(2));
	h.remove(std::to_string(3));

	std::vector<int> result;
	for (int i = 0; i < 7; i++) {
		result.push_back(h.get(std::to_string(i)).value_or(-1));
	}
	assert ((result == std::vector<int>{0, 1, -1, -1, 4, 5, 6}));

	h.insert(std::to_string(3), 3);

	result.clear();
	for (int i = 0; i < 7; i++) {
		result.push_back(h.get(std::to_string(i)).value_or(-1));
	}

	assert ((result == std::vector<int>{0, 1, -1, 3, 4, 5, 6}));

	std::cout << "test_remove_in_the_middle[DONE]" << std::endl;
}

void test_first() {
	HashTable h(kWordCount, [](const auto& s) { return 1; });

	for (int i = 0; i < 10; i++) {
		h.insert(std::to_string(i), i);
	}

	assert ((h.get_first() == std::make_pair<std::string_view, int>("0", 0)));

	h.remove("0");
	h.remove("1");

	assert ((h.get_first() == std::make_pair<std::string_view, int>("2", 2)));

	std::cout << "test_first[DONE]" << std::endl;
}

void test_last() {
	HashTable h(kWordCount, [](const auto& s) { return 1; });

	for (int i = 0; i < 10; i++) {
		h.insert(std::to_string(i), i);
	}

	assert ((h.get_last() == std::make_pair<std::string_view, int>("9", 9)));

	h.remove("9");
	h.remove("8");

	assert ((h.get_last() == std::make_pair<std::string_view, int>("7", 7)));

	std::cout << "test_last[DONE]" << std::endl;
}


int main() {
	/*
	std::ifstream fin("words.txt");
	std::unordered_set<std::string> words;

	for (std::string word; fin >> word;) {
		words.insert(word);
	}*/
	// Read the words.
	
	test_easy();
	test_remove_in_the_middle();
	test_first();
	test_last();
	std::cout << "DONE" << std::endl;
	return 0;
}
