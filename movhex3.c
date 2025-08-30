#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>

// =============================================================
// Movhex — C11 implementation (readable edition)
//
// Semantics
//  - even‑r hex grid
//  - land step cost = exit_cost[u]
//  - if exit_cost[u] == 0, you CANNOT leave u (neither by land nor by air)
//  - air edges: up to 5 per tile; toggle adds cost = floor(avg(outgoing air costs + exit_cost[u]))
//  - change_cost: BFS rings; for each node at ring d < r: Δ = v * max(0, r^2 − d)
//                  applied to exit_cost and all outgoing air edge costs; clamp to ≥ 0
//  - travel_cost: Dijkstra with monotone radix heap; early exit when target is popped
//  - safe integer math for distance updates (saturating additions)
// =============================================================

#define MAX_AIR  5
#define INF_I32  0x3f3f3f3f

// -------------------------------------------------------------
// Fast I/O (1 MB buffered)
// -------------------------------------------------------------

typedef struct {
    int            idx;
    int            size;
    unsigned char  buf[1 << 20];
} FastIn;

static FastIn In = {0, 0, {0}};

static inline int fin_gc(void) {
    if (In.idx >= In.size) {
        In.size = (int)fread(In.buf, 1, sizeof In.buf, stdin);
        In.idx  = 0;
        if (!In.size) return 0;
    }
    return In.buf[In.idx++];
}

static inline void fin_skip_blanks(void) {
    int c;
    while ((c = fin_gc()) != 0) {
        if (c > ' ') { In.idx--; break; }
    }
}

static inline int fin_read_int(int *x) {
    fin_skip_blanks();
    int c = fin_gc();
    if (!c) return 0;

    int sgn = 1;
    if (c == '-') { sgn = -1; c = fin_gc(); }

    long long v = 0;
    while (c >= '0' && c <= '9') { v = v * 10 + (c - '0'); c = fin_gc(); }

    *x = (int)(v * sgn);
    if (c) In.idx--;
    return 1;
}

static inline int fin_read_word(char *s, int maxlen) {
    fin_skip_blanks();
    int c = fin_gc();
    if (!c) { s[0] = '\0'; return 0; }

    int i = 0;
    while (c > ' ') {
        if (i + 1 < maxlen) s[i++] = (char)c;
        c = fin_gc();
    }
    if (c) In.idx--;
    s[i] = '\0';
    return 1;
}


typedef struct {
    int            idx;
    unsigned char  buf[1 << 20];
} FastOut;

static FastOut Out = {0, {0}};

static inline void fout_flush(void) {
    if (Out.idx) { fwrite(Out.buf, 1, Out.idx, stdout); Out.idx = 0; }
}

static inline void fout_pc(char c) {
    if (Out.idx >= (int)sizeof Out.buf) fout_flush();
    Out.buf[Out.idx++] = (unsigned char)c;
}

static inline void fout_ps(const char *s) {
    while (*s) fout_pc(*s++);
}

static inline void fout_pn(const char *s) {
    fout_ps(s);
    fout_pc('\n');
}

static inline void fout_write_uint(uint32_t x) {
    char s[16];
    int  n = 0;
    if (x == 0) { fout_pc('0'); return; }
    while (x) { s[n++] = (char)('0' + (x % 10)); x /= 10; }
    while (n--) fout_pc(s[n]);
}

static inline void fout_write_int(int x) {
    if (x == 0) { fout_pc('0'); return; }
    if (x < 0)  { fout_pc('-'); fout_write_uint((uint32_t)(-1LL * (long long)x)); return; }
    fout_write_uint((uint32_t)x);
}

// -------------------------------------------------------------
// Tiny vector for radix-heap buckets (pair of arrays)
// -------------------------------------------------------------

typedef struct {
    uint32_t *a;   // keys
    uint32_t *b;   // values
    size_t    sz;
    size_t    cap;
} PairBucket;

static inline void pb_init(PairBucket *p)                { p->a = p->b = NULL; p->sz = p->cap = 0; }
static inline int  pb_empty(const PairBucket *p)         { return p->sz == 0; }
static inline void pb_clear(PairBucket *p)               { p->sz = 0; }
static inline void pb_free(PairBucket *p)                { free(p->a); free(p->b); p->a = p->b = NULL; p->sz = p->cap = 0; }

static inline void pb_reserve(PairBucket *p, size_t need) {
    if (need <= p->cap) return;
    size_t n = p->cap ? p->cap : 8;
    while (n < need) n <<= 1;
    p->a = (uint32_t*)realloc(p->a, n * sizeof(uint32_t));
    p->b = (uint32_t*)realloc(p->b, n * sizeof(uint32_t));
    p->cap = n;
}

static inline void pb_push(PairBucket *p, uint32_t key, uint32_t val) {
    pb_reserve(p, p->sz + 1);
    p->a[p->sz] = key;
    p->b[p->sz] = val;
    p->sz++;
}

static inline void pb_pop_back(PairBucket *p, uint32_t *key, uint32_t *val) {
    size_t i = --p->sz;
    *key = p->a[i];
    *val = p->b[i];
}

// -------------------------------------------------------------
// Monotone Radix Heap (32-bit keys)
// -------------------------------------------------------------

typedef struct {
    PairBucket  b[33];
    uint32_t    last;
    size_t      sz;
} RadixHeap32;

static inline void rh_init(RadixHeap32 *h) {
    for (int i = 0; i < 33; i++) pb_init(&h->b[i]);
    h->last = 0u;
    h->sz   = 0;
}

static inline int rh_empty(const RadixHeap32 *h) { return h->sz == 0; }

static inline int rh_idx(const RadixHeap32 *h, uint32_t x) {
    if (x == h->last) return 0;
    uint32_t y = x ^ h->last; // y != 0
#ifdef __GNUC__
    int msb = 31 - __builtin_clz(y);
#else
    int msb = 0; while (y >> msb) msb++; msb--; if (msb < 0) msb = 0;
#endif
    return msb + 1;
}

static inline void rh_push(RadixHeap32 *h, uint32_t key, uint32_t val) {
    int i = rh_idx(h, key);
    pb_push(&h->b[i], key, val);
    h->sz++;
}

static inline void rh_pop(RadixHeap32 *h, uint32_t *key, uint32_t *val) {
    if (pb_empty(&h->b[0])) {
        int i = 1;
        while (i <= 32 && pb_empty(&h->b[i])) ++i;

        uint32_t    new_last = UINT32_MAX;
        PairBucket *v        = &h->b[i];
        for (size_t k = 0; k < v->sz; k++) if (v->a[k] < new_last) new_last = v->a[k];
        h->last = new_last;

        for (size_t k = 0; k < v->sz; k++) {
            uint32_t kk = v->a[k];
            uint32_t vv = v->b[k];
            pb_push(&h->b[rh_idx(h, kk)], kk, vv);
        }
        pb_clear(v);
    }
    pb_pop_back(&h->b[0], key, val);
    h->sz--;
}

static inline void rh_free(RadixHeap32 *h) {
    for (int i = 0; i < 33; i++) pb_free(&h->b[i]);
}

// -------------------------------------------------------------
// Helpers
// -------------------------------------------------------------

static inline int32_t sat_add_i32(int32_t a, int32_t b) {
    int64_t s = (int64_t)a + (int64_t)b;
    if (s > INT_MAX) return INT_MAX;
    if (s < 0)       return 0;
    return (int32_t)s;
}

// -------------------------------------------------------------
// World state
// -------------------------------------------------------------

static int        W = 0, H = 0;
static uint32_t   N = 0;

static int32_t   *exit_cost = NULL; // default 1

typedef struct {
    uint32_t  dst[5];
    int32_t   cost[5];
    uint8_t   cnt;
} AirEdges;

static AirEdges  *air   = NULL;
static int32_t   *dist  = NULL; // valid when seen[u] == gen
static uint32_t  *seen  = NULL; // stamp array
static uint32_t   gen   = 1;
static uint32_t  *vseen = NULL; // change_cost BFS stamps
static uint32_t   vgen  = 1;
static uint32_t  *xof = NULL, *yof = NULL; // precomputed coords

// light vector for BFS rings

typedef struct { uint32_t *a; size_t sz, cap; } U32Vec;

static inline void v_init(U32Vec *v)               { v->a = NULL; v->sz = v->cap = 0; }
static inline void v_free(U32Vec *v)               { free(v->a); v->a = NULL; v->sz = v->cap = 0; }
static inline void v_clear(U32Vec *v)              { v->sz = 0; }
static inline void v_swap(U32Vec *a, U32Vec *b)    { U32Vec t = *a; *a = *b; *b = t; }

static inline void v_reserve(U32Vec *v, size_t need) {
    if (need <= v->cap) return;
    size_t n = v->cap ? v->cap : 256;
    while (n < need) n <<= 1;
    v->a   = (uint32_t*)realloc(v->a, n * sizeof(uint32_t));
    v->cap = n;
}

static inline void v_push(U32Vec *v, uint32_t x) {
    v_reserve(v, v->sz + 1);
    v->a[v->sz++] = x;
}

static U32Vec qcurr, qnext;

// even‑r neighbor deltas
static int dx_even[6] = { -1, +1,  0, +1,  0, +1 };
static int dy_even[6] = {  0,  0, -1, -1, +1, +1 };
static int dx_odd [6] = { -1, +1, -1,  0, -1,  0 };
static int dy_odd [6] = {  0,  0, -1, -1, +1, +1 };

static inline int inb(int x, int y)        { return ((unsigned)x < (unsigned)W && (unsigned)y < (unsigned)H); }
static inline uint32_t idxy(int x, int y)  { return (uint32_t)y * (uint32_t)W + (uint32_t)x; }

// -------------------------------------------------------------
// Lifecycle
// -------------------------------------------------------------

static void destroy_world(void) {
    free(exit_cost); exit_cost = NULL;
    free(air);       air       = NULL;
    free(dist);      dist      = NULL;
    free(seen);      seen      = NULL;
    free(vseen);     vseen     = NULL;
    free(xof);       xof       = NULL;
    free(yof);       yof       = NULL;
    v_free(&qcurr);  v_free(&qnext);
    W = H = 0; N = 0; gen = vgen = 1;
}

static int do_init(int w, int h) {
    destroy_world();
    if (w <= 0 || h <= 0) return 0;

    uint64_t n = (uint64_t)w * (uint64_t)h;
    if (n > 0xFFFFFFFFu) return 0; // guard

    W = w; H = h; N = (uint32_t)n;

    exit_cost = (int32_t*) malloc(N * sizeof(int32_t));
    air       = (AirEdges*)calloc(N, sizeof(AirEdges));
    dist      = (int32_t*) malloc(N * sizeof(int32_t));
    seen      = (uint32_t*)calloc(N, sizeof(uint32_t));
    vseen     = (uint32_t*)calloc(N, sizeof(uint32_t));
    xof       = (uint32_t*) malloc(N * sizeof(uint32_t));
    yof       = (uint32_t*) malloc(N * sizeof(uint32_t));

    if (!exit_cost || !air || !dist || !seen || !vseen || !xof || !yof) {
        destroy_world();
        return 0;
    }

    for (uint32_t i = 0; i < N; i++) { exit_cost[i] = 1; dist[i] = 0; }

    for (uint32_t yy = 0; yy < (uint32_t)H; ++yy) {
        uint32_t base = yy * (uint32_t)W;
        for (uint32_t xx = 0; xx < (uint32_t)W; ++xx) {
            uint32_t i = base + xx;
            xof[i] = xx;
            yof[i] = yy;
        }
    }

    v_init(&qcurr); v_init(&qnext);
    v_reserve(&qcurr, 256); v_reserve(&qnext, 256);
    gen = 1; vgen = 1;
    return 1;
}

// -------------------------------------------------------------
// Air edges
// -------------------------------------------------------------

static inline int find_air(int u, uint32_t v) {
    AirEdges *A = &air[u];
    for (uint8_t i = 0; i < A->cnt; i++) if (A->dst[i] == v) return (int)i;
    return -1;
}

static inline int add_air(int u, uint32_t v, int32_t c) {
    AirEdges *A = &air[u];
    if (A->cnt >= MAX_AIR) return 0;
    if (c < 0) c = 0;
    A->dst[A->cnt]  = v;
    A->cost[A->cnt] = c;
    A->cnt++;
    return 1;
}

static inline int remove_air(int u, int at) {
    AirEdges *A = &air[u];
    if (at < 0 || at >= A->cnt) return 0;
    uint8_t last = --A->cnt;
    if (at != last) { A->dst[at] = A->dst[last]; A->cost[at] = A->cost[last]; }
    return 1;
}

static int toggle_air_route(int x1, int y1, int x2, int y2) {
    if (!inb(x1, y1) || !inb(x2, y2)) return 0;

    int       u = (int)idxy(x1, y1);
    uint32_t  v = idxy(x2, y2);
    int       at = find_air(u, v);

    if (at >= 0) return remove_air(u, at);

    AirEdges *A = &air[u];
    if (A->cnt >= MAX_AIR) return 0;

    long long sum = exit_cost[u];
    for (uint8_t i = 0; i < A->cnt; i++) sum += A->cost[i];

    int32_t newc = (int32_t)(sum / (long long)(A->cnt + 1));
    if (newc < 0) newc = 0;
    return add_air(u, v, newc);
}

// -------------------------------------------------------------
// change_cost (radius BFS with ring deltas)
// -------------------------------------------------------------

static inline void apply_delta(uint32_t u, long long dlt) {
    long long ec = (long long)exit_cost[u] + dlt;
    if (ec < 0) ec = 0; if (ec > INT_MAX) ec = INT_MAX;
    exit_cost[u] = (int32_t)ec;

    AirEdges *A = &air[u];
    for (uint8_t i = 0; i < A->cnt; i++) {
        long long ac = (long long)A->cost[i] + dlt;
        if (ac < 0) ac = 0; if (ac > INT_MAX) ac = INT_MAX;
        A->cost[i] = (int32_t)ac;
    }
}

static int change_cost_cmd(int x, int y, int v, int radius) {
    if (!inb(x, y) || radius <= 0) return 0;

    vgen++; if (vgen == 0) { memset(vseen, 0, N * sizeof(uint32_t)); vgen = 1; }

    v_clear(&qcurr); v_clear(&qnext);

    uint32_t s = idxy(x, y);
    vseen[s] = vgen;
    v_push(&qcurr, s);

    long long R2 = (long long)radius * (long long)radius;
    int       d  = 0;

    while (qcurr.sz && d < radius) {
        long long delta = (long long)v * ((R2 - d) > 0 ? (R2 - d) : 0);

        // Apply to current ring
        for (size_t i = 0; i < qcurr.sz; i++) apply_delta(qcurr.a[i], delta);

        // Last ring: don't expand further
        if (d + 1 >= radius) break;

        // Build next ring
        for (size_t i = 0; i < qcurr.sz; i++) {
            uint32_t u  = qcurr.a[i];
            uint32_t ux = xof[u], uy = yof[u];
            const int *dx = ((uy & 1u) == 0u) ? dx_even : dx_odd;
            const int *dy = ((uy & 1u) == 0u) ? dy_even : dy_odd;

            for (int k = 0; k < 6; k++) {
                int nx = (int)ux + dx[k];
                int ny = (int)uy + dy[k];
                if (inb(nx, ny)) {
                    uint32_t w = idxy(nx, ny);
                    if (vseen[w] != vgen) { vseen[w] = vgen; v_push(&qnext, w); }
                }
            }
        }

        v_clear(&qcurr);
        v_swap(&qcurr, &qnext);
        d++;
    }

    return 1;
}

// -------------------------------------------------------------
// Dijkstra (radix heap) — depart only if exit_cost[u] > 0
// -------------------------------------------------------------

static int travel_cost_cmd(int x1, int y1, int x2, int y2) {
    if (!inb(x1, y1) || !inb(x2, y2)) return -1;

    uint32_t s = idxy(x1, y1);
    uint32_t t = idxy(x2, y2);
    if (s == t) return 0;

    gen++; if (gen == 0) { memset(seen, 0, N * sizeof(uint32_t)); gen = 1; }

    RadixHeap32 pq; rh_init(&pq);
    rh_push(&pq, 0u, s);
    seen[s] = gen; dist[s] = 0;

    while (!rh_empty(&pq)) {
        uint32_t key, u;
        rh_pop(&pq, &key, &u);
        int32_t du = (int32_t)key;

        if (seen[u] != gen || du != dist[u]) continue; // stale
        if (u == t) { rh_free(&pq); return du; }

        int32_t step = exit_cost[u];
        if (step > 0) {
            // Air edges (only if we can leave u)
            AirEdges *A = &air[u];
            for (uint8_t i = 0; i < A->cnt; i++) {
                uint32_t v  = A->dst[i];
                int32_t  w  = A->cost[i]; if (w < 0) w = 0;
                int32_t  nd = sat_add_i32(du, w);
                if (seen[v] != gen || nd < dist[v]) { seen[v] = gen; dist[v] = nd; rh_push(&pq, (uint32_t)nd, v); }
            }

            // Land neighbors
            uint32_t  ux = xof[u], uy = yof[u];
            const int *dx = ((uy & 1u) == 0u) ? dx_even : dx_odd;
            const int *dy = ((uy & 1u) == 0u) ? dy_even : dy_odd;

            int32_t nd_land = sat_add_i32(du, step);
            for (int k = 0; k < 6; k++) {
                int nx = (int)ux + dx[k];
                int ny = (int)uy + dy[k];
                if (inb(nx, ny)) {
                    uint32_t v = idxy(nx, ny);
                    if (seen[v] != gen || nd_land < dist[v]) { seen[v] = gen; dist[v] = nd_land; rh_push(&pq, (uint32_t)nd_land, v); }
                }
            }
        }
    }

    rh_free(&pq);
    return -1;
}

// -------------------------------------------------------------
// Main loop
// -------------------------------------------------------------

int main(void) {
    char cmd[32];

    while (fin_read_word(cmd, (int)sizeof cmd)) {
        if (cmd[0] == '#') {
            continue; // comment / ignored line
        }
        else if (strcmp(cmd, "init") == 0) {
            int C, R; if (!fin_read_int(&C) || !fin_read_int(&R)) break;
            int ok = do_init(C, R);
            fout_pn(ok ? "OK" : "KO");
        }
        else if (strcmp(cmd, "change_cost") == 0) {
            int x, y, v, r; if (!fin_read_int(&x) || !fin_read_int(&y) || !fin_read_int(&v) || !fin_read_int(&r)) break;
            int ok = change_cost_cmd(x, y, v, r);
            fout_pn(ok ? "OK" : "KO");
        }
        else if (strcmp(cmd, "toggle_air_route") == 0) {
            int x1, y1, x2, y2; if (!fin_read_int(&x1) || !fin_read_int(&y1) || !fin_read_int(&x2) || !fin_read_int(&y2)) break;
            int ok = toggle_air_route(x1, y1, x2, y2);
            fout_pn(ok ? "OK" : "KO");
        }
        else if (strcmp(cmd, "travel_cost") == 0) {
            int x1, y1, x2, y2; if (!fin_read_int(&x1) || !fin_read_int(&y1) || !fin_read_int(&x2) || !fin_read_int(&y2)) break;
            int ans = travel_cost_cmd(x1, y1, x2, y2);
            fout_write_int(ans);
            fout_pc('\n');
        }
        else {
            // Unknown token: ignore gracefully
        }
    }

    fout_flush();
    destroy_world();
    return 0;
}
