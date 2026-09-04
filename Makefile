CC = gcc

CFLAGS = -O2 -Wall -Wextra
X11_LIBS = -lX11
GTK_LIBS = $$(pkg-config --cflags --libs gtk+-3.0)

all:
	$(CC) $(CFLAGS) -o duckywm src/main.c $(X11_LIBS)

clean:
	rm -f duckywm ducky-files ducky-settings
