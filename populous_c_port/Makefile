CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2 `sdl2-config --cflags`
LDFLAGS = `sdl2-config --libs`

TARGET = populous_init_port
SRCS = populous_init.c populous_prg.c sdl_lowlevel.c
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

run: $(TARGET)
	./$(TARGET)