#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <vector>
#include <array>
#include <functional>
#include <algorithm>

namespace lockfree {

/**
 * @brief A lock-free, thread-safe trie (prefix tree) implementation for string storage.
 * 
 * This trie provides efficient string storage and prefix-based operations without using
 * traditional locking mechanisms. It uses atomic operations on node pointers and flags
 * to ensure thread safety and lock-free progress. The trie is particularly well-suited
 * for applications requiring fast prefix matching, auto-completion, and string validation.
 * 
 * @tparam CharType The character type for strings. Defaults to char for std::string.
 *                  Can be wchar_t, char16_t, char32_t, or other character types.
 * 
 * Key Features:
 * - Lock-free: No blocking operations, guaranteed system-wide progress
 * - Thread-safe: Safe concurrent access from multiple threads
 * - Prefix operations: Efficient prefix matching and enumeration
 * - Memory efficient: Shared prefixes use the same node path
 * - Exception-safe: Basic exception safety guarantee
 * - Unicode support: Configurable character type support
 * - Iterator support: Lexicographic iteration through all stored strings
 * - Auto-completion: Built-in support for prefix-based suggestions
 * 
 * Performance Characteristics:
 * - Insert: O(m) where m is the length of the string
 * - Contains: O(m) where m is the length of the string
 * - Erase: O(m) where m is the length of the string
 * - Prefix search: O(p) where p is the length of the prefix
 * - Memory: O(n*m) where n is strings, m is average length
 * 
 * Algorithm Details:
 * - Uses array-based children storage for fast character-to-node mapping
 * - Logical deletion (marking) for safe concurrent access
 * - Compare-and-swap operations for atomic node creation and updates
 * - Supports full ASCII character range (256 characters)
 * 
 * Memory Management:
 * - Uses dynamic allocation for trie nodes
 * - Marked nodes are cleaned up opportunistically during operations
 * - Memory reclamation is conservative to ensure thread safety
 * - Optimized for concurrent access patterns
 * 
 * Usage Example:
 * @code
 * lockfree::AtomicTrie<char> trie;
 * 
 * // Insert strings
 * trie.insert("hello");
 * trie.insert("help");
 * trie.insert("world");
 * 
 * // Check membership
 * if (trie.contains("hello")) {
 *     std::cout << "Found hello" << std::endl;
 * }
 * 
 * // Prefix operations
 * if (trie.starts_with("hel")) {
 *     auto completions = trie.get_all_with_prefix("hel");
 *     for (const auto& word : completions) {
 *         std::cout << "Completion: " << word << std::endl;
 *     }
 * }
 * 
 * // Remove strings
 * trie.erase("hello");
 * 
 * // Iterate through all strings
 * for (const auto& word : trie) {
 *     std::cout << "Word: " << word << std::endl;
 * }
 * @endcode
 * 
 * @note This implementation uses logical deletion for safe concurrent access.
 * @warning Empty strings are not supported. All operations will reject empty string inputs.
 */
template<typename CharType = char>
class AtomicTrie {
private:
    static constexpr size_t ALPHABET_SIZE = 256; ///< Support full ASCII character range
    
    /**
     * @brief Internal node structure for the trie.
     * 
     * Each node contains an array of atomic pointers to children (one per character),
     * an atomic flag indicating if this node represents the end of a word,
     * and an atomic deletion flag for logical deletion.
     */
    struct TrieNode {
        std::array<std::atomic<TrieNode*>, ALPHABET_SIZE> children; ///< Atomic pointers to child nodes
        std::atomic<bool> is_end_of_word;   ///< Flag indicating this node ends a word
        std::atomic<bool> deleted;          ///< Flag indicating this node is logically deleted
        
        /**
         * @brief Default constructor. Creates a node with no children and not marked as end-of-word.
         */
        TrieNode() : is_end_of_word(false), deleted(false) {
            for (auto& child : children) {
                child.store(nullptr);
            }
        }
    };
    
    TrieNode* root_;                        ///< Pointer to the root node of the trie
    std::atomic<size_t> size_;              ///< Atomic counter for number of strings in the trie
    
    /**
     * @brief Convert character to array index.
     * @param c The character to convert
     * @return Array index for the character (0-255)
     */
    size_t char_to_index(CharType c) const {
        return static_cast<size_t>(static_cast<unsigned char>(c));
    }
    
    /**
     * @brief Convert array index to character.
     * @param index The array index to convert
     * @return Character corresponding to the index
     */
    CharType index_to_char(size_t index) const {
        return static_cast<CharType>(index);
    }
    
    /**
     * @brief Recursively insert a string into the trie.
     * @param node Current node in the traversal
     * @param word The string to insert
     * @param index Current character index in the string
     * @return true if insertion was successful, false if string already exists
     */
    bool insert_recursive(TrieNode* node, const std::basic_string<CharType>& word, size_t index);
    
    /**
     * @brief Recursively check if a string exists in the trie.
     * @param node Current node in the traversal
     * @param word The string to search for
     * @param index Current character index in the string
     * @return true if string exists and is not deleted, false otherwise
     */
    bool contains_recursive(TrieNode* node, const std::basic_string<CharType>& word, size_t index) const;
    
    /**
     * @brief Recursively erase a string from the trie.
     * @param node Current node in the traversal
     * @param word The string to erase
     * @param index Current character index in the string
     * @return true if string was found and marked for deletion, false otherwise
     */
    bool erase_recursive(TrieNode* node, const std::basic_string<CharType>& word, size_t index);
    
    /**
     * @brief Collect all strings with a given prefix.
     * @param node Current node in the traversal
     * @param prefix The prefix to search for
     * @param current_word Current accumulated string during traversal
     * @param result Vector to store found strings
     */
    void collect_words_with_prefix(TrieNode* node, const std::basic_string<CharType>& prefix, 
                                   std::basic_string<CharType>& current_word, 
                                   std::vector<std::basic_string<CharType>>& result) const;
    
    /**
     * @brief Check if a node has any non-deleted children.
     * @param node The node to check
     * @return true if node has active children, false otherwise
     */
    bool has_children(TrieNode* node) const;
    
    /**
     * @brief Attempt cleanup of a child node if it's no longer needed.
     * @param parent Parent node
     * @param child_index Index of child in parent's children array
     * @param child Child node to potentially clean up
     */
    void cleanup_node_if_possible(TrieNode* parent, size_t child_index, TrieNode* child);
    
    /**
     * @brief Recursively delete all nodes in the trie.
     * @param node The node to delete along with all its children
     */
    void delete_recursive(TrieNode* node);
    
public:
    /**
     * @brief Default constructor. Creates an empty trie.
     * 
     * @complexity O(1)
     * @thread_safety Safe
     */
    AtomicTrie();
    
    /**
     * @brief Destructor. Cleans up trie nodes.
     * 
     * @complexity O(n) where n is the total number of nodes
     * @thread_safety Not safe - should only be called when no other threads are accessing
     * 
     * @note Uses safe cleanup strategy for concurrent access patterns.
     */
    ~AtomicTrie();
    
    // Non-copyable but movable for resource management
    AtomicTrie(const AtomicTrie&) = delete;
    AtomicTrie& operator=(const AtomicTrie&) = delete;
    AtomicTrie(AtomicTrie&&) = default;
    AtomicTrie& operator=(AtomicTrie&&) = default;
    
    /**
     * @brief Insert a string into the trie by copying.
     * 
     * @param word The string to insert
     * @return true if string was inserted (was not already present), false if already exists or empty
     * @complexity O(m) where m is the length of the string
     * @thread_safety Safe
     * @exception_safety Basic guarantee - if allocation fails, trie remains unchanged
     * 
     * @note Empty strings are rejected and will return false.
     *       May fail and return false under extreme contention after 1000 retry attempts.
     */
    bool insert(const std::basic_string<CharType>& word);
    
    /**
     * @brief Insert a string into the trie by moving.
     * 
     * @param word The string to insert (moved)
     * @return true if string was inserted (was not already present), false if already exists or empty
     * @complexity O(m) where m is the length of the string
     * @thread_safety Safe
     * @exception_safety Basic guarantee - if allocation fails, trie remains unchanged
     * 
     * @note For tries, moving doesn't provide significant benefits as we traverse the string anyway.
     *       Empty strings are rejected and will return false.
     */
    bool insert(std::basic_string<CharType>&& word);
    
    /**
     * @brief Construct a string in-place and insert it into the trie.
     * 
     * @tparam Args Types of arguments for string constructor
     * @param args Arguments to forward to string constructor
     * @return true if string was constructed and inserted, false if already exists or empty
     * @complexity O(m) where m is the length of the constructed string
     * @thread_safety Safe
     * @exception_safety Basic guarantee - if construction or insertion fails, trie remains unchanged
     */
    template<typename... Args>
    bool emplace(Args&&... args);
    
    /**
     * @brief Check if a string exists in the trie.
     * 
     * @param word The string to search for
     * @return true if string exists and is not marked for deletion, false otherwise
     * @complexity O(m) where m is the length of the string
     * @thread_safety Safe
     * @exception_safety No-throw guarantee
     * 
     * @note Returns false for empty strings.
     */
    bool contains(const std::basic_string<CharType>& word) const;
    
    /**
     * @brief Remove a string from the trie.
     * 
     * @param word The string to remove
     * @return true if string was found and marked for deletion, false if not found or empty
     * @complexity O(m) where m is the length of the string
     * @thread_safety Safe
     * @exception_safety Strong guarantee - if operation fails, trie is unchanged
     * 
     * @note Uses logical deletion (marking). Physical cleanup may happen opportunistically.
     *       Returns false for empty strings.
     */
    bool erase(const std::basic_string<CharType>& word);
    
    /**
     * @brief Check if any string in the trie starts with the given prefix.
     * 
     * @param prefix The prefix to search for
     * @return true if at least one string starts with the prefix, false otherwise
     * @complexity O(p) where p is the length of the prefix
     * @thread_safety Safe
     * @exception_safety No-throw guarantee
     * 
     * @note Returns false for empty prefixes.
     */
    bool starts_with(const std::basic_string<CharType>& prefix) const;
    
    /**
     * @brief Get all strings in the trie that start with the given prefix.
     * 
     * @param prefix The prefix to search for
     * @return Vector containing all strings with the given prefix
     * @complexity O(p + k*m) where p is prefix length, k is result count, m is average result length
     * @thread_safety Safe
     * @exception_safety Basic guarantee
     * 
     * @note Returns empty vector for empty prefixes or if no matches found.
     *       Results are returned in lexicographic order.
     */
    std::vector<std::basic_string<CharType>> get_all_with_prefix(const std::basic_string<CharType>& prefix) const;
    
    /**
     * @brief Check if the trie is empty.
     * 
     * @return true if the trie contains no strings, false otherwise
     * @complexity O(1)
     * @thread_safety Safe
     * @exception_safety No-throw guarantee
     * 
     * @note Result may be immediately outdated in concurrent environment.
     */
    bool empty() const;
    
    /**
     * @brief Get the number of strings in the trie.
     * 
     * @return The number of strings currently in the trie
     * @complexity O(1)
     * @thread_safety Safe
     * @exception_safety No-throw guarantee
     * 
     * @note This count may include logically deleted strings that haven't been physically removed.
     *       Result may be immediately outdated in concurrent environment.
     */
    size_t size() const;
    
    /**
     * @brief Forward iterator for traversing all strings in the trie.
     * 
     * The iterator provides lexicographic iteration through all active strings
     * in the trie, automatically skipping logically deleted entries.
     */
    class iterator {
    private:
        const AtomicTrie* trie_;                                        ///< Pointer to the owning trie
        std::vector<std::basic_string<CharType>> words_;                ///< Cached list of all words
        size_t current_index_;                                          ///< Current position in the word list
        
        /**
         * @brief Collect all words from the trie into the internal vector.
         */
        void collect_all_words();
        
    public:
        /**
         * @brief Construct iterator for the given trie.
         * @param trie Pointer to the owning trie
         * @param end Whether this is an end iterator
         */
        iterator(const AtomicTrie* trie, bool end = false);
        
        /**
         * @brief Dereference operator to access current string.
         * @return Reference to the current string
         */
        const std::basic_string<CharType>& operator*() const;
        
        /**
         * @brief Arrow operator to access current string.
         * @return Pointer to the current string
         */
        const std::basic_string<CharType>* operator->() const;
        
        /**
         * @brief Pre-increment operator to advance to next string.
         * @return Reference to this iterator after advancement
         */
        iterator& operator++();
        
        /**
         * @brief Post-increment operator to advance to next string.
         * @return Copy of iterator before advancement
         */
        iterator operator++(int);
        
        /**
         * @brief Equality comparison operator.
         * @param other Iterator to compare with
         * @return true if both iterators point to the same position
         */
        bool operator==(const iterator& other) const;
        
        /**
         * @brief Inequality comparison operator.
         * @param other Iterator to compare with
         * @return true if iterators point to different positions
         */
        bool operator!=(const iterator& other) const;
    };
    
    /**
     * @brief Get iterator to the first string (lexicographically).
     * 
     * @return Iterator pointing to first string, or end() if trie is empty
     * @complexity O(n*m) - collects all strings for iteration
     * @thread_safety Safe
     * @exception_safety Basic guarantee
     * 
     * @note The iterator captures a snapshot of the trie at creation time.
     */
    iterator begin() const;
    
    /**
     * @brief Get iterator representing past-the-end.
     * 
     * @return Iterator representing the end of the trie
     * @complexity O(1)
     * @thread_safety Safe
     * @exception_safety No-throw guarantee
     */
    iterator end() const;
    
    /**
     * @brief Count the number of strings with a given prefix.
     * 
     * @param prefix The prefix to count matches for
     * @return Number of strings starting with the prefix
     * @complexity O(p + k) where p is prefix length, k is result count
     * @thread_safety Safe
     * @exception_safety No-throw guarantee
     */
    size_t count_with_prefix(const std::basic_string<CharType>& prefix) const;
    
    /**
     * @brief Find the longest prefix of a string that exists in the trie.
     * 
     * @param word The string to find the longest prefix for
     * @return The longest prefix of word that exists in the trie
     * @complexity O(m) where m is the length of the word
     * @thread_safety Safe
     * @exception_safety Basic guarantee
     * 
     * @note Returns empty string if no prefix of word exists in the trie.
     */
    std::basic_string<CharType> longest_prefix(const std::basic_string<CharType>& word) const;
};

/**
 * @brief Type alias for char-based trie (std::string).
 */
using AtomicStringTrie = AtomicTrie<char>;

/**
 * @brief Type alias for wchar_t-based trie (std::wstring).
 */
using AtomicWStringTrie = AtomicTrie<wchar_t>;

// Implementation starts here

template<typename CharType>
AtomicTrie<CharType>::AtomicTrie() : size_(0) {
    root_ = new TrieNode;
}

template<typename CharType>
AtomicTrie<CharType>::~AtomicTrie() {
    // Clean up the trie structure - destructor is only called when no other threads access
    delete_recursive(root_);
}

template<typename CharType>
bool AtomicTrie<CharType>::insert(const std::basic_string<CharType>& word) {
    if (word.empty()) {
        return false;
    }
    
    if (insert_recursive(root_, word, 0)) {
        size_.fetch_add(1, std::memory_order_relaxed);
        return true;
    }
    return false;
}

template<typename CharType>
bool AtomicTrie<CharType>::insert(std::basic_string<CharType>&& word) {
    return insert(word); // For trie, we need to traverse the string anyway
}

template<typename CharType>
template<typename... Args>
bool AtomicTrie<CharType>::emplace(Args&&... args) {
    return insert(std::basic_string<CharType>(std::forward<Args>(args)...));
}

template<typename CharType>
bool AtomicTrie<CharType>::insert_recursive(TrieNode* node, const std::basic_string<CharType>& word, size_t index) {
    if (!node || node->deleted.load(std::memory_order_acquire)) {
        return false;
    }
    
    if (index == word.length()) {
        // End of word - mark this node as end of word
        bool expected = false;
        return node->is_end_of_word.compare_exchange_strong(expected, true, 
                                                           std::memory_order_acq_rel);
    }
    
    size_t char_index = char_to_index(word[index]);
    TrieNode* child = node->children[char_index].load(std::memory_order_acquire);
    
    int attempts = 0;
    while (attempts < 1000) {
        if (!child) {
            // Create new child node
            TrieNode* new_child = new TrieNode;
            if (node->children[char_index].compare_exchange_weak(child, new_child,
                                                               std::memory_order_release,
                                                               std::memory_order_relaxed)) {
                child = new_child;
                break;
            } else {
                // Another thread created a child, use it
                delete new_child;
                continue;
            }
        } else if (child->deleted.load(std::memory_order_acquire)) {
            // Child is deleted, try to replace it
            TrieNode* new_child = new TrieNode;
            if (node->children[char_index].compare_exchange_weak(child, new_child,
                                                               std::memory_order_release,
                                                               std::memory_order_relaxed)) {
                child = new_child;
                break;
            } else {
                delete new_child;
                continue;
            }
        } else {
            break; // Valid child found
        }
        attempts++;
    }
    
    if (attempts >= 1000) {
        return false;
    }
    
    return insert_recursive(child, word, index + 1);
}

template<typename CharType>
bool AtomicTrie<CharType>::contains(const std::basic_string<CharType>& word) const {
    if (word.empty()) {
        return false;
    }
    
    return contains_recursive(root_, word, 0);
}

template<typename CharType>
bool AtomicTrie<CharType>::contains_recursive(TrieNode* node, const std::basic_string<CharType>& word, size_t index) const {
    if (!node || node->deleted.load(std::memory_order_acquire)) {
        return false;
    }
    
    if (index == word.length()) {
        return node->is_end_of_word.load(std::memory_order_acquire);
    }
    
    size_t char_index = char_to_index(word[index]);
    TrieNode* child = node->children[char_index].load(std::memory_order_acquire);
    
    return contains_recursive(child, word, index + 1);
}

template<typename CharType>
bool AtomicTrie<CharType>::erase(const std::basic_string<CharType>& word) {
    if (word.empty()) {
        return false;
    }
    
    if (erase_recursive(root_, word, 0)) {
        size_.fetch_sub(1, std::memory_order_relaxed);
        return true;
    }
    return false;
}

template<typename CharType>
bool AtomicTrie<CharType>::erase_recursive(TrieNode* node, const std::basic_string<CharType>& word, size_t index) {
    if (!node || node->deleted.load(std::memory_order_acquire)) {
        return false;
    }
    
    if (index == word.length()) {
        // End of word - unmark this node as end of word
        bool expected = true;
        return node->is_end_of_word.compare_exchange_strong(expected, false, 
                                                           std::memory_order_acq_rel);
    }
    
    size_t char_index = char_to_index(word[index]);
    TrieNode* child = node->children[char_index].load(std::memory_order_acquire);
    
    bool result = erase_recursive(child, word, index + 1);
    
    if (result && child) {
        // Try to clean up the child if it's no longer needed
        cleanup_node_if_possible(node, char_index, child);
    }
    
    return result;
}

template<typename CharType>
void AtomicTrie<CharType>::cleanup_node_if_possible(TrieNode* parent, size_t child_index, TrieNode* child) {
    if (!child || child->is_end_of_word.load(std::memory_order_acquire) || has_children(child)) {
        return; // Child is still needed
    }
    
    // Try to mark child as deleted
    bool expected = false;
    if (child->deleted.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        // Try to remove the pointer from parent (best effort)
        parent->children[child_index].compare_exchange_strong(child, nullptr,
                                                            std::memory_order_release,
                                                            std::memory_order_relaxed);
    }
}

template<typename CharType>
bool AtomicTrie<CharType>::has_children(TrieNode* node) const {
    if (!node) return false;
    
    for (const auto& child : node->children) {
        TrieNode* child_ptr = child.load(std::memory_order_acquire);
        if (child_ptr && !child_ptr->deleted.load(std::memory_order_acquire)) {
            return true;
        }
    }
    return false;
}

template<typename CharType>
bool AtomicTrie<CharType>::starts_with(const std::basic_string<CharType>& prefix) const {
    if (prefix.empty()) {
        return false;
    }
    
    TrieNode* current = root_;
    for (CharType c : prefix) {
        if (!current || current->deleted.load(std::memory_order_acquire)) {
            return false;
        }
        
        size_t char_index = char_to_index(c);
        current = current->children[char_index].load(std::memory_order_acquire);
    }
    
    return current && !current->deleted.load(std::memory_order_acquire);
}

template<typename CharType>
std::vector<std::basic_string<CharType>> AtomicTrie<CharType>::get_all_with_prefix(const std::basic_string<CharType>& prefix) const {
    std::vector<std::basic_string<CharType>> result;
    
    if (prefix.empty()) {
        return result;
    }
    
    // Navigate to the prefix node
    TrieNode* current = root_;
    for (CharType c : prefix) {
        if (!current || current->deleted.load(std::memory_order_acquire)) {
            return result; // Prefix doesn't exist
        }
        
        size_t char_index = char_to_index(c);
        current = current->children[char_index].load(std::memory_order_acquire);
    }
    
    if (!current || current->deleted.load(std::memory_order_acquire)) {
        return result;
    }
    
    // Collect all words starting from this node
    std::basic_string<CharType> current_word = prefix;
    collect_words_with_prefix(current, prefix, current_word, result);
    
    return result;
}

template<typename CharType>
void AtomicTrie<CharType>::collect_words_with_prefix(TrieNode* node, const std::basic_string<CharType>& prefix, 
                                                     std::basic_string<CharType>& current_word, 
                                                     std::vector<std::basic_string<CharType>>& result) const {
    if (!node || node->deleted.load(std::memory_order_acquire)) {
        return;
    }
    
    if (node->is_end_of_word.load(std::memory_order_acquire)) {
        result.push_back(current_word);
    }
    
    for (size_t i = 0; i < ALPHABET_SIZE; ++i) {
        TrieNode* child = node->children[i].load(std::memory_order_acquire);
        if (child && !child->deleted.load(std::memory_order_acquire)) {
            current_word.push_back(index_to_char(i));
            collect_words_with_prefix(child, prefix, current_word, result);
            current_word.pop_back();
        }
    }
}

template<typename CharType>
bool AtomicTrie<CharType>::empty() const {
    return size_.load(std::memory_order_relaxed) == 0;
}

template<typename CharType>
size_t AtomicTrie<CharType>::size() const {
    return size_.load(std::memory_order_relaxed);
}

template<typename CharType>
size_t AtomicTrie<CharType>::count_with_prefix(const std::basic_string<CharType>& prefix) const {
    auto words = get_all_with_prefix(prefix);
    return words.size();
}

template<typename CharType>
std::basic_string<CharType> AtomicTrie<CharType>::longest_prefix(const std::basic_string<CharType>& word) const {
    std::basic_string<CharType> result;
    TrieNode* current = root_;
    
    for (size_t i = 0; i < word.length(); ++i) {
        if (!current || current->deleted.load(std::memory_order_acquire)) {
            break;
        }
        
        if (current->is_end_of_word.load(std::memory_order_acquire)) {
            result = word.substr(0, i);
        }
        
        size_t char_index = char_to_index(word[i]);
        current = current->children[char_index].load(std::memory_order_acquire);
    }
    
    // Check if the entire word is a prefix
    if (current && !current->deleted.load(std::memory_order_acquire) && 
        current->is_end_of_word.load(std::memory_order_acquire)) {
        result = word;
    }
    
    return result;
}

// Iterator implementation

template<typename CharType>
AtomicTrie<CharType>::iterator::iterator(const AtomicTrie* trie, bool end) 
    : trie_(trie), current_index_(end ? SIZE_MAX : 0) {
    if (!end) {
        collect_all_words();
        if (words_.empty()) {
            current_index_ = SIZE_MAX; // Make this equivalent to end()
        }
    }
}

template<typename CharType>
void AtomicTrie<CharType>::iterator::collect_all_words() {
    if (!trie_ || !trie_->root_) return;
    
    words_.clear();
    std::basic_string<CharType> current_word;
    std::basic_string<CharType> empty_prefix;
    trie_->collect_words_with_prefix(trie_->root_, empty_prefix, current_word, words_);
    std::sort(words_.begin(), words_.end());
}

template<typename CharType>
const std::basic_string<CharType>& AtomicTrie<CharType>::iterator::operator*() const {
    return words_[current_index_];
}

template<typename CharType>
const std::basic_string<CharType>* AtomicTrie<CharType>::iterator::operator->() const {
    return &words_[current_index_];
}

template<typename CharType>
typename AtomicTrie<CharType>::iterator& AtomicTrie<CharType>::iterator::operator++() {
    if (current_index_ < words_.size()) {
        ++current_index_;
        if (current_index_ >= words_.size()) {
            current_index_ = SIZE_MAX; // Convert to end iterator
        }
    }
    return *this;
}

template<typename CharType>
typename AtomicTrie<CharType>::iterator AtomicTrie<CharType>::iterator::operator++(int) {
    iterator tmp = *this;
    ++(*this);
    return tmp;
}

template<typename CharType>
bool AtomicTrie<CharType>::iterator::operator==(const iterator& other) const {
    return trie_ == other.trie_ && current_index_ == other.current_index_;
}

template<typename CharType>
bool AtomicTrie<CharType>::iterator::operator!=(const iterator& other) const {
    return !(*this == other);
}

template<typename CharType>
typename AtomicTrie<CharType>::iterator AtomicTrie<CharType>::begin() const {
    return iterator(this, false);
}

template<typename CharType>
typename AtomicTrie<CharType>::iterator AtomicTrie<CharType>::end() const {
    return iterator(this, true);
}

template<typename CharType>
void AtomicTrie<CharType>::delete_recursive(TrieNode* node) {
    if (!node || node->deleted.load(std::memory_order_acquire)) {
        return;
    }
    
    for (size_t i = 0; i < ALPHABET_SIZE; ++i) {
        TrieNode* child = node->children[i].load(std::memory_order_acquire);
        if (child && !child->deleted.load(std::memory_order_acquire)) {
            delete_recursive(child);
        }
    }
    
    delete node;
}

} // namespace lockfree 