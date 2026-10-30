/*
 * Aurora ERP - fiscal.c
 * Algoritmos fiscais exatos:
 *  - Chave de acesso da NF-e (44 digitos) com DV Modulo 11 da SEFAZ.
 *  - Linha digitavel e codigo de barras de boleto (FEBRABAN):
 *    Modulo 10 nos campos da linha digitavel e Modulo 11 no codigo de barras.
 *  - Validacao de CPF/CNPJ e utilitarios de data (AAAAMMDD).
 */
#include "erp.h"

/* ------------------------------------------------------------------ */
/* Digitos / documentos                                                */
/* ------------------------------------------------------------------ */
int fiscal_somente_digitos(const char *origem, char *dest, size_t tam)
{
    size_t n = 0;
    size_t i;

    if (origem == NULL || dest == NULL || tam == 0) {
        return -1;
    }
    for (i = 0; origem[i] != '\0'; i++) {
        if (origem[i] >= '0' && origem[i] <= '9') {
            if (n + 1 >= tam) {
                dest[0] = '\0';
                return -1;
            }
            dest[n++] = origem[i];
        }
    }
    dest[n] = '\0';
    return (int)n;
}

static bool todos_digitos_iguais(const char *s, size_t n)
{
    size_t i;

    for (i = 1; i < n; i++) {
        if (s[i] != s[0]) {
            return false;
        }
    }
    return true;
}

bool fiscal_validar_cpf(const char *cpf)
{
    int i;
    int soma;
    int dv1;
    int dv2;

    if (cpf == NULL || strlen(cpf) != 11) {
        return false;
    }
    for (i = 0; i < 11; i++) {
        if (cpf[i] < '0' || cpf[i] > '9') {
            return false;
        }
    }
    if (todos_digitos_iguais(cpf, 11)) {
        return false;
    }
    soma = 0;
    for (i = 0; i < 9; i++) {
        soma += (cpf[i] - '0') * (10 - i);
    }
    dv1 = 11 - (soma % 11);
    if (dv1 >= 10) {
        dv1 = 0;
    }
    soma = 0;
    for (i = 0; i < 10; i++) {
        soma += (cpf[i] - '0') * (11 - i);
    }
    dv2 = 11 - (soma % 11);
    if (dv2 >= 10) {
        dv2 = 0;
    }
    return (cpf[9] - '0') == dv1 && (cpf[10] - '0') == dv2;
}

bool fiscal_validar_cnpj(const char *cnpj)
{
    static const int p1[12] = {5, 4, 3, 2, 9, 8, 7, 6, 5, 4, 3, 2};
    static const int p2[13] = {6, 5, 4, 3, 2, 9, 8, 7, 6, 5, 4, 3, 2};
    int i;
    int soma;
    int resto;
    int dv1;
    int dv2;

    if (cnpj == NULL || strlen(cnpj) != 14) {
        return false;
    }
    for (i = 0; i < 14; i++) {
        if (cnpj[i] < '0' || cnpj[i] > '9') {
            return false;
        }
    }
    if (todos_digitos_iguais(cnpj, 14)) {
        return false;
    }
    soma = 0;
    for (i = 0; i < 12; i++) {
        soma += (cnpj[i] - '0') * p1[i];
    }
    resto = soma % 11;
    dv1 = (resto < 2) ? 0 : 11 - resto;
    soma = 0;
    for (i = 0; i < 13; i++) {
        int d = (i < 12) ? (cnpj[i] - '0') : dv1;
        soma += d * p2[i];
    }
    resto = soma % 11;
    dv2 = (resto < 2) ? 0 : 11 - resto;
    return (cnpj[12] - '0') == dv1 && (cnpj[13] - '0') == dv2;
}

/* ------------------------------------------------------------------ */
/* Modulo 11 SEFAZ (chave NF-e)                                        */
/* ------------------------------------------------------------------ */
/*
 * Pesos de 2 a 9, aplicados da direita para a esquerda, reiniciando em 2
 * apos o 9. DV = 11 - (soma % 11); se o resultado for 10 ou 11 (resto 1 ou
 * 0), o DV e 0. Retorna -1 se a entrada for vazia ou nao numerica.
 */
int fiscal_calcular_dv_mod11_sefaz(const char *digitos)
{
    size_t n;
    size_t i;
    int soma = 0;
    int peso = 2;
    int dv;

    if (digitos == NULL || digitos[0] == '\0') {
        return -1;
    }
    n = strlen(digitos);
    for (i = n; i > 0; i--) {
        char c = digitos[i - 1];

        if (c < '0' || c > '9') {
            return -1;
        }
        soma += (c - '0') * peso;
        peso++;
        if (peso > 9) {
            peso = 2;
        }
    }
    dv = 11 - (soma % 11);
    if (dv >= 10) {
        dv = 0;
    }
    return dv;
}

/*
 * Estrutura da chave (44 digitos):
 *  cUF(2) AAMM(4) CNPJ(14) mod(2) serie(3) nNF(9) tpEmis(1) cNF(8) cDV(1)
 */
int fiscal_gerar_chave_nfe(int uf, int ano, int mes, const char *cnpj,
                           int modelo, int serie, int32_t numero,
                           int tipo_emissao, int32_t codigo_numerico,
                           char *out_chave, size_t tam)
{
    char base[64];
    char cnpj_dig[TAM_DOC];
    int n;
    int dv;

    if (out_chave == NULL || tam < TAM_CHAVE_NFE || cnpj == NULL) {
        return ERP_ERR_PARAM;
    }
    if (uf < 11 || uf > 99 || mes < 1 || mes > 12 || ano < 0) {
        return ERP_ERR_PARAM;
    }
    if (modelo < 0 || modelo > 99 || serie < 0 || serie > 999) {
        return ERP_ERR_PARAM;
    }
    if (numero < 1 || numero > 999999999) {
        return ERP_ERR_PARAM;
    }
    if (tipo_emissao < 1 || tipo_emissao > 9) {
        return ERP_ERR_PARAM;
    }
    if (codigo_numerico < 0 || codigo_numerico > 99999999) {
        return ERP_ERR_PARAM;
    }
    if (fiscal_somente_digitos(cnpj, cnpj_dig, sizeof cnpj_dig) != 14) {
        return ERP_ERR_DOCUMENTO;
    }

    n = snprintf(base, sizeof base, "%02d%02d%02d%s%02d%03d%09d%d%08d",
                 uf, ano % 100, mes, cnpj_dig, modelo, serie, (int)numero,
                 tipo_emissao, (int)codigo_numerico);
    if (n != 43) {
        return ERP_ERR_PARAM;
    }
    dv = fiscal_calcular_dv_mod11_sefaz(base);
    if (dv < 0) {
        return ERP_ERR_PARAM;
    }
    snprintf(out_chave, tam, "%s%d", base, dv);
    return ERP_OK;
}

bool fiscal_validar_chave_nfe(const char *chave)
{
    char base[44];
    size_t i;
    int dv;

    if (chave == NULL || strlen(chave) != 44) {
        return false;
    }
    for (i = 0; i < 44; i++) {
        if (chave[i] < '0' || chave[i] > '9') {
            return false;
        }
    }
    memcpy(base, chave, 43);
    base[43] = '\0';
    dv = fiscal_calcular_dv_mod11_sefaz(base);
    return dv == (chave[43] - '0');
}

/* ------------------------------------------------------------------ */
/* Boleto bancario (FEBRABAN)                                          */
/* ------------------------------------------------------------------ */
/*
 * Modulo 10: pesos 2,1,2,1... da direita para a esquerda; cada produto de
 * dois algarismos tem os algarismos somados (ex.: 14 -> 1+4).
 * DV = (10 - soma % 10) % 10.
 */
int fiscal_calcular_dv_mod10_febraban(const char *digitos)
{
    size_t n;
    size_t i;
    int soma = 0;
    int peso = 2;
    int resto;

    if (digitos == NULL || digitos[0] == '\0') {
        return -1;
    }
    n = strlen(digitos);
    for (i = n; i > 0; i--) {
        char c = digitos[i - 1];
        int prod;

        if (c < '0' || c > '9') {
            return -1;
        }
        prod = (c - '0') * peso;
        soma += prod / 10 + prod % 10;
        peso = (peso == 2) ? 1 : 2;
    }
    resto = soma % 10;
    return (resto == 0) ? 0 : 10 - resto;
}

/*
 * Modulo 11 do codigo de barras: pesos 2 a 9 da direita para a esquerda.
 * DV = 11 - (soma % 11); se for 0, 10 ou 11, o DV e 1.
 */
int fiscal_calcular_dv_mod11_boleto(const char *digitos)
{
    size_t n;
    size_t i;
    int soma = 0;
    int peso = 2;
    int dv;

    if (digitos == NULL || digitos[0] == '\0') {
        return -1;
    }
    n = strlen(digitos);
    for (i = n; i > 0; i--) {
        char c = digitos[i - 1];

        if (c < '0' || c > '9') {
            return -1;
        }
        soma += (c - '0') * peso;
        peso++;
        if (peso > 9) {
            peso = 2;
        }
    }
    dv = 11 - (soma % 11);
    if (dv == 0 || dv == 10 || dv == 11) {
        dv = 1;
    }
    return dv;
}

/*
 * Fator de vencimento:
 *  - Ciclo original: dias corridos desde 07/10/1997 (ate 21/02/2025 = 9999).
 *  - A partir de 22/02/2025 o fator recomeca em 1000 e segue ate 9999,
 *    reiniciando em 1000 novamente (ciclos de 9000 dias).
 */
int fiscal_calcular_fator_vencimento(int32_t data_vencimento)
{
    int64_t base_antiga;
    int64_t base_nova;
    int64_t d;

    if (!data_valida(data_vencimento)) {
        return -1;
    }
    base_antiga = data_para_dias(19971007);
    base_nova   = data_para_dias(20250222);
    d = data_para_dias(data_vencimento);
    if (d < base_antiga) {
        return -1;
    }
    if (d < base_nova) {
        return (int)(d - base_antiga);
    }
    return (int)(1000 + ((d - base_nova) % 9000));
}

/*
 * Codigo de barras (44): banco(3) moeda(1) DV(1) fator(4) valor(10)
 *                        campo_livre(25)
 * Linha digitavel (47):
 *   AAABC.CCCCD DDDDD.DDDDDE FFFFF.FFFFFG H UUUUVVVVVVVVVV
 *   campo 1 = banco+moeda+5 primeiros do campo livre + DV (Mod 10)
 *   campo 2 = 10 digitos seguintes do campo livre + DV (Mod 10)
 *   campo 3 = 10 ultimos digitos do campo livre + DV (Mod 10)
 *   H       = DV geral do codigo de barras (Mod 11)
 *   U..V    = fator de vencimento (4) + valor (10)
 */
int fiscal_gerar_linha_digitavel(const char *banco, char moeda,
                                 const char *campo_livre,
                                 int32_t data_vencimento,
                                 int64_t valor_centavos,
                                 char *out_linha, size_t tam_linha,
                                 char *out_barras, size_t tam_barras)
{
    char corpo[64];
    char barras[TAM_CODIGO_BARRAS];
    char dig[48];
    char tmp[16];
    int fator;
    int dv_geral;
    int dv1;
    int dv2;
    int dv3;
    size_t i;
    int n;

    if (banco == NULL || campo_livre == NULL || out_linha == NULL ||
        tam_linha < 55) {
        return ERP_ERR_PARAM;
    }
    if (strlen(banco) != 3 || strlen(campo_livre) != 25 ||
        moeda < '0' || moeda > '9') {
        return ERP_ERR_PARAM;
    }
    for (i = 0; i < 3; i++) {
        if (banco[i] < '0' || banco[i] > '9') {
            return ERP_ERR_PARAM;
        }
    }
    for (i = 0; i < 25; i++) {
        if (campo_livre[i] < '0' || campo_livre[i] > '9') {
            return ERP_ERR_PARAM;
        }
    }
    if (valor_centavos < 0 || valor_centavos > 9999999999LL) {
        return ERP_ERR_PARAM;
    }
    fator = fiscal_calcular_fator_vencimento(data_vencimento);
    if (fator < 0) {
        return ERP_ERR_PARAM;
    }

    /* 43 digitos sem o DV geral */
    n = snprintf(corpo, sizeof corpo, "%s%c%04d%010lld%s", banco, moeda, fator,
                 (long long)valor_centavos, campo_livre);
    if (n != 43) {
        return ERP_ERR_PARAM;
    }
    dv_geral = fiscal_calcular_dv_mod11_boleto(corpo);
    if (dv_geral < 0) {
        return ERP_ERR_PARAM;
    }
    memcpy(barras, corpo, 4);
    barras[4] = (char)('0' + dv_geral);
    memcpy(barras + 5, corpo + 4, 39);
    barras[44] = '\0';

    /* Campo 1: banco + moeda + livre[0..4] */
    memcpy(tmp, corpo, 4);
    memcpy(tmp + 4, campo_livre, 5);
    tmp[9] = '\0';
    dv1 = fiscal_calcular_dv_mod10_febraban(tmp);
    memcpy(dig, tmp, 9);
    dig[9] = (char)('0' + dv1);

    /* Campo 2: livre[5..14] */
    memcpy(tmp, campo_livre + 5, 10);
    tmp[10] = '\0';
    dv2 = fiscal_calcular_dv_mod10_febraban(tmp);
    memcpy(dig + 10, tmp, 10);
    dig[20] = (char)('0' + dv2);

    /* Campo 3: livre[15..24] */
    memcpy(tmp, campo_livre + 15, 10);
    tmp[10] = '\0';
    dv3 = fiscal_calcular_dv_mod10_febraban(tmp);
    memcpy(dig + 21, tmp, 10);
    dig[31] = (char)('0' + dv3);

    /* DV geral + fator + valor */
    dig[32] = (char)('0' + dv_geral);
    memcpy(dig + 33, corpo + 4, 14);
    dig[47] = '\0';

    snprintf(out_linha, tam_linha, "%.5s.%.5s %.5s.%.6s %.5s.%.6s %c %.14s",
             dig, dig + 5, dig + 10, dig + 15, dig + 21, dig + 26, dig[32],
             dig + 33);

    if (out_barras != NULL) {
        if (tam_barras < TAM_CODIGO_BARRAS) {
            return ERP_ERR_PARAM;
        }
        memcpy(out_barras, barras, TAM_CODIGO_BARRAS);
    }
    return ERP_OK;
}

/* Reconstroi o codigo de barras (44) a partir da linha digitavel (47). */
int fiscal_linha_para_barras(const char *linha, char *out_barras, size_t tam)
{
    char dig[64];
    int n;

    if (linha == NULL || out_barras == NULL || tam < TAM_CODIGO_BARRAS) {
        return ERP_ERR_PARAM;
    }
    n = fiscal_somente_digitos(linha, dig, sizeof dig);
    if (n != 47) {
        return ERP_ERR_PARAM;
    }
    memcpy(out_barras, dig, 4);          /* banco + moeda */
    out_barras[4] = dig[32];             /* DV geral */
    memcpy(out_barras + 5, dig + 33, 14);/* fator + valor */
    memcpy(out_barras + 19, dig + 4, 5); /* livre[0..4] */
    memcpy(out_barras + 24, dig + 10, 10);/* livre[5..14] */
    memcpy(out_barras + 34, dig + 21, 10);/* livre[15..24] */
    out_barras[44] = '\0';
    return ERP_OK;
}

bool fiscal_validar_linha_digitavel(const char *linha)
{
    char dig[64];
    char barras[TAM_CODIGO_BARRAS];
    char campo[16];
    char corpo[48];
    int dv;

    if (fiscal_somente_digitos(linha, dig, sizeof dig) != 47) {
        return false;
    }
    memcpy(campo, dig, 9);
    campo[9] = '\0';
    if (fiscal_calcular_dv_mod10_febraban(campo) != dig[9] - '0') {
        return false;
    }
    memcpy(campo, dig + 10, 10);
    campo[10] = '\0';
    if (fiscal_calcular_dv_mod10_febraban(campo) != dig[20] - '0') {
        return false;
    }
    memcpy(campo, dig + 21, 10);
    campo[10] = '\0';
    if (fiscal_calcular_dv_mod10_febraban(campo) != dig[31] - '0') {
        return false;
    }
    if (fiscal_linha_para_barras(linha, barras, sizeof barras) != ERP_OK) {
        return false;
    }
    memcpy(corpo, barras, 4);
    memcpy(corpo + 4, barras + 5, 39);
    corpo[43] = '\0';
    dv = fiscal_calcular_dv_mod11_boleto(corpo);
    return dv == (barras[4] - '0');
}

/* ------------------------------------------------------------------ */
/* Datas (AAAAMMDD) - algoritmo de dias desde 1970-01-01               */
/* ------------------------------------------------------------------ */
int32_t data_hoje(void)
{
    time_t agora = time(NULL);
    struct tm *tmv = localtime(&agora);

    if (tmv == NULL) {
        return 20250101;
    }
    return (int32_t)((tmv->tm_year + 1900) * 10000 + (tmv->tm_mon + 1) * 100 +
                     tmv->tm_mday);
}

bool data_valida(int32_t data)
{
    static const int dias_mes[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int ano = (int)(data / 10000);
    int mes = (int)((data / 100) % 100);
    int dia = (int)(data % 100);
    int max;

    if (data <= 0 || ano < 1900 || ano > 9999 || mes < 1 || mes > 12 || dia < 1) {
        return false;
    }
    max = dias_mes[mes - 1];
    if (mes == 2 && ((ano % 4 == 0 && ano % 100 != 0) || ano % 400 == 0)) {
        max = 29;
    }
    return dia <= max;
}

int64_t data_para_dias(int32_t data)
{
    int64_t y = data / 10000;
    int64_t m = (data / 100) % 100;
    int64_t d = data % 100;
    int64_t era;
    int64_t yoe;
    int64_t doy;
    int64_t doe;

    if (m <= 2) {
        y -= 1;
    }
    era = (y >= 0 ? y : y - 399) / 400;
    yoe = y - era * 400;
    doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

int32_t dias_para_data(int64_t dias)
{
    int64_t z = dias + 719468;
    int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    int64_t doe = z - era * 146097;
    int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    int64_t y = yoe + era * 400;
    int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    int64_t mp = (5 * doy + 2) / 153;
    int64_t d = doy - (153 * mp + 2) / 5 + 1;
    int64_t m = (mp < 10) ? mp + 3 : mp - 9;

    if (m <= 2) {
        y += 1;
    }
    return (int32_t)(y * 10000 + m * 100 + d);
}

int32_t data_somar_dias(int32_t data, int dias)
{
    return dias_para_data(data_para_dias(data) + dias);
}

void data_formatar(int32_t data, char *out, size_t tam)
{
    if (data <= 0) {
        snprintf(out, tam, "--/--/----");
        return;
    }
    snprintf(out, tam, "%02d/%02d/%04d", (int)(data % 100),
             (int)((data / 100) % 100), (int)(data / 10000));
}

/* Aceita "dd/mm/aaaa" (ou 8 digitos). Texto vazio devolve 0 (sem data). */
bool data_parse_br(const char *txt, int32_t *out)
{
    char dig[16];
    int64_t v = 0;
    int64_t acc = 0;
    size_t i;
    int n;

    if (txt == NULL || out == NULL) {
        return false;
    }
    n = fiscal_somente_digitos(txt, dig, sizeof dig);
    if (n == 0) {
        *out = 0;
        return true;
    }
    if (n != 8) {
        return false;
    }
    for (i = 0; i < 8; i++) {
        acc = acc * 10 + (dig[i] - '0');
    }
    v = acc;
    *out = (int32_t)((v % 10000) * 10000 + ((v / 10000) % 100) * 100 + v / 1000000);
    return data_valida(*out);
}
