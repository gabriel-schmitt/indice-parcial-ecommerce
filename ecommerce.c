#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ARQ_CSV "dados/2019-Oct-200k.csv"
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

#define TAM_LINHA 512
#define N_CAMPOS_CSV 9

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

typedef struct {
  int produto;
  int usuario;
  int preco;
  char tipo[TAM_TIPO];
  char hora[TAM_HORA];
  char marca[TAM_MARCA];
  char categoria[TAM_CATEGORIA];
} Evento;

void preencher_texto(char *destino, int tam, const char *origem) {
  int len = (int)strlen(origem);

  if (len > tam) len = tam;
  memcpy(destino, origem, (size_t)len);
  memset(destino + len, ' ', (size_t)(tam - len));
}

/* strtok pularia os campos vazios (",,") e desalinharia as colunas */
int separar_campos(char *linha, char *campos[], int max) {
  int n = 1;

  campos[0] = linha;
  for (char *p = linha; *p != '\0' && n < max; p++) {
    if (*p == ',') {
      *p = '\0';
      campos[n++] = p + 1;
    }
  }
  return n;
}

FILE *abrir_csv(void) {
  char cabecalho[TAM_LINHA];
  FILE *f = fopen(ARQ_CSV, "r");

  if (f == NULL) {
    perror(ARQ_CSV);
    return NULL;
  }
  if (fgets(cabecalho, sizeof(cabecalho), f) == NULL) {
    fclose(f);
    return NULL;
  }
  return f;
}

int ler_evento(FILE *f, Evento *e) {
  char buffer[TAM_LINHA];
  char *campos[N_CAMPOS_CSV];

  while (fgets(buffer, sizeof(buffer), f) != NULL) {
    buffer[strcspn(buffer, "\r\n")] = '\0';
    if (separar_campos(buffer, campos, N_CAMPOS_CSV) < N_CAMPOS_CSV) continue;

    memset(e, 0, sizeof(*e));
    preencher_texto(e->hora, TAM_HORA, campos[0]);
    preencher_texto(e->tipo, TAM_TIPO, campos[1]);
    e->produto = atoi(campos[2]);
    preencher_texto(e->categoria, TAM_CATEGORIA, campos[4]);
    preencher_texto(e->marca, TAM_MARCA, campos[5]);
    e->preco = (int)(strtod(campos[6], NULL) * 100.0 + 0.5);
    e->usuario = atoi(campos[7]);
    return 1;
  }
  return 0;
}

int main(void) {
  FILE *csv;
  Evento e;
  int total = 0;

  printf("Produto %d bytes, Acesso %d bytes\n", (int)sizeof(Produto), (int)sizeof(Acesso));

  csv = abrir_csv();
  if (csv == NULL) return 1;
  while (ler_evento(csv, &e)) {
    if (total < 2 || (total == 999999))
      printf("  [%.*s] %.*s produto %d usuario %d preco %d marca [%.*s] categoria [%.*s]\n",
             TAM_HORA, e.hora, TAM_TIPO, e.tipo, e.produto, e.usuario, e.preco,
             TAM_MARCA, e.marca, TAM_CATEGORIA, e.categoria);
    total++;
  }
  fclose(csv);
  printf("%d eventos lidos de %s\n", total, ARQ_CSV);
  return 0;
}
