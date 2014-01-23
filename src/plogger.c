#include <termios.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/signal.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define FALSE 0
#define TRUE 1

volatile int STOP=FALSE;
volatile int TIMEOUT=FALSE;

void signal_handler_IO (int status);    // Definition of signal handler
int wait_flag=TRUE;                     // TRUE while no signal received
int status;


int getkey() // Read keys from keyboard non-blocking
{
	int character;
	struct termios orig_term_attr;
	struct termios new_term_attr;

	// Set the terminal to raw mode
	tcgetattr(fileno(stdin), &orig_term_attr);
	memcpy(&new_term_attr, &orig_term_attr, sizeof(struct termios));
	new_term_attr.c_lflag &= ~(ECHO|ICANON);
	new_term_attr.c_cc[VTIME] = 0;
	new_term_attr.c_cc[VMIN] = 0;
	tcsetattr(fileno(stdin), TCSANOW, &new_term_attr);

	// Read a character from the stdin stream without blocking
	// Returns EOF (-1) if no character is available
	character = fgetc(stdin);

	// Restore the original terminal attributes
	tcsetattr(fileno(stdin), TCSANOW, &orig_term_attr);

	return character;
}

char *replace(char *s,char old, char new) // Replace characters in a string
{
	char *p=s;

	while(*p)
	{
		if (*p==old) *p=new;
		++p;
	}
	return s;
}

void signal_handler_IO (int status) // Signal handler for serial interface
{
	wait_flag = FALSE;
}

main(int Parm_Count, char *Parms[]) // Main
{
	char Param_strings[4][80];
  
	int fd, tty, c, error, nbytes, i;
	char Key;
	struct termios oldtio, newtio;       
	struct sigaction saio;              
	char buffer[255];                    
	char *bufptr;
	time_t t1,rawtime;
	FILE *out=NULL;
	char line[80];
	char devicename[80];
	char FileName[256];
	long Interval;
	long sample_count=0;
	char tbuffer[80];
   
	error=0;
   
	// Read the parameters from the command line
	if (Parm_Count==4)  // If there are the right number of parameters on the command line
	{
		for (i=1; i<Parm_Count; i++) strcpy(Param_strings[i-1],Parms[i]);
		i=sscanf(Param_strings[0],"%s",devicename);
		if (i != 1) error=1;
		i=sscanf(Param_strings[1],"%li",&Interval);
		if (i != 1) error=1;
		i=sscanf(Param_strings[2],"%s",FileName);
		if (i != 1) error=1;
	} 

	if (Interval < 1) // Make sure we have at least one second intervals
	{	
		printf("\nMust be at least one second interval!\n");
		exit(0);
	}

	if ((Parm_Count==4) && (error==0))  
	{                                    
		// Open the device(com port) to be non-blocking (read will return immediately)
		fd = open(devicename, O_RDWR | O_NOCTTY | O_NONBLOCK);
		if (fd < 0)
		{
			perror(devicename);
			exit(-1);
		}

		// Install the serial handler before making the device asynchronous
		saio.sa_handler = signal_handler_IO;
		sigemptyset(&saio.sa_mask);   
		saio.sa_flags = 0;
		saio.sa_restorer = NULL;
		sigaction(SIGIO,&saio,NULL);

		
		fcntl(fd, F_SETOWN, getpid()); // Allow the process to receive SIGIO
		
		fcntl(fd, F_SETFL, FASYNC); // Make the file descriptor asynchronous 
	 
		tcgetattr(fd,&oldtio); // Save current port settings 
      
		// Set new port settings for canonical input processing 
		newtio.c_cflag = B1200 | CS7 | CRTSCTS | CSTOPB | CREAD | CLOCAL;
		newtio.c_iflag = IGNPAR;
		newtio.c_oflag = 0;
		newtio.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
		newtio.c_cc[VMIN]=1;
		newtio.c_cc[VTIME]=0;
		tcflush(fd, TCIFLUSH);
		tcsetattr(fd,TCSANOW,&newtio);
		
		// Open our file for writing and create if does not exist
		out = fopen(FileName,"w+");
		if(NULL == out)
		{
			printf("\n Cannot Create New Log File!!!\n");
			exit(0);
		}

		printf("\nPress ESC to exit!\n\n");

		// Start taking readings
		while (TRUE)
		{
			t1 = time(NULL); // Get first time

			while(time(NULL)-t1 < Interval) // Check for ESC key to exit program while waiting for next sample
			{
				if (getkey()==0x1b) {STOP=TRUE;break;}
			}
	
			if (STOP==TRUE) break; // Exit program if ESC was pressed

			write(fd," ",1); // Tell the meter to send a reading

			while (wait_flag==TRUE) //Wait for reading
			{
				if (getkey()==0x1b) {STOP=TRUE;break;}
			} 
			if (STOP==TRUE) break; // Exit program if ESC was pressed

			bufptr = buffer;
			while ((nbytes = read(fd, bufptr, buffer + sizeof(buffer) - bufptr - 1)) > 0) //Read from meter
			{
				bufptr += nbytes;
				if (bufptr[-1] == '\r') break; // End when we have the last character
			}
			*(bufptr-1) = '\0'; // Terminate our string

	
			sample_count++; // Increment sample counter
			
			rawtime=time(NULL); // Get the time
			strftime(tbuffer,sizeof(tbuffer),"%F %T",localtime(&rawtime)); // Format the time
			
			
			sprintf(line,"%ld %s",sample_count,buffer); // Create our line
			strcpy(line,replace(line,' ',',')); // Replace spaces with commas
			sprintf(line,"%s,%s\n",line,tbuffer); // Add the timestamp
			printf("%s",line); // Send the line to the screen
			fprintf(out,"%s",line); // Send the line to a file
			wait_flag = TRUE; // Reset the signal handler flag
			fflush(out); // Make sure that if something goes wrong our data file is safe
		}  
		
		tcsetattr(fd,TCSANOW,&oldtio); // Restore old COM port settings
		close(fd); // Close the com port
		fclose(out); // Close the file
		exit(0);
	} 
	else  //Give instructions on how to use the command line
	{
		printf("\nplogger <device_name> <interval> <filename>\n\n");
	}
} 



