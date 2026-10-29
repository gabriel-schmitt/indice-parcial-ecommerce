#ifndef ECOMMERCE_H
#define ECOMMERCE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

/* nomes dos arquivos */
#define ARQ_PRODUTOS "produtos.bin"
#define IDX_PRODUTOS "produtos.idx"
#define EXT_PRODUTOS "produtos.ovf"
#define TMP_PRODUTOS "produtos.tmp"
#define TMP_INDICE "produtos.idx.tmp"
#define ARQ_ACESSOS "acessos.bin"
#define IDX_ACESSOS "acessos.idx"
#define EXT_ACESSOS "acessos.ovf"
#define TMP_ACESSOS "acessos.tmp"
#define TMP_INDICE_ACESSOS "acessos.idx.tmp"

/* os tamanhos fecham cada struct num multiplo de 4: senao o gcc poe
   padding depois do fim e o registro deixa de terminar em '\n' */
#define TAM_MARCA 20
#define TAM_CATEGORIA 42
#define TAM_TIPO 11
#define TAM_HORA 19

/* uma entrada do indice para cada SPAN_PRODUTOS posicoes fisicas do arquivo
   principal. Registro de 80 bytes: 51 * 80 = 4080, cabe numa pagina de 4 KB */
#define SPAN_PRODUTOS 51
#define MAX_EXTENSAO 64

/* registro de 56 bytes: 73 * 56 = 4088, cabe numa pagina de 4 KB */
#define SPAN_ACESSOS 73

/* ---------- tipos ---------- */

typedef enum {
  AREA_PRINCIPAL,
  AREA_EXTENSAO,
  AREA_NENHUMA
} Area;

/* localizacao fisica de um registro: area + offset em bytes naquele arquivo */
typedef struct {
  Area area;
  int offset;
} Referencia;

/* layout sem padding: id 0-3, preco 4-7, elo 8-15, ativo 16, marca 17-36,
   categoria 37-78, fim 79 */
typedef struct {
  int id;
  int preco;
  Referencia elo;
  char ativo;
  char marca[TAM_MARCA];
  char categoria[TAM_CATEGORIA];
  char fim;
} Produto;

_Static_assert(sizeof(Produto) == 80, "Produto ganhou padding: o formato do arquivo mudaria");

/* chave sequencial na ordem do csv. layout sem padding: id 0-3, produto 4-7,
   usuario 8-11, preco 12-15, elo 16-23, ativo 24, tipo 25-35, hora 36-54, fim 55 */
typedef struct {
  int id;
  int produto;
  int usuario;
  int preco;
  Referencia elo;
  char ativo;
  char tipo[TAM_TIPO];
  char hora[TAM_HORA];
  char fim;
} Acesso;

_Static_assert(sizeof(Acesso) == 56, "Acesso ganhou padding: o formato do arquivo mudaria");

/* a entrada aponta sempre para a cabeca de um bloco do arquivo principal */
typedef struct {
  int id;
  int offset;
} EntradaIndice;

typedef struct {
  int span;
  int total_principal;
  int quantidade_entradas;
  EntradaIndice *entradas;
} IndiceParcial;

typedef struct {
  FILE *dados;
  FILE *ext;
  int total;
  int total_ext;
  IndiceParcial ind;
} BaseProdutos;

typedef struct {
  FILE *dados;
  FILE *ext;
  int total;
  int total_ext;
  IndiceParcial ind;
} BaseAcessos;

/* resultado de percorrer a cadeia de um bloco atras de um id */
typedef struct {
  int bloco;
  Referencia anterior;
  Referencia atual;
  int comp_indice;
  int lidos;
} Percurso;

/* codigos de retorno da insercao */
enum {
  INS_ERRO = -1,
  INS_DUPLICADO = 0,
  INS_RESERVADO = 1,
  INS_NA_EXTENSAO = 2,
  INS_NA_CABECA = 3,
  INS_BASE_VAZIA = 4
};

/* ---------- produtos.c ---------- */

Referencia ref_principal_offset(int offset);
Referencia ref_principal(int pos);
Referencia ref_extensao(int pos);
Referencia ref_fim(void);
int ref_eh_fim(Referencia r);
void texto_ref(char *buf, size_t tam, Referencia r);

void preencher_texto(char *destino, int tam, const char *origem);

int ler_produto(FILE *f, int i, Produto *destino);
int ler_registro(BaseProdutos *b, Referencia ref, Produto *p);

void imprimir_em(Referencia ref, const Produto *p);
void imprimir_indice_produtos(const IndiceParcial *ind);

int abrir_base_produtos(BaseProdutos *b);
void fechar_base_produtos(BaseProdutos *b);

int buscar_produto(BaseProdutos *b, int id, int incluir_excluidos, Produto *p, Percurso *r);
int inserir_produto(BaseProdutos *b, Produto *novo, int *regenerou);
int remover_produto(BaseProdutos *b, int id);
int reorganizar_produtos(BaseProdutos *b);
int buscar_binaria_produto(BaseProdutos *b, int id, Produto *p, Referencia *ref, int *leituras);

int gravar_indice(const IndiceParcial *ind, const char *arq);
int ler_indice(const char *arq, int total, int span, IndiceParcial *ind);
int encontrar_bloco(const IndiceParcial *ind, int id, int *comparacoes);
FILE *abrir_ou_criar(const char *arq);
int substituir_arquivo(const char *origem, const char *destino);

/* ---------- acessos.c ---------- */

Referencia ref_acesso(int pos);
Referencia ref_acesso_ext(int pos);

int ler_acesso(FILE *f, int i, Acesso *destino);
int ler_registro_acesso(BaseAcessos *b, Referencia ref, Acesso *a);
int ler_acesso_n(BaseAcessos *b, int n, Acesso *a);

void imprimir_acesso_em(Referencia ref, const Acesso *a);
void imprimir_indice_acessos(const IndiceParcial *ind);

int abrir_base_acessos(BaseAcessos *b);
void fechar_base_acessos(BaseAcessos *b);

int buscar_acesso(BaseAcessos *b, int id, int incluir_excluidos, Acesso *a, Percurso *r);
int buscar_binaria_acesso(BaseAcessos *b, int id, Acesso *a, Referencia *ref, int *leituras);
int proximo_id_acesso(BaseAcessos *b);
int inserir_acesso(BaseAcessos *b, Acesso *novo, int *regenerou);
int remover_acesso(BaseAcessos *b, int id);
int reorganizar_acessos(BaseAcessos *b);

/* ---------- consultas.c ---------- */

int texto_igual(const char *campo, int tam, const char *s);
void marca_mais_vendida(BaseProdutos *bp, BaseAcessos *ba);
void conversao_produto(BaseProdutos *bp, BaseAcessos *ba, int id);
void faturamento_por_categoria(BaseProdutos *bp, BaseAcessos *ba);

#endif
