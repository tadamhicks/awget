ANGET=gcc -o awget awget.c
STEP=gcc -o ss ss.c -lpthread

all:
	$(ANGET)
	$(STEP)
clean: 
	rm -f awget ss *.html *.pdf

awget:
	$(ANGET)
ss:
	$(STEP)
