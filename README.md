# SO_SENSORES


gcc -O2 -Wall -I . -c csapp.c
gcc -O2 -Wall -I . -o client client.c csapp.o -lpthread
gcc -O2 -Wall -I . -o server server.c csapp.o -lpthread
