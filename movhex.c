#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MAX_CHAR 16
#define MAX(a,b) ((a) > (b) ? (a) : (b)) //macro for max
#define NUM_AIR_ROUTES 5

/*---------------------------------------------------------FUNCTIONS DECLARATION----------------------------------------------------*/

// main functions:

void init(int, int);

void change_cost(int, int, int, int);

void toggle_air_route(int, int, int, int);

int travel_cost(int, int, int, int);


//utils funcionts:

int calculate_air_route_cost(int, int);

void remove_air_route(int, int, int, int, int);

int dist_hex(int, int, int, int);


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


/*-------------------------------------------------------- GLOBAL VARIABLES ----------------------------------------------------*/
Hexagon_t **map = NULL;  // matrice dinamica
int cols = 0, rows = 0; //num air routes fot that hex;
int k=1;

/*--------------------------------------------------------------INIT------------------------------------------------------------*/
void init(int M, int N) {
    if (map != NULL) {
        for(int i=0; i<rows; i++) {
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

    for (int i=0; i<rows; i++) {
        for (int j=0; j<cols; j++) {
            map[i][j].cost = 1;

            printf("%d ", map[i][j].cost); //debug
        }
        printf("\n"); //debug
    }

    printf("OK\n");
}

/*--------------------------------------------------------------CHANGE COST------------------------------------------------------------*/

void change_cost(int x, int y, int v, int r){
    if(v <-10 || v >10 || r <= 0 || map==NULL || x>=cols || y>=rows || x<0 || y<0){
        printf("KO\n");
        return;
    }


    for (int i=0; i <rows; i++) {
        for (int j=0; j <cols; j++) {
            int distance_hex = dist_hex(i, j, x, y);
            if (distance_hex < r) {
                float interpolation_factor;
                
                interpolation_factor = 1.0f - ((float)distance_hex / (float)r);

                if(interpolation_factor < 0.0f){
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

/*--------------------------------------------------------------TOGGLE AIR ROUTE------------------------------------------------------------*/

void toggle_air_route(int xp, int yp, int xd, int yd){
    if(map==NULL || xp>=cols || yp>=rows || xd>=cols || yd>=rows || xp<0 || yp<0 || xd<0 || yd<0){
        printf("KO\n");
        return;
    }

    if(map[xp][yp].num_air_routes >= 5){
        printf("KO\n");
        return;
    }

    //inserisci nuova route (la prima)
    if(map[xp][yp].air_routes == NULL){

        map[xp][yp].num_air_routes=0;

        map[xp][yp].air_routes = malloc(sizeof(Air_Route_t));
        if(!map[xp][yp].air_routes){
            return;
        }

        map[xp][yp].air_routes[map[xp][yp].num_air_routes].part_x=xp;
        map[xp][yp].air_routes[map[xp][yp].num_air_routes].part_y=yp;
        map[xp][yp].air_routes[map[xp][yp].num_air_routes].dest_x=xd;
        map[xp][yp].air_routes[map[xp][yp].num_air_routes].dest_y=yd;
        map[xp][yp].air_routes[map[xp][yp].num_air_routes].cost = calculate_air_route_cost(xp, yp);

        map[xp][yp].num_air_routes++;

        
        printf("OK: aggiunta la prima rotta in questo hex\n");
        printf("costo: %d", map[xp][yp].air_routes[0].cost);
        printf("\n");
        printf("%d %d", map[xp][yp].air_routes[0].dest_x, map[xp][yp].air_routes[0].dest_y);
        printf("\n");
        

    }else if(map[xp][yp].air_routes != NULL && map[xp][yp].num_air_routes != 0 && map[xp][yp].num_air_routes < 5){
        // inserisci una nuova rotta
            map[xp][yp].air_routes = realloc(map[xp][yp].air_routes,(map[xp][yp].num_air_routes + 1) * sizeof(Air_Route_t));
            if(!map[xp][yp].air_routes){
                return;
            }

            map[xp][yp].air_routes[map[xp][yp].num_air_routes].part_x = xp;
            map[xp][yp].air_routes[map[xp][yp].num_air_routes].part_y = yp;
            map[xp][yp].air_routes[map[xp][yp].num_air_routes].dest_x = xd;
            map[xp][yp].air_routes[map[xp][yp].num_air_routes].dest_y = yd;
            map[xp][yp].air_routes[map[xp][yp].num_air_routes].cost = calculate_air_route_cost(xp, yp);

            map[xp][yp].num_air_routes++;

            //DEBUG
            printf("OK: aggiunta una nuova rotta in questo hex\n");
            printf("costo: %d", map[xp][yp].air_routes[k].cost);
            printf("\n");
            printf("%d %d", map[xp][yp].air_routes[k].dest_x, map[xp][yp].air_routes[k].dest_y);
            printf("\n");
            k++;
    }else{
        //checking for existing air route and deleting it
        int flag=0;
        for(int i=0; i<map[xp][yp].num_air_routes; i++){
            if(map[xp][yp].air_routes[i].dest_x ==xd && map[xp][yp].air_routes[i].dest_y == yd){
                remove_air_route(xp, yp, xd, yd, i);

                /*
                //DEBUG
                printf("OK: tolta una rotta in questo hex\n");
                */
            }

        }
    }

    printf("OK\n");
    
}

int calculate_air_route_cost(int xp, int yp){
    int sum_connection_costs = 0;
    
    for(int i = 0; i < map[xp][yp].num_air_routes; i++){
        sum_connection_costs += map[xp][yp].air_routes[i].cost;
    }
    
    int avg = (sum_connection_costs + map[xp][yp].cost) / (map[xp][yp].num_air_routes + 1);
    
    if(avg > 100){
        avg = 100;
    } else if(avg < 0){
        avg = 0;
    }

    /*
    printf("DEBUG: num_air_routes = %d\n", map[xp][yp].num_air_routes);
    printf("DEBUG: sum_connection_costs = %d\n", sum_connection_costs);
    printf("DEBUG: map[xp][yp].cost = %d\n", map[xp][yp].cost);
    printf("DEBUG: denominatore = %d\n", map[xp][yp].num_air_routes + 1);
    printf("DEBUG: avg calcolato = %d\n", avg);
    */
    
    
    return avg;
}

void remove_air_route(int xp, int yp, int xd, int yd, int i){
    if(i<0 || i>=map[xp][yp].num_air_routes) {
        return;
    }

    //shift to sx
    for (int j=i; j<map[xp][yp].num_air_routes-1; j++) {
        map[xp][yp].air_routes[j] = map[xp][yp].air_routes[j + 1];
    }

    map[xp][yp].num_air_routes--;

    //Memory re-sizing
    if(map[xp][yp].num_air_routes == 0){
        free(map[xp][yp].air_routes);
        map[xp][yp].air_routes = NULL;
    }else{
        Air_Route_t *tmp = realloc(
            map[xp][yp].air_routes,
            map[xp][yp].num_air_routes * sizeof(Air_Route_t));
        if(tmp != NULL){
            map[xp][yp].air_routes = tmp;
        }
    }
}

/*--------------------------------------------------------------TRAVEL COST------------------------------------------------------------*/


//calculate terrestrial costs (TODO: aggiungiere anche rotte aeree)
int travel_cost(int xp, int yp, int xd, int yd){

    if(xp==xd && yp==yd){
        return 0;
    }
    if(map[xd][yd].cost == 0){
        return -1;
    }


}


//------------------------------------------------------------MAIN------------------------------------------------------------


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
                toggle_air_route(x1, y1, x2, y2);
            }
        }
    }


    return 0;
}

