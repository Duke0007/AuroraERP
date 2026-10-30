/*
 * Aurora ERP - auth.c
 * Autenticacao (usuario + senha) e autorizacao por perfil.
 *
 *  - Senhas nunca sao gravadas: guarda-se sal aleatorio de 16 bytes e
 *    SHA-256 iterado (50.000 rodadas) sobre sal + senha.
 *  - Perfis: ADMIN, GERENTE, FISCAL e FINANCEIRO, com matriz de permissoes
 *    por modulo (ver / editar).
 *  - Bloqueio temporario apos 5 falhas de login seguidas.
 *  - Trilha de auditoria em auditoria.log.
 *
 * Observacao: SHA-256 iterado e adequado ao escopo academico do PIM; em
 * producao o recomendado e usar bcrypt/scrypt/Argon2.
 */
#include "erp.h"

Usuario g_usuarios[MAX_USUARIOS];
int32_t g_num_usuarios   = 0;
int32_t g_prox_id_usuario = 1;

/* ------------------------------------------------------------------ */
/* SHA-256 (FIPS 180-4)                                                */
/* ------------------------------------------------------------------ */
static const uint32_t SHA_K[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u,
    0x923f82a4u, 0xab1c5ed5u, 0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
    0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u, 0xe49b69c1u, 0xefbe4786u,
    0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u,
    0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
    0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u, 0xa2bfe8a1u, 0xa81a664bu,
    0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au,
    0x5b9cca4fu, 0x682e6ff3u, 0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
    0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u
};

#define ROR32(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

static void sha256_bloco(uint32_t h[8], const uint8_t bloco[64])
{
    uint32_t w[64];
    uint32_t a, b, c, d, e, f, g, hh;
    int i;

    for (i = 0; i < 16; i++) {
        w[i] = ((uint32_t)bloco[i * 4] << 24) | ((uint32_t)bloco[i * 4 + 1] << 16) |
               ((uint32_t)bloco[i * 4 + 2] << 8) | (uint32_t)bloco[i * 4 + 3];
    }
    for (i = 16; i < 64; i++) {
        uint32_t s0 = ROR32(w[i - 15], 7) ^ ROR32(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = ROR32(w[i - 2], 17) ^ ROR32(w[i - 2], 19) ^ (w[i - 2] >> 10);

        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    a = h[0]; b = h[1]; c = h[2]; d = h[3];
    e = h[4]; f = h[5]; g = h[6]; hh = h[7];
    for (i = 0; i < 64; i++) {
        uint32_t S1 = ROR32(e, 6) ^ ROR32(e, 11) ^ ROR32(e, 25);
        uint32_t ch = (e & f) ^ (~e & g);
        uint32_t t1 = hh + S1 + ch + SHA_K[i] + w[i];
        uint32_t S0 = ROR32(a, 2) ^ ROR32(a, 13) ^ ROR32(a, 22);
        uint32_t mj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t t2 = S0 + mj;

        hh = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d;
    h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
}

void auth_sha256(const uint8_t *dados, size_t tam, uint8_t out[32])
{
    uint32_t h[8] = {0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
                     0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};
    uint8_t bloco[64];
    size_t i = 0;
    size_t resto;
    uint64_t bits = (uint64_t)tam * 8u;
    int k;

    while (tam - i >= 64) {
        sha256_bloco(h, dados + i);
        i += 64;
    }
    resto = tam - i;
    memset(bloco, 0, sizeof bloco);
    if (resto > 0) {
        memcpy(bloco, dados + i, resto);
    }
    bloco[resto] = 0x80;
    if (resto >= 56) {
        sha256_bloco(h, bloco);
        memset(bloco, 0, sizeof bloco);
    }
    for (k = 0; k < 8; k++) {
        bloco[63 - k] = (uint8_t)(bits >> (8 * k));
    }
    sha256_bloco(h, bloco);
    for (k = 0; k < 8; k++) {
        out[k * 4]     = (uint8_t)(h[k] >> 24);
        out[k * 4 + 1] = (uint8_t)(h[k] >> 16);
        out[k * 4 + 2] = (uint8_t)(h[k] >> 8);
        out[k * 4 + 3] = (uint8_t)h[k];
    }
}

/* h0 = SHA256(salt || senha); h(n) = SHA256(h(n-1) || salt || senha) */
#define ITERACOES_HASH 50000

void auth_derivar_hash(const char *senha, const uint8_t salt[TAM_SALT],
                       uint8_t out[TAM_HASH])
{
    uint8_t buf[TAM_HASH + TAM_SALT + 256];
    size_t n = strlen(senha);
    int i;

    if (n > 256) {
        n = 256;
    }
    memcpy(buf, salt, TAM_SALT);
    memcpy(buf + TAM_SALT, senha, n);
    auth_sha256(buf, TAM_SALT + n, out);
    for (i = 1; i < ITERACOES_HASH; i++) {
        memcpy(buf, out, TAM_HASH);
        memcpy(buf + TAM_HASH, salt, TAM_SALT);
        memcpy(buf + TAM_HASH + TAM_SALT, senha, n);
        auth_sha256(buf, TAM_HASH + TAM_SALT + n, out);
    }
    memset(buf, 0, sizeof buf);
}

static bool hash_igual(const uint8_t *a, const uint8_t *b)
{
    uint8_t dif = 0;
    int i;

    for (i = 0; i < TAM_HASH; i++) {
        dif |= (uint8_t)(a[i] ^ b[i]); /* tempo constante */
    }
    return dif == 0;
}

/* Sal aleatorio: /dev/urandom quando existe; senao mistura tempo/relogio. */
static void gerar_salt(uint8_t salt[TAM_SALT])
{
    FILE *f = fopen("/dev/urandom", "rb");

    if (f != NULL) {
        size_t lidos = fread(salt, 1, TAM_SALT, f);

        fclose(f);
        if (lidos == TAM_SALT) {
            return;
        }
    }
    {
        static uint32_t contador = 0;
        uint8_t semente[64];
        uint8_t digest[32];
        time_t agora = time(NULL);
        clock_t clk = clock();

        memset(semente, 0, sizeof semente);
        memcpy(semente, &agora, sizeof agora);
        memcpy(semente + 16, &clk, sizeof clk);
        memcpy(semente + 32, &contador, sizeof contador);
        contador++;
        auth_sha256(semente, sizeof semente, digest);
        memcpy(salt, digest, TAM_SALT);
    }
}

/* ------------------------------------------------------------------ */
/* Permissoes                                                          */
/* ------------------------------------------------------------------ */
/* bit 1 = ver, bit 2 = editar */
static const uint8_t MATRIZ[5][MOD_TOTAL] = {
    /* dash cli est ven fin fis usr cfg */
    {0, 0, 0, 0, 0, 0, 0, 0},   /* indice 0 sem uso */
    {3, 3, 3, 3, 3, 3, 3, 3},   /* ADMIN */
    {1, 3, 3, 3, 1, 1, 0, 0},   /* GERENTE */
    {1, 1, 1, 1, 0, 3, 0, 0},   /* FISCAL */
    {1, 1, 0, 1, 3, 1, 0, 0}    /* FINANCEIRO */
};

bool auth_pode(PerfilAcesso perfil, Modulo modulo, AcaoAcesso acao)
{
    if ((int)perfil < 1 || (int)perfil > 4 || (int)modulo < 0 ||
        (int)modulo >= MOD_TOTAL) {
        return false;
    }
    return (MATRIZ[perfil][modulo] & (uint8_t)acao) != 0;
}

const char *auth_nome_perfil(PerfilAcesso perfil)
{
    switch (perfil) {
    case PERFIL_ADMIN:      return "Administrador";
    case PERFIL_GERENTE:    return "Gerente";
    case PERFIL_FISCAL:     return "Fiscal";
    case PERFIL_FINANCEIRO: return "Financeiro";
    default:                return "?";
    }
}

const char *auth_nome_modulo(Modulo modulo)
{
    switch (modulo) {
    case MOD_DASHBOARD:  return "Visao geral";
    case MOD_CLIENTES:   return "Clientes";
    case MOD_ESTOQUE:    return "Estoque";
    case MOD_VENDAS:     return "Vendas";
    case MOD_FINANCEIRO: return "Financeiro";
    case MOD_FISCAL:     return "NF-e e boletos";
    case MOD_USUARIOS:   return "Usuarios";
    case MOD_CONFIG:     return "Configuracoes";
    default:             return "?";
    }
}

/* ------------------------------------------------------------------ */
/* Auditoria                                                           */
/* ------------------------------------------------------------------ */
void auth_log(const char *usuario, const char *evento)
{
    FILE *f = fopen(ARQ_AUDITORIA, "a");
    time_t agora = time(NULL);
    struct tm *tmv = localtime(&agora);
    char ts[24] = "0000-00-00 00:00:00";
    char u[TAM_LOGIN];
    char e[200];
    size_t i;

    if (f == NULL) {
        return;
    }
    if (tmv != NULL) {
        strftime(ts, sizeof ts, "%Y-%m-%d %H:%M:%S", tmv);
    }
    util_copiar_string(u, sizeof u, usuario != NULL ? usuario : "-");
    util_copiar_string(e, sizeof e, evento != NULL ? evento : "");
    for (i = 0; u[i] != '\0'; i++) {
        if (u[i] == '\n' || u[i] == '\r' || u[i] == '|') u[i] = ' ';
    }
    for (i = 0; e[i] != '\0'; i++) {
        if (e[i] == '\n' || e[i] == '\r') e[i] = ' ';
    }
    fprintf(f, "%s | %s | %s\n", ts, u, e);
    fclose(f);
}

/* ------------------------------------------------------------------ */
/* Usuarios                                                            */
/* ------------------------------------------------------------------ */
Usuario *auth_buscar_id(int32_t id)
{
    int32_t i;

    for (i = 0; i < g_num_usuarios; i++) {
        if (g_usuarios[i].id == id) {
            return &g_usuarios[i];
        }
    }
    return NULL;
}

Usuario *auth_buscar_login(const char *login)
{
    int32_t i;

    if (login == NULL) {
        return NULL;
    }
    for (i = 0; i < g_num_usuarios; i++) {
        if (strcmp(g_usuarios[i].login, login) == 0) {
            return &g_usuarios[i];
        }
    }
    return NULL;
}

static int32_t contar_admins_ativos(void)
{
    int32_t i;
    int32_t n = 0;

    for (i = 0; i < g_num_usuarios; i++) {
        if (g_usuarios[i].perfil == PERFIL_ADMIN && g_usuarios[i].ativo) {
            n++;
        }
    }
    return n;
}

static bool login_valido(const char *login, char *norm, size_t tam)
{
    size_t n;
    size_t i;

    if (login == NULL) {
        return false;
    }
    n = strlen(login);
    if (n < 3 || n >= TAM_LOGIN || n >= tam) {
        return false;
    }
    for (i = 0; i < n; i++) {
        char c = login[i];

        if (c >= 'A' && c <= 'Z') {
            c = (char)(c - 'A' + 'a');
        }
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' ||
              c == '_' || c == '-')) {
            return false;
        }
        norm[i] = c;
    }
    norm[n] = '\0';
    return true;
}

static bool senha_valida(const char *senha, const char *login)
{
    size_t n;

    if (senha == NULL) {
        return false;
    }
    n = strlen(senha);
    if (n < 6 || n > 128) {
        return false;
    }
    return login == NULL || strcmp(senha, login) != 0;
}

int auth_criar_usuario(const char *login, const char *nome, const char *senha,
                       PerfilAcesso perfil)
{
    Usuario u;
    char norm[TAM_LOGIN];

    if (!login_valido(login, norm, sizeof norm) || nome == NULL ||
        nome[0] == '\0' || (int)perfil < 1 || (int)perfil > 4) {
        return ERP_ERR_PARAM;
    }
    if (!senha_valida(senha, norm)) {
        return ERP_ERR_SENHA_FRACA;
    }
    if (g_num_usuarios >= MAX_USUARIOS) {
        return ERP_ERR_LIMITE;
    }
    if (auth_buscar_login(norm) != NULL) {
        return ERP_ERR_DUPLICADO;
    }
    memset(&u, 0, sizeof u);
    u.id = g_prox_id_usuario;
    util_copiar_string(u.login, sizeof u.login, norm);
    util_copiar_string(u.nome, sizeof u.nome, nome);
    u.perfil = perfil;
    gerar_salt(u.salt);
    auth_derivar_hash(senha, u.salt, u.hash);
    u.ativo = true;
    u.trocar_senha = true; /* senha inicial definida por outra pessoa */
    g_usuarios[g_num_usuarios++] = u;
    g_prox_id_usuario++;
    return u.id;
}

int auth_trocar_senha(int32_t id, const char *nova, bool exigir_troca)
{
    Usuario *u = auth_buscar_id(id);

    if (u == NULL) {
        return ERP_ERR_NAO_ENCONTRADO;
    }
    if (!senha_valida(nova, u->login)) {
        return ERP_ERR_SENHA_FRACA;
    }
    gerar_salt(u->salt);
    auth_derivar_hash(nova, u->salt, u->hash);
    u->trocar_senha = exigir_troca;
    return ERP_OK;
}

int auth_definir_ativo(int32_t id, bool ativo)
{
    Usuario *u = auth_buscar_id(id);

    if (u == NULL) {
        return ERP_ERR_NAO_ENCONTRADO;
    }
    if (!ativo && u->perfil == PERFIL_ADMIN && u->ativo &&
        contar_admins_ativos() <= 1) {
        return ERP_ERR_ESTADO; /* nunca desativa o ultimo admin */
    }
    u->ativo = ativo;
    return ERP_OK;
}

int auth_alterar_perfil(int32_t id, PerfilAcesso perfil)
{
    Usuario *u = auth_buscar_id(id);

    if (u == NULL) {
        return ERP_ERR_NAO_ENCONTRADO;
    }
    if ((int)perfil < 1 || (int)perfil > 4) {
        return ERP_ERR_PARAM;
    }
    if (u->perfil == PERFIL_ADMIN && perfil != PERFIL_ADMIN && u->ativo &&
        contar_admins_ativos() <= 1) {
        return ERP_ERR_ESTADO; /* nunca rebaixa o ultimo admin */
    }
    u->perfil = perfil;
    return ERP_OK;
}

/* ------------------------------------------------------------------ */
/* Login com bloqueio temporario                                       */
/* ------------------------------------------------------------------ */
#define MAX_FALHAS     5
#define BLOQUEIO_SEG   300

static int    g_falhas[MAX_USUARIOS + 1];   /* ultimo indice: logins inexistentes */
static time_t g_bloqueado_ate[MAX_USUARIOS + 1];

int auth_login(const char *login, const char *senha, Usuario **out)
{
    char norm[TAM_LOGIN];
    Usuario *u = NULL;
    int idx = MAX_USUARIOS;
    uint8_t calc[TAM_HASH];
    uint8_t salt_falso[TAM_SALT] = {0};
    time_t agora = time(NULL);
    bool ok;
    int32_t i;

    if (out != NULL) {
        *out = NULL;
    }
    if (senha == NULL || !login_valido(login != NULL ? login : "", norm, sizeof norm)) {
        /* mesma resposta e mesmo custo de um login inexistente */
        auth_derivar_hash(senha != NULL ? senha : "", salt_falso, calc);
        return ERP_ERR_ACESSO;
    }
    for (i = 0; i < g_num_usuarios; i++) {
        if (strcmp(g_usuarios[i].login, norm) == 0) {
            u = &g_usuarios[i];
            idx = (int)i;
            break;
        }
    }
    if (g_bloqueado_ate[idx] > agora) {
        auth_log(norm, "LOGIN BLOQUEADO (tentativas excedidas)");
        return ERP_ERR_BLOQUEADO;
    }
    /* o hash e sempre calculado, exista o usuario ou nao */
    auth_derivar_hash(senha, u != NULL ? u->salt : salt_falso, calc);
    ok = (u != NULL) && hash_igual(calc, u->hash);
    memset(calc, 0, sizeof calc);
    if (!ok || !u->ativo) {
        g_falhas[idx]++;
        if (g_falhas[idx] >= MAX_FALHAS) {
            g_bloqueado_ate[idx] = agora + BLOQUEIO_SEG;
            g_falhas[idx] = 0;
        }
        auth_log(norm, ok ? "LOGIN NEGADO (usuario inativo)" : "LOGIN FALHOU");
        return ERP_ERR_ACESSO;
    }
    g_falhas[idx] = 0;
    u->ultimo_acesso = data_hoje();
    auth_log(u->login, "LOGIN OK");
    if (out != NULL) {
        *out = u;
    }
    return ERP_OK;
}

/* Confere a senha de um usuario ja autenticado (nao altera contadores). */
bool auth_verificar_senha(const Usuario *u, const char *senha)
{
    uint8_t calc[TAM_HASH];
    bool ok;

    if (u == NULL || senha == NULL) {
        return false;
    }
    auth_derivar_hash(senha, u->salt, calc);
    ok = hash_igual(calc, u->hash);
    memset(calc, 0, sizeof calc);
    return ok;
}

/* ------------------------------------------------------------------ */
/* Persistencia de usuarios.dat                                        */
/* ------------------------------------------------------------------ */
typedef struct {
    uint32_t magic;
    uint32_t versao;
    uint32_t quantidade;
    uint32_t tam_registro;
} CabUsuarios;

#define USR_MAGIC 0x52535541u /* "AUSR" */

int auth_salvar(void)
{
    FILE *f;
    CabUsuarios h;
    char tmp[64];

    snprintf(tmp, sizeof tmp, "%s.tmp", ARQ_USUARIOS);
    f = fopen(tmp, "wb");
    if (f == NULL) {
        return ERP_ERR_ARQUIVO;
    }
    memset(&h, 0, sizeof h);
    h.magic = USR_MAGIC;
    h.versao = 1;
    h.quantidade = (uint32_t)g_num_usuarios;
    h.tam_registro = (uint32_t)sizeof(Usuario);
    if (fwrite(&h, sizeof h, 1, f) != 1 ||
        (g_num_usuarios > 0 &&
         fwrite(g_usuarios, sizeof(Usuario), (size_t)g_num_usuarios, f) !=
             (size_t)g_num_usuarios) ||
        fflush(f) != 0 || fclose(f) != 0) {
        remove(tmp);
        return ERP_ERR_ARQUIVO;
    }
    if (rename(tmp, ARQ_USUARIOS) != 0) {
        remove(ARQ_USUARIOS);
        if (rename(tmp, ARQ_USUARIOS) != 0) {
            remove(tmp);
            return ERP_ERR_ARQUIVO;
        }
    }
    return ERP_OK;
}

static int carregar_usuarios(void)
{
    FILE *f = fopen(ARQ_USUARIOS, "rb");
    CabUsuarios h;
    static Usuario tmp[MAX_USUARIOS];
    int32_t i;

    if (f == NULL) {
        return ERP_ERR_ARQUIVO;
    }
    if (fread(&h, sizeof h, 1, f) != 1 || h.magic != USR_MAGIC || h.versao != 1 ||
        h.tam_registro != sizeof(Usuario) || h.quantidade > MAX_USUARIOS ||
        h.quantidade == 0 ||
        fread(tmp, sizeof(Usuario), h.quantidade, f) != h.quantidade) {
        fclose(f);
        return ERP_ERR_ARQUIVO;
    }
    fclose(f);
    memset(g_usuarios, 0, sizeof g_usuarios);
    memcpy(g_usuarios, tmp, sizeof(Usuario) * h.quantidade);
    g_num_usuarios = (int32_t)h.quantidade;
    g_prox_id_usuario = 1;
    for (i = 0; i < g_num_usuarios; i++) {
        if (g_usuarios[i].id >= g_prox_id_usuario) {
            g_prox_id_usuario = g_usuarios[i].id + 1;
        }
    }
    return ERP_OK;
}

/*
 * Carrega usuarios.dat. Na primeira execucao cria o administrador inicial
 * (login "admin", senha "admin") com troca de senha obrigatoria no 1o acesso.
 */
int auth_inicializar(void)
{
    int rc;

    if (carregar_usuarios() == ERP_OK) {
        return ERP_OK;
    }
    memset(g_usuarios, 0, sizeof g_usuarios);
    g_num_usuarios = 0;
    g_prox_id_usuario = 1;
    {
        Usuario u;

        memset(&u, 0, sizeof u);
        u.id = g_prox_id_usuario++;
        util_copiar_string(u.login, sizeof u.login, "admin");
        util_copiar_string(u.nome, sizeof u.nome, "Administrador");
        u.perfil = PERFIL_ADMIN;
        gerar_salt(u.salt);
        auth_derivar_hash("admin", u.salt, u.hash);
        u.ativo = true;
        u.trocar_senha = true;
        g_usuarios[g_num_usuarios++] = u;
    }
    rc = auth_salvar();
    auth_log("sistema", "Administrador inicial criado (troca de senha obrigatoria)");
    return rc;
}
