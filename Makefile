CC = gcc
CFLAGS = -Wall -Wextra -O2 -g

SRCS = src/produtos.c src/main.c
OBJS = $(SRCS:.c=.o)

ecommerce: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

src/%.o: src/%.c src/ecommerce.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f ecommerce $(OBJS)
