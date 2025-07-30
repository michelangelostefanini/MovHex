#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MAX_CHAR 16

/*---------------------------------------------------------FUNCTIONS DECLARATION----------------------------------------------------*/

// main functions:

void init(int, int);

void change_cost(int, int, int, int);

void toggle_air_route(int, int, int, int);

int travel_cost(int, int, int, int);


//utils funcionts:

void calculate_air_route_cost();


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
            map[j][i].cost = 1;
            printf("%d ", map[i][j].cost); //debug
        }
        printf("\n"); //debug
    }

    printf("OK\n");
}


void change_cost(int x, int y, int v, int r){}

void toggle_air_route(int x1, int y1, int x2, int y2){}

int travel_cost(int xp, int yp, int xd, int yd){}

int main(){
    char comando[MAX_CHAR];

    //condizione di fine ricez comandi.....
    scanf("%s", comando);
    if(strcmp(comando, "init")==0){
        int x, y;
        if(scanf("%d %d", &x, &y)==2){
            init(x, y);
        }
    }else if(strcmp(comando, "change_cost)")==0){
        int x, y, v, r;
        if(scanf("%d %d %d %d", &x, &y, &v, &r)==4){
            change_cost(x, y, v, r);
        }
    }else if(strcmp(comando, "travel_cost")==0){
        int xp, yp, xd, yd;
        if(scanf("%d %d %d %d", &xp, &yp, &xd, &yd)==4){
            travel_cost(xp, yp, xd, yd);
        }
    }else if(strcmp(comando, "toggle_air_route)")==0){
        int x1, y1, x2, y2;
        if(scanf("%d %d %d %d", &x1, &y1, &x2, &y2)==4){
            change_cost(x1, y1, x2, y2);
        }
    }

    return 0;
}

