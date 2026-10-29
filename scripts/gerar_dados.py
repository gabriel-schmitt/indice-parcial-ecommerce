#!/usr/bin/env python3
"""Gera produtos.bin e acessos.bin a partir do CSV de eventos.

Uso:
    python3 scripts/gerar_dados.py                        # usa o CSV padrao
    python3 scripts/gerar_dados.py dados/outro.csv        # CSV alternativo

Com uv (instala pandas automaticamente):
    uv run --with pandas python3 scripts/gerar_dados.py
"""

import argparse
import struct
import sys
from pathlib import Path

import pandas as pd

SPAN_PRODUTOS = 51
SPAN_ACESSOS = 73
TAM_MARCA = 20
TAM_CATEGORIA = 42
TAM_TIPO = 11
TAM_HORA = 19

AREA_PRINCIPAL = 0
AREA_NENHUMA = 2

PRODUTO_FMT = "=iiiic20s42sc"
PRODUTO_TAM = struct.calcsize(PRODUTO_FMT)
assert PRODUTO_TAM == 80, f"Produto deveria ter 80 bytes, tem {PRODUTO_TAM}"

ACESSO_FMT = "=iiiiiic11s19sc"
ACESSO_TAM = struct.calcsize(ACESSO_FMT)
assert ACESSO_TAM == 56, f"Acesso deveria ter 56 bytes, tem {ACESSO_TAM}"


def pad(texto, tam):
    if pd.isna(texto):
        return b" " * tam
    b = str(texto).encode("latin-1", errors="replace")[:tam]
    return b.ljust(tam, b" ")


def preco_centavos(valor):
    if pd.isna(valor):
        return 0
    return int(float(valor) * 100.0 + 0.5)


def gerar_produtos(df, caminho):
    produtos = {}

    for row in df.itertuples(index=False):
        pid = int(row.product_id)
        preco = preco_centavos(row.price)
        marca = "" if pd.isna(row.brand) else str(row.brand)
        cat = "" if pd.isna(row.category_code) else str(row.category_code)

        if pid not in produtos:
            produtos[pid] = {"preco": preco, "marca": marca, "categoria": cat}
        else:
            produtos[pid]["preco"] = preco
            if marca:
                produtos[pid]["marca"] = marca

    ids_ordenados = sorted(produtos.keys())
    total = len(ids_ordenados)

    with open(caminho, "wb") as f:
        for i, pid in enumerate(ids_ordenados):
            p = produtos[pid]

            fim_bloco = (i // SPAN_PRODUTOS + 1) * SPAN_PRODUTOS
            if fim_bloco > total:
                fim_bloco = total
            if i + 1 < fim_bloco:
                elo_area, elo_offset = AREA_PRINCIPAL, (i + 1) * PRODUTO_TAM
            else:
                elo_area, elo_offset = AREA_NENHUMA, -1

            f.write(
                struct.pack(
                    PRODUTO_FMT,
                    pid,
                    p["preco"],
                    elo_area,
                    elo_offset,
                    b"1",
                    pad(p["marca"], TAM_MARCA),
                    pad(p["categoria"], TAM_CATEGORIA),
                    b"\n",
                )
            )

    print(f"  {caminho}: {total} produtos ({PRODUTO_TAM} bytes cada)")
    return total


def elo(i, total, span, tam):
    fim_bloco = min((i // span + 1) * span, total)
    if i + 1 < fim_bloco:
        return AREA_PRINCIPAL, (i + 1) * tam
    return AREA_NENHUMA, -1


def gerar_acessos(df, caminho):
    """Um registro por evento, com id sequencial na ordem do CSV (ja ordenado pela chave)."""
    total = len(df)

    with open(caminho, "wb") as f:
        for i, row in enumerate(df.itertuples(index=False)):
            elo_area, elo_offset = elo(i, total, SPAN_ACESSOS, ACESSO_TAM)
            f.write(
                struct.pack(
                    ACESSO_FMT,
                    i + 1,
                    int(row.product_id),
                    int(row.user_id),
                    preco_centavos(row.price),
                    elo_area,
                    elo_offset,
                    b"1",
                    pad(row.event_type, TAM_TIPO),
                    pad(str(row.event_time)[:TAM_HORA], TAM_HORA),
                    b"\n",
                )
            )

    print(f"  {caminho}: {total} acessos ({ACESSO_TAM} bytes cada)")
    return total


def main():
    parser = argparse.ArgumentParser(description="Gera produtos.bin a partir do CSV")
    parser.add_argument("csv", nargs="?", default="dados/2019-Oct-200k.csv")
    args = parser.parse_args()

    csv_path = Path(args.csv)
    if not csv_path.exists():
        print(f"erro: {csv_path} nao encontrado", file=sys.stderr)
        sys.exit(1)

    print(f"lendo {csv_path} ...")
    df = pd.read_csv(csv_path, dtype={"product_id": int, "price": float})
    print(f"  {len(df)} linhas lidas")

    gerar_produtos(df, "produtos.bin")
    gerar_acessos(df, "acessos.bin")
    print("pronto (o programa C recria os .idx e os .ovf ao abrir)")


if __name__ == "__main__":
    main()
