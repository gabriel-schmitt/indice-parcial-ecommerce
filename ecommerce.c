#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ARQ_CSV "dados/2019-Oct-200k.csv"
#define ARQ_PRODUTOS "produtos.bin"
#define IDX_PRODUTOS "produtos.idx"
#define OVF_PRODUTOS "produtos.ovf"
#define TMP_PRODUTOS "produtos.tmp"
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

/* 56 * 72 = 4032 bytes: o intervalo inteiro vem num fread de uma pagina de 4 KB */
#define K_PRODUTOS 56
#define MAX_OVERFLOW 64
#define MAX_GRUPOS 2000
#define TOP 10

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

typedef struct {
  int id;
  int pos;
} EntradaIndice;

typedef struct {
  int k; // tamanho do intervalo apontado por cada entrada
  int total; // registros no arquivo de dados quando o indice foi criado
  int n;
  EntradaIndice *entradas;
} IndiceParcial;

typedef struct {
  FILE *dados;
  FILE *ovf;
  int total;
  int total_ovf;
  IndiceParcial ind;
} BaseProdutos;

typedef struct {
  char nome[TAM_CATEGORIA];
  int qtd;
  long long centavos;
} Grupo;

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

int gravar_produto(FILE *f, int i, const Produto *p) {
  if (fseek(f, (long)i * (long)sizeof(Produto), SEEK_SET) != 0) return 0;
  if (fwrite(p, sizeof(Produto), 1, f) != 1) return 0;
  return fflush(f) == 0;
}

int ler_inteiro(const char *rotulo, int *v) {
  char entrada[32];
  char *fim;
  long x;

  if (!ler_texto(rotulo, entrada, sizeof(entrada))) return 0;
  x = strtol(entrada, &fim, 10);
  if (fim == entrada || *fim != '\0') return 0;
  *v = (int)x;
  return 1;
}

int texto_igual(const char *campo, int tam, const char *s) {
  int len = (int)strlen(s);

  if (len > tam || memcmp(campo, s, (size_t)len) != 0) return 0;
  return len == tam || campo[len] == ' ';
}

int criar_indice_produtos(FILE *f, int total, IndiceParcial *ind) {
  Produto p;

  ind->k = K_PRODUTOS;
  ind->total = total;
  ind->n = (total + K_PRODUTOS - 1) / K_PRODUTOS;
  ind->entradas = malloc((size_t)(ind->n > 0 ? ind->n : 1) * sizeof(EntradaIndice));
  if (ind->entradas == NULL) return 0;

  for (int i = 0; i < ind->n; i++) {
    if (!ler_produto(f, i * K_PRODUTOS, &p)) {
      free(ind->entradas);
      ind->entradas = NULL;
      return 0;
    }
    ind->entradas[i].id = p.id;
    ind->entradas[i].pos = i * K_PRODUTOS;
  }
  return 1;
}

int gravar_indice(const IndiceParcial *ind, const char *arq) {
  FILE *idx = fopen(arq, "wb");

  if (idx == NULL) {
    perror(arq);
    return 0;
  }
  if (fwrite(&ind->k, sizeof(ind->k), 1, idx) != 1 ||
      fwrite(&ind->total, sizeof(ind->total), 1, idx) != 1 ||
      fwrite(&ind->n, sizeof(ind->n), 1, idx) != 1 ||
      (int)fwrite(ind->entradas, sizeof(EntradaIndice), (size_t)ind->n, idx) != ind->n) {
    perror(arq);
    fclose(idx);
    return 0;
  }
  if (fclose(idx) != 0) {
    perror(arq);
    return 0;
  }
  return 1;
}

int ler_indice(const char *arq, int total, int k, IndiceParcial *ind) {
  FILE *idx = fopen(arq, "rb");
  int ok;

  ind->entradas = NULL;
  if (idx == NULL) return 0;

  ok = fread(&ind->k, sizeof(ind->k), 1, idx) == 1 &&
       fread(&ind->total, sizeof(ind->total), 1, idx) == 1 &&
       fread(&ind->n, sizeof(ind->n), 1, idx) == 1 &&
       ind->k == k && ind->total == total && ind->n == (total + k - 1) / k;
  if (ok) {
    ind->entradas = malloc((size_t)(ind->n > 0 ? ind->n : 1) * sizeof(EntradaIndice));
    ok = ind->entradas != NULL &&
         (int)fread(ind->entradas, sizeof(EntradaIndice), (size_t)ind->n, idx) == ind->n;
  }
  fclose(idx);

  if (ok) return 1;
  free(ind->entradas);
  ind->entradas = NULL;
  return 0;
}

int carregar_indice_produtos(BaseProdutos *b) {
  if (ler_indice(IDX_PRODUTOS, b->total, K_PRODUTOS, &b->ind)) return 1;

  printf("%s ausente ou desatualizado, recriando\n", IDX_PRODUTOS);
  if (!criar_indice_produtos(b->dados, b->total, &b->ind)) return 0;
  gravar_indice(&b->ind, IDX_PRODUTOS);
  return 1;
}

/* devolve a posicao no arquivo de dados, ou -1 */
int pesquisar_indice_produtos(FILE *f, const IndiceParcial *ind, int id, Produto *p, int *comparacoes) {
  Produto bloco[K_PRODUTOS];
  int lo = 0, hi = ind->n - 1, i = -1, inicio, qtd;

  *comparacoes = 0;
  while (lo <= hi) {
    int mid = lo + (hi - lo) / 2;

    (*comparacoes)++;
    if (ind->entradas[mid].id <= id) {
      i = mid;
      lo = mid + 1;
    } else {
      hi = mid - 1;
    }
  }
  if (i < 0) return -1;

  inicio = ind->entradas[i].pos;
  qtd = ind->total - inicio < K_PRODUTOS ? ind->total - inicio : K_PRODUTOS;
  if (fseek(f, (long)inicio * (long)sizeof(Produto), SEEK_SET) != 0) return -1;
  if ((int)fread(bloco, sizeof(Produto), (size_t)qtd, f) != qtd) return -1;

  for (int j = 0; j < qtd; j++) {
    (*comparacoes)++;
    if (bloco[j].id == id) {
      *p = bloco[j];
      return inicio + j;
    }
    if (bloco[j].id > id) break;
  }
  return -1;
}

/* devolve a posicao no arquivo de dados, ou -1 */
int buscar_produto_arquivo(FILE *f, int total, int id, Produto *p, int *comparacoes) {
  int lo = 0, hi = total - 1;

  *comparacoes = 0;
  while (lo <= hi) {
    int mid = lo + (hi - lo) / 2; /* (lo + hi) / 2 estouraria o int */

    if (!ler_produto(f, mid, p)) return -1;
    (*comparacoes)++;
    if (p->id == id) return mid;
    if (p->id < id) lo = mid + 1;
    else hi = mid - 1;
  }
  return -1;
}

/* o overflow nao e ordenado: busca sequencial, limitada a MAX_OVERFLOW registros */
int buscar_overflow(FILE *ovf, int total_ovf, int id, Produto *p) {
  if (fseek(ovf, 0, SEEK_SET) != 0) return -1;
  for (int i = 0; i < total_ovf; i++) {
    if (fread(p, sizeof(Produto), 1, ovf) != 1) return -1;
    if (p->id == id) return i;
  }
  return -1;
}

/* acha o id no arquivo principal ou no overflow, mesmo se removido */
int localizar_produto(BaseProdutos *b, int id, Produto *p, int *no_ovf) {
  int comparacoes, pos;

  *no_ovf = 0;
  pos = pesquisar_indice_produtos(b->dados, &b->ind, id, p, &comparacoes);
  if (pos >= 0) return pos;

  *no_ovf = 1;
  return buscar_overflow(b->ovf, b->total_ovf, id, p);
}

FILE *abrir_ou_criar(const char *arq) {
  FILE *f = fopen(arq, "r+b");

  if (f == NULL) f = fopen(arq, "w+b");
  if (f == NULL) perror(arq);
  return f;
}

int abrir_base_produtos(BaseProdutos *b) {
  b->dados = fopen(ARQ_PRODUTOS, "r+b");
  if (b->dados == NULL) {
    printf("%s nao existe, gerando a partir de %s\n", ARQ_PRODUTOS, ARQ_CSV);
    if (gerar_produtos() < 0) return 0;
    remove(OVF_PRODUTOS);
    remove(IDX_PRODUTOS);
    b->dados = fopen(ARQ_PRODUTOS, "r+b");
    if (b->dados == NULL) {
      perror(ARQ_PRODUTOS);
      return 0;
    }
  }
  b->ovf = abrir_ou_criar(OVF_PRODUTOS);
  if (b->ovf == NULL) {
    fclose(b->dados);
    return 0;
  }
  b->total = total_produtos(b->dados);
  b->total_ovf = total_produtos(b->ovf);
  return carregar_indice_produtos(b);
}

void fechar_base_produtos(BaseProdutos *b) {
  gravar_indice(&b->ind, IDX_PRODUTOS);
  free(b->ind.entradas);
  b->ind.entradas = NULL;
  fclose(b->dados);
  fclose(b->ovf);
}

int comparar_produtos(const void *a, const void *b) {
  const Produto *x = a, *y = b;

  if (x->id != y->id) return x->id < y->id ? -1 : 1;
  return 0;
}

int copiar_se_ativo(FILE *destino, const Produto *p, int *total) {
  if (p->ativo != '1') return 1;
  if (fwrite(p, sizeof(Produto), 1, destino) != 1) return 0;
  (*total)++;
  return 1;
}

/* intercala o principal com o overflow ordenado, descarta os removidos e
   recria o indice; devolve o novo total, ou -1 */
int reorganizar_produtos(BaseProdutos *b) {
  Produto ovf[MAX_OVERFLOW], p;
  FILE *tmp;
  int n = 0, j = 0, total = 0, ok = 1;

  if (fseek(b->ovf, 0, SEEK_SET) != 0) return -1;
  while (n < b->total_ovf && n < MAX_OVERFLOW && fread(&ovf[n], sizeof(Produto), 1, b->ovf) == 1) n++;
  qsort(ovf, (size_t)n, sizeof(Produto), comparar_produtos);

  tmp = fopen(TMP_PRODUTOS, "wb");
  if (tmp == NULL) {
    perror(TMP_PRODUTOS);
    return -1;
  }
  if (fseek(b->dados, 0, SEEK_SET) != 0) ok = 0;
  for (int i = 0; ok && i < b->total; i++) {
    if (fread(&p, sizeof(Produto), 1, b->dados) != 1) ok = 0;
    for (; ok && j < n && ovf[j].id < p.id; j++) ok = copiar_se_ativo(tmp, &ovf[j], &total);
    if (ok) ok = copiar_se_ativo(tmp, &p, &total);
  }
  for (; ok && j < n; j++) ok = copiar_se_ativo(tmp, &ovf[j], &total);

  if (fclose(tmp) != 0) ok = 0;
  if (!ok) {
    perror(TMP_PRODUTOS);
    remove(TMP_PRODUTOS);
    return -1;
  }

  fclose(b->dados);
  fclose(b->ovf);
  free(b->ind.entradas);
  b->ind.entradas = NULL;
  if (rename(TMP_PRODUTOS, ARQ_PRODUTOS) != 0) {
    perror(ARQ_PRODUTOS);
    return -1;
  }
  b->dados = fopen(ARQ_PRODUTOS, "r+b");
  b->ovf = fopen(OVF_PRODUTOS, "w+b");
  if (b->dados == NULL || b->ovf == NULL) {
    perror(b->dados == NULL ? ARQ_PRODUTOS : OVF_PRODUTOS);
    return -1;
  }
  b->total = total;
  b->total_ovf = 0;
  if (!criar_indice_produtos(b->dados, b->total, &b->ind)) return -1;
  gravar_indice(&b->ind, IDX_PRODUTOS);
  return total;
}

/* 1 inserido, 0 se o id ja existe e esta ativo, -1 erro */
int inserir_produto(BaseProdutos *b, const Produto *novo) {
  Produto p;
  int no_ovf, pos;

  pos = localizar_produto(b, novo->id, &p, &no_ovf);
  if (pos >= 0 && p.ativo == '1') return 0;

  /* um removido com o mesmo id ja esta na posicao certa da ordem */
  if (pos >= 0) return gravar_produto(no_ovf ? b->ovf : b->dados, pos, novo) ? 1 : -1;

  if (!gravar_produto(b->ovf, b->total_ovf, novo)) return -1;
  b->total_ovf++;
  if (b->total_ovf >= MAX_OVERFLOW && reorganizar_produtos(b) < 0) return -1;
  return 1;
}

/* 1 removido, 0 se nao existe ou ja estava removido, -1 erro */
int remover_produto(BaseProdutos *b, int id) {
  Produto p;
  int no_ovf, pos;

  pos = localizar_produto(b, id, &p, &no_ovf);
  if (pos < 0 || p.ativo != '1') return 0;

  p.ativo = '0';
  return gravar_produto(no_ovf ? b->ovf : b->dados, pos, &p) ? 1 : -1;
}

void relatar_busca(BaseProdutos *b, int id, int pos, const Produto *p, int comparacoes, int leituras) {
  Produto q;

  if (pos >= 0) {
    printf("  encontrado no registro %d: %d comparacoes, %d leituras no arquivo%s\n", pos,
           comparacoes, leituras, p->ativo == '1' ? "" : " (removido)");
    imprimir_produto(pos, p);
    return;
  }
  printf("  nao esta no arquivo principal: %d comparacoes, %d leituras\n", comparacoes, leituras);
  pos = buscar_overflow(b->ovf, b->total_ovf, id, &q);
  if (pos < 0) {
    printf("  nem na area de overflow (%d registros)\n", b->total_ovf);
    return;
  }
  printf("  encontrado na area de overflow, registro %d%s\n", pos, q.ativo == '1' ? "" : " (removido)");
  imprimir_produto(pos, &q);
}

void buscar_no_arquivo(BaseProdutos *b) {
  Produto p;
  int id, pos, comparacoes;

  if (!ler_inteiro("id do produto: ", &id)) return;
  pos = buscar_produto_arquivo(b->dados, b->total, id, &p, &comparacoes);
  relatar_busca(b, id, pos, &p, comparacoes, comparacoes);
}

void buscar_pelo_indice(BaseProdutos *b) {
  Produto p;
  int id, pos, comparacoes;

  if (!ler_inteiro("id do produto: ", &id)) return;
  pos = pesquisar_indice_produtos(b->dados, &b->ind, id, &p, &comparacoes);
  relatar_busca(b, id, pos, &p, comparacoes, id < b->ind.entradas[0].id ? 0 : 1);
}

void imprimir_indice_produtos(const IndiceParcial *ind) {
  printf("\n  entrada    pos        id\n");
  for (int i = 0; i < ind->n; i++) {
    if (i == 5 && ind->n > 10) {
      printf("      ...\n");
      i = ind->n - 5;
    }
    printf("  %7d %6d  %8d\n", i, ind->entradas[i].pos, ind->entradas[i].id);
  }
  printf("\n  %d entradas de %d bytes para %d registros, uma a cada %d\n", ind->n,
         (int)sizeof(EntradaIndice), ind->total, ind->k);
}

void listar_overflow(BaseProdutos *b) {
  Produto p;

  printf("  %d de %d registros na area de overflow\n", b->total_ovf, MAX_OVERFLOW);
  if (fseek(b->ovf, 0, SEEK_SET) != 0) return;
  for (int i = 0; i < b->total_ovf; i++) {
    if (fread(&p, sizeof(Produto), 1, b->ovf) != 1) return;
    imprimir_produto(i, &p);
  }
}

int ler_produto_novo(Produto *p) {
  char entrada[TAM_CATEGORIA + 1];
  int id;

  if (!ler_inteiro("id do produto: ", &id) || id <= 0) return 0;
  memset(p, 0, sizeof(*p));
  p->id = id;
  if (!ler_texto("preco (ex. 129.90): ", entrada, sizeof(entrada))) return 0;
  p->preco = (int)(strtod(entrada, NULL) * 100.0 + 0.5);
  if (!ler_texto("marca: ", entrada, TAM_MARCA + 1)) entrada[0] = '\0';
  preencher_texto(p->marca, TAM_MARCA, entrada);
  if (!ler_texto("categoria: ", entrada, sizeof(entrada))) entrada[0] = '\0';
  preencher_texto(p->categoria, TAM_CATEGORIA, entrada);
  p->ativo = '1';
  p->fim = '\n';
  return 1;
}

void menu_inserir(BaseProdutos *b) {
  Produto p;
  int antes = b->total_ovf, r;

  if (!ler_produto_novo(&p)) {
    printf("  entrada invalida\n");
    return;
  }
  r = inserir_produto(b, &p);
  if (r < 0) printf("  erro ao inserir\n");
  else if (r == 0) printf("  o produto %d ja existe\n", p.id);
  else if (b->total_ovf > antes) printf("  inserido na area de overflow (%d/%d)\n", b->total_ovf, MAX_OVERFLOW);
  else if (b->total_ovf == 0 && antes > 0) printf("  overflow cheio: arquivo reorganizado com %d produtos\n", b->total);
  else printf("  inserido no lugar de um registro removido com o mesmo id\n");
}

void menu_remover(BaseProdutos *b) {
  int id, r;

  if (!ler_inteiro("id do produto: ", &id)) return;
  r = remover_produto(b, id);
  if (r < 0) printf("  erro ao remover\n");
  else if (r == 0) printf("  o produto %d nao existe ou ja foi removido\n", id);
  else printf("  produto %d marcado como removido\n", id);
}

int somar_grupo(Grupo *g, int *n, const char *nome, int tam, int centavos) {
  char chave[TAM_CATEGORIA];
  int i;

  memset(chave, ' ', sizeof(chave));
  memcpy(chave, nome, (size_t)tam);
  for (i = 0; i < *n && memcmp(g[i].nome, chave, sizeof(chave)) != 0; i++);
  if (i == *n) {
    if (*n == MAX_GRUPOS) return 0;
    memcpy(g[i].nome, chave, sizeof(chave));
    g[i].qtd = 0;
    g[i].centavos = 0;
    (*n)++;
  }
  g[i].qtd++;
  g[i].centavos += centavos;
  return 1;
}

int comparar_qtd(const void *a, const void *b) {
  const Grupo *x = a, *y = b;

  return y->qtd - x->qtd;
}

int comparar_centavos(const void *a, const void *b) {
  const Grupo *x = a, *y = b;

  if (x->centavos != y->centavos) return x->centavos < y->centavos ? 1 : -1;
  return 0;
}

FILE *abrir_acessos(void) {
  FILE *f = fopen(ARQ_ACESSOS, "rb");

  if (f == NULL) perror(ARQ_ACESSOS);
  return f;
}

/* le acessos.bin em sequencia, um registro por vez, e agrupa as compras pela
   marca ou pela categoria do produto; devolve o numero de grupos, ou -1 */
int agrupar_compras(BaseProdutos *b, Grupo *g, int por_marca, int *compras, int *sem_nome) {
  FILE *f = abrir_acessos();
  Acesso a;
  Produto p;
  int n = 0, no_ovf;

  *compras = 0;
  *sem_nome = 0;
  if (f == NULL) return -1;

  while (fread(&a, sizeof(Acesso), 1, f) == 1) {
    if (a.ativo != '1' || !texto_igual(a.tipo, TAM_TIPO, "purchase")) continue;
    if (localizar_produto(b, a.produto, &p, &no_ovf) < 0 || p.ativo != '1') continue;

    (*compras)++;
    if ((por_marca ? p.marca[0] : p.categoria[0]) == ' ') {
      (*sem_nome)++;
      continue;
    }
    if (por_marca) somar_grupo(g, &n, p.marca, TAM_MARCA, a.preco);
    else somar_grupo(g, &n, p.categoria, TAM_CATEGORIA, a.preco);
  }
  fclose(f);
  return n;
}

void marca_mais_vendida(BaseProdutos *b) {
  Grupo g[MAX_GRUPOS];
  int n, compras, sem_marca;

  n = agrupar_compras(b, g, 1, &compras, &sem_marca);
  if (n < 0) return;
  qsort(g, (size_t)n, sizeof(Grupo), comparar_qtd);

  printf("\n  %d compras, %d sem marca, %d marcas\n\n", compras, sem_marca, n);
  for (int i = 0; i < n && i < TOP; i++)
    printf("  %2d  %.*s  %5d compras\n", i + 1, TAM_MARCA, g[i].nome, g[i].qtd);
}

void faturamento_por_categoria(BaseProdutos *b) {
  Grupo g[MAX_GRUPOS];
  long long total = 0;
  int n, compras, sem_categoria;

  n = agrupar_compras(b, g, 0, &compras, &sem_categoria);
  if (n < 0) return;
  qsort(g, (size_t)n, sizeof(Grupo), comparar_centavos);

  for (int i = 0; i < n; i++) total += g[i].centavos;
  printf("\n  %d compras, %d sem categoria, %d categorias\n\n", compras, sem_categoria, n);
  for (int i = 0; i < n && i < TOP; i++)
    printf("  %2d  %.*s  %10lld.%02lld  %5d compras\n", i + 1, TAM_CATEGORIA, g[i].nome,
           g[i].centavos / 100, g[i].centavos % 100, g[i].qtd);
  printf("\n  total das categorias: %lld.%02lld\n", total / 100, total % 100);
}

void conversao_produto(BaseProdutos *b) {
  FILE *f;
  Acesso a;
  Produto p;
  int id, no_ovf, pos, views = 0, carts = 0, compras = 0;

  if (!ler_inteiro("id do produto: ", &id)) return;
  pos = localizar_produto(b, id, &p, &no_ovf);
  if (pos < 0 || p.ativo != '1') {
    printf("  o produto %d nao existe\n", id);
    return;
  }
  imprimir_produto(pos, &p);

  f = abrir_acessos();
  if (f == NULL) return;
  while (fread(&a, sizeof(Acesso), 1, f) == 1) {
    if (a.ativo != '1' || a.produto != id) continue;
    if (texto_igual(a.tipo, TAM_TIPO, "view")) views++;
    else if (texto_igual(a.tipo, TAM_TIPO, "cart")) carts++;
    else if (texto_igual(a.tipo, TAM_TIPO, "purchase")) compras++;
  }
  fclose(f);

  printf("  view %d  cart %d  purchase %d\n", views, carts, compras);
  if (views > 0) printf("  conversao (compras / visualizacoes): %.1f%%\n", 100.0 * compras / views);
}

/* a posicao no vetor e o numero da opcao */
static const char *OPCOES[] = {
    "sair",
    "regravar " ARQ_PRODUTOS " a partir de " ARQ_CSV,
    "listar faixa de produtos",
    "listar area de overflow",
    "pesquisa binaria direto no arquivo de produtos",
    "consulta pelo indice parcial de produtos",
    "imprimir indice parcial de produtos",
    "inserir produto",
    "remover produto",
    "reorganizar produtos (intercala o overflow e recria o indice)",
    "marca mais vendida",
    "taxa de conversao de um produto",
    "faturamento por categoria",
};
#define N_OPCOES (int)(sizeof(OPCOES) / sizeof(OPCOES[0]))

void menu(void) {
  printf("\n");
  for (int i = 1; i < N_OPCOES; i++) printf("%d - %s\n", i, OPCOES[i]);
  printf("0 - %s\n", OPCOES[0]);
}

int ler_opcao(void) {
  char linha[32];
  char *fim;
  long opcao;

  if (!ler_texto("opcao: ", linha, sizeof(linha))) return 0;
  opcao = strtol(linha, &fim, 10);
  if (fim == linha || *fim != '\0' || opcao < 0 || opcao >= N_OPCOES) return -1;
  return (int)opcao;
}

void resumir(const BaseProdutos *b) {
  printf("%s: %d produtos de %d bytes, %d/%d no overflow\n", ARQ_PRODUTOS, b->total,
         (int)sizeof(Produto), b->total_ovf, MAX_OVERFLOW);
  printf("%s: %d entradas, uma a cada %d registros\n", IDX_PRODUTOS, b->ind.n, b->ind.k);
}

int main(void) {
  BaseProdutos b = {0};
  int opcao;

  if (!abrir_base_produtos(&b)) return 1;
  resumir(&b);

  while (menu(), (opcao = ler_opcao()) != 0) {
    switch (opcao) {
      case 1:
        fechar_base_produtos(&b);
        remove(ARQ_PRODUTOS);
        if (!abrir_base_produtos(&b)) return 1;
        resumir(&b);
        break;
      case 2:
        listar_produtos(b.dados, b.total);
        break;
      case 3:
        listar_overflow(&b);
        break;
      case 4:
        buscar_no_arquivo(&b);
        break;
      case 5:
        buscar_pelo_indice(&b);
        break;
      case 6:
        imprimir_indice_produtos(&b.ind);
        break;
      case 7:
        menu_inserir(&b);
        break;
      case 8:
        menu_remover(&b);
        break;
      case 9:
        if (reorganizar_produtos(&b) < 0) return 1;
        resumir(&b);
        break;
      case 10:
        marca_mais_vendida(&b);
        break;
      case 11:
        conversao_produto(&b);
        break;
      case 12:
        faturamento_por_categoria(&b);
        break;
      default:
        printf("  opcao invalida\n");
    }
  }

  fechar_base_produtos(&b);
  return 0;
}
