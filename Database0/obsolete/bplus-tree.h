#ifndef _BPLUS_TREE_H_
#define _BPLUS_TREE_H_

#include <vector>
#include <type_traits>
#include <stdint.h>
#include <map>

std::map<int, int> mm;
/* A generic and configurable b+ tree */
template<
	typename Key,
	typename Value,
	typename LessCompare = std::less<Key>,
	template<typename>
	typename Alloc = std::allocator,
	int MaxLeafSize = 4,
	int MinLeafSize = 2,
	int NumKey = 2,
	bool AllowDuplicate = true
>
class BPTree {
	static_assert(MinLeafSize >= 2, "Minimum size of the array clip must be larger than 2!");
	struct Leaf {
		Leaf(Leaf *sib) : sibling(sib) {}
		Value elems[MaxLeafSize];
		Leaf *sibling;
		int size;
	};
	
	struct Node {
		Key keys[NumKey];
		Node* kids[NumKey + 1];
		bool isKeyFull() { return kids[NumKey] != nullptr; }
	};

	template<typename T>
	struct TempStack {
		TempStack(n) :num(0) { data = static_cast<T*>(malloc(sizeof(T) * n)); }
		void push(T) {}
		T* data;
		int num;
	};
public:
	//TODO: assert current is null
	class iterator; /*{
	public:
		iterator(Leaf *cur) : current_(cur), pos_(0) {}
		iterator(Leaf *cur, int position) : current_(cur), pos_(position) {}
		
		ElemType& operator*() { return current_->elems[pos_]; }

		ElemType* operator->() { return &(**this); }

		iterator& operator++() {
			if (pos_ > current_->size) {
				current_ = current_->sibling;
				pos_ = -1;
			}
			++pos_;
			return *this;
		}

		iterator operator++(int) {
			iterator me = *this;
			++*this;
			return me;
		}

		bool operator==(const iterator& rhs) { return rhs.current_ == current_ && rhs.pos_ == pos_; }

		static iterator buildInvalid() { return iterator(nullptr); }

		bool isValid() { return current_ != nullptr; }
	private:
		friend class BPTree<ElemType, Compare, Alloc,
			MaxLeafSize, MinLeafSize,
			NumKey, AllowDuplicate>;
		Leaf *current() { return current_; }
		int pos() { return pos_; }
		Leaf *current_;
		int pos_;
	};*/

	uint64_t size() { return size_; }
	iterator begin() { return iterator(head_); }
	iterator end() { return iterator(nullptr); }

	iterator find(const ElemType& key) {
		Node *node = root_;
		for (int level; level < height_; ++level) {
			int pos = 0;
			// we loop over every keys, if the kids is null, 
			// that means the key is invalid
			for (; pos < NumKey && node->kids[pos + 1] != nullptr; ++pos)
				if ( !LessCompare(node->keys[pos], key)) // node->keys[pos] >= key
					break;
			node = node->kids[pos];
		}
		
		Leaf *leaf = reinterpret_cast<Leaf*>(node);
		return findInLeaf(leaf, key);
	}
	
	template<typename ArgumentType>
	void insert(ArgumentType&& key) {
		std::vector<Node *> stack;
		Node *node = root_;
		for (int level; level < height_; ++level) {
			int pos = 0;
			for (; pos < NumKey && node->kids[pos + 1] != nullptr; ++pos)
				if (node->keys[pos] >= key)
					break;
			stack.push_back(node);
			node = node->kids[pos];
		}
		Leaf *leaf = reinterpret_cast<Leaf*>(node);
		auto result = findInLeaf(leaf, key);


		if (!AllowDuplicate)
			if (result.isValid())
				return;

		if (result.pos() >= NumKey) {
			//TODO: Split the tree
		}

		makeSlotAfter(result);
		*(++result) = key;
	}


private:
	iterator findInLeaf(Leaf *leaf, const ElemType key) {
		for (int slot = 0; slot < leaf->size; ++slot)
			if (leaf->elems[slot] == key)
				return iterator(leaf, slot);
		return iterator::buildInvalid();
	}


	Node *root_;
	Leaf *head_;
	uint64_t size_;
	int height_;
	Alloc<Node> node_alloc_;
	Alloc<Leaf> leaf_alloc_;

	void splitTree(iterator& itr, const std::vector<Node *> stack) {
		int last = stack.size() - 2;
		while (stack[last]->isKeyFull() && last >= 0)
			--last;
		

		auto oldLeaf = itr.current();
		auto newLeaf = leaf_alloc_.allocate(1);
		constexpr int mid = MaxLeafSize / 2;
		constexpr int sz_tobe_moved = MaxLeafSize - mid;
		moveSlots(newLeaf->elems, oldLeaf->elems + mid, sz_tobe_moved);

		auto newNode = node_alloc_.allocate(1);

	}

	template<typename = std::enable_if_t<std::is_trivially_copyable<ElemType>::value>>
	void moveSlots(ElemType *dst, ElemType *src, int numElem) {
		std::memmove(dst, src, numElem * sizeof(ElemType));
	}

	template<typename = std::enable_if_t<!std::is_trivially_copyable<ElemType>::value>>
	void moveSlots(ElemType *dst, ElemType *src, int numElem) {
		// src   | -------    | <- overlaps
		// dst   |   -------  |
		if (src < dst && dst < src + numElem)
			for (int i = numElem - 1; i >= 0; --i)
				dst[i] = src[i];
		else
			for (int i = 0; i < numElem; ++i)
				dst[i] = src[i];
	}


	void makeSlotAfter(iterator& itr) {
		++size_;
		// pointing to the slot after itr
		ElemType *from = &(*itr) + 1;
		size_t size = itr.current()->size - itr.pos();
		moveSlots(from + 1, from, size);
	}
};


#endif // !_BPLUS_TREE_H_


int main()
{
	BPTree<float> bp;
}
