#include "ecommerce.h"

/* ---------- referencias ---------- */

Referencia ref_principal_offset(int offset) {
  Referencia r = {AREA_PRINCIPAL, offset};
  return r;
}

Referencia ref_principal(int pos) { return ref_principal_offset(pos * (int)sizeof(Produto)); }

Referencia ref_extensao(int pos) {
  Referencia r = {AREA_EXTENSAO, pos * (int)sizeof(Produto)};
  return r;
}

Referencia ref_fim(void) {
  Referencia r = {AREA_NENHUMA, -1};
  return r;
}

int ref_eh_fim(Referencia r) { return r.area == AREA_NENHUMA; }

/* "P12" = registro 12 do principal, "E3" = slot 3 da extensao */
void texto_ref(char *buf, size_t tam, Referencia r) {
  if (r.area == AREA_PRINCIPAL) snprintf(buf, tam, "P%d", r.offset / (int)sizeof(Produto));
  else if (r.area == AREA_EXTENSAO) snprintf(buf, tam, "E%d", r.offset / (int)sizeof(Produto));
  else if (r.area == AREA_NENHUMA) snprintf(buf, tam, "FIM");
  else snprintf(buf, tam, "???");
}

/* ---------- texto ---------- */

void preencher_texto(char *destino, int tam, const char *origem) {
  int len = (int)strlen(origem);

  if (len > tam) len = tam;
  memcpy(destino, origem, (size_t)len);
  memset(destino + len, ' ', (size_t)(tam - len));
}

/* ---------- acesso por posicao ao arquivo ---------- */

static int total_produtos(FILE *f) {
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

static int gravar_produto(FILE *f, int i, const Produto *p) {
  if (fseek(f, (long)i * (long)sizeof(Produto), SEEK_SET) != 0) return 0;
  if (fwrite(p, sizeof(Produto), 1, f) != 1) return 0;
  return fflush(f) == 0;
}

/* resolve area + offset para o FILE* e posicao correspondentes */
static int resolver_ref(BaseProdutos *b, Referencia ref, FILE **f, int *pos) {
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
  if (ref.offset < 0 || ref.offset % (int)sizeof(Produto) != 0) return 0;
  *pos = ref.offset / (int)sizeof(Produto);
  return *pos < total;
}

int ler_registro(BaseProdutos *b, Referencia ref, Produto *p) {
  FILE *f;
  int pos;

  return resolver_ref(b, ref, &f, &pos) && ler_produto(f, pos, p);
}

static int gravar_registro(BaseProdutos *b, Referencia ref, const Produto *p) {
  FILE *f;
  int pos;

  return resolver_ref(b, ref, &f, &pos) && gravar_produto(f, pos, p);
}

/* grava no proximo slot livre da extensao */
static int anexar_extensao(BaseProdutos *b, const Produto *p, Referencia *ref) {
  if (b->total_ext >= MAX_EXTENSAO) return 0;
  if (!gravar_produto(b->ext, b->total_ext, p)) return 0;
  *ref = ref_extensao(b->total_ext);
  b->total_ext++;
  return 1;
}

/* ---------- impressao ---------- */

void imprimir_em(Referencia ref, const Produto *p) {
  char local[16], elo[16];

  texto_ref(local, sizeof(local), ref);
  texto_ref(elo, sizeof(elo), p->elo);
  printf("  %5s  %8d  %c  %6d.%02d  %.*s  %.*s  -> %s\n", local, p->id, p->ativo,
         p->preco / 100, p->preco % 100, TAM_MARCA, p->marca, TAM_CATEGORIA, p->categoria, elo);
}

void imprimir_indice_produtos(const IndiceParcial *ind) {
  printf("\n  entrada  offset   pos        id\n");
  for (int i = 0; i < ind->quantidade_entradas; i++) {
    if (i == 5 && ind->quantidade_entradas > 10) {
      printf("      ...\n");
      i = ind->quantidade_entradas - 5;
    }
    printf("  %7d  %6d  %4d  %8d\n", i, ind->entradas[i].offset,
           ind->entradas[i].offset / (int)sizeof(Produto), ind->entradas[i].id);
  }
  printf("\n  %d entradas de %d bytes para %d registros, uma a cada %d\n",
         ind->quantidade_entradas, (int)sizeof(EntradaIndice), ind->total_principal, ind->span);
}

/* ---------- indice esparso ---------- */

static int criar_indice_produtos(FILE *f, int total, IndiceParcial *ind) {
  Produto p;

  ind->span = SPAN_PRODUTOS;
  ind->total_principal = total;
  ind->quantidade_entradas = (total + SPAN_PRODUTOS - 1) / SPAN_PRODUTOS;
  ind->entradas = malloc((size_t)(ind->quantidade_entradas > 0 ? ind->quantidade_entradas : 1) *
                         sizeof(EntradaIndice));
  if (ind->entradas == NULL) return 0;

  for (int i = 0; i < ind->quantidade_entradas; i++) {
    int pos = i * SPAN_PRODUTOS;

    if (!ler_produto(f, pos, &p)) {
      free(ind->entradas);
      ind->entradas = NULL;
      return 0;
    }
    ind->entradas[i].id = p.id;
    ind->entradas[i].offset = pos * (int)sizeof(Produto);
  }
  return 1;
}

int gravar_indice(const IndiceParcial *ind, const char *arq) {
  FILE *idx = fopen(arq, "wb");

  if (idx == NULL) {
    perror(arq);
    return 0;
  }
  if (fwrite(&ind->span, sizeof(ind->span), 1, idx) != 1 ||
      fwrite(&ind->total_principal, sizeof(ind->total_principal), 1, idx) != 1 ||
      fwrite(&ind->quantidade_entradas, sizeof(ind->quantidade_entradas), 1, idx) != 1 ||
      (int)fwrite(ind->entradas, sizeof(EntradaIndice), (size_t)ind->quantidade_entradas, idx) !=
          ind->quantidade_entradas) {
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

int ler_indice(const char *arq, int total, int span, IndiceParcial *ind) {
  FILE *idx = fopen(arq, "rb");
  int ok;

  ind->entradas = NULL;
  if (idx == NULL) return 0;

  ok = fread(&ind->span, sizeof(ind->span), 1, idx) == 1 &&
       fread(&ind->total_principal, sizeof(ind->total_principal), 1, idx) == 1 &&
       fread(&ind->quantidade_entradas, sizeof(ind->quantidade_entradas), 1, idx) == 1 &&
       ind->span == span && ind->total_principal == total &&
       ind->quantidade_entradas == (total + span - 1) / span;
  if (ok) {
    ind->entradas = malloc((size_t)(ind->quantidade_entradas > 0 ? ind->quantidade_entradas : 1) *
                           sizeof(EntradaIndice));
    ok = ind->entradas != NULL &&
         (int)fread(ind->entradas, sizeof(EntradaIndice), (size_t)ind->quantidade_entradas, idx) ==
             ind->quantidade_entradas;
  }
  fclose(idx);

  if (ok) return 1;
  free(ind->entradas);
  ind->entradas = NULL;
  return 0;
}

/* cada entrada tem de apontar para o inicio de um bloco e o id gravado tem de
   ser o do registro que esta la */
static int indice_confere(BaseProdutos *b) {
  Produto p;

  for (int i = 0; i < b->ind.quantidade_entradas; i++) {
    int pos = i * b->ind.span;

    if (b->ind.entradas[i].offset != pos * (int)sizeof(Produto)) return 0;
    if (!ler_produto(b->dados, pos, &p) || p.id != b->ind.entradas[i].id) return 0;
  }
  return 1;
}

static int carregar_indice_produtos(BaseProdutos *b) {
  if (ler_indice(IDX_PRODUTOS, b->total, SPAN_PRODUTOS, &b->ind)) {
    if (indice_confere(b)) return 1;
    free(b->ind.entradas);
    b->ind.entradas = NULL;
  }

  printf("%s ausente, desatualizado ou inconsistente, recriando\n", IDX_PRODUTOS);
  if (!criar_indice_produtos(b->dados, b->total, &b->ind)) return 0;
  gravar_indice(&b->ind, IDX_PRODUTOS);
  return 1;
}

/* ---------- busca ---------- */

/* ultima entrada cujo id de cabeca e <= id; -1 se o id e menor que a primeira cabeca */
int encontrar_bloco(const IndiceParcial *ind, int id, int *comparacoes) {
  int lo = 0, hi = ind->quantidade_entradas - 1, resultado = -1;

  *comparacoes = 0;
  while (lo <= hi) {
    int mid = lo + (hi - lo) / 2;

    (*comparacoes)++;
    if (ind->entradas[mid].id <= id) {
      resultado = mid;
      lo = mid + 1;
    } else {
      hi = mid - 1;
    }
  }
  return resultado;
}

/* indice para achar o bloco e depois segue os elos da cadeia dele.
   1 achou, 0 nao achou, -1 cadeia invalida (elo fora do arquivo ou ciclo).
   Removidos nao interrompem a travessia: o elo deles continua valendo */
int buscar_produto(BaseProdutos *b, int id, int incluir_excluidos, Produto *p, Percurso *r) {
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
    if (!ler_registro(b, r->atual, p)) {
      fprintf(stderr, "cadeia do bloco %d tem um elo invalido\n", r->bloco);
      return -1;
    }
    r->lidos++;

    if (p->id == id) return (p->ativo == '1' || incluir_excluidos) ? 1 : 0;
    if (p->id > id) return 0;
    r->anterior = r->atual;
    r->atual = p->elo;
  }
  return 0;
}

/* sem indice: a binaria vale so no principal, que segue ordenado por posicao;
   a extensao nao tem ordem e e lida em sequencia. 1 achou, 0 nao, -1 erro */
int buscar_binaria_produto(BaseProdutos *b, int id, Produto *p, Referencia *ref, int *leituras) {
  int lo = 0, hi = b->total - 1;

  *leituras = 0;
  while (lo <= hi) {
    int mid = lo + (hi - lo) / 2;

    if (!ler_produto(b->dados, mid, p)) return -1;
    (*leituras)++;
    if (p->id == id) {
      *ref = ref_principal(mid);
      return 1;
    }
    if (p->id < id) lo = mid + 1;
    else hi = mid - 1;
  }
  for (int i = 0; i < b->total_ext; i++) {
    if (!ler_produto(b->ext, i, p)) return -1;
    (*leituras)++;
    if (p->id == id) {
      *ref = ref_extensao(i);
      return 1;
    }
  }
  return 0;
}

/* o arquivo esta ordenado e dividido em blocos de SPAN_PRODUTOS posicoes:
   cada registro aponta para o seguinte do mesmo bloco, e o ultimo do bloco
   termina a cadeia */
static int reconstruir_elos(FILE *f, int total) {
  Produto p;

  for (int i = 0; i < total; i++) {
    int fim_bloco = (i / SPAN_PRODUTOS + 1) * SPAN_PRODUTOS;

    if (fim_bloco > total) fim_bloco = total;
    if (!ler_produto(f, i, &p)) return 0;
    p.elo = i + 1 < fim_bloco ? ref_principal(i + 1) : ref_fim();
    if (!gravar_produto(f, i, &p)) return 0;
  }
  return 1;
}

/* ---------- abrir e fechar ---------- */

FILE *abrir_ou_criar(const char *arq) {
  FILE *f = fopen(arq, "r+b");

  if (f == NULL) f = fopen(arq, "w+b");
  if (f == NULL) perror(arq);
  return f;
}

/* arquivos do formato antigo (72 bytes, sem elo) nao podem ser lidos como o novo */
static int formato_compativel(FILE *f) {
  Produto p;
  long bytes;
  int total;

  if (fseek(f, 0, SEEK_END) != 0) return 0;
  bytes = ftell(f);
  if (bytes < 0 || bytes % (long)sizeof(Produto) != 0) return 0;
  total = (int)(bytes / (long)sizeof(Produto));
  if (total == 0) return 1;
  if (!ler_produto(f, 0, &p) || p.fim != '\n' || p.elo.area > AREA_NENHUMA) return 0;
  if (!ler_produto(f, total - 1, &p) || p.fim != '\n' || p.elo.area > AREA_NENHUMA) return 0;
  return 1;
}

int abrir_base_produtos(BaseProdutos *b) {
  b->dados = fopen(ARQ_PRODUTOS, "r+b");
  if (b->dados == NULL) {
    perror(ARQ_PRODUTOS);
    printf("gere o arquivo com: python3 scripts/gerar_dados.py\n");
    return 0;
  }
  if (!formato_compativel(b->dados)) {
    printf("%s esta num formato incompativel: regere com scripts/gerar_dados.py\n", ARQ_PRODUTOS);
    fclose(b->dados);
    return 0;
  }
  b->ext = abrir_ou_criar(EXT_PRODUTOS);
  if (b->ext == NULL) {
    fclose(b->dados);
    return 0;
  }
  if (!formato_compativel(b->ext)) {
    printf("%s esta num formato incompativel: apague-o e abra o programa de novo\n", EXT_PRODUTOS);
    fclose(b->dados);
    fclose(b->ext);
    return 0;
  }
  b->total = total_produtos(b->dados);
  b->total_ext = total_produtos(b->ext);
  return carregar_indice_produtos(b);
}

void fechar_base_produtos(BaseProdutos *b) {
  if (b->ind.entradas != NULL) gravar_indice(&b->ind, IDX_PRODUTOS);
  free(b->ind.entradas);
  b->ind.entradas = NULL;
  if (b->dados != NULL) fclose(b->dados);
  if (b->ext != NULL) fclose(b->ext);
  b->dados = NULL;
  b->ext = NULL;
}

/* ---------- regeneracao ---------- */

/* no Windows o rename falha se o destino ja existe */
int substituir_arquivo(const char *origem, const char *destino) {
  if (rename(origem, destino) == 0) return 1;
  remove(destino);
  return rename(origem, destino) == 0;
}

/* depois de fechar os arquivos antigos nao ha como voltar atras */
static void falha_fatal(const char *arq) {
  perror(arq);
  fprintf(stderr, "base em estado inconsistente: apague os arquivos produtos.* e rode de novo\n");
  exit(1);
}

/* percorre as cadeias na ordem das entradas do indice e copia so os ativos.
   Confere ciclo, elo invalido e ordem estritamente crescente */
static int copiar_cadeias(BaseProdutos *b, FILE *tmp, int *total) {
  Produto p;
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
      if (!ler_registro(b, atual, &p)) {
        fprintf(stderr, "regeneracao: elo invalido no bloco %d\n", e);
        return 0;
      }
      if (p.id <= ultimo) {
        fprintf(stderr, "regeneracao: id %d fora de ordem no bloco %d\n", p.id, e);
        return 0;
      }
      ultimo = p.id;
      proximo = p.elo;

      if (p.ativo == '1') {
        p.elo = ref_fim();
        if (fwrite(&p, sizeof(Produto), 1, tmp) != 1) return 0;
        (*total)++;
      }
      atual = proximo;
    }
  }
  if (visitados < limite)
    printf("  aviso: %d registros inalcancaveis foram descartados\n", limite - visitados);
  return 1;
}

int reorganizar_produtos(BaseProdutos *b) {
  IndiceParcial novo = {0};
  FILE *tmp;
  int total = 0, ok;

  tmp = fopen(TMP_PRODUTOS, "w+b");
  if (tmp == NULL) {
    perror(TMP_PRODUTOS);
    return -1;
  }
  ok = copiar_cadeias(b, tmp, &total) && reconstruir_elos(tmp, total) &&
       criar_indice_produtos(tmp, total, &novo) && gravar_indice(&novo, TMP_INDICE);
  if (fclose(tmp) != 0) ok = 0;
  if (!ok) {
    remove(TMP_PRODUTOS);
    remove(TMP_INDICE);
    free(novo.entradas);
    return -1;
  }

  fclose(b->dados);
  fclose(b->ext);
  b->dados = NULL;
  b->ext = NULL;
  free(b->ind.entradas);
  b->ind.entradas = NULL;

  if (!substituir_arquivo(TMP_PRODUTOS, ARQ_PRODUTOS)) falha_fatal(ARQ_PRODUTOS);
  if (!substituir_arquivo(TMP_INDICE, IDX_PRODUTOS)) falha_fatal(IDX_PRODUTOS);
  b->dados = fopen(ARQ_PRODUTOS, "r+b");
  if (b->dados == NULL) falha_fatal(ARQ_PRODUTOS);
  b->ext = fopen(EXT_PRODUTOS, "w+b");
  if (b->ext == NULL) falha_fatal(EXT_PRODUTOS);

  b->total = total;
  b->total_ext = 0;
  b->ind = novo;
  return total;
}

/* ---------- insercao e remocao ---------- */

/* o novo registro vai para a extensao e entra na cadeia depois do predecessor */
static int inserir_na_cadeia(BaseProdutos *b, Produto *novo, const Percurso *r) {
  Produto anterior;
  Referencia nova;

  if (ref_eh_fim(r->anterior)) return INS_ERRO;

  novo->elo = r->atual;
  if (!anexar_extensao(b, novo, &nova)) return INS_ERRO;
  if (!ler_registro(b, r->anterior, &anterior)) return INS_ERRO;
  anterior.elo = nova;
  if (!gravar_registro(b, r->anterior, &anterior)) return INS_ERRO;
  return INS_NA_EXTENSAO;
}

/* o indice so aponta para o principal, entao a cabeca nao pode virar um
   registro da extensao: o novo ocupa a posicao da cabeca, e o antigo vai
   para a extensao */
static int inserir_antes_da_cabeca(BaseProdutos *b, Produto *novo) {
  Produto antiga;
  Referencia ref_cabeca, copia;

  ref_cabeca = ref_principal_offset(b->ind.entradas[0].offset);
  if (!ler_registro(b, ref_cabeca, &antiga)) return INS_ERRO;
  if (!anexar_extensao(b, &antiga, &copia)) return INS_ERRO;

  novo->elo = copia;
  if (!gravar_registro(b, ref_cabeca, novo)) return INS_ERRO;

  b->ind.entradas[0].id = novo->id;
  return gravar_indice(&b->ind, IDX_PRODUTOS) ? INS_NA_CABECA : INS_ERRO;
}

static int inserir_em_base_vazia(BaseProdutos *b, Produto *novo) {
  novo->elo = ref_fim();
  if (!gravar_produto(b->dados, 0, novo)) return INS_ERRO;
  b->total = 1;
  free(b->ind.entradas);
  b->ind.entradas = NULL;
  if (!criar_indice_produtos(b->dados, b->total, &b->ind)) return INS_ERRO;
  return gravar_indice(&b->ind, IDX_PRODUTOS) ? INS_BASE_VAZIA : INS_ERRO;
}

int inserir_produto(BaseProdutos *b, Produto *novo, int *regenerou) {
  Produto p;
  Percurso r;
  int achou;

  *regenerou = 0;
  achou = buscar_produto(b, novo->id, 1, &p, &r);
  if (achou < 0) return INS_ERRO;
  if (achou) return p.ativo == '1' ? INS_DUPLICADO : INS_RESERVADO;

  novo->ativo = '1';
  novo->fim = '\n';
  novo->elo = ref_fim();

  /* as duas formas de inserir na extensao gastam exatamente um slot;
     a base vazia grava direto no principal */
  if (b->total_ext + 1 > MAX_EXTENSAO) {
    if (reorganizar_produtos(b) < 0) return INS_ERRO;
    *regenerou = 1;
    if (buscar_produto(b, novo->id, 1, &p, &r) != 0) return INS_ERRO;
  }

  if (b->total == 0) return inserir_em_base_vazia(b, novo);
  if (r.bloco < 0) return inserir_antes_da_cabeca(b, novo);
  return inserir_na_cadeia(b, novo, &r);
}

/* 1 removido, 0 se nao existe ou ja estava removido, -1 erro.
   So troca o campo ativo: o registro e o elo continuam onde estavam */
int remover_produto(BaseProdutos *b, int id) {
  Produto p;
  Percurso r;
  int achou = buscar_produto(b, id, 1, &p, &r);

  if (achou < 0) return -1;
  if (!achou || p.ativo != '1') return 0;

  p.ativo = '0';
  return gravar_registro(b, r.atual, &p) ? 1 : -1;
}
