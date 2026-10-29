#include "ecommerce.h"

/* ---------- referencias ---------- */

Referencia ref_acesso(int pos) { return ref_principal_offset(pos * (int)sizeof(Acesso)); }

Referencia ref_acesso_ext(int pos) {
  Referencia r = {AREA_EXTENSAO, pos * (int)sizeof(Acesso)};
  return r;
}

/* "P12" = registro 12 do principal, "E3" = slot 3 da extensao */
static void texto_ref_acesso(char *buf, size_t tam, Referencia r) {
  if (r.area == AREA_PRINCIPAL) snprintf(buf, tam, "P%d", r.offset / (int)sizeof(Acesso));
  else if (r.area == AREA_EXTENSAO) snprintf(buf, tam, "E%d", r.offset / (int)sizeof(Acesso));
  else if (r.area == AREA_NENHUMA) snprintf(buf, tam, "FIM");
  else snprintf(buf, tam, "???");
}

/* ---------- acesso por posicao ao arquivo ---------- */

static int total_acessos(FILE *f) {
  long bytes;

  if (fseek(f, 0, SEEK_END) != 0) return -1;
  bytes = ftell(f);
  if (bytes < 0) return -1;
  return (int)(bytes / (long)sizeof(Acesso));
}

int ler_acesso(FILE *f, int i, Acesso *destino) {
  if (fseek(f, (long)i * (long)sizeof(Acesso), SEEK_SET) != 0) return 0;
  return fread(destino, sizeof(Acesso), 1, f) == 1;
}

static int gravar_acesso(FILE *f, int i, const Acesso *a) {
  if (fseek(f, (long)i * (long)sizeof(Acesso), SEEK_SET) != 0) return 0;
  if (fwrite(a, sizeof(Acesso), 1, f) != 1) return 0;
  return fflush(f) == 0;
}

static int resolver_ref(BaseAcessos *b, Referencia ref, FILE **f, int *pos) {
  int total;

  if (ref.area == AREA_PRINCIPAL) {
    *f = b->dados;
    total = b->total;
  } else if (ref.area == AREA_EXTENSAO) {
    *f = b->ext;
    total = b->total_ext;
  } else {
    return 0;
  }
  if (ref.offset < 0 || ref.offset % (int)sizeof(Acesso) != 0) return 0;
  *pos = ref.offset / (int)sizeof(Acesso);
  return *pos < total;
}

int ler_registro_acesso(BaseAcessos *b, Referencia ref, Acesso *a) {
  FILE *f;
  int pos;

  return resolver_ref(b, ref, &f, &pos) && ler_acesso(f, pos, a);
}

static int gravar_registro(BaseAcessos *b, Referencia ref, const Acesso *a) {
  FILE *f;
  int pos;

  return resolver_ref(b, ref, &f, &pos) && gravar_acesso(f, pos, a);
}

/* n-esimo registro fisico: primeiro o principal, depois a extensao */
int ler_acesso_n(BaseAcessos *b, int n, Acesso *a) {
  if (n < b->total) return ler_acesso(b->dados, n, a);
  return ler_acesso(b->ext, n - b->total, a);
}

static int anexar_extensao(BaseAcessos *b, const Acesso *a, Referencia *ref) {
  if (b->total_ext >= MAX_EXTENSAO) return 0;
  if (!gravar_acesso(b->ext, b->total_ext, a)) return 0;
  *ref = ref_acesso_ext(b->total_ext);
  b->total_ext++;
  return 1;
}

/* ---------- impressao ---------- */

void imprimir_acesso_em(Referencia ref, const Acesso *a) {
  char local[16], elo[16];

  texto_ref_acesso(local, sizeof(local), ref);
  texto_ref_acesso(elo, sizeof(elo), a->elo);
  printf("  %6s  %6d  %c  %.*s  %.*s  produto %8d  usuario %9d  %6d.%02d  -> %s\n", local, a->id,
         a->ativo, TAM_HORA, a->hora, TAM_TIPO, a->tipo, a->produto, a->usuario, a->preco / 100,
         a->preco % 100, elo);
}

void imprimir_indice_acessos(const IndiceParcial *ind) {
  printf("\n  entrada    offset     pos        id\n");
  for (int i = 0; i < ind->quantidade_entradas; i++) {
    if (i == 5 && ind->quantidade_entradas > 10) {
      printf("      ...\n");
      i = ind->quantidade_entradas - 5;
    }
    printf("  %7d  %8d  %6d  %8d\n", i, ind->entradas[i].offset,
           ind->entradas[i].offset / (int)sizeof(Acesso), ind->entradas[i].id);
  }
  printf("\n  %d entradas de %d bytes para %d registros, uma a cada %d\n",
         ind->quantidade_entradas, (int)sizeof(EntradaIndice), ind->total_principal, ind->span);
}

/* ---------- indice esparso ---------- */

static int criar_indice_acessos(FILE *f, int total, IndiceParcial *ind) {
  Acesso a;

  ind->span = SPAN_ACESSOS;
  ind->total_principal = total;
  ind->quantidade_entradas = (total + SPAN_ACESSOS - 1) / SPAN_ACESSOS;
  ind->entradas = malloc((size_t)(ind->quantidade_entradas > 0 ? ind->quantidade_entradas : 1) *
                         sizeof(EntradaIndice));
  if (ind->entradas == NULL) return 0;

  for (int i = 0; i < ind->quantidade_entradas; i++) {
    int pos = i * SPAN_ACESSOS;

    if (!ler_acesso(f, pos, &a)) {
      free(ind->entradas);
      ind->entradas = NULL;
      return 0;
    }
    ind->entradas[i].id = a.id;
    ind->entradas[i].offset = pos * (int)sizeof(Acesso);
  }
  return 1;
}

static int indice_confere(BaseAcessos *b) {
  Acesso a;

  for (int i = 0; i < b->ind.quantidade_entradas; i++) {
    int pos = i * b->ind.span;

    if (b->ind.entradas[i].offset != pos * (int)sizeof(Acesso)) return 0;
    if (!ler_acesso(b->dados, pos, &a) || a.id != b->ind.entradas[i].id) return 0;
  }
  return 1;
}

static int carregar_indice_acessos(BaseAcessos *b) {
  if (ler_indice(IDX_ACESSOS, b->total, SPAN_ACESSOS, &b->ind)) {
    if (indice_confere(b)) return 1;
    free(b->ind.entradas);
    b->ind.entradas = NULL;
  }

  printf("%s ausente, desatualizado ou inconsistente, recriando\n", IDX_ACESSOS);
  if (!criar_indice_acessos(b->dados, b->total, &b->ind)) return 0;
  gravar_indice(&b->ind, IDX_ACESSOS);
  return 1;
}

/* ---------- busca ---------- */

/* mesmo contrato de buscar_produto: 1 achou, 0 nao achou, -1 cadeia invalida */
int buscar_acesso(BaseAcessos *b, int id, int incluir_excluidos, Acesso *a, Percurso *r) {
  int limite = b->total + b->total_ext;

  r->anterior = ref_fim();
  r->atual = ref_fim();
  r->lidos = 0;
  r->bloco = encontrar_bloco(&b->ind, id, &r->comp_indice);
  if (r->bloco < 0) return 0;

  r->atual = ref_principal_offset(b->ind.entradas[r->bloco].offset);
  while (!ref_eh_fim(r->atual)) {
    if (r->lidos >= limite) {
      fprintf(stderr, "cadeia do bloco %d tem ciclo\n", r->bloco);
      return -1;
    }
    if (!ler_registro_acesso(b, r->atual, a)) {
      fprintf(stderr, "cadeia do bloco %d tem um elo invalido\n", r->bloco);
      return -1;
    }
    r->lidos++;

    if (a->id == id) return (a->ativo == '1' || incluir_excluidos) ? 1 : 0;
    if (a->id > id) return 0;
    r->anterior = r->atual;
    r->atual = a->elo;
  }
  return 0;
}

int buscar_binaria_acesso(BaseAcessos *b, int id, Acesso *a, Referencia *ref, int *leituras) {
  int lo = 0, hi = b->total - 1;

  *leituras = 0;
  while (lo <= hi) {
    int mid = lo + (hi - lo) / 2;

    if (!ler_acesso(b->dados, mid, a)) return -1;
    (*leituras)++;
    if (a->id == id) {
      *ref = ref_acesso(mid);
      return 1;
    }
    if (a->id < id) lo = mid + 1;
    else hi = mid - 1;
  }
  for (int i = 0; i < b->total_ext; i++) {
    if (!ler_acesso(b->ext, i, a)) return -1;
    (*leituras)++;
    if (a->id == id) {
      *ref = ref_acesso_ext(i);
      return 1;
    }
  }
  return 0;
}

/* o maior id esta no fim da cadeia do ultimo bloco; buscar INT_MAX percorre
   essa cadeia inteira e deixa o ultimo registro em r.anterior */
int proximo_id_acesso(BaseAcessos *b) {
  Acesso a;
  Percurso r;

  if (b->total == 0) return 1;
  if (buscar_acesso(b, INT_MAX, 1, &a, &r) < 0 || ref_eh_fim(r.anterior)) return -1;
  if (!ler_registro_acesso(b, r.anterior, &a)) return -1;
  return a.id + 1;
}

static int reconstruir_elos(FILE *f, int total) {
  Acesso a;

  for (int i = 0; i < total; i++) {
    int fim_bloco = (i / SPAN_ACESSOS + 1) * SPAN_ACESSOS;

    if (fim_bloco > total) fim_bloco = total;
    if (!ler_acesso(f, i, &a)) return 0;
    a.elo = i + 1 < fim_bloco ? ref_acesso(i + 1) : ref_fim();
    if (!gravar_acesso(f, i, &a)) return 0;
  }
  return 1;
}

/* ---------- abrir e fechar ---------- */

static int formato_compativel(FILE *f) {
  Acesso a;
  long bytes;
  int total;

  if (fseek(f, 0, SEEK_END) != 0) return 0;
  bytes = ftell(f);
  if (bytes < 0 || bytes % (long)sizeof(Acesso) != 0) return 0;
  total = (int)(bytes / (long)sizeof(Acesso));
  if (total == 0) return 1;
  if (!ler_acesso(f, 0, &a) || a.fim != '\n' || a.elo.area > AREA_NENHUMA) return 0;
  if (!ler_acesso(f, total - 1, &a) || a.fim != '\n' || a.elo.area > AREA_NENHUMA) return 0;
  return 1;
}

int abrir_base_acessos(BaseAcessos *b) {
  b->dados = fopen(ARQ_ACESSOS, "r+b");
  if (b->dados == NULL) {
    perror(ARQ_ACESSOS);
    printf("gere o arquivo com: python3 scripts/gerar_dados.py\n");
    return 0;
  }
  if (!formato_compativel(b->dados)) {
    printf("%s esta num formato incompativel: regere com scripts/gerar_dados.py\n", ARQ_ACESSOS);
    fclose(b->dados);
    return 0;
  }
  b->ext = abrir_ou_criar(EXT_ACESSOS);
  if (b->ext == NULL) {
    fclose(b->dados);
    return 0;
  }
  if (!formato_compativel(b->ext)) {
    printf("%s esta num formato incompativel: apague-o e abra o programa de novo\n", EXT_ACESSOS);
    fclose(b->dados);
    fclose(b->ext);
    return 0;
  }
  b->total = total_acessos(b->dados);
  b->total_ext = total_acessos(b->ext);
  return carregar_indice_acessos(b);
}

void fechar_base_acessos(BaseAcessos *b) {
  if (b->ind.entradas != NULL) gravar_indice(&b->ind, IDX_ACESSOS);
  free(b->ind.entradas);
  b->ind.entradas = NULL;
  if (b->dados != NULL) fclose(b->dados);
  if (b->ext != NULL) fclose(b->ext);
  b->dados = NULL;
  b->ext = NULL;
}

/* ---------- regeneracao ---------- */

static void falha_fatal(const char *arq) {
  perror(arq);
  fprintf(stderr, "base em estado inconsistente: apague os arquivos acessos.* e rode de novo\n");
  exit(1);
}

static int copiar_cadeias(BaseAcessos *b, FILE *tmp, int *total) {
  Acesso a;
  Referencia atual, proximo;
  long long ultimo = LLONG_MIN;
  int visitados = 0, limite = b->total + b->total_ext;

  *total = 0;
  for (int e = 0; e < b->ind.quantidade_entradas; e++) {
    atual = ref_principal_offset(b->ind.entradas[e].offset);
    while (!ref_eh_fim(atual)) {
      if (++visitados > limite) {
        fprintf(stderr, "regeneracao: ciclo ou registro alcancado duas vezes (bloco %d)\n", e);
        return 0;
      }
      if (!ler_registro_acesso(b, atual, &a)) {
        fprintf(stderr, "regeneracao: elo invalido no bloco %d\n", e);
        return 0;
      }
      if (a.id <= ultimo) {
        fprintf(stderr, "regeneracao: id %d fora de ordem no bloco %d\n", a.id, e);
        return 0;
      }
      ultimo = a.id;
      proximo = a.elo;

      if (a.ativo == '1') {
        a.elo = ref_fim();
        if (fwrite(&a, sizeof(Acesso), 1, tmp) != 1) return 0;
        (*total)++;
      }
      atual = proximo;
    }
  }
  if (visitados < limite)
    printf("  aviso: %d registros inalcancaveis foram descartados\n", limite - visitados);
  return 1;
}

int reorganizar_acessos(BaseAcessos *b) {
  IndiceParcial novo = {0};
  FILE *tmp;
  int total = 0, ok;

  tmp = fopen(TMP_ACESSOS, "w+b");
  if (tmp == NULL) {
    perror(TMP_ACESSOS);
    return -1;
  }
  ok = copiar_cadeias(b, tmp, &total) && reconstruir_elos(tmp, total) &&
       criar_indice_acessos(tmp, total, &novo) && gravar_indice(&novo, TMP_INDICE_ACESSOS);
  if (fclose(tmp) != 0) ok = 0;
  if (!ok) {
    remove(TMP_ACESSOS);
    remove(TMP_INDICE_ACESSOS);
    free(novo.entradas);
    return -1;
  }

  fclose(b->dados);
  fclose(b->ext);
  b->dados = NULL;
  b->ext = NULL;
  free(b->ind.entradas);
  b->ind.entradas = NULL;

  if (!substituir_arquivo(TMP_ACESSOS, ARQ_ACESSOS)) falha_fatal(ARQ_ACESSOS);
  if (!substituir_arquivo(TMP_INDICE_ACESSOS, IDX_ACESSOS)) falha_fatal(IDX_ACESSOS);
  b->dados = fopen(ARQ_ACESSOS, "r+b");
  if (b->dados == NULL) falha_fatal(ARQ_ACESSOS);
  b->ext = fopen(EXT_ACESSOS, "w+b");
  if (b->ext == NULL) falha_fatal(EXT_ACESSOS);

  b->total = total;
  b->total_ext = 0;
  b->ind = novo;
  return total;
}

/* ---------- insercao e remocao ---------- */

static int inserir_na_cadeia(BaseAcessos *b, Acesso *novo, const Percurso *r) {
  Acesso anterior;
  Referencia nova;

  if (ref_eh_fim(r->anterior)) return INS_ERRO;

  novo->elo = r->atual;
  if (!anexar_extensao(b, novo, &nova)) return INS_ERRO;
  if (!ler_registro_acesso(b, r->anterior, &anterior)) return INS_ERRO;
  anterior.elo = nova;
  if (!gravar_registro(b, r->anterior, &anterior)) return INS_ERRO;
  return INS_NA_EXTENSAO;
}

static int inserir_em_base_vazia(BaseAcessos *b, Acesso *novo) {
  novo->elo = ref_fim();
  if (!gravar_acesso(b->dados, 0, novo)) return INS_ERRO;
  b->total = 1;
  free(b->ind.entradas);
  b->ind.entradas = NULL;
  if (!criar_indice_acessos(b->dados, b->total, &b->ind)) return INS_ERRO;
  return gravar_indice(&b->ind, IDX_ACESSOS) ? INS_BASE_VAZIA : INS_ERRO;
}

/* o id vem de proximo_id_acesso, sempre maior que todos: o novo entra no fim
   da cadeia do ultimo bloco e nunca antes da primeira cabeca */
int inserir_acesso(BaseAcessos *b, Acesso *novo, int *regenerou) {
  Acesso a;
  Percurso r;
  int achou;

  *regenerou = 0;
  achou = buscar_acesso(b, novo->id, 1, &a, &r);
  if (achou < 0) return INS_ERRO;
  if (achou) return a.ativo == '1' ? INS_DUPLICADO : INS_RESERVADO;

  novo->ativo = '1';
  novo->fim = '\n';
  novo->elo = ref_fim();

  if (b->total_ext + 1 > MAX_EXTENSAO) {
    if (reorganizar_acessos(b) < 0) return INS_ERRO;
    *regenerou = 1;
    if (buscar_acesso(b, novo->id, 1, &a, &r) != 0) return INS_ERRO;
  }

  if (b->total == 0) return inserir_em_base_vazia(b, novo);
  if (r.bloco < 0) return INS_ERRO;
  return inserir_na_cadeia(b, novo, &r);
}

int remover_acesso(BaseAcessos *b, int id) {
  Acesso a;
  Percurso r;
  int achou = buscar_acesso(b, id, 1, &a, &r);

  if (achou < 0) return -1;
  if (!achou || a.ativo != '1') return 0;

  a.ativo = '0';
  return gravar_registro(b, r.atual, &a) ? 1 : -1;
}
