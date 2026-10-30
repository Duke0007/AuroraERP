/*
 * Aurora ERP - gui.h
 * Interface grafica GTK3 (modulo opcional). Compilar com: make gui
 */
#ifndef GUI_H
#define GUI_H

#include <gtk/gtk.h>
#include "erp.h"

typedef gboolean (*GuiRowPred)(GtkTreeModel *m, GtkTreeIter *it);

enum { CAMPO_TEXTO = 0, CAMPO_SENHA = 1, CAMPO_COMBO = 2 };

typedef struct {
    const char *rotulo;
    int         tipo;      /* CAMPO_* */
    const char *inicial;   /* texto inicial (ou opcao pre-selecionada) */
    const char *opcoes;    /* "a|b|c" para CAMPO_COMBO */
} Campo;

typedef struct {
    GtkWidget   *janela;
    GtkWidget   *stack;
    GtkWidget   *search;
    GtkWidget   *status_msg;
    GtkWidget   *nav[MOD_TOTAL];
    Usuario     *usuario;
    PerfilAcesso perfil;
    Modulo       atual;
    char         busca[160];
    gboolean     relogin;
    void       (*atualizar[MOD_TOTAL])(void);
} Gui;

extern Gui G;

/* gui_util.c */
void       gui_aplicar_css(void);
GtkWidget *gui_label(const char *txt, const char *classe);
GtkWidget *gui_botao(const char *texto, const char *icone, const char *classe);
void       gui_msg(GtkWindow *pai, GtkMessageType tipo, const char *titulo, const char *txt);
void       gui_erro_rc(int rc);
gboolean   gui_confirmar(const char *txt);
gboolean   gui_pode(Modulo m, AcaoAcesso a, gboolean avisar);
gboolean   gui_form(const char *titulo, const char *ok, const Campo *campos, int n, char valores[][160]);
void       gui_resultado(const char *titulo, const char *texto);
void       gui_mostrar_texto(const char *titulo, const char *texto);
GtkWidget *gui_pagina(const char *titulo, const char *sub, GtkWidget **acoes);
GtkWidget *gui_tabela(int n, const char *const *titulos, const char *alinhas,
                      int col_badge, GuiRowPred pred, GtkListStore **store,
                      GtkWidget **tv);
void       gui_store_add(GtkListStore *s, const char *const *cols, int n);
gboolean   gui_selecionado(GtkWidget *tv, int *id);
void       gui_status(const char *txt);
void       gui_refiltrar(void);
void       gui_limpar_filtros(void);
void       gui_atualizar_tudo(void);
void       gui_atualizar_modulo(Modulo m);
void       gui_registrar(const char *evento);
void       gui_gravar(const char *evento);   /* auditoria + salva .dat + atualiza telas */
void       gui_copiar(const char *texto);
void       gui_formatar_doc(const char *dig, char *out, size_t tam);
void       gui_opcoes_clientes(GString *s, gboolean so_ativos, gboolean com_zero);
void       gui_opcoes_produtos(GString *s);
int        gui_id_da_opcao(const char *op);

/* gui_pages.c */
GtkWidget *pg_dashboard_criar(void);
GtkWidget *pg_clientes_criar(void);
GtkWidget *pg_estoque_criar(void);
GtkWidget *pg_vendas_criar(void);

/* gui_pages2.c */
GtkWidget *pg_financeiro_criar(void);
GtkWidget *pg_fiscal_criar(void);
GtkWidget *pg_usuarios_criar(void);
GtkWidget *pg_config_criar(void);

/* gui_main.c */
void       gui_alterar_minha_senha(gboolean obrigatoria, gboolean *ok);

#endif /* GUI_H */
