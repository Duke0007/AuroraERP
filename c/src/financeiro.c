/*
 * Aurora ERP - financeiro.c
 * Contas a pagar e a receber, baixa/liquidacao e fluxo de caixa.
 */
#include "erp.h"

TituloFinanceiro g_titulos[MAX_TITULOS];
int32_t g_num_titulos    = 0;
int32_t g_prox_id_titulo = 1;

int financeiro_incluir_titulo(TipoTitulo tipo, const char *descricao,
                              int32_t entidade_id, int32_t venda_id,
                              int32_t parcela, int32_t total_parcelas,
                              int64_t valor_centavos, int32_t data_emissao,
                              int32_t data_vencimento)
{
    TituloFinanceiro t;

    if (tipo != TITULO_RECEBER && tipo != TITULO_PAGAR) {
        return ERP_ERR_PARAM;
    }
    if (descricao == NULL || descricao[0] == '\0' || valor_centavos <= 0) {
        return ERP_ERR_PARAM;
    }
    if (parcela < 1 || total_parcelas < parcela) {
        return ERP_ERR_PARAM;
    }
    if (!data_valida(data_emissao) || !data_valida(data_vencimento)) {
        return ERP_ERR_PARAM;
    }
    if (g_num_titulos >= MAX_TITULOS) {
        return ERP_ERR_LIMITE;
    }

    memset(&t, 0, sizeof t);
    t.id   = g_prox_id_titulo;
    t.tipo = tipo;
    t.status = TITULO_ABERTO;
    util_copiar_string(t.descricao, sizeof t.descricao, descricao);
    t.entidade_id    = entidade_id;
    t.venda_id       = venda_id;
    t.parcela        = parcela;
    t.total_parcelas = total_parcelas;
    t.valor_centavos = valor_centavos;
    t.valor_pago_centavos = 0;
    t.data_emissao    = data_emissao;
    t.data_vencimento = data_vencimento;
    t.data_pagamento  = 0;

    g_titulos[g_num_titulos] = t;
    g_num_titulos++;
    g_prox_id_titulo++;
    return t.id;
}

TituloFinanceiro *financeiro_buscar_id(int32_t id)
{
    int32_t i;

    for (i = 0; i < g_num_titulos; i++) {
        if (g_titulos[i].id == id) {
            return &g_titulos[i];
        }
    }
    return NULL;
}

int64_t financeiro_saldo_titulo(const TituloFinanceiro *t)
{
    if (t == NULL || t->status != TITULO_ABERTO) {
        return 0;
    }
    return t->valor_centavos - t->valor_pago_centavos;
}

/*
 * Baixa total ou parcial. O titulo so passa a LIQUIDADO quando o valor
 * pago acumulado atinge o valor do titulo. Pagamento maior que o saldo e
 * rejeitado.
 */
int financeiro_baixar_titulo(int32_t titulo_id, int64_t valor_pago_centavos,
                             int32_t data_pagamento)
{
    TituloFinanceiro *t = financeiro_buscar_id(titulo_id);
    int64_t saldo;

    if (t == NULL) {
        return ERP_ERR_NAO_ENCONTRADO;
    }
    if (t->status != TITULO_ABERTO) {
        return ERP_ERR_ESTADO;
    }
    if (valor_pago_centavos <= 0 || !data_valida(data_pagamento)) {
        return ERP_ERR_PARAM;
    }
    saldo = financeiro_saldo_titulo(t);
    if (valor_pago_centavos > saldo) {
        return ERP_ERR_PARAM;
    }
    t->valor_pago_centavos += valor_pago_centavos;
    t->data_pagamento = data_pagamento;
    if (t->valor_pago_centavos == t->valor_centavos) {
        t->status = TITULO_LIQUIDADO;
    }
    return ERP_OK;
}

int64_t financeiro_total_aberto(TipoTitulo tipo)
{
    int64_t total = 0;
    int32_t i;

    for (i = 0; i < g_num_titulos; i++) {
        if (g_titulos[i].tipo == tipo) {
            total += financeiro_saldo_titulo(&g_titulos[i]);
        }
    }
    return total;
}

int64_t financeiro_saldo_devedor_cliente(int32_t cliente_id)
{
    int64_t total = 0;
    int32_t i;

    for (i = 0; i < g_num_titulos; i++) {
        if (g_titulos[i].tipo == TITULO_RECEBER &&
            g_titulos[i].entidade_id == cliente_id) {
            total += financeiro_saldo_titulo(&g_titulos[i]);
        }
    }
    return total;
}

static const char *texto_status_titulo(StatusTitulo s)
{
    switch (s) {
    case TITULO_ABERTO:    return "ABERTO";
    case TITULO_LIQUIDADO: return "LIQUIDADO";
    case TITULO_CANCELADO: return "CANCELADO";
    default:               return "?";
    }
}

void financeiro_listar(int tipo_filtro)
{
    const char *sep =
        "+-------+---------+--------------------------------+------------+---------------+---------------+------------+\n";
    int32_t i;
    int total = 0;
    char valor[TAM_MOEDA];
    char saldo[TAM_MOEDA];
    char venc[TAM_DATA];

    printf("\n%s", sep);
    printf("| %-5s | %-7s | %-30s | %-10s | %13s | %13s | %-10s |\n", "ID",
           "Tipo", "Descricao", "Vencimento", "Valor", "Saldo", "Status");
    printf("%s", sep);
    for (i = 0; i < g_num_titulos; i++) {
        const TituloFinanceiro *t = &g_titulos[i];

        if (tipo_filtro != 0 && (int)t->tipo != tipo_filtro) {
            continue;
        }
        data_formatar(t->data_vencimento, venc, sizeof venc);
        util_formatar_moeda(t->valor_centavos, valor, sizeof valor);
        util_formatar_moeda(financeiro_saldo_titulo(t), saldo, sizeof saldo);
        printf("| %5d | %-7s | %-30.30s | %-10s | %13s | %13s | %-10s |\n",
               (int)t->id, t->tipo == TITULO_RECEBER ? "RECEBER" : "PAGAR",
               t->descricao, venc, valor, saldo, texto_status_titulo(t->status));
        total++;
    }
    if (total == 0) {
        printf("| %-105s |\n", "Nenhum titulo encontrado");
    }
    printf("%s", sep);
}

/*
 * Relatorio consolidado de fluxo de caixa:
 *  - total a receber / a pagar (saldos em aberto) e saldo projetado;
 *  - valores ja realizados (recebido / pago);
 *  - aging dos saldos em aberto por faixa de vencimento.
 */
void financeiro_relatorio_fluxo_caixa(void)
{
    int64_t a_receber = 0;
    int64_t a_pagar = 0;
    int64_t recebido = 0;
    int64_t pago = 0;
    int64_t aging[2][4];
    int64_t hoje_dias = data_para_dias(data_hoje());
    int32_t i;
    int lado;
    char b1[TAM_MOEDA];
    char b2[TAM_MOEDA];
    char b3[TAM_MOEDA];
    char b4[TAM_MOEDA];
    char dh[TAM_DATA];

    memset(aging, 0, sizeof aging);
    for (i = 0; i < g_num_titulos; i++) {
        const TituloFinanceiro *t = &g_titulos[i];
        int64_t saldo = financeiro_saldo_titulo(t);
        int64_t atraso;
        int faixa;

        if (t->status == TITULO_CANCELADO) {
            continue;
        }
        lado = (t->tipo == TITULO_RECEBER) ? 0 : 1;
        if (lado == 0) {
            a_receber += saldo;
            recebido  += t->valor_pago_centavos;
        } else {
            a_pagar += saldo;
            pago    += t->valor_pago_centavos;
        }
        if (saldo > 0) {
            atraso = data_para_dias(t->data_vencimento) - hoje_dias;
            if (atraso < 0) {
                faixa = 0;
            } else if (atraso <= 30) {
                faixa = 1;
            } else if (atraso <= 60) {
                faixa = 2;
            } else {
                faixa = 3;
            }
            aging[lado][faixa] += saldo;
        }
    }

    data_formatar(data_hoje(), dh, sizeof dh);
    printf("\n=========== FLUXO DE CAIXA (posicao em %s) ===========\n", dh);
    util_formatar_moeda(a_receber, b1, sizeof b1);
    util_formatar_moeda(a_pagar, b2, sizeof b2);
    util_formatar_moeda(a_receber - a_pagar, b3, sizeof b3);
    printf("Total a receber (em aberto) : %20s\n", b1);
    printf("Total a pagar   (em aberto) : %20s\n", b2);
    printf("Saldo projetado             : %20s\n", b3);
    util_formatar_moeda(recebido, b1, sizeof b1);
    util_formatar_moeda(pago, b2, sizeof b2);
    util_formatar_moeda(recebido - pago, b3, sizeof b3);
    printf("-------------------------------------------------------\n");
    printf("Ja recebido                 : %20s\n", b1);
    printf("Ja pago                     : %20s\n", b2);
    printf("Saldo realizado             : %20s\n", b3);
    printf("-------------------------------------------------------\n");
    printf("%-12s %16s %16s %16s %16s\n", "Aging", "Vencido", "0-30 dias",
           "31-60 dias", "> 60 dias");
    for (lado = 0; lado < 2; lado++) {
        util_formatar_moeda(aging[lado][0], b1, sizeof b1);
        util_formatar_moeda(aging[lado][1], b2, sizeof b2);
        util_formatar_moeda(aging[lado][2], b3, sizeof b3);
        util_formatar_moeda(aging[lado][3], b4, sizeof b4);
        printf("%-12s %16s %16s %16s %16s\n", lado == 0 ? "A receber" : "A pagar",
               b1, b2, b3, b4);
    }
    printf("=======================================================\n");
}

/*
 * Gera linha digitavel e codigo de barras de um titulo a receber.
 * Campo livre (layout generico academico, 25 digitos):
 *   agencia(4) + conta(7) + carteira(3) + nosso numero(11)
 */
int financeiro_gerar_boleto(int32_t titulo_id, char *linha, size_t tam_linha,
                            char *barras, size_t tam_barras)
{
    TituloFinanceiro *t = financeiro_buscar_id(titulo_id);
    char campo_livre[32];

    if (t == NULL) {
        return ERP_ERR_NAO_ENCONTRADO;
    }
    if (t->tipo != TITULO_RECEBER || t->status != TITULO_ABERTO) {
        return ERP_ERR_ESTADO;
    }
    snprintf(campo_livre, sizeof campo_livre, "%04d%07d%03d%011d", 1234, 456789,
             109, (int)(t->id % 100000000));
    if (strlen(campo_livre) != 25) {
        return ERP_ERR_PARAM;
    }
    return fiscal_gerar_linha_digitavel("341", '9', campo_livre,
                                        t->data_vencimento,
                                        financeiro_saldo_titulo(t), linha,
                                        tam_linha, barras, tam_barras);
}

int financeiro_emitir_boleto(int32_t titulo_id)
{
    TituloFinanceiro *t = financeiro_buscar_id(titulo_id);
    char linha[TAM_LINHA_DIG];
    char barras[TAM_CODIGO_BARRAS];
    char valor[TAM_MOEDA];
    char venc[TAM_DATA];
    int rc;

    rc = financeiro_gerar_boleto(titulo_id, linha, sizeof linha, barras,
                                 sizeof barras);
    if (rc != ERP_OK) {
        return rc;
    }
    util_formatar_moeda(financeiro_saldo_titulo(t), valor, sizeof valor);
    data_formatar(t->data_vencimento, venc, sizeof venc);
    printf("\n--- BOLETO - titulo %d (%s) ---\n", (int)t->id, t->descricao);
    printf("Vencimento     : %s\n", venc);
    printf("Valor          : %s\n", valor);
    printf("Linha digitavel: %s\n", linha);
    printf("Codigo de barra: %s\n", barras);
    return ERP_OK;
}
