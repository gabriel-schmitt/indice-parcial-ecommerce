#include <stdio.h>

#define ARQ_CSV "dados/2019-Oct-1M.csv"
#define ARQ_PRODUTOS "produtos.bin"
#define IDX_PRODUTOS "produtos.idx"
#define OVF_PRODUTOS "produtos.ovf"
#define ARQ_ACESSOS "acessos.bin"
#define IDX_ACESSOS "acessos.idx"

/* os tamanhos fecham cada struct num multiplo de 4: senao o gcc poe
   padding depois do fim e o registro deixa de terminar em '\n' */
#define TAM_MARCA 20
#define TAM_CATEGORIA 42
#define TAM_TIPO 11
#define TAM_HORA 19

typedef struct {
  int id;
  int preco;
  char ativo;
  char marca[TAM_MARCA];
  char categoria[TAM_CATEGORIA];
  char fim;
} Produto;

typedef struct {
  int id;
  int produto;
  int usuario;
  int preco;
  char ativo;
  char tipo[TAM_TIPO];
  char hora[TAM_HORA];
  char fim;
} Acesso;

int main(void) {
  printf("Produto %d bytes, Acesso %d bytes\n", (int)sizeof(Produto), (int)sizeof(Acesso));
  return 0;
}
