/*
 * Aurora ERP - main.c
 * Menu interativo no terminal e testes automatizados de validacao.
 * Uso: ./aurora_erp            (modo interativo)
 *      ./aurora_erp --testes   (executa os testes e retorna 0 se todos passarem)
 */
#include "erp.h"

/* ------------------------------------------------------------------ */
/* Auxiliares de interface                                             */
/* ------------------------------------------------------------------ */
static void mostrar_resultado(int rc)
{
    if (rc >= 0) {
        printf(">> OK (%s)\n", erp_mensagem_erro(ERP_OK));
    } else {
        printf(">> ERRO %d: %s\n", rc, erp_mensagem_erro(rc));
    }
}

static bool ler_opcao(const char *prompt, int *opcao)
{
    int64_t v;

    if (!util_ler_int64(prompt, &v)) {
        return false;
    }
    if (v < -1 || v > 999) {
        return false;
    }
    *opcao = (int)v;
    return true;
}

static bool ler_data_br(const char *prompt, int32_t *out)
{
    char buf[32];

    if (!util_ler_linha(prompt, buf, sizeof buf)) {
        return false;
    }
    return data_parse_br(buf, out);
}

/* ------------------------------------------------------------------ */
/* Cadastros                                                           */
/* ------------------------------------------------------------------ */
static void tela_cadastrar_cliente(void)
{
    Cliente c;
    int64_t v;

    memset(&c, 0, sizeof c);
    if (!util_ler_int64("Tipo (1=Pessoa Fisica, 2=Pessoa Juridica): ", &v) ||
        (v != 1 && v != 2)) {
        printf("Tipo invalido.\n");
        return;
    }
    c.tipo = (TipoPessoa)v;
    util_ler_linha("Nome: ", c.nome, sizeof c.nome);
    util_ler_linha("CPF/CNPJ: ", c.documento, sizeof c.documento);
    util_ler_linha("E-mail: ", c.email, sizeof c.email);
    util_ler_linha("Cidade: ", c.cidade, sizeof c.cidade);
    util_ler_linha("UF: ", c.uf, sizeof c.uf);
    if (util_ler_centavos("Limite de credito (R$, 0 = sem limite): ", &v)) {
        c.limite_credito_centavos = v;
    } else {
        printf("Limite invalido.\n");
        return;
    }
    {
        int rc = cadastros_inserir_cliente(&c);

        if (rc > 0) {
            printf(">> Cliente cadastrado com ID %d\n", rc);
        } else {
            mostrar_resultado(rc);
        }
    }
}

static void tela_cadastrar_produto(void)
{
    Produto p;
    int64_t v;
    int rc;

    memset(&p, 0, sizeof p);
    util_ler_linha("Codigo: ", p.codigo, sizeof p.codigo);
    util_ler_linha("Descricao: ", p.descricao, sizeof p.descricao);
    util_ler_linha("Unidade (ex.: UN, KG): ", p.unidade, sizeof p.unidade);
    if (!util_ler_centavos("Preco de venda (R$): ", &v)) {
        printf("Preco invalido.\n");
        return;
    }
    p.preco_venda_centavos = v;
    rc = cadastros_inserir_produto(&p);
    if (rc > 0) {
        printf(">> Produto cadastrado com ID %d (use Estoque > Entrada para dar saldo)\n", rc);
    } else {
        mostrar_resultado(rc);
    }
}

static void menu_cadastros(void)
{
    int op = -1;

    do {
        printf("\n--- CADASTROS ---\n1) Cadastrar cliente\n2) Cadastrar produto\n"
               "3) Listar clientes\n4) Listar produtos\n5) Buscar produto por codigo\n"
               "0) Voltar\n");
        if (!ler_opcao("Opcao: ", &op)) {
            op = -1;
            if (feof(stdin)) {
                return;
            }
            continue;
        }
        switch (op) {
        case 1: tela_cadastrar_cliente(); break;
        case 2: tela_cadastrar_produto(); break;
        case 3: cadastros_listar_clientes(); break;
        case 4: cadastros_listar_produtos(); break;
        case 5: {
            char cod[TAM_CODIGO];
            Produto *p;

            util_ler_linha("Codigo: ", cod, sizeof cod);
            p = cadastros_buscar_produto_codigo(cod);
            if (p == NULL) {
                printf("Produto nao encontrado.\n");
            } else {
                char cmp[TAM_MOEDA];
                char preco[TAM_MOEDA];

                util_formatar_moeda(p->cmp_centavos, cmp, sizeof cmp);
                util_formatar_moeda(p->preco_venda_centavos, preco, sizeof preco);
                printf("ID %d | %s | %s | saldo %lld | CMP %s | preco %s\n",
                       (int)p->id, p->codigo, p->descricao,
                       (long long)p->saldo_qtd, cmp, preco);
            }
            break;
        }
        case 0: break;
        default: printf("Opcao invalida.\n");
        }
    } while (op != 0);
}

/* ------------------------------------------------------------------ */
/* Estoque                                                             */
/* ------------------------------------------------------------------ */
static void menu_estoque(void)
{
    int op = -1;

    do {
        printf("\n--- ESTOQUE ---\n1) Entrada de mercadoria (recalcula CMP)\n"
               "2) Baixa manual\n3) Relatorio de estoque\n"
               "4) Simulador de CMP (nao grava)\n0) Voltar\n");
        if (!ler_opcao("Opcao: ", &op)) {
            op = -1;
            if (feof(stdin)) {
                return;
            }
            continue;
        }
        switch (op) {
        case 1: {
            int64_t id;
            int64_t qtd;
            int64_t valor;

            if (!util_ler_int64("ID do produto: ", &id) ||
                !util_ler_int64("Quantidade: ", &qtd) ||
                !util_ler_centavos("Custo unitario da entrada (R$): ", &valor)) {
                printf("Entrada invalida.\n");
                break;
            }
            mostrar_resultado(estoque_registrar_entrada((int32_t)id, qtd, valor));
            break;
        }
        case 2: {
            int64_t id;
            int64_t qtd;

            if (!util_ler_int64("ID do produto: ", &id) ||
                !util_ler_int64("Quantidade: ", &qtd)) {
                printf("Entrada invalida.\n");
                break;
            }
            mostrar_resultado(estoque_dar_baixa((int32_t)id, qtd));
            break;
        }
        case 3: estoque_relatorio(); break;
        case 4: {
            int64_t saldo;
            int64_t cmp;
            int64_t qtd;
            int64_t valor;
            int64_t r;
            char b[TAM_MOEDA];

            if (!util_ler_int64("Saldo anterior (qtd): ", &saldo) ||
                !util_ler_centavos("CMP anterior (R$): ", &cmp) ||
                !util_ler_int64("Quantidade de entrada: ", &qtd) ||
                !util_ler_centavos("Valor unitario de entrada (R$): ", &valor)) {
                printf("Entrada invalida.\n");
                break;
            }
            r = estoque_calcular_cmp(saldo, cmp, qtd, valor);
            if (r < 0) {
                printf("Nao foi possivel calcular (parametros invalidos ou estouro).\n");
            } else {
                util_formatar_moeda(r, b, sizeof b);
                printf("Novo CMP = %s (saldo total %lld)\n", b, (long long)(saldo + qtd));
            }
            break;
        }
        case 0: break;
        default: printf("Opcao invalida.\n");
        }
    } while (op != 0);
}

/* ------------------------------------------------------------------ */
/* Vendas                                                              */
/* ------------------------------------------------------------------ */
static void menu_vendas(void)
{
    int op = -1;

    do {
        printf("\n--- VENDAS ---\n1) Novo pedido (orcamento)\n2) Adicionar item\n"
               "3) Faturar venda\n4) Cancelar venda\n5) Listar vendas\n"
               "6) Detalhar venda\n0) Voltar\n");
        if (!ler_opcao("Opcao: ", &op)) {
            op = -1;
            if (feof(stdin)) {
                return;
            }
            continue;
        }
        switch (op) {
        case 1: {
            int64_t cid;
            int rc;

            cadastros_listar_clientes();
            if (!util_ler_int64("ID do cliente: ", &cid)) {
                printf("Entrada invalida.\n");
                break;
            }
            rc = vendas_criar((int32_t)cid);
            if (rc > 0) {
                printf(">> Pedido %d criado (ORCAMENTO)\n", rc);
            } else {
                mostrar_resultado(rc);
            }
            break;
        }
        case 2: {
            int64_t vid;
            int64_t pid;
            int64_t qtd;
            int64_t bp = 0;
            char buf[32];

            if (!util_ler_int64("ID da venda: ", &vid)) {
                printf("Entrada invalida.\n");
                break;
            }
            cadastros_listar_produtos();
            if (!util_ler_int64("ID do produto: ", &pid) ||
                !util_ler_int64("Quantidade: ", &qtd)) {
                printf("Entrada invalida.\n");
                break;
            }
            if (util_ler_linha("Desconto % (ex.: 5 ou 2,5; vazio = 0): ", buf, sizeof buf) &&
                buf[0] != '\0') {
                if (!util_parse_centavos(buf, &bp) || bp > 10000) {
                    printf("Desconto invalido.\n");
                    break;
                }
            }
            /* "2,5" -> 250 centavos de percentual = 250 pontos-base */
            mostrar_resultado(vendas_adicionar_item((int32_t)vid, (int32_t)pid, qtd, (int32_t)bp));
            break;
        }
        case 3: {
            int64_t vid;
            int64_t parc;
            int32_t venc = 0;

            if (!util_ler_int64("ID da venda: ", &vid) ||
                !util_ler_int64("Numero de parcelas: ", &parc)) {
                printf("Entrada invalida.\n");
                break;
            }
            if (!ler_data_br("1o vencimento (dd/mm/aaaa, vazio = hoje+30): ", &venc)) {
                printf("Data invalida.\n");
                break;
            }
            mostrar_resultado(vendas_faturar((int32_t)vid, (int)parc, venc));
            if (vendas_buscar_id((int32_t)vid) != NULL &&
                vendas_buscar_id((int32_t)vid)->status == VENDA_FATURADA) {
                vendas_imprimir(vendas_buscar_id((int32_t)vid));
            }
            break;
        }
        case 4: {
            int64_t vid;

            if (!util_ler_int64("ID da venda: ", &vid)) {
                printf("Entrada invalida.\n");
                break;
            }
            mostrar_resultado(vendas_cancelar((int32_t)vid));
            break;
        }
        case 5: vendas_listar(); break;
        case 6: {
            int64_t vid;
            Venda *v;

            if (!util_ler_int64("ID da venda: ", &vid)) {
                printf("Entrada invalida.\n");
                break;
            }
            v = vendas_buscar_id((int32_t)vid);
            if (v == NULL) {
                printf("Venda nao encontrada.\n");
            } else {
                vendas_imprimir(v);
            }
            break;
        }
        case 0: break;
        default: printf("Opcao invalida.\n");
        }
    } while (op != 0);
}

/* ------------------------------------------------------------------ */
/* Financeiro                                                          */
/* ------------------------------------------------------------------ */
static void menu_financeiro(void)
{
    int op = -1;

    do {
        printf("\n--- FINANCEIRO ---\n1) Incluir titulo\n2) Baixar/liquidar titulo\n"
               "3) Listar contas a receber\n4) Listar contas a pagar\n"
               "5) Fluxo de caixa\n6) Emitir boleto (linha digitavel)\n0) Voltar\n");
        if (!ler_opcao("Opcao: ", &op)) {
            op = -1;
            if (feof(stdin)) {
                return;
            }
            continue;
        }
        switch (op) {
        case 1: {
            int64_t tipo;
            int64_t ent;
            int64_t valor;
            int32_t venc = 0;
            char desc[TAM_DESCRICAO];
            int rc;

            if (!util_ler_int64("Tipo (1=Receber, 2=Pagar): ", &tipo)) {
                printf("Entrada invalida.\n");
                break;
            }
            util_ler_linha("Descricao: ", desc, sizeof desc);
            if (!util_ler_int64("ID da entidade (cliente/fornecedor, 0 se nenhum): ", &ent) ||
                !util_ler_centavos("Valor (R$): ", &valor)) {
                printf("Entrada invalida.\n");
                break;
            }
            if (!ler_data_br("Vencimento (dd/mm/aaaa): ", &venc) || venc == 0) {
                printf("Data invalida.\n");
                break;
            }
            rc = financeiro_incluir_titulo((TipoTitulo)tipo, desc, (int32_t)ent, 0,
                                           1, 1, valor, data_hoje(), venc);
            if (rc > 0) {
                printf(">> Titulo %d incluido\n", rc);
            } else {
                mostrar_resultado(rc);
            }
            break;
        }
        case 2: {
            int64_t id;
            int64_t valor;
            int32_t data = 0;
            TituloFinanceiro *t;

            if (!util_ler_int64("ID do titulo: ", &id)) {
                printf("Entrada invalida.\n");
                break;
            }
            t = financeiro_buscar_id((int32_t)id);
            if (t == NULL) {
                printf("Titulo nao encontrado.\n");
                break;
            }
            printf("Saldo em aberto: ");
            {
                char b[TAM_MOEDA];

                util_formatar_moeda(financeiro_saldo_titulo(t), b, sizeof b);
                printf("%s\n", b);
            }
            if (!util_ler_centavos("Valor pago (R$): ", &valor)) {
                printf("Valor invalido.\n");
                break;
            }
            if (!ler_data_br("Data do pagamento (dd/mm/aaaa, vazio = hoje): ", &data)) {
                printf("Data invalida.\n");
                break;
            }
            if (data == 0) {
                data = data_hoje();
            }
            mostrar_resultado(financeiro_baixar_titulo((int32_t)id, valor, data));
            break;
        }
        case 3: financeiro_listar(TITULO_RECEBER); break;
        case 4: financeiro_listar(TITULO_PAGAR); break;
        case 5: financeiro_relatorio_fluxo_caixa(); break;
        case 6: {
            int64_t id;

            if (!util_ler_int64("ID do titulo a receber: ", &id)) {
                printf("Entrada invalida.\n");
                break;
            }
            mostrar_resultado(financeiro_emitir_boleto((int32_t)id));
            break;
        }
        case 0: break;
        default: printf("Opcao invalida.\n");
        }
    } while (op != 0);
}

/* ------------------------------------------------------------------ */
/* Fiscal                                                              */
/* ------------------------------------------------------------------ */
static void menu_fiscal(void)
{
    int op = -1;

    do {
        printf("\n--- FISCAL ---\n1) Gerar chave de acesso NF-e (manual)\n"
               "2) Validar chave de acesso\n3) Gerar linha digitavel de boleto\n"
               "4) Validar linha digitavel\n5) Calcular DV Modulo 11 SEFAZ\n"
               "6) Validar CPF/CNPJ\n0) Voltar\n");
        if (!ler_opcao("Opcao: ", &op)) {
            op = -1;
            if (feof(stdin)) {
                return;
            }
            continue;
        }
        switch (op) {
        case 1: {
            int64_t uf, ano, mes, modelo, serie, numero, tp, cnf;
            char cnpj[TAM_DOC];
            char chave[TAM_CHAVE_NFE];
            int rc;

            util_ler_linha("CNPJ do emitente: ", cnpj, sizeof cnpj);
            if (!util_ler_int64("UF (codigo IBGE, ex.: 35): ", &uf) ||
                !util_ler_int64("Ano (ex.: 2026): ", &ano) ||
                !util_ler_int64("Mes: ", &mes) ||
                !util_ler_int64("Modelo (55): ", &modelo) ||
                !util_ler_int64("Serie: ", &serie) ||
                !util_ler_int64("Numero da NF: ", &numero) ||
                !util_ler_int64("Tipo de emissao (1): ", &tp) ||
                !util_ler_int64("Codigo numerico (8 digitos): ", &cnf)) {
                printf("Entrada invalida.\n");
                break;
            }
            rc = fiscal_gerar_chave_nfe((int)uf, (int)ano, (int)mes, cnpj, (int)modelo,
                                        (int)serie, (int32_t)numero, (int)tp,
                                        (int32_t)cnf, chave, sizeof chave);
            if (rc == ERP_OK) {
                printf("Chave: %s\n", chave);
            } else {
                mostrar_resultado(rc);
            }
            break;
        }
        case 2: {
            char buf[80];
            char dig[64];

            util_ler_linha("Chave (44 digitos): ", buf, sizeof buf);
            if (fiscal_somente_digitos(buf, dig, sizeof dig) < 0) {
                printf("Entrada invalida.\n");
                break;
            }
            printf("Chave %s\n", fiscal_validar_chave_nfe(dig) ? "VALIDA" : "INVALIDA");
            break;
        }
        case 3: {
            char banco[16];
            char livre[48];
            char linha[TAM_LINHA_DIG];
            char barras[TAM_CODIGO_BARRAS];
            int64_t valor;
            int32_t venc = 0;
            int rc;

            util_ler_linha("Codigo do banco (3 digitos): ", banco, sizeof banco);
            util_ler_linha("Campo livre (25 digitos): ", livre, sizeof livre);
            if (!util_ler_centavos("Valor (R$): ", &valor) ||
                !ler_data_br("Vencimento (dd/mm/aaaa): ", &venc) || venc == 0) {
                printf("Entrada invalida.\n");
                break;
            }
            rc = fiscal_gerar_linha_digitavel(banco, '9', livre, venc, valor, linha,
                                              sizeof linha, barras, sizeof barras);
            if (rc == ERP_OK) {
                printf("Linha digitavel: %s\nCodigo de barras: %s\n", linha, barras);
            } else {
                mostrar_resultado(rc);
            }
            break;
        }
        case 4: {
            char buf[80];

            util_ler_linha("Linha digitavel: ", buf, sizeof buf);
            printf("Linha %s\n", fiscal_validar_linha_digitavel(buf) ? "VALIDA" : "INVALIDA");
            break;
        }
        case 5: {
            char buf[80];
            char dig[64];
            int dv;

            util_ler_linha("Digitos (43 para chave): ", buf, sizeof buf);
            if (fiscal_somente_digitos(buf, dig, sizeof dig) <= 0) {
                printf("Entrada invalida.\n");
                break;
            }
            dv = fiscal_calcular_dv_mod11_sefaz(dig);
            printf("DV Modulo 11 SEFAZ = %d\n", dv);
            break;
        }
        case 6: {
            char buf[40];
            char dig[TAM_DOC];

            util_ler_linha("CPF ou CNPJ: ", buf, sizeof buf);
            if (fiscal_somente_digitos(buf, dig, sizeof dig) < 0) {
                printf("Entrada invalida.\n");
                break;
            }
            if (strlen(dig) == 11) {
                printf("CPF %s\n", fiscal_validar_cpf(dig) ? "VALIDO" : "INVALIDO");
            } else if (strlen(dig) == 14) {
                printf("CNPJ %s\n", fiscal_validar_cnpj(dig) ? "VALIDO" : "INVALIDO");
            } else {
                printf("Documento com tamanho invalido.\n");
            }
            break;
        }
        case 0: break;
        default: printf("Opcao invalida.\n");
        }
    } while (op != 0);
}

/* ------------------------------------------------------------------ */
/* Persistencia                                                        */
/* ------------------------------------------------------------------ */
static void menu_persistencia(void)
{
    int op = -1;

    do {
        printf("\n--- PERSISTENCIA ---\n1) Salvar dados (.dat)\n2) Carregar dados (.dat)\n"
               "3) Restaurar dados padrao de demonstracao\n"
#ifdef USE_SQLSERVER
               "4) Salvar no SQL Server\n5) Carregar do SQL Server\n"
#endif
               "0) Voltar\n");
        if (!ler_opcao("Opcao: ", &op)) {
            op = -1;
            if (feof(stdin)) {
                return;
            }
            continue;
        }
        switch (op) {
        case 1: mostrar_resultado(persistencia_salvar_dados()); break;
        case 2: mostrar_resultado(persistencia_carregar_dados()); break;
        case 3:
            sistema_inicializar_dados_padrao();
            printf(">> Dados padrao restaurados (nao gravados em disco ate salvar)\n");
            break;
#ifdef USE_SQLSERVER
        case 4: mostrar_resultado(sql_salvar_dados()); break;
        case 5: mostrar_resultado(sql_carregar_dados()); break;
#endif
        case 0: break;
        default: printf("Opcao invalida.\n");
        }
    } while (op != 0);
}

/* ------------------------------------------------------------------ */
/* Testes automatizados                                                */
/* ------------------------------------------------------------------ */
static int g_testes_total = 0;
static int g_testes_falhas = 0;

static void verificar(bool cond, const char *nome)
{
    g_testes_total++;
    if (cond) {
        printf("  [PASSOU] %s\n", nome);
    } else {
        g_testes_falhas++;
        printf("  [FALHOU] %s\n", nome);
    }
}

static int executar_testes(void)
{
    char chave[TAM_CHAVE_NFE];
    char linha[TAM_LINHA_DIG];
    char barras[TAM_CODIGO_BARRAS];
    char barras2[TAM_CODIGO_BARRAS];
    char moeda[TAM_MOEDA];
    int64_t v = 0;
    int rc;

    g_testes_total = 0;
    g_testes_falhas = 0;
    printf("\n===== TESTES AUTOMATIZADOS DE VALIDACAO =====\n");

    printf("\n[Modulo 11 SEFAZ]\n");
    verificar(fiscal_calcular_dv_mod11_sefaz("1") == 9, "DV(\"1\") == 9");
    verificar(fiscal_calcular_dv_mod11_sefaz("11") == 6, "DV(\"11\") == 6");
    verificar(fiscal_calcular_dv_mod11_sefaz("5") == 1, "DV(\"5\") == 1 (resto 10)");
    verificar(fiscal_calcular_dv_mod11_sefaz("6") == 0, "DV(\"6\") == 0 (resto 1 -> 0)");
    verificar(fiscal_calcular_dv_mod11_sefaz("0") == 0, "DV(\"0\") == 0 (resto 0 -> 0)");
    verificar(fiscal_calcular_dv_mod11_sefaz("12a") == -1, "entrada nao numerica rejeitada");

    printf("\n[Chave de acesso NF-e]\n");
    rc = fiscal_gerar_chave_nfe(35, 2026, 10, "11444777000161", 55, 1, 1234, 1,
                                12345678, chave, sizeof chave);
    verificar(rc == ERP_OK, "gera chave");
    verificar(strlen(chave) == 44, "chave com 44 digitos");
    verificar(strncmp(chave, "35261011444777000161550010000012341", 35) == 0,
              "composicao UF+AAMM+CNPJ+mod+serie+numero+tpEmis");
    verificar(fiscal_validar_chave_nfe(chave), "DV da chave confere");
    chave[20] = (chave[20] == '9') ? '0' : (char)(chave[20] + 1);
    verificar(!fiscal_validar_chave_nfe(chave), "chave adulterada e rejeitada");
    verificar(fiscal_gerar_chave_nfe(35, 2026, 13, "11444777000161", 55, 1, 1, 1, 1,
                                     chave, sizeof chave) == ERP_ERR_PARAM,
              "mes invalido rejeitado");

    printf("\n[Boleto - Modulo 10 e 11]\n");
    verificar(fiscal_calcular_dv_mod10_febraban("1") == 8, "Mod10(\"1\") == 8");
    verificar(fiscal_calcular_dv_mod10_febraban("12") == 5, "Mod10(\"12\") == 5");
    verificar(fiscal_calcular_dv_mod10_febraban("5") == 9, "Mod10(\"5\") == 9 (produto 10 -> 1+0)");
    verificar(fiscal_calcular_dv_mod10_febraban("0") == 0, "Mod10(\"0\") == 0");
    verificar(fiscal_calcular_dv_mod11_boleto("1") == 9, "Mod11 boleto(\"1\") == 9");
    verificar(fiscal_calcular_dv_mod11_boleto("0") == 1, "Mod11 boleto(\"0\") == 1 (DV 11 -> 1)");
    verificar(fiscal_calcular_dv_mod11_boleto("6") == 1, "Mod11 boleto(\"6\") == 1 (DV 10 -> 1)");

    printf("\n[Fator de vencimento]\n");
    verificar(fiscal_calcular_fator_vencimento(19971007) == 0, "07/10/1997 -> 0");
    verificar(fiscal_calcular_fator_vencimento(20000703) == 1000, "03/07/2000 -> 1000");
    verificar(fiscal_calcular_fator_vencimento(20250221) == 9999, "21/02/2025 -> 9999");
    verificar(fiscal_calcular_fator_vencimento(20250222) == 1000, "22/02/2025 -> 1000 (novo ciclo)");
    verificar(fiscal_calcular_fator_vencimento(20250223) == 1001, "23/02/2025 -> 1001");
    verificar(fiscal_calcular_fator_vencimento(20250230) == -1, "data invalida rejeitada");

    printf("\n[Linha digitavel]\n");
    rc = fiscal_gerar_linha_digitavel("341", '9', "1234456789109000000000123",
                                      20261130, 123456, linha, sizeof linha,
                                      barras, sizeof barras);
    verificar(rc == ERP_OK, "gera linha digitavel");
    verificar(strlen(linha) == 54, "linha formatada com 54 caracteres");
    verificar(strlen(barras) == 44, "codigo de barras com 44 digitos");
    verificar(strncmp(barras, "341", 3) == 0 && barras[3] == '9', "banco e moeda no codigo de barras");
    verificar(fiscal_validar_linha_digitavel(linha), "DVs da linha (Mod10 x3 + Mod11) conferem");
    verificar(fiscal_linha_para_barras(linha, barras2, sizeof barras2) == ERP_OK &&
                  strcmp(barras, barras2) == 0,
              "linha -> barras reconstroi o mesmo codigo");
    verificar(strncmp(barras + 9, "0000123456", 10) == 0, "valor em 10 digitos (R$ 1.234,56)");
    {
        char adulterada[TAM_LINHA_DIG];

        util_copiar_string(adulterada, sizeof adulterada, linha);
        adulterada[2] = (adulterada[2] == '9') ? '0' : (char)(adulterada[2] + 1);
        verificar(!fiscal_validar_linha_digitavel(adulterada), "linha adulterada e rejeitada");
    }

    printf("\n[CPF / CNPJ]\n");
    verificar(fiscal_validar_cpf("11144477735"), "CPF valido");
    verificar(!fiscal_validar_cpf("11144477736"), "CPF com DV errado");
    verificar(!fiscal_validar_cpf("11111111111"), "CPF repetido rejeitado");
    verificar(fiscal_validar_cnpj("11222333000181"), "CNPJ valido");
    verificar(fiscal_validar_cnpj("11444777000161"), "CNPJ da empresa valido");
    verificar(!fiscal_validar_cnpj("11222333000182"), "CNPJ com DV errado");

    printf("\n[Custo Medio Ponderado]\n");
    verificar(estoque_calcular_cmp(10, 1000, 10, 2000) == 1500, "CMP(10@10,00 + 10@20,00) = 15,00");
    verificar(estoque_calcular_cmp(100, 2200, 50, 2500) == 2300, "CMP(100@22 + 50@25) = 23,00");
    verificar(estoque_calcular_cmp(0, 0, 5, 333) == 333, "primeira entrada: CMP = custo");
    verificar(estoque_calcular_cmp(1, 100, 1, 101) == 101, "arredondamento meio para cima (100,5 -> 101)");
    verificar(estoque_calcular_cmp(3, 100, 1, 101) == 100, "arredondamento (100,25 -> 100)");
    verificar(estoque_calcular_cmp(0, 0, 0, 100) == -1, "entrada com quantidade zero rejeitada");
    verificar(estoque_calcular_cmp(INT64_MAX / 2, INT64_MAX / 2, 1, 1) == -1, "estouro detectado");

    printf("\n[Baixa de estoque]\n");
    if (g_num_produtos < MAX_PRODUTOS) {
        Produto p;
        int id;
        Produto *pp;
        int32_t id_antes = g_prox_id_produto;

        memset(&p, 0, sizeof p);
        util_copiar_string(p.codigo, sizeof p.codigo, "TESTE-AUTO");
        util_copiar_string(p.descricao, sizeof p.descricao, "Produto de teste");
        id = cadastros_inserir_produto(&p);
        verificar(id > 0, "insere produto de teste");
        verificar(estoque_registrar_entrada(id, 10, 1000) == ERP_OK, "entrada de 10 unidades");
        verificar(estoque_dar_baixa(id, 4) == ERP_OK, "baixa de 4 unidades");
        pp = cadastros_buscar_produto_id(id);
        verificar(pp != NULL && pp->saldo_qtd == 6, "saldo = 6 apos baixa");
        verificar(estoque_dar_baixa(id, 7) == ERP_ERR_SALDO, "baixa acima do saldo rejeitada");
        verificar(pp != NULL && pp->saldo_qtd == 6, "saldo inalterado apos baixa rejeitada");
        verificar(estoque_dar_baixa(id, 0) == ERP_ERR_PARAM, "baixa de quantidade zero rejeitada");
        verificar(pp != NULL && pp->cmp_centavos == 1000, "CMP preservado nas saidas");
        /* remove o produto de teste (e o ultimo do vetor) */
        g_num_produtos--;
        memset(&g_produtos[g_num_produtos], 0, sizeof g_produtos[0]);
        g_prox_id_produto = id_antes;
    } else {
        verificar(false, "sem espaco para produto de teste");
    }

    printf("\n[Autenticacao e perfis]\n");
    {
        uint8_t dig[32];
        static const uint8_t esperado[4] = {0xba, 0x78, 0x16, 0xbf};
        int32_t n_antes = g_num_usuarios;
        int32_t prox_antes = g_prox_id_usuario;
        int uid;
        Usuario *logado = NULL;

        auth_sha256((const uint8_t *)"abc", 3, dig);
        verificar(memcmp(dig, esperado, 4) == 0 && dig[31] == 0xad,
                  "SHA-256(\"abc\") confere com o vetor oficial");
        auth_sha256((const uint8_t *)"", 0, dig);
        verificar(dig[0] == 0xe3 && dig[1] == 0xb0 && dig[31] == 0x55,
                  "SHA-256(\"\") confere com o vetor oficial");
        verificar(auth_pode(PERFIL_ADMIN, MOD_USUARIOS, ACAO_EDITAR), "admin gerencia usuarios");
        verificar(!auth_pode(PERFIL_GERENTE, MOD_USUARIOS, ACAO_VER), "gerente nao ve usuarios");
        verificar(auth_pode(PERFIL_FISCAL, MOD_FISCAL, ACAO_EDITAR), "fiscal edita modulo fiscal");
        verificar(!auth_pode(PERFIL_FISCAL, MOD_FINANCEIRO, ACAO_VER), "fiscal nao ve financeiro");
        verificar(auth_pode(PERFIL_FINANCEIRO, MOD_FINANCEIRO, ACAO_EDITAR), "financeiro edita financeiro");
        verificar(!auth_pode(PERFIL_FINANCEIRO, MOD_VENDAS, ACAO_EDITAR), "financeiro so le vendas");
        verificar(!auth_pode(PERFIL_FINANCEIRO, MOD_ESTOQUE, ACAO_VER), "financeiro nao ve estoque");
        uid = auth_criar_usuario("Teste.Auto", "Usuario de teste", "segredo1", PERFIL_FISCAL);
        verificar(uid > 0, "cria usuario (login normalizado)");
        verificar(auth_buscar_login("teste.auto") != NULL, "login gravado em minusculas");
        verificar(auth_criar_usuario("teste.auto", "Outro", "segredo2", PERFIL_FISCAL) == ERP_ERR_DUPLICADO,
                  "login duplicado rejeitado");
        verificar(auth_criar_usuario("fraco", "Fraco", "123", PERFIL_FISCAL) == ERP_ERR_SENHA_FRACA,
                  "senha curta rejeitada");
        verificar(auth_login("teste.auto", "errada", &logado) == ERP_ERR_ACESSO && logado == NULL,
                  "senha errada recusada");
        verificar(auth_login("nao.existe", "segredo1", &logado) == ERP_ERR_ACESSO,
                  "usuario inexistente recebe a mesma recusa");
        verificar(auth_login("teste.auto", "segredo1", &logado) == ERP_OK && logado != NULL &&
                      logado->perfil == PERFIL_FISCAL,
                  "login correto devolve o perfil");
        verificar(auth_definir_ativo(uid, false) == ERP_OK &&
                      auth_login("teste.auto", "segredo1", &logado) == ERP_ERR_ACESSO,
                  "usuario inativo nao entra");
        g_num_usuarios = n_antes;
        g_prox_id_usuario = prox_antes;
        memset(&g_usuarios[n_antes], 0, sizeof g_usuarios[0]);
    }

    printf("\n[Datas e moeda]\n");
    verificar(data_valida(20240229), "29/02/2024 valida (bissexto)");
    verificar(!data_valida(20230229), "29/02/2023 invalida");
    verificar(data_somar_dias(20240228, 2) == 20240301, "28/02/2024 + 2 dias = 01/03/2024");
    verificar(dias_para_data(data_para_dias(20261028)) == 20261028, "ida e volta de dias <-> data");
    verificar(data_para_dias(19700101) == 0, "01/01/1970 = dia 0");
    verificar(util_parse_centavos("1.234,56", &v) && v == 123456, "parse \"1.234,56\" = 123456");
    verificar(util_parse_centavos("10", &v) && v == 1000, "parse \"10\" = 1000");
    verificar(util_parse_centavos("0,5", &v) && v == 50, "parse \"0,5\" = 50");
    verificar(util_parse_centavos("12.34", &v) && v == 1234, "parse \"12.34\" = 1234");
    verificar(!util_parse_centavos("abc", &v), "parse de texto invalido rejeitado");
    util_formatar_moeda(123456, moeda, sizeof moeda);
    verificar(strcmp(moeda, "R$ 1.234,56") == 0, "formata 123456 -> R$ 1.234,56");
    util_formatar_moeda(-5, moeda, sizeof moeda);
    verificar(strcmp(moeda, "-R$ 0,05") == 0, "formata valor negativo");

    printf("\n===== RESULTADO: %d/%d testes passaram, %d falha(s) =====\n",
           g_testes_total - g_testes_falhas, g_testes_total, g_testes_falhas);
    return g_testes_falhas == 0 ? 0 : 1;
}

/* ------------------------------------------------------------------ */
/* main                                                                */
/* ------------------------------------------------------------------ */
int main(int argc, char **argv)
{
    int op = -1;
    char resp[8];

    cadastros_inicializar_empresa_padrao();

    if (argc > 1 && strcmp(argv[1], "--testes") == 0) {
        return executar_testes();
    }

    if (persistencia_carregar_dados() == ERP_OK) {
        printf("Dados carregados dos arquivos .dat.\n");
    } else {
        sistema_inicializar_dados_padrao();
        printf("Arquivos .dat nao encontrados: dados de demonstracao carregados.\n");
    }

    do {
        printf("\n=====================================\n"
               "        AURORA ERP - PIM II UNIP\n"
               "=====================================\n"
               "1) Cadastros\n2) Estoque\n3) Vendas\n4) Financeiro\n"
               "5) Fiscal\n6) Persistencia\n7) Testes automatizados fiscais\n"
               "0) Sair\n");
        if (!ler_opcao("Opcao: ", &op)) {
            if (feof(stdin)) {
                break;
            }
            op = -1;
            continue;
        }
        switch (op) {
        case 1: menu_cadastros(); break;
        case 2: menu_estoque(); break;
        case 3: menu_vendas(); break;
        case 4: menu_financeiro(); break;
        case 5: menu_fiscal(); break;
        case 6: menu_persistencia(); break;
        case 7: (void)executar_testes(); break;
        case 0: break;
        default: printf("Opcao invalida.\n");
        }
    } while (op != 0);

    if (util_ler_linha("Salvar dados antes de sair? (s/n): ", resp, sizeof resp) &&
        (resp[0] == 's' || resp[0] == 'S')) {
        mostrar_resultado(persistencia_salvar_dados());
    }
#ifdef USE_SQLSERVER
    sql_desconectar();
#endif
    printf("Ate logo!\n");
    return 0;
}
