# MovHex

Progetto del corso di Algoritmi e Strutture Dati, anno accademico 2024/2025.
Implementazione in C del calcolo del costo minimo di viaggio su una mappa
esagonale con costi di attraversamento e rotte aeree direzionali.

Il repository raccoglie il progetto universitario e i relativi test di
regressione. Il programma legge comandi da standard input e scrive le risposte
su standard output.

## Compilazione ed esecuzione

Servono un compilatore C (GCC o Clang) e Make. Python 3 serve solo per i test.
Su Linux e macOS:

```sh
make
./movhex < tests/example.txt
```

Compilazione diretta:

```sh
cc -O2 -std=gnu11 -Wall -Wextra movhex.c -lm -o movhex
```

## Comandi

Le coordinate sono a base zero; `x` indica la colonna e `y` la riga.

| Comando | Descrizione |
| --- | --- |
| `init colonne righe` | Crea o reinizializza la mappa con costo iniziale 1. |
| `change_cost x y variazione raggio` | Modifica i costi delle celle e delle rotte uscenti entro il raggio. La variazione è compresa fra −10 e 10; il raggio deve essere positivo. |
| `toggle_air_route x1 y1 x2 y2` | Aggiunge o rimuove una rotta direzionale; ogni cella può avere fino a cinque rotte uscenti. |
| `travel_cost x1 y1 x2 y2` | Restituisce il costo minimo, oppure `-1` se le coordinate sono invalide o la destinazione è irraggiungibile. |

I comandi che modificano la mappa rispondono `OK` o `KO`. I costi sono limitati
all'intervallo 0–100. Una cella di costo zero non consente di ripartire;
il costo della cella di destinazione non viene addebitato all'arrivo.

```text
init 3 3
travel_cost 0 0 2 0
toggle_air_route 0 0 2 0
travel_cost 0 0 2 0
```

Output:

```text
OK
2
OK
1
```

## Algoritmo

La griglia usa coordinate offset con righe dispari traslate. Le celle sono
vertici di un grafo con fino a sei vicini terrestri e cinque rotte aeree uscenti.
Il costo minimo viene calcolato con Dijkstra e una coda di priorità basata su
heap binario. Una tabella hash conserva i risultati delle interrogazioni;
le modifiche alla mappa invalidano la cache.

## Test

```sh
make test       # confronta gli output con i risultati attesi
make sanitize   # esegue i test con AddressSanitizer e UndefinedBehaviorSanitizer
make clean      # elimina gli eseguibili generati
```

`tests/` contiene sette casi del progetto originale e un test aggiuntivo per
la rimozione e il reinserimento delle rotte e la reinizializzazione della mappa.
I file `.txt.result` sono gli output attesi. La CI esegue i test su Linux e macOS.

Durante la pulizia sono stati corretti l'invalidazione della cache alla
rimozione di una rotta, la capacità del vettore delle rotte dopo una rimozione
e il rilascio della cache alla reinizializzazione. Il progetto conserva
l'impostazione originale e presuppone input nel formato previsto dall'esercizio;
non è un parser pensato per input arbitrario.
