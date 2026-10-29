#include "ecommerce.h"

#define MAX_GRUPOS 2000
#define TOP 10

typedef struct {
  char nome[TAM_CATEGORIA];
  int qtd;
  long long centavos;
} Grupo;

int texto_igual(const char *campo, int tam, const char *s) {
  int len = (int)strlen(s);

  if (len > tam || memcmp(campo, s, (size_t)len) != 0) return 0;
  return len == tam || campo[len] == ' ';
}

static int somar_grupo(Grupo *g, int *n, const char *nome, int tam, int centavos) {
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

static int comparar_qtd(const void *a, const void *b) {
  const Grupo *x = a, *y = b;

  return y->qtd - x->qtd;
}

static int comparar_centavos(const void *a, const void *b) {
  const Grupo *x = a, *y = b;

  if (x->centavos != y->centavos) return x->centavos < y->centavos ? 1 : -1;
  return 0;
}

/* le os acessos um registro por vez e busca o produto de cada compra pelo
   indice; agrupa pela marca ou pela categoria e devolve o numero de grupos */
static int agrupar_compras(BaseProdutos *bp, BaseAcessos *ba, Grupo *g, int por_marca, int *compras,
                           int *sem_nome) {
  Acesso a;
  Produto p;
  Percurso r;
  int n = 0, total = ba->total + ba->total_ext;

  *compras = 0;
  *sem_nome = 0;
  for (int i = 0; i < total; i++) {
    if (!ler_acesso_n(ba, i, &a)) return -1;
    if (a.ativo != '1' || !texto_igual(a.tipo, TAM_TIPO, "purchase")) continue;
    if (buscar_produto(bp, a.produto, 0, &p, &r) != 1) continue;

    (*compras)++;
    if ((por_marca ? p.marca[0] : p.categoria[0]) == ' ') {
      (*sem_nome)++;
      continue;
    }
    if (por_marca) somar_grupo(g, &n, p.marca, TAM_MARCA, a.preco);
    else somar_grupo(g, &n, p.categoria, TAM_CATEGORIA, a.preco);
  }
  return n;
}

void marca_mais_vendida(BaseProdutos *bp, BaseAcessos *ba) {
  Grupo g[MAX_GRUPOS];
  int n, compras, sem_marca;

  n = agrupar_compras(bp, ba, g, 1, &compras, &sem_marca);
  if (n < 0) {
    printf("  erro ao ler %s\n", ARQ_ACESSOS);
    return;
  }
  qsort(g, (size_t)n, sizeof(Grupo), comparar_qtd);

  printf("\n  %d compras, %d sem marca, %d marcas\n\n", compras, sem_marca, n);
  for (int i = 0; i < n && i < TOP; i++)
    printf("  %2d  %.*s  %5d compras\n", i + 1, TAM_MARCA, g[i].nome, g[i].qtd);
}

void faturamento_por_categoria(BaseProdutos *bp, BaseAcessos *ba) {
  Grupo g[MAX_GRUPOS];
  long long total = 0;
  int n, compras, sem_categoria;

  n = agrupar_compras(bp, ba, g, 0, &compras, &sem_categoria);
  if (n < 0) {
    printf("  erro ao ler %s\n", ARQ_ACESSOS);
    return;
  }
  qsort(g, (size_t)n, sizeof(Grupo), comparar_centavos);

  for (int i = 0; i < n; i++) total += g[i].centavos;
  printf("\n  %d compras, %d sem categoria, %d categorias\n\n", compras, sem_categoria, n);
  for (int i = 0; i < n && i < TOP; i++)
    printf("  %2d  %.*s  %10lld.%02lld  %5d compras\n", i + 1, TAM_CATEGORIA, g[i].nome,
           g[i].centavos / 100, g[i].centavos % 100, g[i].qtd);
  printf("\n  total das categorias: %lld.%02lld\n", total / 100, total % 100);
}

void conversao_produto(BaseProdutos *bp, BaseAcessos *ba, int id) {
  Acesso a;
  Produto p;
  Percurso r;
  int views = 0, carts = 0, compras = 0, total = ba->total + ba->total_ext;

  if (buscar_produto(bp, id, 0, &p, &r) != 1) {
    printf("  o produto %d nao existe\n", id);
    return;
  }
  imprimir_em(r.atual, &p);

  for (int i = 0; i < total; i++) {
    if (!ler_acesso_n(ba, i, &a)) {
      printf("  erro ao ler %s\n", ARQ_ACESSOS);
      return;
    }
    if (a.ativo != '1' || a.produto != id) continue;
    if (texto_igual(a.tipo, TAM_TIPO, "view")) views++;
    else if (texto_igual(a.tipo, TAM_TIPO, "cart")) carts++;
    else if (texto_igual(a.tipo, TAM_TIPO, "purchase")) compras++;
  }

  printf("  view %d  cart %d  purchase %d\n", views, carts, compras);
  if (views > 0) printf("  conversao (compras / visualizacoes): %.1f%%\n", 100.0 * compras / views);
}
