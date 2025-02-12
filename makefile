#
#	Makefile for EDIF parser.
#

# CFLAGS = -DDEBUG
# CFLAGS = -O
CC	   = gcc
CFLAGS = -g -static

SOURCES = edif.y

all  :	e2net ppedif e2sch edif2gschem

ppedif : ppedif.o
	gcc $(CFLAGS) ppedif.c -o ppedif

e2net :	ed.h eelibsl.h e2net.o edif.o savelib.o
	gcc $(CFLAGS) e2net.o edif.o savelib.o -o e2net -lm

e2sch :	ed.h eelibsl.h e2sch.o edif.o savelib.o
	gcc $(CFLAGS) e2sch.o edif.o savelib.o -o e2sch -lm

edif2gschem :	ed.h eelibsl.h edif2gschem.o edif.o
	gcc $(CFLAGS) edif2gschem.o edif.o -o $@ -lm

savelib : fctsys.h eelibsl.h savelib.o 
	gcc $(CFLAGS) -c savelib.c

edif :	ed.h eelibsl.h edif.o 
	gcc $(CFLAGS) -c edif.c

// main.o : main.c
edif.o : edif.c

edif.c : edif.y
	bison -y -g -t -v -d edif.y 
	cp y.tab.c edif.c
#	cp edif.tab.c edif.c

#	mv y.tab.c edif.c

# edif.y : edif.y.1 edif.y.2
# 	cat edif.y.1 edif.y.2 > edif.y

clean :
	$(RM) *.o y.* edif.c edif.output edif.tab.c edif.tab.h
	$(RM) e2sch e2net ppedif
	$(RM) e2sch.exe e2net.exe ppedif.exe
