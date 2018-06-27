#ifndef __KDTREE_H__
#define __KDTREE_H__ 

#include "obj.hpp"
#include "bezier.hpp"

struct SPPMnode {
	P3 pos, col, dir;
	int index;
	ld prob, r;
	SPPMnode() {index = -1; prob = 1;}
	SPPMnode(P3 pos_, P3 col_, P3 dir_, ld r_ = 1, int index_ = -1, ld prob_ = 1):
		pos(pos_), col(col_), dir(dir_), index(index_), prob(prob_), r(r_) {}
};

struct IMGbuf {
	ld n; P3 f;
	IMGbuf(): n(0), f(0,0,0) {}
	IMGbuf(ld n_, P3 f_): n(n_), f(f_) {}
	void add(P3 c, ld p = 1.) { n += p, f += c; }
	P3 getcol() { return f / n; }
	void reset() { n = 0; f.x = f.y = f.z = 0; }
	IMGbuf operator+(const IMGbuf& a) const {return IMGbuf(n + a.n, f + a.f);}
	IMGbuf operator*(ld p) const {return IMGbuf(n * p, f * p);}
	IMGbuf& operator+=(const IMGbuf& a) {return *this = *this + a;}
	P3 get() { return n < eps ? f : f / n; }
};

class KDTree
{
public:
	static int D;
	int n, root;
	struct KDTreeNode {
		SPPMnode sppm;
		P3 m[2];
		int s[2];
		KDTreeNode(): sppm() {s[0] = s[1] = 0;}
		bool operator<(const KDTreeNode&a) const {
			if (D == 0)
				return sppm.pos.x < a.sppm.pos.x;
			else if (D == 1)
				return sppm.pos.y < a.sppm.pos.y;
			else
				return sppm.pos.z < a.sppm.pos.z;
		}
	};
	KDTreeNode* tree;
	KDTree() { tree = NULL; }
	~KDTree() { if (tree != NULL) delete[] tree; }
	void mt(int f, int x) {
		tree[f].m[0] = tree[f].m[0].min(tree[x].m[0]);
		tree[f].m[1] = tree[f].m[1].max(tree[x].m[1]);
	}
	int bt(int l, int r, int d) {
		D = d;
		int o = l + r >> 1;
		std::nth_element(tree + l, tree + o, tree + r + 1);
		tree[o].m[0] = tree[o].sppm.pos - tree[o].sppm.r;
		tree[o].m[1] = tree[o].sppm.pos + tree[o].sppm.r;
		if (l < o) tree[o].s[0] = bt(l, o - 1, d == 2 ? 0 : d + 1), mt(o, tree[o].s[0]);
		if (o < r) tree[o].s[1] = bt(o + 1, r, d == 2 ? 0 : d + 1), mt(o, tree[o].s[1]);
		return o;
	}
	KDTree(std::vector<SPPMnode>& node) { // multi-thread forbid !!!
		n = node.size();
		tree = new KDTreeNode[n + 10];
		for (int i = 0; i < n; ++i) {
			tree[i + 1].sppm = node[i];
		}
		root = bt(1, n, 0);
	}
	ld getdis2(P3 pos, P3 m0, P3 m1) {
		return (P3().max(pos - m1).max(m0 - pos)).len2();
	}
	void _query(const SPPMnode&node, IMGbuf* c, int o) {
		if ((tree[o].sppm.pos - node.pos).len2() <= sqr(tree[o].sppm.r) && tree[o].sppm.dir.dot(node.dir) >= 0)
			c[tree[o].sppm.index].add(node.col, node.prob);
		ld d[2];
		if (tree[o].s[0] > 0) d[0] = getdis2(node.pos, tree[tree[o].s[0]].m[0], tree[tree[o].s[0]].m[1]); else d[0] = INF;
		if (tree[o].s[1] > 0) d[1] = getdis2(node.pos, tree[tree[o].s[1]].m[0], tree[tree[o].s[1]].m[1]); else d[1] = INF;
		int tmp = d[0] >= d[1];
		if (d[tmp] < eps) _query(node, c, tree[o].s[tmp]); tmp ^= 1;
		if (d[tmp] < eps) _query(node, c, tree[o].s[tmp]);
	}
	void _modify(int o) {
		tree[o].m[0] = tree[o].sppm.pos - tree[o].sppm.r;
		tree[o].m[1] = tree[o].sppm.pos + tree[o].sppm.r;
		if (tree[o].s[0] > 0) _modify(tree[o].s[0]), mt(o, tree[o].s[0]);
		if (tree[o].s[1] > 0) _modify(tree[o].s[1]), mt(o, tree[o].s[1]);
	}
	void query(SPPMnode node, IMGbuf* c) {
		_query(node, c, root);
	}
	void modify() {
		_modify(root);
	}
};

int KDTree::D = 0;

#endif // __KDTREE_H__