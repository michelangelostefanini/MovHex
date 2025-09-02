#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <limits.h>
#include <ctype.h>

#define MAX_CHAR 32
#define MAX(a,b) ((a) > (b) ? (a) : (b)) //macro for max
#define NUM_AIR_ROUTES 5
#define ID_FROM_XY(x,y) ((y) * cols + (x))
#define X_FROM_ID(id)   ((id) % cols)
#define Y_FROM_ID(id)   ((id) / cols)


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


/*-------------------------------------------------------- GLOBAL VARIABLES ----------------------------------------------------*/
Hexagon_t **map = NULL;  
int cols = 0, rows = 0; 

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

// Fast I/O:
static inline int get_char_fast();
static inline int skip_spaces();
static inline int read_word(char *);
static inline int read_int(int *);
static inline void fastio_setup();
static inline void write_str(const char *);
static inline void write_int_ln(int);

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

    // Shift to sx
    for (int j=i; j<map[yp][xp].num_air_routes-1; j++) {
        map[yp][xp].air_routes[j] = map[yp][xp].air_routes[j + 1];
    }

    map[yp][xp].num_air_routes--;

    // Memory re-sizing
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
    
    if (!is_valid_hex(xp, yp) || !is_valid_hex(xd, yd)) {
        return -1; 
    }

    // DIJKSTRA ALGORITHM

    // Iniz:
    init_distances_and_visited();

    Priority_Queue_t pq;
    pq_init(&pq, rows*cols);

    distances[yp][xp] = 0;
    pq_push(&pq, xp, yp, 0);

    while (!pq_empty(&pq)) {
        HeapNode_t current = pq_pop(&pq);

        if (current.distance != distances[current.y][current.x]) continue;

        if (visited[current.y][current.x]) continue;
        visited[current.y][current.x] = true;

        // Early exit
        if (current.x == xd && current.y == yd) {
            int result = current.distance;
            free_distances_and_visited();
            pq_free(&pq);
            return result;
        }

        // Neighbors
        HeapNode_t neighbors[11]; // Max 6 terrestrials + 5 planes
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

//----------------------------------------------------------FAST I/O----------------------------------------------------------

// ---------- FAST INPUT (stdin buffer 1 MiB) ----------

static unsigned char in_buffer[1<<20];
static int in_pos = 0;     
static int in_size = 0;   


static inline int get_char_fast(){
    if (in_pos >= in_size){
        in_size = (int)fread(in_buffer, 1, sizeof in_buffer, stdin);
        in_pos = 0;
        if (in_size == 0) return EOF;
    }
    return in_buffer[in_pos++];
}

static inline int skip_spaces(){
    int c = get_char_fast();
    while (c != EOF && c <= ' ') c = get_char_fast();
    return c;
}

static inline int read_word(char *dst){ 
    int c = skip_spaces(); 
    if (c == EOF) return 0;
    int i = 0;
    while (c != EOF && c > ' ') { 
        dst[i++] = (char)c; 
        c = get_char_fast(); 
        if (i == 31) break; 
    }
    dst[i] = '\0';
    return 1;
}

static inline int read_int(int *x){
    int c = skip_spaces(); 
    if (c == EOF) return 0;
    int sign = 1; 
    if (c == '-') { sign = -1; c = get_char_fast(); }
    int val = 0;
    while (c > ' '){ 
        val = val*10 + (c - '0'); 
        c = get_char_fast(); 
    }
    *x = sign * val; 
    return 1;
}

// ---------- FAST OUTPUT (stdout buffer 1 MiB) ----------

static inline void fastio_setup(){
    setvbuf(stdout, NULL, _IOFBF, 1<<20); 
}

static inline void write_str(const char *s){
    fputs(s, stdout);
}

static inline void write_int_ln(int v){
    char buf[32]; 
    int i = 0; 
    int n = v; 
    int neg = (v < 0);
    if (neg) n = -n;
    do {
        buf[i++] = (char)('0' + (n % 10)); 
        n /= 10;
    } while (n);
    if (neg) buf[i++] = '-';
    while (i--) fputc(buf[i], stdout);
    fputc('\n', stdout);
}

//------------------------------------------------------------MAIN------------------------------------------------------------

int main(){
    // Fast I/O 
    fastio_setup();                        // stdout 1 MiB
    setvbuf(stdin,  NULL, _IOFBF, 1<<20);  // stdin  1 MiB

    char cmd[32];
    while (read_word(cmd)) {
        if (cmd[0]=='i') { // init
            int M, N;
            if (!read_int(&M) || !read_int(&N)) break;
            init(M, N); 

        } else if (cmd[0]=='c') { // change_cost
            int x, y, v, r;
            if (!read_int(&x) || !read_int(&y) || !read_int(&v) || !read_int(&r)) break;
            change_cost(x, y, v, r); 

        } else if (cmd[0]=='t' && cmd[7]=='a') { // toggle_air_route
            int xp, yp, xd, yd;
            if (!read_int(&xp) || !read_int(&yp) || !read_int(&xd) || !read_int(&yd)) break;
            toggle_air_route(xp, yp, xd, yd); 

        } else if (cmd[0]=='t') { // travel_cost
            int xp, yp, xd, yd;
            if (!read_int(&xp) || !read_int(&yp) || !read_int(&xd) || !read_int(&yd)) break;
            int ans = travel_cost(xp, yp, xd, yd);
            write_int_ln(ans);
        }
    }
    return 0;
}
