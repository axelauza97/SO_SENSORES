# SO_SENSORES


gcc -O2 -Wall -I . -c csapp.c
gcc -O2 -Wall -I . -o consola consola.c csapp.o -lpthread
gcc -O2 -Wall -I . -o mysat mysat.c csapp.o -lpthread
