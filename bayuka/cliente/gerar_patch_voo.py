import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from mpq import MpqFileEntry, build_mpq_v1, read_mpq_file

AREA_FLAG_OUTLAND = 0x00000400
AREA_FLAG_NO_FLY_ZONE = 0x20000000
CAMPO_MAPA = 1
CAMPO_FLAGS = 4
MAPAS_LIBERADOS = {0, 1}
CAMINHO_NO_MPQ = "DBFilesClient\\AreaTable.dbc"


def ler_dbc(dados):
    assinatura, registros, campos, tamanho, tamanho_textos = struct.unpack("<4s4I", dados[:20])
    if assinatura != b"WDBC" or len(dados) != 20 + registros * tamanho + tamanho_textos:
        raise ValueError("AreaTable.dbc invalida")
    return registros, campos, tamanho


def liberar_voo(dados):
    registros, _, tamanho = ler_dbc(dados)
    saida = bytearray(dados)
    alteradas = 0
    for i in range(registros):
        base = 20 + i * tamanho
        mapa = struct.unpack_from("<I", saida, base + CAMPO_MAPA * 4)[0]
        if mapa not in MAPAS_LIBERADOS:
            continue
        flags = struct.unpack_from("<I", saida, base + CAMPO_FLAGS * 4)[0]
        novas = (flags | AREA_FLAG_OUTLAND) & ~AREA_FLAG_NO_FLY_ZONE
        if novas != flags:
            struct.pack_into("<I", saida, base + CAMPO_FLAGS * 4, novas)
            alteradas += 1
    return bytes(saida), registros, alteradas


def main():
    origem = Path(sys.argv[1])
    destino = Path(sys.argv[2])
    original = origem.read_bytes()
    patch, registros, alteradas = liberar_voo(original)
    mpq = build_mpq_v1((MpqFileEntry(CAMINHO_NO_MPQ, patch),))
    destino.parent.mkdir(parents=True, exist_ok=True)
    destino.write_bytes(mpq)

    relido = read_mpq_file(mpq, CAMINHO_NO_MPQ)
    if relido != patch:
        raise SystemExit("MPQ gerado nao devolve a DBC esperada")
    print(f"{alteradas} de {registros} areas liberadas para voo; {destino} ({len(mpq)} bytes)")


if __name__ == "__main__":
    main()
