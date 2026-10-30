-- Aurora ERP - esquema para Microsoft SQL Server
-- Opcional: o proprio programa cria banco e tabelas ao salvar (make SQL=1).
-- Valores monetarios em centavos (BIGINT).
IF DB_ID(N'AuroraERP') IS NULL CREATE DATABASE AuroraERP;
GO
USE AuroraERP;
GO

CREATE TABLE dbo.clientes (
    id INT NOT NULL PRIMARY KEY,
    tipo INT NOT NULL,                       -- 1 = PF, 2 = PJ
    nome VARCHAR(80) NOT NULL,
    documento VARCHAR(14) NOT NULL UNIQUE,
    email VARCHAR(60) NOT NULL,
    cidade VARCHAR(40) NOT NULL,
    uf VARCHAR(2) NOT NULL,
    limite_credito_centavos BIGINT NOT NULL,
    ativo BIT NOT NULL
);

CREATE TABLE dbo.produtos (
    id INT NOT NULL PRIMARY KEY,
    codigo VARCHAR(20) NOT NULL UNIQUE,
    descricao VARCHAR(60) NOT NULL,
    unidade VARCHAR(5) NOT NULL,
    saldo_qtd BIGINT NOT NULL,
    cmp_centavos BIGINT NOT NULL,
    preco_venda_centavos BIGINT NOT NULL,
    ativo BIT NOT NULL
);

CREATE TABLE dbo.vendas (
    id INT NOT NULL PRIMARY KEY,
    cliente_id INT NOT NULL REFERENCES dbo.clientes(id),
    data_emissao DATE NOT NULL,
    status INT NOT NULL,                     -- 0 orcamento, 1 faturada, 2 cancelada
    total_bruto_centavos BIGINT NOT NULL,
    total_desconto_centavos BIGINT NOT NULL,
    total_liquido_centavos BIGINT NOT NULL,
    custo_total_centavos BIGINT NOT NULL,
    num_parcelas INT NOT NULL,
    numero_nf INT NOT NULL,
    chave_nfe VARCHAR(44) NOT NULL
);

CREATE TABLE dbo.venda_itens (
    venda_id INT NOT NULL REFERENCES dbo.vendas(id),
    item_num INT NOT NULL,
    produto_id INT NOT NULL REFERENCES dbo.produtos(id),
    quantidade BIGINT NOT NULL,
    preco_unitario_centavos BIGINT NOT NULL,
    desconto_bp INT NOT NULL,
    desconto_centavos BIGINT NOT NULL,
    subtotal_centavos BIGINT NOT NULL,
    custo_unitario_centavos BIGINT NOT NULL,
    PRIMARY KEY (venda_id, item_num)
);

CREATE TABLE dbo.titulos (
    id INT NOT NULL PRIMARY KEY,
    tipo INT NOT NULL,                       -- 1 receber, 2 pagar
    status INT NOT NULL,                     -- 0 aberto, 1 liquidado, 2 cancelado
    descricao VARCHAR(60) NOT NULL,
    entidade_id INT NOT NULL,
    venda_id INT NOT NULL,
    parcela INT NOT NULL,
    total_parcelas INT NOT NULL,
    valor_centavos BIGINT NOT NULL,
    valor_pago_centavos BIGINT NOT NULL,
    data_emissao DATE NOT NULL,
    data_vencimento DATE NOT NULL,
    data_pagamento DATE NULL
);
GO
