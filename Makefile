CC = gcc
CFLAGS = -O2 -Wall -Wextra
X11_LIBS = -lX11

all:
	$(CC) $(CFLAGS) -o duckywm src/main.c $(X11_LIBS)

run: all
	@echo "Starting Xephyr on display :1..."
	Xephyr :1 -screen 1280x720 &
	sleep 1
	@echo "Starting DuckyWM..."
	DISPLAY=:1 ./duckywm

test: run

clean:
	rm -f duckywm
