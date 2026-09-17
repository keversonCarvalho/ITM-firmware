# Fluxo De Trabalho Com IA

## Papel Da IA No Projeto

A IA deve ser usada como copiloto tecnico permanente, mas o projeto precisa preservar engenharia verificavel: requisitos claros, revisoes humanas, testes automatizados, evidencias e decisoes registradas.

## Rotina Recomendada

1. Planejar
   - Converter documentos de referencia em requisitos atomicos.
   - Gerar criterios de aceitacao.
   - Criar tarefas pequenas e verificaveis.

2. Projetar
   - Pedir alternativas de arquitetura.
   - Registrar decisoes em ADRs.
   - Validar riscos antes de codificar.

3. Implementar
   - Solicitar implementacoes pequenas.
   - Exigir testes junto com o codigo.
   - Manter codigo dependente de hardware isolado.

4. Verificar
   - Rodar testes unitarios e integracao.
   - Revisar cobertura por requisito.
   - Usar simuladores antes de HIL.

5. Revisar
   - Pedir revisao de codigo focada em falhas, seguranca, concorrencia e estados perigosos.
   - Revisar requisitos e testes em conjunto.

## Prompts Padrao

### Extracao De Requisito

```text
Leia este trecho de especificacao e extraia requisitos atomicos para firmware embarcado.
Para cada requisito, gere ID, texto normativo, fonte, racional, criterio de aceitacao e testes sugeridos.
Nao invente valores ausentes; marque como TBD.
```

### Projeto De Modulo

```text
Projete o modulo <nome> do firmware do ITM.
Considere STM32G473, RTOS, testes em host, fail-safe e isolamento de HAL.
Entregue responsabilidades, interfaces C, maquina de estados, falhas, testes unitarios e riscos.
```

### Implementacao

```text
Implemente apenas o modulo <nome> conforme os requisitos <IDs>.
Mantenha o codigo testavel em host.
Inclua testes unitarios e nao altere interfaces nao relacionadas.
Explique como verificar.
```

### Revisao De Codigo

```text
Revise este codigo como firmware critico.
Priorize bugs, condicoes de corrida, estouro de buffer, falhas de timeout, estados inseguros, perda de diagnostico e lacunas de teste.
Mostre achados por severidade com arquivo e linha.
```

## Guardrails Para Uso De IA

- Nunca aceitar codigo que manipula 150 V sem requisito, intertravamento e teste.
- Nunca aceitar parser de protocolo sem testes de frame valido, invalido, truncado, CRC errado e timeout.
- Nunca aceitar callbacks de interrupcao com logica longa ou bloqueante.
- Nunca aceitar requisito sem fonte ou status.
- Nunca aceitar mudanca de contrato de comunicacao sem versionamento.
- Nunca usar valores eletricos, temporais ou IDs de rede inventados.

## Artefatos Que A IA Deve Manter Atualizados

- Requisitos.
- Matriz de rastreabilidade.
- ADRs.
- Backlog.
- Contratos de dados.
- Testes.
- Evidencias de validacao.
- Release notes.
