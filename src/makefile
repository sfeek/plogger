plogger : plogger.o 
	gcc -o plogger plogger.o 
     
plogger.o : plogger.c
	gcc -c plogger.c
             
clean :
	@find ../ -name "*.o" -exec rm -rf {} \;
	@rm plogger


