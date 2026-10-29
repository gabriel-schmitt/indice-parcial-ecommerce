#include "ecommerce.h"

/* ---------- entrada do usuario ---------- */

static int ler_texto(const char *rotulo, char *destino, int tam) {
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

static int ler_inteiro(const char *rotulo, int *v) {
  char entrada[32];
  char *fim;
  long x;

  if (!ler_texto(rotulo, entrada, sizeof(entrada))) return 0;
  x = strtol(entrada, &fim, 10);
  if (fim == entrada || *fim != '\0') return 0;
  *v = (int)x;
  return 1;
}

/* ---------- listagem ---------- */

static void listar_produtos(FILE *f, int total) {
  Produto p;
  int inicio, quantos;

  if (!ler_inteiro("registro inicial: ", &inicio)) return;
  if (!ler_inteiro("quantos: ", &quantos)) return;

  if (inicio < 0) inicio = 0;
  if (inicio + quantos > total) quantos = total - inicio;

  for (int i = inicio; i < inicio + quantos; i++) {
    if (!ler_produto(f, i, &p)) return;
    imprimir_em(ref_principal(i), &p);
  }
}

static void listar_extensao(BaseProdutos *b) {
  Produto p;

  printf("  %d de %d slots usados na extensao\n", b->total_ext, MAX_EXTENSAO);
  for (int i = 0; i < b->total_ext; i++) {
    if (!ler_produto(b->ext, i, &p)) return;
    imprimir_em(ref_extensao(i), &p);
  }
}

/* ---------- consulta ---------- */

static void buscar_pelo_indice(BaseProdutos *b) {
  Produto p;
  Percurso r;
  int id, achou;

  if (!ler_inteiro("id do produto: ", &id)) return;
  achou = buscar_produto(b, id, 1, &p, &r);
  if (achou < 0) {
    printf("  cadeia invalida\n");
    return;
  }
  if (r.bloco < 0) {
    printf("  %d comparacoes no indice: o id e menor que a primeira cabeca, nao existe\n", r.comp_indice);
    return;
  }
  printf("  indice: %d comparacoes, bloco %d; cadeia: %d registros lidos\n", r.comp_indice, r.bloco, r.lidos);
  if (!achou) {
    printf("  nao esta na cadeia\n");
    return;
  }
  if (p.ativo != '1') printf("  mas o produto foi removido\n");
  imprimir_em(r.atual, &p);
}

static void imprimir_cadeia(BaseProdutos *b) {
  Produto p;
  Referencia atual;
  int bloco, passos = 0, limite = b->total + b->total_ext;

  if (!ler_inteiro("bloco (entrada do indice): ", &bloco)) return;
  if (bloco < 0 || bloco >= b->ind.quantidade_entradas) {
    printf("  bloco invalido\n");
    return;
  }
  atual = ref_principal_offset(b->ind.entradas[bloco].offset);
  while (!ref_eh_fim(atual)) {
    if (passos++ >= limite || !ler_registro(b, atual, &p)) {
      printf("  cadeia invalida\n");
      return;
    }
    imprimir_em(atual, &p);
    atual = p.elo;
  }
}

/* ---------- insercao e remocao ---------- */

static int ler_produto_novo(Produto *p) {
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
  p->elo = ref_fim();
  p->ativo = '1';
  p->fim = '\n';
  return 1;
}

static void menu_inserir(BaseProdutos *b) {
  Produto p;
  int r, regenerou;

  if (!ler_produto_novo(&p)) {
    printf("  entrada invalida\n");
    return;
  }
  r = inserir_produto(b, &p, &regenerou);
  if (regenerou) printf("  extensao cheia: base reorganizada com %d produtos\n", b->total);

  switch (r) {
    case INS_DUPLICADO:
      printf("  o produto %d ja existe\n", p.id);
      break;
    case INS_RESERVADO:
      printf("  o id %d pertence a um produto removido e continua reservado ate a reorganizacao\n", p.id);
      break;
    case INS_NA_EXTENSAO:
      printf("  inserido na extensao (%d/%d) e ligado a cadeia\n", b->total_ext, MAX_EXTENSAO);
      break;
    case INS_NA_CABECA:
      printf("  id menor que a primeira cabeca: o novo registro ficou na posicao da cabeca e o\n"
             "  antigo foi para a extensao (%d/%d)\n", b->total_ext, MAX_EXTENSAO);
      break;
    case INS_BASE_VAZIA:
      printf("  inserido como unico registro da base\n");
      break;
    default:
      printf("  erro ao inserir\n");
  }
}

static void menu_remover(BaseProdutos *b) {
  int id, r;

  if (!ler_inteiro("id do produto: ", &id)) return;
  r = remover_produto(b, id);
  if (r < 0) printf("  erro ao remover\n");
  else if (r == 0) printf("  o produto %d nao existe ou ja foi removido\n", id);
  else printf("  produto %d marcado como removido\n", id);
}

/* ---------- menu ---------- */

static const char *OPCOES[] = {
    "sair",
    "listar faixa de produtos do arquivo principal",
    "listar area de extensao",
    "buscar produto pelo indice esparso",
    "imprimir indice esparso",
    "imprimir cadeia de um bloco",
    "inserir produto",
    "remover produto",
    "regenerar (percorre as cadeias, descarta removidos, refaz elos e indice)",
};
#define N_OPCOES (int)(sizeof(OPCOES) / sizeof(OPCOES[0]))

static void menu(void) {
  printf("\n");
  for (int i = 1; i < N_OPCOES; i++) printf("%d - %s\n", i, OPCOES[i]);
  printf("0 - %s\n", OPCOES[0]);
}

static int ler_opcao(void) {
  char linha[32];
  char *fim;
  long opcao;

  if (!ler_texto("opcao: ", linha, sizeof(linha))) return 0;
  opcao = strtol(linha, &fim, 10);
  if (fim == linha || *fim != '\0' || opcao < 0 || opcao >= N_OPCOES) return -1;
  return (int)opcao;
}

static void resumir(const BaseProdutos *b) {
  printf("%s: %d produtos de %d bytes, %d/%d na extensao\n", ARQ_PRODUTOS, b->total,
         (int)sizeof(Produto), b->total_ext, MAX_EXTENSAO);
  printf("%s: %d entradas, uma a cada %d registros\n", IDX_PRODUTOS, b->ind.quantidade_entradas,
         b->ind.span);
}

int main(void) {
  BaseProdutos b = {0};
  int opcao;

  if (!abrir_base_produtos(&b)) return 1;
  resumir(&b);

  while (menu(), (opcao = ler_opcao()) != 0) {
    switch (opcao) {
      case 1:
        listar_produtos(b.dados, b.total);
        break;
      case 2:
        listar_extensao(&b);
        break;
      case 3:
        buscar_pelo_indice(&b);
        break;
      case 4:
        imprimir_indice_produtos(&b.ind);
        break;
      case 5:
        imprimir_cadeia(&b);
        break;
      case 6:
        menu_inserir(&b);
        break;
      case 7:
        menu_remover(&b);
        break;
      case 8:
        if (reorganizar_produtos(&b) < 0) {
          printf("  erro ao regenerar, os arquivos antigos foram mantidos\n");
        } else {
          resumir(&b);
        }
        break;
      default:
        printf("  opcao invalida\n");
    }
  }

  fechar_base_produtos(&b);
  return 0;
}
