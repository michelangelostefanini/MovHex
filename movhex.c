#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <limits.h>
#include <ctype.h>
#include <stdint.h>

#define MAX_CHAR 32
#define MAX(a,b) ((a) > (b) ? (a) : (b)) //macro for max
#define NUM_AIR_ROUTES 5
#define ID_FROM_XY(x,y) ((y) * cols + (x))
#define X_FROM_ID(id) ((id) % cols)
#define Y_FROM_ID(id) ((id) / cols)
#define EMPTY_KEY UINT64_MAX


/*---------------------------------------------------------DATA STRUCTURES------------------------------------------------------*/
typedef struct Air_Route{
    int dest_id;  
    int cost;    
} Air_Route_t;

typedef struct Hexagon{
    int cost;
    Air_Route_t *air_routes;
    int num_air_routes;             
} Hexagon_t;

typedef struct HeapNode{
    int x, y;
    int distance;
} HeapNode_t;

typedef struct Priority_Queue{
    HeapNode_t *heap;
    int size;
    int capacity;
} Priority_Queue_t;

typedef struct CacheEntry{
    uint64_t key;   
    int32_t  cost; 
} CacheEntry_t;

typedef struct Cache{
    CacheEntry_t *tab;   
    uint32_t    cap;    
    uint32_t    mask;   
    uint32_t    size;
} Cache;


/*-------------------------------------------------------- GLOBAL VARIABLES ----------------------------------------------------*/
Hexagon_t **map = NULL;  
int cols = 0, rows = 0; 

// Offsets

int hex_offsets_even[6][2] = {
    { 1,  0},  
    {-1,  0}, 
    { 0,  1},  
    {-1,  1},  
    { 0, -1},  
    {-1, -1}   
};

int hex_offsets_odd[6][2] = {
    { 1,  0},  
    {-1,  0},  
    { 1,  1}, 
    { 0,  1},  
    { 1, -1},  
    { 0, -1}  
};

static int *dist = NULL;
static size_t dist_cap = 0;
static Cache g_cache;

/*---------------------------------------------------------FUNCTIONS DECLARATION----------------------------------------------------*/

void init(int, int);


void change_cost(int, int, int, int);
//utils:
int dist_hex(int, int, int, int);


void toggle_air_route(int, int, int, int);
//utils:
int calculate_air_route_cost(int, int);
void remove_air_route(int, int, int);

int travel_cost(int, int, int, int);
//utils:
void init_distances_and_visited();
void free_distances_and_visited();
void pq_init(Priority_Queue_t*, int);
void pq_free(Priority_Queue_t*);
void pq_push(Priority_Queue_t*, int, int, int);
HeapNode_t pq_pop(Priority_Queue_t*);
void heap_up(HeapNode_t*, int);
void heap_down(HeapNode_t*, int, int) ;
bool pq_empty(Priority_Queue_t*);
bool is_valid_hex(int, int);
int get_neighbors(int, int, HeapNode_t*);

//Cache
static inline void cache_free(Cache*);
static void  cache_init(Cache*, uint32_t);
static inline void cache_clear(Cache *);
static inline int  cache_get(const Cache *, uint32_t, uint32_t, int *);
static inline void cache_put(Cache *, uint32_t, uint32_t, int);

/*--------------------------------------------------------------INIT------------------------------------------------------------*/
void init(int M, int N) {
    if (map != NULL) {
        for(int i=0; i<rows; i++) {
            for(int j = 0; j < cols; j++) {
                if(map[i][j].air_routes != NULL){
                    free(map[i][j].air_routes);
                }
            }
            free(map[i]);
        }
        free(map);
    }

    cols = M;
    rows = N;

    map = malloc(sizeof(Hexagon_t *)*rows);   
    for (int i=0; i<rows; i++) {
        map[i] = malloc(sizeof(Hexagon_t)*cols);  
    }

    for (int y = 0; y < rows; y++) {
        for (int x = 0; x < cols; x++) {
            map[y][x].cost = 1;
            map[y][x].air_routes = NULL;
            map[y][x].num_air_routes = 0;
        }
    }
    
    cache_free(&g_cache);
    cache_init(&g_cache, 1u << 16);

    printf("OK\n");
}

/*--------------------------------------------------------------CHANGE COST------------------------------------------------------------*/

void change_cost(int x, int y, int v, int r){
    
    if (v < -10 || v > 10 || r <= 0 || map == NULL || x < 0 || y < 0 || x >= cols || y >= rows) {
        printf("KO\n");
        return;
    }
    
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            int d = dist_hex(j, i, x, y);

            if (d < r) {
                float factor = (r - d) / (float)r;
                if (factor < 0.0f) factor = 0.0f;

                int delta = (int)floorf(v * factor);

                map[i][j].cost += delta;
                if (map[i][j].cost > 100) map[i][j].cost = 100;
                else if (map[i][j].cost < 0) map[i][j].cost = 0;

                for (int k = 0; k < map[i][j].num_air_routes; k++) {
                    map[i][j].air_routes[k].cost += delta;
                    if (map[i][j].air_routes[k].cost > 100) map[i][j].air_routes[k].cost = 100;
                    else if (map[i][j].air_routes[k].cost < 0) map[i][j].air_routes[k].cost = 0;
                }
            }
        }
    }

    cache_clear(&g_cache);

    printf("OK\n");
}



int dist_hex(int xa, int ya, int xb, int yb){
    // Odd-r layout
    int x1 = xa - ((ya - (ya & 1)) / 2);
    int z1 = ya;
    int y1 = -x1 - z1;

    int x2 = xb - ((yb - (yb & 1)) / 2);
    int z2 = yb;
    int y2 = -x2 - z2;

    int dx = x2 - x1;
    int dy = y2 - y1;
    int dz = z2 - z1;

    int adx = dx < 0 ? -dx : dx;
    int ady = dy < 0 ? -dy : dy;
    int adz = dz < 0 ? -dz : dz;

    return (adx + ady + adz) / 2;
}


/*--------------------------------------------------------------TOGGLE AIR ROUTE------------------------------------------------------------*/

void toggle_air_route(int xp, int yp, int xd, int yd){

    if (map == NULL || !is_valid_hex(xp, yp) || !is_valid_hex(xd, yd) || (xp == xd && yp == yd)) {
        printf("KO\n");
        return;
    }
    
    // If exists delete
    if (map[yp][xp].air_routes != NULL) {
        int want = ID_FROM_XY(xd, yd);
        for (int i = 0; i < map[yp][xp].num_air_routes; i++) {
            if (map[yp][xp].air_routes[i].dest_id == want) {
                remove_air_route(xp, yp, i);
                cache_clear(&g_cache);
                printf("OK\n");
                return;
            }
        }
    }

    if (map[yp][xp].num_air_routes >= NUM_AIR_ROUTES) {
        printf("KO\n");
        return;
    }

    // First route
    if (map[yp][xp].air_routes == NULL) {
        map[yp][xp].air_routes = malloc(NUM_AIR_ROUTES * sizeof(Air_Route_t));
    }
    

    if (!map[yp][xp].air_routes) {
        printf("KO\n");
        return;
    }

    int idx = map[yp][xp].num_air_routes;

    map[yp][xp].air_routes[idx].dest_id = ID_FROM_XY(xd, yd);
    map[yp][xp].air_routes[idx].cost    = calculate_air_route_cost(xp, yp);

    map[yp][xp].num_air_routes++;
   
    cache_clear(&g_cache);

    printf("OK\n");
}

int calculate_air_route_cost(int xp, int yp){
    int sum_connection_costs = 0;
    
    for(int i = 0; i < map[yp][xp].num_air_routes; i++){
        sum_connection_costs += map[yp][xp].air_routes[i].cost;
    }
    
    
    float avg_float = (float)(sum_connection_costs + map[yp][xp].cost) / (float)(map[yp][xp].num_air_routes + 1);
    int avg = (int)floor(avg_float);
    
    if(avg > 100){
        avg = 100;
    } else if(avg < 0){
        avg = 0;
    }
    
    return avg;
}

void remove_air_route(int xp, int yp, int i){
    if(i<0 || i>=map[yp][xp].num_air_routes) {
        return;
    }

    // Shift to sx
    for (int j=i; j<map[yp][xp].num_air_routes-1; j++) {
        map[yp][xp].air_routes[j] = map[yp][xp].air_routes[j + 1];
    }

    map[yp][xp].num_air_routes--;

    // Keep capacity for future routes; free only when no routes remain.
    if(map[yp][xp].num_air_routes == 0){
        free(map[yp][xp].air_routes);
        map[yp][xp].air_routes = NULL;
    }

}

/*--------------------------------------------------------------TRAVEL COST------------------------------------------------------------*/

int travel_cost(int xp, int yp, int xd, int yd) {
    if(map == NULL)return -1;
    if(!is_valid_hex(xp, yp) || !is_valid_hex(xd, yd)) return -1;
    if(xp == xd && yp == yd) return 0;

    // Keys for cache
    uint32_t src = (uint32_t)yp * (uint32_t)cols + (uint32_t)xp;
    uint32_t dst = (uint32_t)yd * (uint32_t)cols + (uint32_t)xd;

    // Lookup in cache
    int cached_cost;
    if (cache_get(&g_cache, src, dst, &cached_cost)) {
        return cached_cost; 
    }

    // DIJKSTRA ALGORITHM
    init_distances_and_visited();

    Priority_Queue_t pq;
    pq_init(&pq, rows * cols);

    dist[ID_FROM_XY(xp, yp)] = 0;
    pq_push(&pq, xp, yp, 0);

    // Neighbors buffer
    HeapNode_t neighbors[11];

    while (!pq_empty(&pq)) {
        HeapNode_t current = pq_pop(&pq);

        if (current.distance != dist[ID_FROM_XY(current.x, current.y)]) continue;

        // Early exit
        if (current.x == xd && current.y == yd) {
            int result = current.distance;
            cache_put(&g_cache, src, dst, result);

            free_distances_and_visited();
            pq_free(&pq);
            return result;
        }

        int num_neighbors = get_neighbors(current.x, current.y, neighbors);
        for (int i = 0; i < num_neighbors; ++i) {
            int nx = neighbors[i].x;
            int ny = neighbors[i].y;
            int nd = current.distance + neighbors[i].distance;
            uint32_t nid = ID_FROM_XY(nx, ny);
            if (nd < dist[nid]) {
                dist[nid] = nd;
                pq_push(&pq, nx, ny, nd);
            }
        }
    }

    cache_put(&g_cache, src, dst, -1);
    free_distances_and_visited();
    pq_free(&pq);
    return -1;
}


// Priority queue functions

void pq_init(Priority_Queue_t* pq, int capacity) {
    if (capacity < 16) capacity = 16;
    pq->heap = (HeapNode_t*)malloc(capacity * sizeof(HeapNode_t));
    pq->size = 0;
    pq->capacity = capacity;
}

void pq_free(Priority_Queue_t* pq) {
    free(pq->heap);
    pq->heap = NULL;
    pq->size = 0;
}

void heap_up(HeapNode_t* heap, int index) {
    if (index == 0) return;
    
    int parent = (index - 1) / 2;
    if (heap[index].distance < heap[parent].distance) {
        // Swap
        HeapNode_t temp = heap[index];
        heap[index] = heap[parent];
        heap[parent] = temp;
        
        heap_up(heap, parent);
    }
}

void heap_down(HeapNode_t* heap, int size, int index) {
    int left = 2 * index + 1;
    int right = 2 * index + 2;
    int smallest = index;
    
    if (left < size && heap[left].distance < heap[smallest].distance)
        smallest = left;
    
    if (right < size && heap[right].distance < heap[smallest].distance)
        smallest = right;
    
    if (smallest != index) {
        // Swap
        HeapNode_t temp = heap[index];
        heap[index] = heap[smallest];
        heap[smallest] = temp;
        
        heap_down(heap, size, smallest);
    }
}

void pq_push(Priority_Queue_t* pq, int x, int y, int dist) {
    if (pq->size >= pq->capacity) {
        int new_capacity = pq->capacity * 2;
        HeapNode_t *new_heap = (HeapNode_t*)realloc(pq->heap, new_capacity * sizeof(HeapNode_t));
        if (new_heap == NULL) {
            return;
        }
        pq->heap = new_heap;
        pq->capacity = new_capacity;
    }

    pq->heap[pq->size].x = x;
    pq->heap[pq->size].y = y;
    pq->heap[pq->size].distance = dist;

    heap_up(pq->heap, pq->size);
    pq->size++;
}

HeapNode_t pq_pop(Priority_Queue_t* pq) {
    HeapNode_t min = pq->heap[0];
    
    pq->size--;
    pq->heap[0] = pq->heap[pq->size];
    
    if (pq->size > 0) {
        heap_down(pq->heap, pq->size, 0);
    }
    
    return min;
}

bool pq_empty(Priority_Queue_t* pq) {
    return pq->size == 0;
}

void init_distances_and_visited() {
    size_t need = (size_t)rows * (size_t)cols;
    if (need > dist_cap) {
        free(dist);
        dist = (int*)malloc(need * sizeof(int));
        dist_cap = need;
    }
    memset(dist, 0x3f, need * sizeof(int));
}

void free_distances_and_visited() {}

bool is_valid_hex(int x, int y) {
    return x >= 0 && x < cols && y >= 0 && y < rows;
}

int get_neighbors(int x, int y, HeapNode_t* neighbors) {
    int count = 0;
    
    if (map[y][x].cost == 0) {
        return 0;
    }
    
    int (*offsets)[2] = (y % 2 == 0) ? hex_offsets_even : hex_offsets_odd;
    
    // Terrestrial
    for (int i = 0; i < 6; i++) {
        int nx = x + offsets[i][0];
        int ny = y + offsets[i][1];
        
        if (is_valid_hex(nx, ny)) {
            neighbors[count].x = nx;
            neighbors[count].y = ny;
            neighbors[count].distance = map[y][x].cost;
            count++;
        }
    }
    
    // Air routes
    if (map[y][x].cost > 0) {
        for (int i = 0; i < map[y][x].num_air_routes; i++) {
            Air_Route_t* route = &map[y][x].air_routes[i];
            int nx = X_FROM_ID(route->dest_id);
            int ny = Y_FROM_ID(route->dest_id);
            if (is_valid_hex(nx, ny)) {
                neighbors[count].x = nx;
                neighbors[count].y = ny;
                neighbors[count].distance = route->cost;
                count++;
            }
        }
    }
    
    return count;
}

//----------------------------------------------------------CACHE----------------------------------------------------------

static inline uint32_t next_pow2(uint32_t x){
    if (x <= 1) return 1;
    x--; x |= x>>1; x |= x>>2; x |= x>>4; x |= x>>8; x |= x>>16;
    return x+1;
}

// Hash
static inline uint64_t mix64(uint64_t x){
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}

static inline uint64_t make_key(uint32_t src, uint32_t dst){
    return ((uint64_t)src << 32) | (uint64_t)dst;
}

static void cache_init(Cache *c, uint32_t min_capacity){
    uint32_t cap = next_pow2(min_capacity);
    if (cap < 1024u) cap = 1024u;          
    c->cap = cap; c->mask = cap - 1; c->size = 0;
    c->tab = (CacheEntry_t*)malloc(sizeof(CacheEntry_t) * cap);
    memset(c->tab, 0xFF, sizeof(CacheEntry_t) * cap);
}

static inline void cache_free(Cache *c){
    free(c->tab); c->tab = NULL; c->cap = c->mask = c->size = 0;
}

static inline void cache_clear(Cache *c){
    memset(c->tab, 0xFF, sizeof(CacheEntry_t) * c->cap);
    c->size = 0;
}

// Re-hash (when load factor > 0.7)
static void cache_grow(Cache *c){
    Cache old = *c;
    cache_init(c, old.cap << 1);
    for (uint32_t i=0; i<old.cap; ++i){
        if (old.tab[i].key != EMPTY_KEY){
            uint64_t k = old.tab[i].key, h = mix64(k);
            for (uint32_t j=0;;++j){
                uint32_t idx = (uint32_t)(h + j) & c->mask;
                if (c->tab[idx].key == EMPTY_KEY){
                    c->tab[idx] = old.tab[i]; c->size++;
                    break;
                }
            }
        }
    }
    free(old.tab);
}

static inline int cache_get(const Cache *c, uint32_t src, uint32_t dst, int *out_cost){
    uint64_t k = make_key(src, dst), h = mix64(k);
    for (uint32_t i=0;;++i){
        uint32_t idx = (uint32_t)(h + i) & c->mask;
        uint64_t key = c->tab[idx].key;
        if (key == k){ *out_cost = c->tab[idx].cost; return 1; }
        if (key == EMPTY_KEY) return 0; 
    }
}

static inline void cache_put(Cache *c, uint32_t src, uint32_t dst, int cost){
    if (c->size >= (c->cap * 7) / 10) cache_grow(c);
    uint64_t k = make_key(src, dst), h = mix64(k);
    for (uint32_t i=0;;++i){
        uint32_t idx = (uint32_t)(h + i) & c->mask;
        if (c->tab[idx].key == EMPTY_KEY || c->tab[idx].key == k){
            if (c->tab[idx].key == EMPTY_KEY) c->size++;
            c->tab[idx].key  = k;
            c->tab[idx].cost = cost;
            return;
        }
    }
}

//------------------------------------------------------------MAIN------------------------------------------------------------

int main(){
    char comando[MAX_CHAR];

    while(scanf("%31s", comando) == 1){
        if(strcmp(comando, "init")==0){
            int x, y; 
            if(scanf("%d %d", &x, &y)==2){
                init(x, y);
            }
        }else if(strcmp(comando, "change_cost")==0){
            int x, y, v, r; 
            if(scanf("%d %d %d %d", &x, &y, &v, &r)==4){
                change_cost(x, y, v, r);
            }
        }else if(strcmp(comando, "travel_cost")==0){
            int xp, yp, xd, yd; 
            if(scanf("%d %d %d %d", &xp, &yp, &xd, &yd)==4){
                printf("%d\n", travel_cost(xp, yp, xd, yd));
            }
        }else if(strcmp(comando, "toggle_air_route")==0){
            int xp, yp, xd, yd; 
            if(scanf("%d %d %d %d", &xp, &yp, &xd, &yd)==4){
                toggle_air_route(xp, yp, xd, yd);
            }
        }
    }

    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < cols; ++x) free(map[y][x].air_routes);
        free(map[y]);
    }
    free(map);
    free(dist);
    cache_free(&g_cache);
    return 0;
}
