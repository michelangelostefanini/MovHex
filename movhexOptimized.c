#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <limits.h>
#include <stdint.h>

/*
   ==========================================
   OPT PASS: 1) Cache cap + open addressing
             2) Air routes compattate (dest_id+cost)
             3) Map e distanze 1D (niente visited)
             4) PQ leggera (id+dist)
   ==========================================
   NOTE: Output invariato.
*/

#define MAX_CHAR 16
#define NUM_AIR_ROUTES 5

/* Cache: capacità fissa e open addressing (power-of-two per mask) */
#define CACHE_CAPACITY (1u<<19) /* 524,288 slot ~8–10 MiB */
#define CACHE_MASK     (CACHE_CAPACITY-1)

/*--------------------------------------------------------- GLOBALS / MACROS ------------------------------------------------------*/

typedef struct {
    uint32_t dest_id; /* yd*cols + xd */
    uint16_t cost;    /* 0..100 */
} Air_Route_t;

typedef struct {
    uint16_t cost;          /* 0..100: costo d'uscita dalla cella */
    uint8_t  num_air_routes;/* 0..5 */
    Air_Route_t *air;       /* allocato alla prima toggle, 5 entry */
} Hexagon_t;

/* mappa 1D */
static Hexagon_t *map_cells = NULL;  
static int cols = 0, rows = 0; 

/* distanze 1D (Dijkstra), niente visited: si scartano gli estratti obsoleti */
static int *distances = NULL; 

/* indici di vicinato su griglia esagonale odd-r */
static int hex_offsets_even[6][2] = {
    { 1,  0},  // E
    {-1,  0},  // W
    { 0,  1},  // NE
    {-1,  1},  // NW
    { 0, -1},  // SE
    {-1, -1}   // SW
};

static int hex_offsets_odd[6][2] = {
    { 1,  0},  // E
    {-1,  0},  // W
    { 1,  1},  // NE
    { 0,  1},  // NW
    { 1, -1},  // SE
    { 0, -1}   // SW
};

/* helper mapping */
#define ID(X,Y)        ((uint32_t)((Y) * (long long)cols + (X)))
#define IN_BOUNDS(X,Y) ((X) >= 0 && (X) < cols && (Y) >= 0 && (Y) < rows)
#define HEX_AT(X,Y)    (map_cells[ ID((X),(Y)) ])

/*--------------------------------------------------------- CACHE (open addressing) -----------------------------------------------*/

typedef struct {
    uint32_t src;  /* ID(xp,yp) */
    uint32_t dst;  /* ID(xd,yd) */
    int32_t  cost;
    uint8_t  used; /* 0=empty, 1=occupied */
} CacheEntryOA;

static CacheEntryOA *cache_table = NULL;

static inline uint32_t cache_hash_u32(uint32_t a, uint32_t b) {
    /* 2x32 → mix → 32; moltiplicazioni prime + xor, mask su power-of-two */
    uint32_t h = a * 2654435761u; 
    h ^= b * 1597334677u; 
    return h & CACHE_MASK;
}

static void cache_init(void) {
    if (!cache_table) cache_table = (CacheEntryOA*)calloc(CACHE_CAPACITY, sizeof(CacheEntryOA));
    else memset(cache_table, 0, CACHE_CAPACITY * sizeof(CacheEntryOA));
}

static void cache_clear(void) {
    if (cache_table) memset(cache_table, 0, CACHE_CAPACITY * sizeof(CacheEntryOA));
}

static bool cache_lookup_ids(uint32_t src, uint32_t dst, int *out_cost) {
    if (!cache_table) return false;
    uint32_t pos = cache_hash_u32(src, dst);
    for (uint32_t k = 0; k < CACHE_CAPACITY; ++k) {
        CacheEntryOA *e = &cache_table[(pos + k) & CACHE_MASK];
        if (!e->used) return false;               /* slot libero ⇒ stop */
        if (e->used && e->src == src && e->dst == dst) {
            *out_cost = e->cost;
            return true;
        }
    }
    return false; /* tabella piena e non trovato */
}

static void cache_insert_ids(uint32_t src, uint32_t dst, int cost) {
    if (!cache_table) return;
    uint32_t pos = cache_hash_u32(src, dst);
    for (uint32_t k = 0; k < CACHE_CAPACITY; ++k) {
        CacheEntryOA *e = &cache_table[(pos + k) & CACHE_MASK];
        if (!e->used || (e->src == src && e->dst == dst)) {
            e->src = src; e->dst = dst; e->cost = cost; e->used = 1; 
            return;
        }
    }
    /* Tabella piena: sovrascrivi posizione di hash base */
    cache_table[pos] = (CacheEntryOA){ .src=src, .dst=dst, .cost=cost, .used=1 };
}

/* wrapper compatibili con vecchia API */
static inline bool cache_lookup(int xp,int yp,int xd,int yd,int *res){
    return cache_lookup_ids(ID(xp,yp), ID(xd,yd), res);
}
static inline void cache_insert(int xp,int yp,int xd,int yd,int cost){
    cache_insert_ids(ID(xp,yp), ID(xd,yd), cost);
}

/*--------------------------------------------------------- PRIORITY QUEUE (min-heap) ---------------------------------------------*/

typedef struct { int id; int distance; } PQNode;

typedef struct {
    PQNode *heap; int size; int capacity;
} Priority_Queue_t;

static void pq_init(Priority_Queue_t *pq, int capacity) {
    if (capacity < 16) capacity = 16;
    pq->heap = (PQNode*)malloc((size_t)capacity * sizeof(PQNode));
    pq->size = 0; pq->capacity = capacity;
}

static void pq_free(Priority_Queue_t *pq) {
    free(pq->heap); pq->heap=NULL; pq->size=0; pq->capacity=0;
}

static inline void heap_swap(PQNode *a, PQNode *b){ PQNode t=*a; *a=*b; *b=t; }

static void heap_up(Priority_Queue_t *pq, int idx){
    while (idx>0){
        int p=(idx-1)/2; 
        if (pq->heap[idx].distance >= pq->heap[p].distance) break;
        heap_swap(&pq->heap[idx], &pq->heap[p]); idx=p;
    }
}

static void heap_down(Priority_Queue_t *pq, int idx){
    for(;;){
        int l=2*idx+1, r=l+1, s=idx; 
        if (l<pq->size && pq->heap[l].distance < pq->heap[s].distance) s=l;
        if (r<pq->size && pq->heap[r].distance < pq->heap[s].distance) s=r;
        if (s==idx) break;
        heap_swap(&pq->heap[idx], &pq->heap[s]);
        idx = s;
    }
}

static void pq_push(Priority_Queue_t *pq, int id, int dist){
    if (pq->size >= pq->capacity){
        int newcap = pq->capacity*2; if (newcap < 16) newcap=16;
        PQNode *nh = (PQNode*)realloc(pq->heap, (size_t)newcap*sizeof(PQNode));
        if (!nh) return;
        pq->heap = nh;
        pq->capacity = newcap;
    }
    pq->heap[pq->size].id = id; pq->heap[pq->size].distance = dist;
    heap_up(pq, pq->size); pq->size++;
}

static PQNode pq_pop(Priority_Queue_t *pq){
    PQNode m = pq->heap[0];
    pq->size--; pq->heap[0] = pq->heap[pq->size];
    if (pq->size>0) heap_down(pq, 0);
    return m;
}

static inline bool pq_empty(Priority_Queue_t *pq){ return pq->size==0; }

/*--------------------------------------------------------- DECLARATIONS ------------------------------------------------------*/
static void init(int M, int N);
static void change_cost(int x, int y, int v, int r);
static int  dist_hex(int xa, int ya, int xb, int yb);
static void toggle_air_route(int xp, int yp, int xd, int yd);
static int  calculate_air_route_cost(int xp, int yp);
static void remove_air_route(int xp, int yp, int xd, int yd, int idx);
static int  travel_cost(int xp, int yp, int xd, int yd);
static void init_distances(void);
static void free_distances(void);
static bool is_valid_hex(int x, int y);

/*-------------------------------------------------------------- INIT ------------------------------------------------------------*/
static void init(int M, int N) {
    /* free precedente */
    if (map_cells != NULL) {
        int total = rows*cols;
        for (int i=0;i<total;i++){
            if (map_cells[i].air){ free(map_cells[i].air); map_cells[i].air=NULL; }
        }
        free(map_cells); map_cells=NULL;
    }

    cols = M; rows = N;
    int total = rows*cols;
    map_cells = (Hexagon_t*)calloc((size_t)total, sizeof(Hexagon_t));
    if (!map_cells){ printf("KO\n"); return; }

    for (int i=0;i<total;i++){
        map_cells[i].cost = 1;
        map_cells[i].num_air_routes = 0;
        map_cells[i].air = NULL; /* alloc lazy */
    }

    if (!cache_table) cache_init(); else cache_clear();

    printf("OK\n");
}

/*-------------------------------------------------------------- CHANGE COST ------------------------------------------------------*/
static void change_cost(int x, int y, int v, int r){
    if (v < -10 || v > 10 || r <= 0 || map_cells == NULL || !IN_BOUNDS(x,y)) { printf("KO\n"); return; }

    cache_clear();

    for (int iy=0; iy<rows; ++iy){
        for (int ix=0; ix<cols; ++ix){
            int d = dist_hex(ix, iy, x, y);
            if (d < r) {
                float factor = (r - d) / (float)r; if (factor < 0.0f) factor = 0.0f;
                int delta = (int)floorf(v * factor);

                Hexagon_t *h = &HEX_AT(ix,iy);
                int nc = (int)h->cost + delta; if (nc>100) nc=100; else if (nc<0) nc=0; h->cost = (uint16_t)nc;

                for (int k=0; k<h->num_air_routes; ++k){
                    int ac = (int)h->air[k].cost + delta; if (ac>100) ac=100; else if (ac<0) ac=0; h->air[k].cost = (uint16_t)ac;
                }
            }
        }
    }

    printf("OK\n");
}

static int dist_hex(int xa, int ya, int xb, int yb){
    /* odd-r → cube coords */
    int x1 = xa - ((ya - (ya & 1)) / 2);
    int z1 = ya; int y1 = -x1 - z1;
    int x2 = xb - ((yb - (yb & 1)) / 2);
    int z2 = yb; int y2 = -x2 - z2;
    int dx = x2 - x1, dy = y2 - y1, dz = z2 - z1;
    int adx = dx < 0 ? -dx : dx;
    int ady = dy < 0 ? -dy : dy;
    int adz = dz < 0 ? -dz : dz;
    return (adx + ady + adz) / 2;
}

/*-------------------------------------------------------------- TOGGLE AIR ROUTE ------------------------------------------------*/
static void toggle_air_route(int xp, int yp, int xd, int yd){
    if (!map_cells || !IN_BOUNDS(xp,yp) || !IN_BOUNDS(xd,yd)) { printf("KO\n"); return; }

    cache_clear();

    Hexagon_t *h = &HEX_AT(xp,yp);
    uint32_t want = ID(xd,yd);

    /* se esiste → rimuovi */
    for (int i=0; i<h->num_air_routes; ++i){
        if (h->air && h->air[i].dest_id == want){
            remove_air_route(xp,yp,xd,yd,i); printf("OK\n"); return;
        }
    }

    if (h->num_air_routes >= NUM_AIR_ROUTES) { printf("KO\n"); return; }
    if (!h->air) {
        h->air = (Air_Route_t*)malloc(NUM_AIR_ROUTES * sizeof(Air_Route_t));
        if (!h->air){ printf("KO\n"); return; }
    }

    int idx = h->num_air_routes;
    h->air[idx].dest_id = want;
    h->air[idx].cost    = (uint16_t)calculate_air_route_cost(xp, yp);
    h->num_air_routes++;

    printf("OK\n");
}

static int calculate_air_route_cost(int xp, int yp){
    Hexagon_t *h = &HEX_AT(xp,yp);
    int sum = 0;
    for (int i=0; i<h->num_air_routes; ++i) sum += h->air[i].cost;
    float avgf = (float)(sum + h->cost) / (float)(h->num_air_routes + 1);
    int avg = (int)floorf(avgf); if (avg>100) avg=100; else if (avg<0) avg=0; return avg;
}

static void remove_air_route(int xp, int yp, int xd, int yd, int idx){
    Hexagon_t *h = &HEX_AT(xp,yp);
    if (idx < 0 || idx >= h->num_air_routes) return;
    for (int j=idx; j<h->num_air_routes-1; ++j) h->air[j] = h->air[j+1];
    h->num_air_routes--;
    if (h->num_air_routes==0){ free(h->air); h->air=NULL; }
}

/*-------------------------------------------------------------- TRAVEL COST (Dijkstra) -------------------------------------------*/

static void init_distances(void){
    int total = rows*cols;
    distances = (int*)malloc((size_t)total * sizeof(int));
    for (int i=0;i<total;i++) distances[i] = INT_MAX;
}

static void free_distances(void){ free(distances); distances=NULL; }

static bool is_valid_hex(int x, int y){ return IN_BOUNDS(x,y); }

static int travel_cost(int xp, int yp, int xd, int yd){
    if (!map_cells) return -1;
    if (!is_valid_hex(xp,yp) || !is_valid_hex(xd,yd)) return -1;

    if (xp==xd && yp==yd){ cache_insert(xp,yp,xd,yd,0); return 0; }

    int cached;
    if (cache_lookup(xp,yp,xd,yd,&cached)) return cached;

    init_distances();
    Priority_Queue_t pq; pq_init(&pq, 32);

    uint32_t src = ID(xp,yp), dst = ID(xd,yd);
    distances[src] = 0; pq_push(&pq, (int)src, 0);

    while (!pq_empty(&pq)){
        PQNode cur = pq_pop(&pq);
        if (cur.distance != distances[cur.id]) continue; /* estrazione obsoleta */

        int cx = (int)(cur.id % cols);
        int cy = (int)(cur.id / cols);

        /* cache intermedio (cap fissa → sicuro) */
        cache_insert(xp, yp, cx, cy, cur.distance);

        if ((uint32_t)cur.id == dst){
            cache_insert(xp, yp, xd, yd, cur.distance);
            int result = cur.distance;
            free_distances(); pq_free(&pq); 
            return result;
        }

        Hexagon_t *h = &HEX_AT(cx,cy);
        if (h->cost == 0) continue; /* nessun vicino se cella non transitabile */

        /* Terrestri */
        int (*offs)[2] = (cy % 2 == 0) ? hex_offsets_even : hex_offsets_odd;
        for (int i=0;i<6;i++){
            int nx = cx + offs[i][0];
            int ny = cy + offs[i][1];
            if (!IN_BOUNDS(nx,ny)) continue;
            uint32_t nid = ID(nx,ny);
            int nd = cur.distance + h->cost; /* costo di uscita dalla cella corrente */
            if (nd < distances[nid]){ distances[nid]=nd; pq_push(&pq,(int)nid, nd); }
        }

        /* Aeree */
        for (int i=0;i<h->num_air_routes;i++){
            uint32_t nid = h->air[i].dest_id;
            int nd = cur.distance + h->air[i].cost;
            if (nd < distances[nid]){ distances[nid]=nd; pq_push(&pq,(int)nid, nd); }
        }
    }

    free_distances(); pq_free(&pq); 
    return -1; /* non raggiungibile */
}

/*-------------------------------------------------------------- MAIN ------------------------------------------------------------*/
int main(void){
    char comando[MAX_CHAR];
    while (scanf("%15s", comando) == 1){
        if(strcmp(comando, "init")==0){
            int x, y; if(scanf("%d %d", &x, &y)==2) init(x, y);
        }else if(strcmp(comando, "change_cost")==0){
            int x, y, v, r; if(scanf("%d %d %d %d", &x, &y, &v, &r)==4) change_cost(x, y, v, r);
        }else if(strcmp(comando, "travel_cost")==0){
            int xp, yp, xd, yd; if(scanf("%d %d %d %d", &xp, &yp, &xd, &yd)==4) printf("%d\n", travel_cost(xp, yp, xd, yd));
        }else if(strcmp(comando, "toggle_air_route")==0){
            int xp, yp, xd, yd; if(scanf("%d %d %d %d", &xp, &yp, &xd, &yd)==4) toggle_air_route(xp, yp, xd, yd);
        }
    }
    return 0;
}
