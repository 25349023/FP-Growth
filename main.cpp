#include <iostream>
#include <cstdlib>
#include <fstream>
#include <vector>
#include <tuple>
#include <map>
#include <set>
#include <unordered_set>
#include <algorithm>
#include <cmath>
#include <functional>
#include <iomanip>
#include <deque>


using Item = int;
using Transaction = std::vector<Item>;
using TransactionDB = std::vector<Transaction>;

using HeaderKey = std::pair<Item, int>;

struct FrequencyCmp {
    // sort key_pair<item, freq> by greater<freq>, less<item>
    bool operator()(const HeaderKey& lhs, const HeaderKey& rhs) const {
        if (lhs.second != rhs.second) {
            return lhs.second > rhs.second;
        } else {
            return lhs.first < rhs.first;
        }
    }
};

struct TreeNode;

using HeadTailPointer = std::pair<TreeNode*, TreeNode*>;
using HeaderTable = std::map<HeaderKey, HeadTailPointer, FrequencyCmp>;

using LightweightTree = std::pair<HeaderTable, TreeNode*>;

using Pattern = std::set<Item>;
using Patterns = std::vector<Pattern>;
using FrequentPatterns = std::map<std::set<Item>, double>;

std::ostream& operator<<(std::ostream& out, const Pattern& p);
std::ostream& operator<<(std::ostream& out, const Patterns& ps);
std::ostream& operator<<(std::ostream& out, const FrequentPatterns& fp);


/// <h1>Input</h1>

TransactionDB read_transaction_file(const std::string& in_fname) {
    TransactionDB transactions(1);
    std::ifstream fin(in_fname);
    fin >> std::noskipws;
    Item data;
    char delim;
    while (fin >> data >> delim) {
        transactions.back().push_back(data);

        if (delim == '\n') {
            transactions.emplace_back();
        }
    }
    if (transactions.back().empty()) {
        transactions.pop_back();
    }

    return transactions;
}

/// <h1>Tree Node</h1>

struct TreeNode {
    using NodeMapping = std::unordered_map<const TreeNode*, TreeNode*>;

    Item item;
    int count{1};
    TreeNode* left_child{};
    TreeNode* right_sibling{};
    TreeNode* cross_link{};
    TreeNode* parent{};

    explicit TreeNode(Item x, TreeNode* p = nullptr) : item(x), parent(p) {}

    TreeNode(const TreeNode& other) : item(other.item), count(other.count) {}

    // Add new node(x) to child, or increment the count if the node(x) already exists.
    // Then return the affected node and whether the insertion is successful or not
    std::pair<TreeNode*, bool> add_item_to_child(Item x) {
        if (!left_child) {
            left_child = new TreeNode(x, this);
            return std::make_pair(left_child, true);
        }

        TreeNode* prev = nullptr;
        for (auto child = left_child; child != nullptr; prev = child, child = child->right_sibling) {
            if (child->item == x) {
                child->count++;
                return std::make_pair(child, false);
            }
        }

        prev->right_sibling = new TreeNode(x, this);
        return std::make_pair(prev->right_sibling, true);
    }

    // return the root of the copied tree
    LightweightTree copy(HeaderTable ht) const {
        NodeMapping node_map;
        TreeNode* new_root = _copy_tree_edge(node_map, nullptr);
        _copy_cross_link(ht, node_map);

        return std::make_pair(std::move(ht), new_root);
    }

    // delete the subtree
    void prune_subtree() {
        delete left_child;
        left_child = nullptr;
    }

    // Return true if `x` not in any subtree and thus can be removed.
    // Does NOT handle cross-links, so after this action, the cross-links of
    // nodes whose item is not `x` will become invalid
    // FIXME: removing is incorrect, missing right sibling may happen.
    bool remove_subtrees_without(Item x) {
        if (!left_child) {
            return item != x;
        }

        bool all_children_without_x = true;
        TreeNode* prev{};
        for (auto child = left_child; child != nullptr;) {
            bool without_x = child->remove_subtrees_without(x);
            all_children_without_x = all_children_without_x && without_x;

            if (without_x) {
                auto temp = child->right_sibling;
                if (prev) {
                    prev->right_sibling = child->right_sibling;
                } else {
                    // child is the leftmost child
                    left_child = child->right_sibling;
                }
                child->right_sibling = nullptr;
//                std::cout << "prune " << *child << std::endl;
                delete child;
                child = temp;
            } else {
                prev = child;
                child = child->right_sibling;
            }
        }

        return all_children_without_x && item != x;
    }

    void append_to_child(TreeNode* target) {
        if (!target) {
            return;
        }
        if (!left_child) {
            left_child = target;
            return;
        }

        auto curr = left_child;
        for (; curr->right_sibling; curr = curr->right_sibling) {}
        curr->right_sibling = target;

        for (auto t = target; t; t = t->right_sibling) {
            t->parent = this;
        }
    }

    void merge_children() {
//        std::cout << "merge " << *this << " start\n";
        if (!left_child) {
            return;
        }

        std::map<Item, TreeNode*> exists;
        TreeNode* prev{};
        for (auto curr = left_child; curr;) {
            if (exists.find(curr->item) == exists.end()) {
                exists[curr->item] = curr;
                prev = curr;
                curr = curr->right_sibling;
            } else {
                // merge counts
                auto major_node = exists[curr->item];
                major_node->count += curr->count;
                major_node->append_to_child(curr->left_child);

                // remove curr from tree
                curr->left_child = nullptr;
                prev->right_sibling = curr->right_sibling;
                curr->right_sibling = nullptr;
                delete curr;
                curr = prev->right_sibling;
            }
        }
//        for (auto curr = left_child; curr; curr = curr->right_sibling) {
//            std::cout << *curr << ", ";
//        }
//        std::cout << std::endl;
//        std::cout << "merge end\n";
    }

    TreeNode* get_right_or_back() const {
        if (right_sibling) {
            return right_sibling;
        } else {
            return parent;
        }
    }

    friend std::ostream& operator<<(std::ostream& out, const TreeNode& tr) {
        out << tr.item << " : " << tr.count;
        return out;
    }

    ~TreeNode() {
//        std::cout << "delete " << this << " (" << *this << ")" << std::endl;
        delete left_child;
        delete right_sibling;
    }

private:
    TreeNode* _copy_tree_edge(NodeMapping& node_map, TreeNode* p) const {
        auto new_root = new TreeNode(*this);
        new_root->parent = p;
        node_map[this] = new_root;

        if (left_child) {
            new_root->left_child = left_child->_copy_tree_edge(node_map, new_root);
        }
        if (right_sibling) {
            new_root->right_sibling = right_sibling->_copy_tree_edge(node_map, p);
        }
        return new_root;
    }

    void _copy_cross_link(HeaderTable& ht, NodeMapping& node_map) const {
        // copy cross-links
        for (auto&[key, head_tail]: ht) {
            if (head_tail.first) {
                for (auto node = head_tail.first; node != nullptr;) {
                    const auto next = node->cross_link;
                    if (next) {
                        node_map[node]->cross_link = node_map[next];
                    }
                    node = next;
                }
                head_tail.first = node_map[head_tail.first];
                head_tail.second = node_map[head_tail.second];
            }
        }
    }

};

/// <h1>Utils</h1>

void combinations_with(
    Item x, int k, Pattern& current_pattern,
    const Pattern::const_iterator& start, const Pattern::const_iterator& end,
    Patterns& patterns) {
    if (k == 0) {
        patterns.push_back(current_pattern);
        return;
    }

    for (auto curr = start; curr != end; ++curr) {
        if (*curr == x) {
            continue;
        }

        current_pattern.insert(*curr);
        auto next = curr;
        ++next;
        combinations_with(x, k - 1, current_pattern, next, end, patterns);
        current_pattern.erase(*curr);
    }
}

FrequentPatterns expand_all_combinations(Item x, FrequentPatterns paths) {
    FrequentPatterns fp;
    for (const auto& path: paths) {
        for (int k = 0; k <= path.first.size(); ++k) {
            Patterns patterns;
            Pattern pat{x};
            combinations_with(x, k, pat, path.first.cbegin(), path.first.cend(), patterns);
            for (const auto& pattern: patterns) {
                fp[pattern] += path.second;
            }
        }
    }

    return fp;
}

/// <h1>FP Tree</h1>

class FPTree {
public:
    explicit FPTree(TransactionDB&& tr, double ms) :
        transactions(std::move(tr)), min_support_count(ms * transactions.size()) {
        build();
    }

    void build() {
        find_frequent_items();
        build_header_table();
        exclude_non_frequent_items();
        sort_transaction_items();
        construct_fp_tree();
    }

    FrequentPatterns mine_all() {
        FrequentPatterns result;
        for (const auto& item: frequent_items) {
            FPTree copied_fptree(*this);
            result.merge(copied_fptree.mine(item));
        }
        return result;
    }

    // mining will make some unrecoverable changes to the tree, so be sure that the tree is copied
    FrequentPatterns mine(Item x) {
        prune_by(x);
        refresh_counts(x);
//        construct_conditional_fp_tree(x);
        auto paths = find_all_pattern_paths(x);
//        for (const auto& path: paths) {
//            std::cout << path.first << " : " << path.second << std::endl;
//        }
        auto frequent_pattens = expand_all_combinations(x, paths);

        for (auto it = frequent_pattens.begin(); it != frequent_pattens.end();) {
            if ((*it).second < min_support_count) {
                it = frequent_pattens.erase(it);
            } else {
                ++it;
            }
        }


        return frequent_pattens;
    }

    void print_transaction_db() {
        for (const auto& transaction: transactions) {
            for (const auto& item: transaction) {
                std::cout << item << " ";
            }
            std::cout << std::endl;
        }
    }

    void print_header_table() {
        for (const auto& row: header_table) {
            std::cout << row.first.first << " : " << row.first.second << " -> "
                      << row.second.first << " ... " << row.second.second << std::endl;
        }
    }

    void traverse_cross_links() {
        for (const auto& links: header_table) {
            for (TreeNode* curr = links.second.first; curr != nullptr; curr = curr->cross_link) {
                std::cout << curr->item << " : " << curr->count << ", ";
            }
            std::cout << std::endl;
        }
    }


    // there is no need to copy transactions and frequent_items
    // since they will not participate in the mining stage
    FPTree(const FPTree& oth)
        : min_support_count(oth.min_support_count), item_counter(oth.item_counter) {
        std::tie(header_table, root) = oth.root->copy(oth.header_table);
    }

    ~FPTree() {
        delete root;
    }

private:
    void find_frequent_items();

    void build_header_table();

    void exclude_non_frequent_items();

    void sort_transaction_items();

    void construct_fp_tree();

public:
    void prune_by(Item x);

    void refresh_counts(Item x);

    void construct_conditional_fp_tree(Item x);

    FrequentPatterns find_all_pattern_paths(Item x);

    bool remove_one_child(TreeNode* node, Item x);

    bool is_frequent(const Item item) {
        return frequent_items.find(item) != frequent_items.end();
    }

    HeaderKey get_key(const Item x) {
        return std::make_pair(x, item_counter[x]);
    }

    bool frequent_than(const Item lhs, const Item rhs) {
        return FrequencyCmp()(get_key(lhs), get_key(rhs));
    }

    TransactionDB transactions;
    double min_support_count;
    std::map<Item, int> item_counter{};
    std::unordered_set<Item> frequent_items{};
    HeaderTable header_table{};
    TreeNode* root{};
    void remove_candidate(TreeNode* node, std::pair<TreeNode*, TreeNode*>& candidate) const;
};

void FPTree::find_frequent_items() {
    for (const auto& transaction: transactions) {
        for (const auto& item: transaction) {
            item_counter[item]++;
        }
    }
    for (auto[item, count]: item_counter) {
        if (count >= min_support_count) {
            frequent_items.insert(item);
        }
    }
}

void FPTree::build_header_table() {
    for (const auto& item: item_counter) {
        if (is_frequent(item.first)) {
            header_table[item] = std::make_pair(nullptr, nullptr);
        }
    }
}

void FPTree::exclude_non_frequent_items() {
    // erase non-frequent items from transaction DB
    for (auto& transaction: transactions) {
        transaction.erase(
            std::remove_if(transaction.begin(), transaction.end(),
                           [this](Item& x) { return !is_frequent(x); }),
            transaction.end());
    }

    // remove empty transactions from DB
    transactions.erase(
        std::remove_if(transactions.begin(), transactions.end(),
                       [](Transaction& t) { return t.empty(); }),
        transactions.end());
}

void FPTree::sort_transaction_items() {
    std::for_each(
        transactions.begin(), transactions.end(),
        [this](Transaction& t) {
            std::sort(t.begin(), t.end(),
                      [this](const Item a, const Item b) { return frequent_than(a, b); });
        }
    );
}

void FPTree::construct_fp_tree() {
    root = new TreeNode(-1);
    for (const auto& transaction: transactions) {
        TreeNode* curr = root;
        for (const auto& item: transaction) {
            auto[node, ok] = curr->add_item_to_child(item);

            // handle cross-links
            if (ok) {
//                std::cout << "add " << item << ", ";
                auto key = get_key(item);
                auto& ht_entry = header_table[key];
                if (!ht_entry.first) {
                    ht_entry.first = ht_entry.second = node;
                } else {
                    ht_entry.second->cross_link = node;
                    ht_entry.second = ht_entry.second->cross_link;
                }
            } else {
//                std::cout << "increase " << item << ", ";
            }

            curr = node;
        }
//        std::cout << std::endl;
    }
}

void FPTree::prune_by(Item x) {
    root->remove_subtrees_without(x);
}

void FPTree::refresh_counts(Item x) {
    auto links = header_table[get_key(x)];

    // refresh item counts in a bottom-up manner
    for (auto curr = links.first; curr; curr = curr->cross_link) {
        for (auto p = curr->parent; p; p = p->parent) {
            p->count = 0;
        }
    }
    for (auto curr = links.first; curr; curr = curr->cross_link) {
        for (auto p = curr->parent; p; p = p->parent) {
            p->count += curr->count;
        }
    }
}

void FPTree::construct_conditional_fp_tree(Item x) {
    std::deque<TreeNode*> nodes_to_be_check;
    nodes_to_be_check.emplace_back(root);

    while (!nodes_to_be_check.empty()) {
        auto node = nodes_to_be_check.front();
        do {
            node->merge_children();
        } while (remove_one_child(node, x));

        for (auto child = node->left_child; child; child = child->right_sibling) {
            nodes_to_be_check.push_back(child);
        }
        nodes_to_be_check.pop_front();
    }
}

bool FPTree::remove_one_child(TreeNode* node, Item x) {
    if (!node->left_child) {
        return false;
    }

    // pair<prev, curr>
    std::pair<TreeNode*, TreeNode*> candidate;

    TreeNode* prev{};
//    TreeNode* most_frequent{node->left_child};
    for (auto curr = node->left_child; curr; prev = curr, curr = curr->right_sibling) {
//        if (frequent_than(curr->item, most_frequent->item)) {
//            most_frequent = curr;
//        }
        if (curr->item != x && curr->count < min_support_count) {
            if (!candidate.second ||
                frequent_than(curr->item, candidate.second->item)) {
                candidate = std::make_pair(prev, curr);
            }
        }
    }
    if (candidate.second) {
        //*
//        std::cout << "candidate " << *candidate.second << std::endl;
//        for (auto ch = candidate.second->left_child; ch; ch = ch->right_sibling) {
//            std::cout << *ch << ", ";
//        }
//        std::cout << std::endl;
         //*/
        remove_candidate(node, candidate);
//                std::cout << "remove " << candidate.second << std::endl;
//                  << " with prev = " << *candidate.first << std::endl;
        return true;
    }

    return false;
}

void FPTree::remove_candidate(TreeNode* node, std::pair<TreeNode*, TreeNode*>& candidate) const {
    if (candidate.first) {
        candidate.first->right_sibling = candidate.second->right_sibling;
    } else {
        node->left_child = candidate.second->right_sibling;
    }
    node->append_to_child(candidate.second->left_child);
    candidate.second->left_child = nullptr;
    candidate.second->right_sibling = nullptr;
    delete candidate.second;
}

FrequentPatterns FPTree::find_all_pattern_paths(Item x) {
    std::deque<TreeNode*> visiting_stack;
    FrequentPatterns paths;

    auto curr = root;
    while (curr) {
        // if returning from left_child, then
        // go to the right_sibling or back to the parent
        if (!visiting_stack.empty() && curr == visiting_stack.back()) {
            visiting_stack.pop_back();
            curr = curr->get_right_or_back();
            continue;
        }

        visiting_stack.push_back(curr);
        if (curr->left_child) {
            curr = curr->left_child;
            continue;
        }

        Pattern pat;
        for (auto it = visiting_stack.cbegin() + 1; it != visiting_stack.cend(); ++it) {
//            if ((*it)->item != x) {
            pat.insert((*it)->item);
//            std::cout << **it << std::endl;
//            }
        }
        paths[pat] += curr->count;

        visiting_stack.pop_back();
        curr = curr->get_right_or_back();
    }
    return paths;
}



/// <h1>Output Formatting</h1>

std::ostream& operator<<(std::ostream& out, const Pattern& p) {
    out << "{ ";
    for (const auto& item: p) {
        out << item << ", ";
    }
    out << "}";
    return out;
}

std::ostream& operator<<(std::ostream& out, const Patterns& ps) {
    for (const auto& p: ps) {
        out << p << std::endl;
    }
    return out;
}

std::ostream& operator<<(std::ostream& out, const FrequentPatterns& fp) {
    for (const auto& p: fp) {
        out << p.first << " : " << p.second << std::endl;
    }
    return out;
}


int main(int argc, char** argv) {
    if (argc != 4) {
        std::cerr << "invalid argument!" << std::endl;
        std::exit(1);
    }

    double min_support = std::strtod(argv[1], nullptr);
    std::string in_filename = std::string(argv[2]);
    std::string out_filename = std::string(argv[3]);

    auto&& transactions = read_transaction_file(in_filename);
    FPTree fp_tree(std::move(transactions), min_support);
//    std::cout << "min supp count: " << fp_tree.min_support_count << std::endl;
//    fp_tree.prune_by(6);
//    fp_tree.refresh_counts(6);
//    fp_tree.construct_conditional_fp_tree(3);
//    auto t = fp_tree.find_all_pattern_paths(6);
//    std::cout << t;

    auto fps = fp_tree.mine_all();
//    auto fps = fp_tree.mine(8);
//    std::cout << fps.size() << std::endl << fps;

//    Patterns ps;
//    Pattern p{1, 4, 6, 3}, p2{8, 3}, empty{3};
//    FrequentPatterns fp;
//    fp[p] = 2;
//    fp[p2] = 3;
//    combinations_with(0, 2, empty, p.cbegin(), p.cend(), ps);
//    auto t = expand_all_combinations(3, fp);
//    std::cout << t;

//    fp_tree.print_transaction_db();
//    std::cout << std::endl;
//    fp_tree.print_header_table();
//    std::cout << std::endl;
//    fp_tree.traverse_cross_links();
//
//    std::cout << std::endl;
//    FPTree copy{fp_tree};
//    copy.print_header_table();
//    std::cout << std::endl;
//    copy.traverse_cross_links();

    return 0;
}
