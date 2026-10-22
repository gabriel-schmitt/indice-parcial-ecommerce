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
#define MAX_EVENTOS 200000

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

int ler_texto(const char *rotulo, char *destino, int tam) {
  char buffer[256];

  printf("%s", rotulo);
  fflush(stdout);
  if (fgets(buffer, sizeof(buffer), stdin) == NULL) return 0;

  buffer[strcspn(buffer, "\r\n")] = '\0';
  if (buffer[0] == '\0') return 0;

  strncpy(destino, buffer, tam - 1);
  destino[tam - 1] = '\0';
  return 1;
}

typedef struct {
  Produto p;
  int ordem;
} Ocorrencia;

/* desempata pela ordem no csv porque o qsort nao e estavel: sem isso o
   "ultimo visto" de um produto sairia de um evento qualquer do grupo */
int comparar_ocorrencias(const void *a, const void *b) {
  const Ocorrencia *x = a, *y = b;

  if (x->p.id != y->p.id) return x->p.id < y->p.id ? -1 : 1;
  return x->ordem - y->ordem;
}

void montar_produto(Produto *p, const Evento *e) {
  memset(p, 0, sizeof(*p));
  p->id = e->produto;
  p->preco = e->preco;
  p->ativo = '1';
  memcpy(p->marca, e->marca, TAM_MARCA);
  memcpy(p->categoria, e->categoria, TAM_CATEGORIA);
  p->fim = '\n';
}

int gerar_produtos(void) {
  Ocorrencia *v;
  Evento e;
  Produto atual;
  FILE *f;
  int n = 0, total = 0, i = 0;

  v = malloc((size_t)MAX_EVENTOS * sizeof(Ocorrencia));
  if (v == NULL) return -1;

  f = abrir_csv();
  if (f == NULL) {
    free(v);
    return -1;
  }
  while (n < MAX_EVENTOS && ler_evento(f, &e)) {
    montar_produto(&v[n].p, &e);
    v[n].ordem = n;
    n++;
  }
  fclose(f);

  qsort(v, (size_t)n, sizeof(Ocorrencia), comparar_ocorrencias);

  /* "wb" e nao "w": em modo texto o Windows converte 0x0A em 0x0D 0x0A e
     desalinha todos os registros seguintes */
  f = fopen(ARQ_PRODUTOS, "wb");
  if (f == NULL) {
    perror(ARQ_PRODUTOS);
    free(v);
    return -1;
  }

  while (i < n) {
    atual = v[i].p;
    for (i++; i < n && v[i].p.id == atual.id; i++) {
      atual.preco = v[i].p.preco;
      if (v[i].p.marca[0] != ' ') memcpy(atual.marca, v[i].p.marca, TAM_MARCA);
    }
    if (fwrite(&atual, sizeof(Produto), 1, f) != 1) {
      perror(ARQ_PRODUTOS);
      fclose(f);
      free(v);
      return -1;
    }
    total++;
  }
  fclose(f);
  free(v);

  return total;
}

int total_produtos(FILE *f) {
  long bytes;

  if (fseek(f, 0, SEEK_END) != 0) return -1;
  bytes = ftell(f);
  if (bytes < 0) return -1;
  return (int)(bytes / (long)sizeof(Produto));
}

int ler_produto(FILE *f, int i, Produto *destino) {
  if (fseek(f, (long)i * (long)sizeof(Produto), SEEK_SET) != 0) return 0;
  return fread(destino, sizeof(Produto), 1, f) == 1;
}

void imprimir_produto(int i, const Produto *p) {
  printf("  %6d  %8d  %c  %6d.%02d  %.*s  %.*s\n", i, p->id, p->ativo,
         p->preco / 100, p->preco % 100, TAM_MARCA, p->marca, TAM_CATEGORIA, p->categoria);
}

void listar_produtos(FILE *f, int total) {
  Produto p;
  char entrada[32];
  int inicio, quantos, i;

  if (!ler_texto("registro inicial: ", entrada, sizeof(entrada))) return;
  inicio = atoi(entrada);
  if (!ler_texto("quantos: ", entrada, sizeof(entrada))) return;
  quantos = atoi(entrada);

  if (inicio < 0) inicio = 0;
  if (inicio + quantos > total) quantos = total - inicio;

  for (i = inicio; i < inicio + quantos; i++) {
    if (!ler_produto(f, i, &p)) return;
    imprimir_produto(i, &p);
  }
}

FILE *abrir_produtos(void) {
  FILE *f = fopen(ARQ_PRODUTOS, "rb");

  if (f == NULL) {
    printf("%s nao existe, gerando a partir de %s\n", ARQ_PRODUTOS, ARQ_CSV);
    if (gerar_produtos() < 0) return NULL;
    f = fopen(ARQ_PRODUTOS, "rb");
    if (f == NULL) perror(ARQ_PRODUTOS);
  }
  return f;
}

int main(void) {
  FILE *produtos;
  int total;

  produtos = abrir_produtos();
  if (produtos == NULL) return 1;
  total = total_produtos(produtos);
  printf("%s: %d produtos de %d bytes\n", ARQ_PRODUTOS, total, (int)sizeof(Produto));

  listar_produtos(produtos, total);
  fclose(produtos);
  return 0;
}
