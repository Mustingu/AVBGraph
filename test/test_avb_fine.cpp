/**
 * Fine-grained (dual-grained timestamp) AVB simulation.
 *
 * Record layout (proposed, 32B per record, same as current):
 *   [0]: {dst(8B),  block(4B coarse_c) + intra_c(4B)}
 *   [1]: {weight(8B), pred_index(4B) + intra_inv(4B)}
 *
 * Key fine-grained behaviors:
 *   1. Same-epoch same-edge → separate TVB entries (not overwritten)
 *   2. Visibility: composite c < T_r ≤ inv  (strict < for creation)
 *   3. VB lookup via vector + binary search (no link pointer)
 */
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <map>
#include <vector>
using epoch_t = uint32_t;
using intra_t = uint32_t;
using dst_t = uint64_t;
#define TMPVB_MASK (1u << 31)
#define DELETION_MASK (1UL << 61)

struct Composite {
  epoch_t e;
  intra_t i;
  Composite() = default;
  Composite(epoch_t ee, intra_t ii) : e(ee), i(ii) {}
  bool operator<(Composite o) const { return e < o.e || (e == o.e && i < o.i); }
  bool operator<=(Composite o) const {
    return *this < o || (e == o.e && i == o.i);
  }
};

struct FGEdge {
  dst_t dst;
  epoch_t block;
  intra_t intra_c;
  dst_t weight;
  unsigned pred_idx;
  intra_t inv;
  FGEdge() = default;
  FGEdge(dst_t d, epoch_t b, intra_t ic, dst_t w, unsigned pi, intra_t iv)
      : dst(d), block(b), intra_c(ic), weight(w), pred_idx(pi), inv(iv) {}
  epoch_t coarse() const { return block & ~TMPVB_MASK; }
  Composite c() const { return Composite(coarse(), intra_c); }
};
struct FGVB {
  epoch_t ts;
  FGVB* prev;
  int n;
  FGEdge* rec;
  FGVB(epoch_t t) : ts(t), prev(0), n(0), rec(0) {}
};

static int g_fail, g_chk;
#define CHECK(c, f, ...)                                                   \
  do {                                                                     \
    g_chk++;                                                               \
    if (!(c)) {                                                            \
      g_fail++;                                                            \
      if (g_fail <= 10) fprintf(stderr, "  FAIL: " f "\n", ##__VA_ARGS__); \
    }                                                                      \
  } while (0)

// ============================================================
// Read: VB chain scan with fine-grained composite visibility
// ============================================================
static std::map<dst_t, dst_t> read_fine(FGVB* vb_end,
                                        const std::map<dst_t, FGEdge>& pa,
                                        Composite Tr) {
  std::map<dst_t, dst_t> r;
  for (auto& [d, e] : pa)
    if (!(e.dst & DELETION_MASK) && e.c() < Tr) r[d] = e.weight;
  for (auto* vb = vb_end; vb; vb = vb->prev) {
    for (int i = 0; i < vb->n; i++) {
      auto& e = vb->rec[i];
      if (e.dst & DELETION_MASK) continue;
      Composite rc = e.c(), ri(vb->ts, e.inv);
      if (rc < Tr && Tr <= ri && r.find(e.dst) == r.end()) r[e.dst] = e.weight;
    }
  }
  return r;
}

// ============================================================
// FineDriver: TVB accumulation + epoch publish + vector lookup
// ============================================================
struct FineDriver {
  std::map<dst_t, FGEdge> pa;
  FGVB* vb_end = 0;
  int vb_cnt = 0;
  std::vector<std::pair<epoch_t, FGVB*>> vb_idx;

  struct TE {
    dst_t d, w;
    intra_t ic;
  };
  std::vector<TE> tvb;

  void write(dst_t d, dst_t w) { tvb.push_back({d, w, ++next_ic}); }
  intra_t next_ic = 0;

  void publish(epoch_t ep) {
    if (tvb.empty()) return;
    std::sort(tvb.begin(), tvb.end(), [](TE& a, TE& b) { return a.ic < b.ic; });

    std::map<dst_t, FGEdge> cur = pa;  // evolving PA within epoch
    struct VR {
      dst_t d;
      epoch_t oc;
      intra_t oi;
      dst_t ow;
      intra_t ni;
      VR(dst_t dd, epoch_t ooc, intra_t ooi, dst_t oow, intra_t nni)
          : d(dd), oc(ooc), oi(ooi), ow(oow), ni(nni) {}
    };
    std::vector<VR> vrs;

    for (auto& t : tvb) {
      dst_t ow = 0;
      epoch_t oc = 0;
      intra_t oi = 0;
      auto it = cur.find(t.d);
      if (it != cur.end()) {
        ow = it->second.weight;
        oc = it->second.coarse();
        oi = it->second.intra_c;
      }
      if (oc > 0) vrs.emplace_back(t.d, oc, oi, ow, t.ic);
      cur[t.d] = FGEdge(t.d, ep, t.ic, t.w, 0, 0);
    }
    if (vrs.empty()) {
      pa = cur;
      tvb.clear();
      next_ic = 0;
      return;
    }

    auto* vb = new FGVB(ep);
    vb->n = vrs.size();
    vb->rec = new FGEdge[vrs.size()];
    for (size_t i = 0; i < vrs.size(); i++) {
      VR& r = vrs[i];
      FGEdge& e = vb->rec[i];
      e.dst = r.d;
      e.block = r.oc;
      e.intra_c = r.oi;
      e.weight = r.ow;
      e.pred_idx = 0;
      e.inv = r.ni;
    }
    vb->prev = vb_end;
    vb_end = vb;
    vb_cnt++;
    vb_idx.emplace_back(ep, vb);
    pa = cur;
    tvb.clear();
    next_ic = 0;
  }

  FGVB* find_vb(epoch_t ts) {
    if (vb_idx.empty()) return 0;
    auto it = std::upper_bound(vb_idx.begin(), vb_idx.end(), ts,
                               [](epoch_t v, auto& p) { return v < p.first; });
    return it == vb_idx.begin() ? 0 : (--it)->second;
  }
};

// ============================================================
static void test_same_epoch_same_edge() {
  printf("--- Test 1: Same-epoch same-edge (key fine-grained feature) ---\n");
  FineDriver drv;
  // Epoch 1: writes to edge 5 at intra=1,3; edge 3 at intra=2
  drv.write(5, 100);
  drv.write(3, 50);
  drv.write(5, 200);  // same edge 5, same epoch!
  drv.publish(1);
  // Epoch 2: update edge 5
  drv.write(5, 300);
  drv.publish(2);
  printf("  VBs: %d (expected 2)\n", drv.vb_cnt);

  // MVCC: c < Tr (strict). Tr=(1,2): c=(1,1) visible, c=(1,2) NOT yet visible
  auto r = read_fine(drv.vb_end, drv.pa, Composite(1, 2));
  CHECK(r[5] == 100 && r[3] == 0, "Tr=(1,2) e5=%lu(exp 100) e3=%lu(exp 0)",
        (unsigned long)r[5], (unsigned long)r[3]);
  r = read_fine(drv.vb_end, drv.pa, Composite(1, 4));
  CHECK(r[5] == 200 && r[3] == 50, "Tr=(1,4) e5=%lu(exp 200) e3=%lu(exp 50)",
        (unsigned long)r[5], (unsigned long)r[3]);
  // Tr=(2,1): c=(2,1) < (2,1)? NO (strict <). Epoch 2 update at intra=1 NOT yet
  // visible.
  //   e5 visible via epoch1 version c=(1,3): (1,3) < (2,1) YES → w=200
  r = read_fine(drv.vb_end, drv.pa, Composite(2, 1));
  CHECK(r[5] == 200, "Tr=(2,1) e5=%lu(exp 200)", (unsigned long)r[5]);
  // Tr=(2,2): c=(2,1) < (2,2) YES → epoch2 version visible
  r = read_fine(drv.vb_end, drv.pa, Composite(2, 2));
  CHECK(r[5] == 300, "Tr=(2,2) e5=%lu(exp 300)", (unsigned long)r[5]);
  printf("  checks=%d failures=%d\n", g_chk, g_fail);
}

static void test_interleaved() {
  printf("--- Test 2: Interleaved intra-epoch writes ---\n");
  FineDriver drv;
  // 6 writes alternating dst 2,1,2,1,2,1 (i%2?1:2)
  for (int i = 0; i < 6; i++) {
    dst_t d = (i % 2) ? 1 : 2;
    drv.write(d, (i + 1) * 10);
  }
  drv.publish(1);

  // Tr=(1,3): c<(1,3). VB: dst=2 c=(1,1), dst=1 c=(1,2) → visible
  auto r = read_fine(drv.vb_end, drv.pa, Composite(1, 3));
  CHECK(r[1] == 20 && r[2] == 10, "Tr=(1,3) e1=%lu(exp 20) e2=%lu(exp 10)",
        (unsigned long)r[1], (unsigned long)r[2]);
  // Tr=(1,6): c<(1,6). VB latest: dst=2 c=(1,5)=50, dst=1 c=(1,4)=40; PA
  // c=(1,6) NOT < (1,6)
  r = read_fine(drv.vb_end, drv.pa, Composite(1, 6));
  CHECK(r[1] == 40 && r[2] == 50, "Tr=(1,6) e1=%lu(exp 40) e2=%lu(exp 50)",
        (unsigned long)r[1], (unsigned long)r[2]);
  // Tr=(2,1): PA visible (epoch 1 < 2)
  r = read_fine(drv.vb_end, drv.pa, Composite(2, 1));
  CHECK(r[1] == 60 && r[2] == 50, "Tr=(2,1) e1=%lu(exp 60) e2=%lu(exp 50)",
        (unsigned long)r[1], (unsigned long)r[2]);
  printf("  checks=%d failures=%d\n", g_chk, g_fail);
}

static void test_vector_lookup() {
  printf("--- Test 3: VB vector binary search ---\n");
  FineDriver drv;
  drv.pa[10] = FGEdge(10, 0, 0, 0, 0, 0);
  for (int ep = 1; ep <= 50; ep++) {
    drv.write(10, ep * 10);
    drv.publish(ep);
  }
  for (epoch_t ts : {5u, 10u, 25u, 49u, 50u}) {
    auto* vb = drv.find_vb(ts);
    CHECK(vb && vb->ts == ts, "find_vb(%u): got ts=%u", ts, vb ? vb->ts : 0);
  }
  CHECK(drv.find_vb(0) == 0, "find_vb(0) should be null");
  CHECK(drv.find_vb(51) == 0 || drv.find_vb(51)->ts == 50,
        "find_vb(51) should be null or last");
  printf("  checks=%d failures=%d\n", g_chk, g_fail);
}

int main() {
  printf("=== Fine-Grained AVB Test ===\n\n");
  test_same_epoch_same_edge();
  test_interleaved();
  test_vector_lookup();
  printf("\n=== %d checks, %d failures ===\n", g_chk, g_fail);
  return g_fail ? 1 : 0;
}
