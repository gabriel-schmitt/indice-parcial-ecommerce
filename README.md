# Índice parcial — produtos de e-commerce

Arquivo binário de registros de tamanho fixo com índice esparso e cadeias
encadeadas. Os registros do arquivo principal são divididos em blocos de 51
posições; cada bloco forma uma cadeia ligada por elos explícitos, e inserções
vão para uma área de extensão linkada na cadeia correspondente.

## Dados

Recorte dos primeiros 200 mil eventos (`dados/2019-Oct-200k.csv`) do dataset
[eCommerce behavior data](https://www.kaggle.com/datasets/mkechinov/ecommerce-behavior-data-from-multi-category-store)
(Kaggle).

O arquivo `produtos.bin` contém um registro por produto (chave `product_id`,
80 bytes por registro). Os textos são completados com espaços e cada registro
termina em `'\n'`.

## Gerar dados iniciais

```
uv run --with pandas python3 scripts/gerar_dados.py
```

O programa C recria o índice (`.idx`) e a extensão (`.ovf`) automaticamente.

## Compilar e rodar

```
make
./ecommerce
```

## Operações

1. Listar faixa de produtos do arquivo principal
2. Listar área de extensão
3. Buscar produto pelo índice esparso
4. Imprimir índice esparso
5. Imprimir cadeia de um bloco
6. Inserir produto
7. Remover produto (marcação lógica)
8. Regenerar (percorre cadeias, descarta removidos, refaz elos e índice)
