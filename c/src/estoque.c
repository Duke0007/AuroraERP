/*
 * Aurora ERP - estoque.c
 * Custo Medio Ponderado (CMP) e movimentacao de estoque.
 *
 *   CMP = ((Saldo_Anterior * CMP_Anterior) + (Qtd_Entrada * Valor_Entrada))
 *         / Saldo_Total
 *
 * Todos os calculos em int64_t (centavos), com arredondamento meio-para-cima
 * e protecao contra estouro.
 */
#include "erp.h"

int64_t estoque_calcular_cmp(int64_t saldo_anterior, int64_t cmp_anterior,
                             int64_t qtd_entrada, int64_t valor_entrada_unit)
{
    int64_t saldo_total;
    int64_t parte_estoque;
    int64_t parte_entrada;
    int64_t numerador;

    if (saldo_anterior < 0 || cmp_anterior < 0 || qtd_entrada <= 0 ||
        valor_entrada_unit < 0) {
        return -1;
    }
    if (!util_soma_segura(saldo_anterior, qtd_entrada, &saldo_total)) {
        return -1;
    }
    if (!util_mul_seguro(saldo_anterior, cmp_anterior, &parte_estoque)) {
        return -1;
    }
    if (!util_mul_seguro(qtd_entrada, valor_entrada_unit, &parte_entrada)) {
        return -1;
    }
    if (!util_soma_segura(parte_estoque, parte_entrada, &numerador)) {
        return -1;
    }
    return util_dividir_arredondado(numerador, saldo_total);
}

int estoque_registrar_entrada(int32_t produto_id, int64_t qtd,
                              int64_t valor_unit_centavos)
{
    Produto *p = cadastros_buscar_produto_id(produto_id);
    int64_t novo_cmp;
    int64_t novo_saldo;

    if (p == NULL) {
        return ERP_ERR_NAO_ENCONTRADO;
    }
    if (!p->ativo) {
        return ERP_ERR_ESTADO;
    }
    if (qtd <= 0 || valor_unit_centavos < 0) {
        return ERP_ERR_PARAM;
    }
    novo_cmp = estoque_calcular_cmp(p->saldo_qtd, p->cmp_centavos, qtd,
                                    valor_unit_centavos);
    if (novo_cmp < 0) {
        return ERP_ERR_OVERFLOW;
    }
    if (!util_soma_segura(p->saldo_qtd, qtd, &novo_saldo)) {
        return ERP_ERR_OVERFLOW;
    }
    /* Atualizacao conjunta: so grava depois de tudo validado */
    p->cmp_centavos = novo_cmp;
    p->saldo_qtd    = novo_saldo;
    return ERP_OK;
}

int estoque_dar_baixa(int32_t produto_id, int64_t qtd)
{
    Produto *p = cadastros_buscar_produto_id(produto_id);
    int64_t novo_saldo;

    if (p == NULL) {
        return ERP_ERR_NAO_ENCONTRADO;
    }
    if (!p->ativo) {
        return ERP_ERR_ESTADO;
    }
    if (qtd <= 0) {
        return ERP_ERR_PARAM;
    }
    if (qtd > p->saldo_qtd) {
        return ERP_ERR_SALDO;
    }
    novo_saldo = p->saldo_qtd - qtd;
    /* A saida nao altera o CMP (regra contabil do custo medio) */
    p->saldo_qtd = novo_saldo;
    return ERP_OK;
}

void estoque_relatorio(void)
{
    const char *sep =
        "+-------+------------+------------------------------+----------+---------------+------------------+\n";
    int32_t i;
    int64_t total = 0;
    char cmp[TAM_MOEDA];
    char valor[TAM_MOEDA];

    printf("\n%s", sep);
    printf("| %-5s | %-10s | %-28s | %8s | %13s | %16s |\n",
           "ID", "Codigo", "Descricao", "Saldo", "CMP", "Valor em estoque");
    printf("%s", sep);
    for (i = 0; i < g_num_produtos; i++) {
        const Produto *p = &g_produtos[i];
        int64_t v = 0;

        if (!util_mul_seguro(p->saldo_qtd, p->cmp_centavos, &v)) {
            v = 0;
        }
        (void)util_soma_segura(total, v, &total);
        util_formatar_moeda(p->cmp_centavos, cmp, sizeof cmp);
        util_formatar_moeda(v, valor, sizeof valor);
        printf("| %5d | %-10.10s | %-28.28s | %8lld | %13s | %16s |\n",
               (int)p->id, p->codigo, p->descricao, (long long)p->saldo_qtd,
               cmp, valor);
    }
    printf("%s", sep);
    util_formatar_moeda(total, valor, sizeof valor);
    printf("Valor total do estoque (a custo medio): %s\n", valor);
}
