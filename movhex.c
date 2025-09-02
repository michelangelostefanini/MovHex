#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <limits.h>

#define MAX_CHAR 16
#define MAX(a,b) ((a) > (b) ? (a) : (b)) //macro for max
#define NUM_AIR_ROUTES 5
#define CACHE_TABLE_SIZE 10007


/*---------------------------------------------------------DATA STRUCTURES------------------------------------------------------*/
typedef struct Air_Route{
    int part_x, part_y;
    int dest_x, dest_y;
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
    int xp, yp, xd, yd;
    int cost;
    struct CacheEntry *next;
} CacheEntry_t;

/*-------------------------------------------------------- GLOBAL VARIABLES ----------------------------------------------------*/
Hexagon_t **map = NULL;  
int cols = 0, rows = 0; 
CacheEntry_t **cache_table = NULL;

int hex_offsets_even[6][2] = {
    { 1,  0},  // E
    {-1,  0},  // W
    { 0,  1},  // NE
    {-1,  1},  // NW
    { 0, -1},  // SE
    {-1, -1}   // SW
};

int hex_offsets_odd[6][2] = {
    { 1,  0},  // E
    {-1,  0},  // W
    { 1,  1},  // NE
    { 0,  1},  // NW
    { 1, -1},  // SE
    { 0, -1}   // SW
};


int** distances; 
bool** visited; //true if processed

/*---------------------------------------------------------FUNCTIONS DECLARATION----------------------------------------------------*/

void init(int, int);


void change_cost(int, int, int, int);
//utils:
int dist_hex(int, int, int, int);


void toggle_air_route(int, int, int, int);
//utils:
int calculate_air_route_cost(int, int);
void remove_air_route(int, int, int, int, int);


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

/* cache utils */
void cache_init();
void cache_clear();
bool cache_lookup(int, int, int, int, int*);
void cache_insert(int, int, int, int, int);


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

    map = malloc(sizeof(Hexagon_t *)*rows);   // array di puntatori alle righe
    for (int i=0; i<rows; i++) {
        map[i] = malloc(sizeof(Hexagon_t)*cols);  // ogni riga ha 'cols' elementi
    }

    // iniz
    for (int y = 0; y < rows; y++) {
        for (int x = 0; x < cols; x++) {
            map[y][x].cost = 1;
            map[y][x].air_routes = NULL; // initialize pointer
            map[y][x].num_air_routes = 0; // initialize count
        }
    }

    if(cache_table == NULL){
        cache_init();
    }else{
        cache_clear();
    }

    printf("OK\n");
}

/*--------------------------------------------------------------CHANGE COST------------------------------------------------------------*/

void change_cost(int x, int y, int v, int r){
    
    if (v < -10 || v > 10 || r <= 0 || map == NULL || x < 0 || y < 0 || x >= cols || y >= rows) {
        printf("KO\n");
        return;
    }

    if (cache_table != NULL) {
        cache_clear();
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

    printf("OK\n");
}



int dist_hex(int xa, int ya, int xb, int yb){
    /*odd-r*/
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

    if (map == NULL || !is_valid_hex(xp, yp) || !is_valid_hex(xd, yd)) {
        printf("KO\n");
        return;
    }

    if(cache_table != NULL){
        cache_clear();
    }

    // If exists delete
    if (map[yp][xp].air_routes != NULL) {
        for (int i = 0; i < map[yp][xp].num_air_routes; i++) {
            if (map[yp][xp].air_routes[i].dest_x == xd && map[yp][xp].air_routes[i].dest_y == yd) {
                remove_air_route(xp, yp, xd, yd, i);
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
        map[yp][xp].air_routes = malloc(sizeof(Air_Route_t));
    } else {
        map[yp][xp].air_routes = realloc(map[yp][xp].air_routes,(map[yp][xp].num_air_routes + 1) * sizeof(Air_Route_t));
    }

    if (!map[yp][xp].air_routes) {
        printf("KO\n");
        return;
    }

    int idx = map[yp][xp].num_air_routes;
    map[yp][xp].air_routes[idx].part_x = xp;
    map[yp][xp].air_routes[idx].part_y = yp;
    map[yp][xp].air_routes[idx].dest_x = xd;
    map[yp][xp].air_routes[idx].dest_y = yd;
    map[yp][xp].air_routes[idx].cost  = calculate_air_route_cost(xp, yp);

    map[yp][xp].num_air_routes++;

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

void remove_air_route(int xp, int yp, int xd, int yd, int i){
    if(i<0 || i>=map[yp][xp].num_air_routes) {
        return;
    }

    //shift to sx
    for (int j=i; j<map[yp][xp].num_air_routes-1; j++) {
        map[yp][xp].air_routes[j] = map[yp][xp].air_routes[j + 1];
    }

    map[yp][xp].num_air_routes--;

    //Memory re-sizing
    if(map[yp][xp].num_air_routes == 0){
        free(map[yp][xp].air_routes);
        map[yp][xp].air_routes = NULL;
    }else{
        Air_Route_t *tmp = realloc(
            map[yp][xp].air_routes,
            map[yp][xp].num_air_routes * sizeof(Air_Route_t));
        if(tmp != NULL){
            map[yp][xp].air_routes = tmp;
        }
    }
}

/*--------------------------------------------------------------TRAVEL COST------------------------------------------------------------*/

int travel_cost(int xp, int yp, int xd, int yd){
    if(map==NULL){
        return -1;
    }

    if (xp == xd && yp == yd) {
        if(cache_table != NULL){
            cache_insert(xp, yp, xd, yd, 0);
        }
        return 0;
    }

    if (!is_valid_hex(xp, yp) || !is_valid_hex(xd, yd)) {
        return -1; 
    }

    int cached_cost;
    if(cache_lookup(xp, yp, xd, yd, &cached_cost)){
        return cached_cost;
    }

    // DIJKSTRA ALGORITHM

    //iniz:
    init_distances_and_visited();

    Priority_Queue_t pq;
    pq_init(&pq, rows*cols);

    distances[yp][xp] = 0;
    pq_push(&pq, xp, yp, 0);

    while (!pq_empty(&pq)) {
        HeapNode_t current = pq_pop(&pq);

        if (visited[current.y][current.x]) continue;
        visited[current.y][current.x] = true;

        cache_insert(xp, yp, current.x, current.y, current.distance);
 
        if (current.x == xd && current.y == yd) {
            int result = current.distance;
            
            cache_insert(xp, yp, xd, yd, result);

            free_distances_and_visited();
            pq_free(&pq);
            
            return result;
        }

        // Neighbors
        HeapNode_t neighbors[11]; // max 6 terrestri + 5 aerei
        int num_neighbors = get_neighbors(current.x, current.y, neighbors);

        for (int i = 0; i < num_neighbors; i++) {
            int nx = neighbors[i].x;
            int ny = neighbors[i].y;
            int new_dist = current.distance + neighbors[i].distance;
            
            if (new_dist < distances[ny][nx]) {
                distances[ny][nx] = new_dist;
                pq_push(&pq, nx, ny, new_dist);
            }
        }
    }

    // Destination unreachable
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
        //swap
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
        //swap
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
    distances = (int**)malloc(rows * sizeof(int*));

    for (int i = 0; i < rows; i++) {
        distances[i] = (int*)malloc(cols * sizeof(int));
        for (int j = 0; j < cols; j++) {
            distances[i][j] = INT_MAX; 
        }
    }
    
    visited = (bool**)malloc(rows * sizeof(bool*));
    for (int i = 0; i < rows; i++) {
        visited[i] = (bool*)malloc(cols * sizeof(bool));
        for (int j = 0; j < cols; j++) {
            visited[i][j] = false;
        }
    }
}

void free_distances_and_visited() {
    for (int i = 0; i < rows; i++) {
        free(distances[i]);
    }
    free(distances);
    
    for (int i = 0; i < rows; i++) {
        free(visited[i]);
    }
    free(visited);
}

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
            
            if (is_valid_hex(route->dest_x, route->dest_y)) {
                neighbors[count].x = route->dest_x;
                neighbors[count].y = route->dest_y;
                neighbors[count].distance = route->cost;
                count++;
            }
        }
    }
    
    return count;
}

/*---------------------------- CACHE IMPLEMENTATION ----------------------------*/
static unsigned int cache_hash(int xp, int yp, int xd, int yd){
    unsigned int hash = 0;
    hash ^= (unsigned int)xp * 73856093u;
    hash ^= (unsigned int)yp * 19349663u;
    hash ^= (unsigned int)xd * 83492791u;
    hash ^= (unsigned int)yd * 1234567u;
    return hash % CACHE_TABLE_SIZE;
}

void cache_init(){
    cache_table = (CacheEntry_t **)calloc(CACHE_TABLE_SIZE, sizeof(CacheEntry_t*));
}

void cache_clear(){
    if(cache_table == NULL) return;
    for(int i = 0; i < CACHE_TABLE_SIZE; i++){
        CacheEntry_t *cur = cache_table[i];
        while(cur){
            CacheEntry_t *tmp = cur->next;
            free(cur);
            cur = tmp;
        }
        cache_table[i] = NULL;
    }
}

bool cache_lookup(int xp, int yp, int xd, int yd, int *result){
    if(cache_table == NULL) return false;
    unsigned int h = cache_hash(xp, yp, xd, yd);
    CacheEntry_t *cur = cache_table[h];
    while(cur){
        if(cur->xp == xp && cur->yp == yp && cur->xd == xd && cur->yd == yd){
            *result = cur->cost;
            return true;
        }
        cur = cur->next;
    }
    return false;
}

void cache_insert(int xp, int yp, int xd, int yd, int cost){
    if(cache_table == NULL) return;
    unsigned int h = cache_hash(xp, yp, xd, yd);
    CacheEntry_t *new_entry = (CacheEntry_t *)malloc(sizeof(CacheEntry_t));
    new_entry->xp = xp;
    new_entry->yp = yp;
    new_entry->xd = xd;
    new_entry->yd = yd;
    new_entry->cost = cost;
    new_entry->next = cache_table[h];
    cache_table[h] = new_entry;
}

//------------------------------------------------------------MAIN------------------------------------------------------------


int main(){
    char comando[MAX_CHAR];

    while (scanf("%s", comando) == 1){
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

    return 0;
}

