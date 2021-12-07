#pragma once
#include <cmath>
#include <vector>
#include <algorithm>
#include "_main.hxx"
#include "vertices.hxx"
#include "edges.hxx"
#include "csr.hxx"
#include "chains.hxx"
#include "pagerank.hxx"

using std::vector;
using std::swap;
using std::abs;
using std::pow;




template <class G, class H, class T>
auto pagerankChains(const G& x, const H& xt, const PagerankOptions<T>& o) {
  if (!o.skipChains) return vector2d<int>();
  auto id = indices(xt.vertices());
  auto a  = chains(x, xt);
  for (auto& vs : a) {
    for (int i=0; i<vs.size(); i++)
      vs[i] = id[vs[i]];
  }
  return a;
}


void pagerankMarkChains(vector<int>& vfrom, const vector2d<int>& ch) {
  for (const auto& vs : ch) {
    for (int i=1; i<vs.size(); i++)
      vfrom[vs[i]] = -abs(vfrom[vs[i]]);
  }
}


template <class T>
T pagerankTeleport(const vector<T>& r, const vector<int>& vdata, int i, int n, int N, T p) {
  T a = (1-p)/N;
  for (int v=i; v<i+n; v++)
    if (vdata[v] == 0) a += p*r[v]/N;
  return a;
}


template <class T>
void pagerankFactor(vector<T>& a, const vector<int>& vdata, int i, int n, T p) {
  for (int u=i; u<i+n; u++) {
    int d = vdata[u];
    a[u] = d>0? p/d : 0;
  }
}


template <class T>
void pagerankCalculate(vector<T>& a, const vector<T>& c, const vector<int>& vfrom, const vector<int>& efrom, int i, int n, T c0) {
  for (int v=i; v<i+n; v++) {
    if (vfrom[v] < 0) continue;
    a[v] = c0 + sumAt(c, slice(efrom, abs(vfrom[v]), abs(vfrom[v+1])));
  }
}


template <class T>
void pagerankCalculateChains(vector<T>& a, const vector<T>& r, const vector2d<int>& ch, T p, T c0) {
  for (const auto& vs : ch) {
    for (int i=1; i<vs.size(); i++) {
      T pi = pow(p, i);
      a[vs[i]] = ((1-pi)/(1-p)) * c0 + pi * r[vs[0]];
    }
  }
}


template <class T>
int pagerankSeqLoop(vector<T>& a, vector<T>& r, vector<T>& c, const vector<T>& f, const vector<int>& vfrom, const vector<int>& efrom, const vector<int>& vdata, const vector2d<int>& ch, int i, int n, int N, T p, T E, int L) {
  int l = 1;
  for (; l<L; l++) {
    T c0 = pagerankTeleport(r, vdata, i, n, N, p);
    multiply(c, r, f, i, n);
    pagerankCalculate(a, c, vfrom, efrom, i, n, c0);
    pagerankCalculateChains(a, r, ch, p, c0);
    T el = l1Norm(a, r, i, n);
    if (el < E) break;
    swap(a, r);
  }
  return l;
}


// Find pagerank using a single thread (pull, CSR).
// @param x  original graph
// @param xt transpose graph, with vertex-data=out-degree
// @param q initial ranks (optional)
// @param o options {damping=0.85, tolerance=1e-6, maxIterations=500}
// @returns {ranks, iterations, time}
template <class G, class H, class T=float>
PagerankResult<T> pagerankSeq(const G& x, const H& xt, const vector<T> *q=nullptr, PagerankOptions<T> o={}) {
  T    p = o.damping;
  T    E = o.tolerance;
  int  L = o.maxIterations, l;
  int  N = xt.order();
  auto vfrom = sourceOffsets(xt);
  auto efrom = destinationIndices(xt);
  auto vdata = vertexData(xt);
  auto ch    = pagerankChains(x, xt, o);
  pagerankMarkChains(vfrom, ch);
  vector<T> a(N), r(N), c(N), f(N);
  float t = measureDurationMarked([&](auto mark) {
    fill(a, T());
    if (q) r = compressContainer(xt, *q);
    else fill(r, T(1)/N);
    mark([&] { pagerankFactor(f, vdata, 0, N, p); });
    mark([&] { l = pagerankSeqLoop(a, r, c, f, vfrom, efrom, vdata, ch, 0, N, N, p, E, L); });
  }, o.repeat);
  return {decompressContainer(xt, a), l, t};
}
