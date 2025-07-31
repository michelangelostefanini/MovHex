#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MAX_CHAR 16
#define MAX(a,b) ((a) > (b) ? (a) : (b)) //macro for max

/*---------------------------------------------------------FUNCTIONS DECLARATION----------------------------------------------------*/

// main functions:

void init(int, int);

void change_cost(int, int, int, int);

void toggle_air_route(int, int, int, int);

int travel_cost(int, int, int, int);


//utils funcionts:

void calculate_air_route_cost();

int dist_hex(int, int, int, int);


/*---------------------------------------------------------DATA STRUCTURES------------------------------------------------------*/
typedef struct Hexagon{
    int x; // DA TOGLIERE prob le coordinate (x,y) dell'esagono sono implicite dalla sua posizione nella matrice map[y][x].
    int y;
    int cost;
} Hexagon_t;

typedef struct Air_Route{
    int dest_x, dest_y;
    int cost;
} Air_Route_t;

/*
// Nella mappa principale
Hexagon_t map[rows][cols];

// Per ogni esagono che ha rotte aeree
Air_Route_t air_routes[5];  // Dentro la struct Hexagon o separato
int num_air_routes;
*/

/*-------------------------------------------------------- GLOBAL VARIABLES ----------------------------------------------------*/
Hexagon_t **map = NULL;  // matrice dinamica
int cols = 0, rows = 0;


/*--------------------------------------------------------------INIT------------------------------------------------------------*/
void init(int M, int N) {
    if (map != NULL) {
        for (int i = 0; i < rows; i++) {
            free(map[i]);
        }
        free(map);
    }

    cols = M;
    rows = N;

    map = malloc(sizeof(Hexagon_t *)*rows);   // array di puntatori alle righe
    for (int i = 0; i < rows; i++) {
        map[i] = malloc(sizeof(Hexagon_t)*cols);  // ogni riga ha 'cols' elementi
    }

    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            map[i][j].cost = 1;
            printf("%d ", map[i][j].cost); //debug
        }
        printf("\n"); //debug
    }

    printf("OK\n");
}

/*--------------------------------------------------------------CHANGE COST------------------------------------------------------------*/

void change_cost(int x, int y, int v, int r){
    if(v < -10 || v > 10 || r <= 0 || map==NULL || x>=cols || y>=rows){
        printf("KO");
        return;
    }


    for (int i=0; i <rows; i++) {
        for (int j=0; j <cols; j++) {
            int distance_hex = dist_hex(i, j, x, y);
            if (distance_hex < r) {
                float interpolation_factor;
                
                interpolation_factor = 1.0f - ((float)distance_hex / (float)r);

                if(interpolation_factor < 0.0f) {
                    interpolation_factor = 0.0f; 
                }
                
                float val = v * interpolation_factor;
                if(v > 0){
                    map[i][j].cost += (int)floor(val);
                    if(map[i][j].cost >= 100) map[i][j].cost = 100;
                }else{
                    map[i][j].cost += (int)floor(val);
                    if(map[i][j].cost <0){
                        map[i][j].cost=0;
                    }
                    
                }
            }
        }
    }


    //print map DEBUG:
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            printf("%d ", map[i][j].cost); //debug
        }
        printf("\n"); //debug
    }
    printf("OK\n");
}


int dist_hex(int xa, int ya, int xb, int yb){
    int dist=0;

    if(xa == xb && ya == yb){
        return 0;
    }
 
    int dist_x = xb - xa;
    int dist_y = yb - ya;

    //offsets:
    if (((xa&1) != (ya&1))){ // righe con parità diversa
        if((xa&1) != 0) { // xa è dispari
          dist_y += dist_x / 2;
        } else { // xa è pari
            dist_y += (dist_x + 1) / 2;
        }
    }else{ // righe con stessa parità
    dist_y += dist_x / 2;
    }

    // Formula della distanza esagonale
    if ((dist_x >= 0 && dist_y >= 0) || (dist_x <= 0 && dist_y <= 0)) {
        return (abs(dist_x) > abs(dist_y)) ? abs(dist_x) : abs(dist_y);
    } else {
        return abs(dist_x) + abs(dist_y);
    }


}

void toggle_air_route(int x1, int y1, int x2, int y2){}

int travel_cost(int xp, int yp, int xd, int yd){}

int main(){
    char comando[MAX_CHAR];

    //condizione di fine ricez comandi.....
    while(1){
    scanf("%s", comando);
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
            travel_cost(xp, yp, xd, yd);
        }
    }else if(strcmp(comando, "toggle_air_route")==0){
        int x1, y1, x2, y2;
        if(scanf("%d %d %d %d", &x1, &y1, &x2, &y2)==4){
            change_cost(x1, y1, x2, y2);
        }
    }
    }
    return 0;
}

