CC = gcc
CFLAGS = -Wall -Wextra -O2 -g

ecommerce: ecommerce.c
	$(CC) $(CFLAGS) -o ecommerce ecommerce.c

clean:
	rm -f ecommerce produtos.bin produtos.idx produtos.ovf acessos.bin acessos.idx
