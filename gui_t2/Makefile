GNOME_INCLUDES= `pkg-config --cflags --libs gtk+-3.0`
CFLAGS= -Wall -g -DDEBUG
# CFLAGS= -Wall -g
# CFLAGS= -O3

APP_NAME= gui_t2
APP_MODULES= sock.o gui_g3.o callbacks.o file.o thread.o

all: $(APP_NAME)
	
clean: 
	rm -f $(APP_NAME) *.o


$(APP_NAME): main.c $(APP_MODULES) gui.h sock.h callbacks.h 
	gcc $(CFLAGS) -o $(APP_NAME) main.c $(APP_MODULES) $(GNOME_INCLUDES) -lm -export-dynamic

sock.o: sock.c sock.h gui.h
	gcc $(CFLAGS) -c $(GNOME_INCLUDES) sock.c -export-dynamic

gui_g3.o: gui_g3.c gui.h
	gcc $(CFLAGS) -c $(GNOME_INCLUDES) gui_g3.c -export-dynamic
	
callbacks.o: callbacks.c callbacks.h sock.h
	gcc $(CFLAGS) -c $(GNOME_INCLUDES) callbacks.c -export-dynamic

file.o: file.c file.h
	gcc $(CFLAGS) -c $(GNOME_INCLUDES) file.c -export-dynamic
		
thread.o: thread.c thread.h sock.h
	gcc $(CFLAGS) -c $(GNOME_INCLUDES) thread.c -export-dynamic

