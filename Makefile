# Damián Krajňák - xkrajn03
# Project #2 for IOS

CFLAGS=-std=gnu99 -pthread -Wall -Wextra -Werror -pedantic

main: main.c
	gcc $(CFLAGS) main.c -o main

zip: *.c Makefile
	zip main.zip *.c Makefile

clean:
	rm -rf main.zip tail