#!/usr/bin/env bash
# Testa o parser C nos 50 casos do pacote.

set -u
if [[ $# -ne 2 ]]; then
    echo "Uso: bash testar_parser_c.sh <diretorio-de-testes> <caminho-do-parser>" >&2
    exit 2
fi

TEST_DIR="$1"
PARSER_SOURCE="$2"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
PARSER_BINARY="$PROJECT_ROOT/.parser-c-test"
TMP_DIR="$(mktemp -d "$PROJECT_ROOT/.parser-c-XXXXXX")"
trap 'rm -rf "$TMP_DIR" "$PARSER_BINARY"' EXIT

[[ -d "$TEST_DIR" ]] || { echo "ERRO: diretório de testes não encontrado: $TEST_DIR" >&2; exit 2; }
[[ -f "$PARSER_SOURCE" ]] || { echo "ERRO: parser não encontrado: $PARSER_SOURCE" >&2; exit 2; }
LEXER_SOURCE="$(dirname "$PARSER_SOURCE")/lexer_v2.c"
[[ -f "$LEXER_SOURCE" ]] || { echo "ERRO: lexer não encontrado: $LEXER_SOURCE" >&2; exit 2; }
gcc -Wall -Wextra -std=c11 -DLEXER_SEM_MAIN "$PARSER_SOURCE" "$LEXER_SOURCE" -o "$PARSER_BINARY" || exit 2

normalize() {
    tr -d '[:space:]' < "$1"
}

mapfile -t CASES < <(find "$TEST_DIR/casos" -mindepth 1 -maxdepth 1 -type d -name '[0-9][0-9]_*' -print | sort)
TOTAL=${#CASES[@]}
[[ "$TOTAL" -eq 50 ]] || { echo "ERRO: esperados 50 casos, encontrados $TOTAL" >&2; exit 2; }

PASS=0; FAIL=0
for case_dir in "${CASES[@]}"; do
    name="$(basename "$case_dir")"
    number="${name%%_*}"
    source="$case_dir/codigo.c"
    ast_expected="$case_dir/ast.esperada.txt"
    result_expected="$case_dir/resultado.esperado.txt"
    out="$TMP_DIR/$number.out"
    err="$TMP_DIR/$number.err"

    [[ -f "$source" && -f "$ast_expected" && -f "$result_expected" ]] || {
        echo "[$name] arquivos esperados ausentes"; FAIL=$((FAIL + 1)); continue
    }
    set +e
    "$PARSER_BINARY" "$source" >"$out" 2>"$err"
    status=$?
    set -e

    if (( 10#$number <= 25 )); then
        if [[ "$status" -eq 0 ]] && normalize "$ast_expected" | cmp -s - <(normalize "$out"); then
            PASS=$((PASS + 1)); echo "[$name] OK"
        else
            FAIL=$((FAIL + 1)); echo "[$name] FALHOU (aceitação ou AST)"
        fi
    elif [[ "$status" -ne 0 ]]; then
        PASS=$((PASS + 1)); echo "[$name] OK"
    else
        FAIL=$((FAIL + 1)); echo "[$name] FALHOU (caso inválido aceito)"
    fi
done

echo "Resumo: $PASS OK / $FAIL falharam / $TOTAL total"
[[ "$FAIL" -eq 0 ]]
