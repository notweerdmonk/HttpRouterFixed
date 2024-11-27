#ifndef HTTPROUTER_HPP
#define HTTPROUTER_HPP

#include <map>
#include <functional>
#include <vector>
#include <cstring>
#include <iostream>
#include <cstdlib>

// placeholder
struct string_view {
    const char *data;
    std::size_t length;
};

std::ostream &operator<<(std::ostream &os, string_view &s) {
    os << std::string(s.data, s.length);
    return os;
}

template <class USERDATA>
class HttpRouter {
private:
    std::vector<std::function<void(USERDATA, std::vector<string_view> &)>> handlers;
    std::vector<string_view> params;

    using frame_type = struct {
        const char *segptr;
        const char *nodeptr;
        std::size_t params_idx;
    };

    template <typename frame>
    class stack {
        frame *data_;
        std::size_t capacity_;
        std::size_t idx_;

        enum { BLOCK_SIZE = 64 };

        frame* alloc(frame *&mem, std::size_t nbytes) {
            mem = reinterpret_cast<frame*>(realloc(data_, nbytes));
            if (!mem) {
                throw std::runtime_error("Could not allocate memory");
            }
            return mem;
        }

        public:
        stack(std::size_t capacity = BLOCK_SIZE)
            : data_(nullptr), capacity_(capacity), idx_(0) {

            (void)alloc(data_, capacity_ * sizeof(frame));
        }

        ~stack() {
            free(data_);
        }

        bool empty() const {
            return idx_ == 0;
        }

        std::size_t size() const {
            return idx_;
        }

        void push(const frame &f) {
            if (idx_ == capacity_) {
                (void)alloc(data_, (capacity_ <<= 1) * sizeof(frame));
            }

            data_[idx_++] = f;
        }

        frame& top() const {
            if (empty()) {
                return frame();
            }

            return data_[idx_ - 1];
        }

        frame pop() {
            if (empty()) {
                return frame();
            }

            return data_[--idx_];
        }

        void clear() {
            idx_ = 0;
        }
    };

    using stack_type = stack<frame_type>;

    struct Node {
        std::string name;
        std::map<std::string, Node *> children;
        short handler;
    };

    Node *tree = new Node({"", {}, -1});
    std::string compiled_tree;
    stack_type s;

    void free_children(Node *parent) {
        if (!parent) {
            return;
        }

        for (auto &child : parent->children) {
            free_children(child.second);
        }

        delete parent;
    }

    void add(std::vector<std::string> route, short handler) {
        Node *parent = tree;
        for (std::string node : route) {
            if (parent->children.find(node) == parent->children.end()) {
                parent->children[node] = new Node({node, {}, handler});
            }
            parent = parent->children[node];
        }
    }

    unsigned short compile_tree(Node *n) {
        unsigned short nodeLength = 6 + n->name.length();
        for (auto c : n->children) {
            nodeLength += compile_tree(c.second);
        }

        unsigned short nodeNameLength = n->name.length();

        std::string compiledNode;
        compiledNode.append((char *) &nodeLength, sizeof(nodeLength));
        compiledNode.append((char *) &nodeNameLength, sizeof(nodeNameLength));
        compiledNode.append((char *) &n->handler, sizeof(n->handler));
        compiledNode.append(n->name.data(), n->name.length());

        compiled_tree = compiledNode + compiled_tree;
        return nodeLength;
    }

    inline unsigned short node_length(const char *node) {
        return *(unsigned short *)node;
    }

    inline unsigned short node_name_length(const char *node) {
        return *(unsigned short *)&node[2];
    }

    inline bool match_node(const char *candidate, const char *name,
            std::size_t name_length, std::size_t &params_idx) {

        unsigned short nodeLength = node_length(candidate);
        unsigned short nodeNameLength = node_name_length(candidate);

        // wildcard, parameter, equal
        //if (nodeNameLength == 0) { /* empty node name is valid case */
        if (candidate[6] == '*') {
            /* use '*' for wildcards */

            return true;

        } else if (candidate[6] == ':') {
            // parameter

            if (params_idx < params.size()) {
                /*
                 * Update params array at params_idx instead of appending
                 * We will always backtrack to the params_idx corresponding to
                 * current URL segment
                 */
                params[params_idx++] = string_view({name, name_length});

            } else {
                // todo: push this pointer on the stack of args!
                params.push_back(string_view({name, name_length}));

                /* maintain index of params to backtrack */
                params_idx = params.size();
            }

            return true;

        } else if (nodeNameLength == name_length &&
                !memcmp(candidate + 6, name, name_length)) {
            return true;
        }

        return false;
    }

#if 0
    inline const char *find_node(const char *parent_node, const char *name,
        std::size_t name_length) {

        unsigned short nodeLength = *(unsigned short *) &parent_node[0];
        unsigned short nodeNameLength = *(unsigned short *) &parent_node[2];

        //std::cout << "Finding node: <" << std::string(name, name_length) << ">" << std::endl;

        const char *stoppp = parent_node + nodeLength;
        for (const char *candidate = parent_node + 6 + nodeNameLength; candidate < stoppp; ) {

            unsigned short nodeLength = *(unsigned short *) &candidate[0];
            unsigned short nodeNameLength = *(unsigned short *) &candidate[2];

            // whildcard, parameter, equal
            if (nodeNameLength == 0) {
                return candidate;
            } else if (candidate[6] == ':') {
                // parameter

                // todo: push this pointer on the stack of args!
                params.push_back(string_view({name, name_length}));

                return candidate;
            } else if (nodeNameLength == name_length && !memcmp(candidate + 6, name, name_length)) {
                return candidate;
            }

            candidate = candidate + nodeLength;
        }

        return nullptr;
    }
#endif

    // returns next slash from start or end
    inline const char *getNextSegment(const char *start, const char *end,
            char delimiter = '/') {
        const char *stop = (const char *) memchr(start, delimiter, end - start);
        return stop ? stop : end;
    }

    inline void push_children(const char *segment, const char *node,
            std::size_t params_idx) {

        for (const char *child = node + 6 + node_name_length(node);
                child < node + node_length(node);
                child = child + node_length(child)) {

            s.push({segment, child, params_idx});
        } 
    }

    // should take method also!
    inline int lookup(const char *url, int length) {

        const char *treeStart = (char *) compiled_tree.data();
        const char *stop, *start = url;
        const char *end_ptr = getNextSegment(url, url + length, '?');

        s.clear();

        /* Push children on to stack */
        push_children(start, treeStart, 0);

        while (!s.empty()) {
            auto frame = s.pop();

            /* Fetch URL segment ptr */
            start = frame.segptr;

            /* Fetch trie node ptr */
            treeStart = frame.nodeptr;

            /* Get the end of current segment */
            stop = getNextSegment(start, end_ptr);

            //std::cout << "Matching(" << std::string(start, stop - start) << ")"
            //    << std::endl;

            if (!match_node(treeStart, start, stop - start, frame.params_idx)) {
                continue;
            }

            /* Move to next URL segment */
            start = stop + 1;

            /* Check if we have reached the end of URL string */
            if (stop == end_ptr || start == end_ptr) {
                break;
            }
            
            /* Push children on to stack */
            push_children(start, treeStart, frame.params_idx);
        }

#if 0
        do {
            stop = getNextSegment(start, end_ptr);

            //std::cout << "Matching(" << std::string(start, stop - start) << ")" << std::endl;

            if(nullptr == (treeStart = find_node(treeStart, start, stop - start))) {
                return -1;
            }

            start = stop + 1;
        } while (stop != end_ptr);
#endif

        return *(short *) &treeStart[4];
    }

public:
    HttpRouter() {
        // maximum 100 parameters
        params.reserve(100);
    }

    ~HttpRouter() {
        free_children(tree);
    }
        
    HttpRouter *add(const char *method, const char *pattern, std::function<void(USERDATA, std::vector<string_view> &)> handler) {

        // step over any initial slash
        if (pattern[0] == '/') {
            pattern++;
        }

        std::vector<std::string> nodes;
        nodes.push_back(method);

        const char *stop, *start = pattern;
        const char *end_ptr =
            getNextSegment(pattern, pattern + strlen(pattern), '?');

        do {
            stop = getNextSegment(start, end_ptr);

            //std::cout << "Segment(" << std::string(start, stop - start) << ")" << std::endl;

            nodes.push_back(std::string(start, stop - start));

            start = stop + 1;
        } while (stop != end_ptr && start != end_ptr);


        // if pattern starts with / then move 1+ and run inline slash parser

        add(nodes, handlers.size());
        handlers.push_back(handler);

        compile();
        return this;
    }

    void compile() {
        compiled_tree.clear();
        compile_tree(tree);
    }

    void route(const char *method, unsigned int method_length, const char *url,
            unsigned int url_length, USERDATA userData) {

        /* Prepend method to URL */
        char target[method_length + url_length + 1];
        memcpy(target, method, method_length);
        memcpy(target + method_length, url, url_length);
        target[method_length + url_length] = '\0';

        auto handler_id = lookup(target, method_length + url_length);
        if (handler_id > -1 && handler_id < handlers.size()) {
            handlers[handler_id](userData, params);
            params.clear();
        }
    }
};

#endif // HTTPROUTER_HPP
