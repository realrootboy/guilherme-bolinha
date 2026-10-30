# Guilherme Bolinha VST — finalização

_Concluído em 2026-10-30. Repositório privado, tag e release publicados._

## Resultado

- [Release Guilherme Bolinha VST v1.0.0](https://github.com/realrootboy/gb-vst/releases/tag/v1.0.0)
- [CI da release: os três builds, testes de host e publicação passaram](https://github.com/realrootboy/gb-vst/actions/runs/41065462369)
- Tag `v1.0.0`, código do commit `63451470af61c8d92006fb8cdeb3c19968eb3535`.
- Visibilidade **PRIVATE** confirmada pela API após a publicação.

| Requisito | Resultado verificado |
| --- | --- |
| Renomear produto e estrutura | Guilherme Bolinha / Guilherme Bolinha VST; código em `Source/` e `Tools/`, namespace `cure`, biblioteca `CureData` |
| Conferir recursos contra o manual | Comparação com manual 1.1.2 documentada na tabela de paridade do README, incluindo ausências e diferenças |
| Interface minimalista | Grafite/coral; cinco abas, teclado, rodas e medidores; revisão visual nos tamanhos 1120×690 e 1040×640, incluindo edição melody/drum |
| Logo transparente no VST | Planeta discreto no cabeçalho, embutido por `BolinhaAssets`; imagem original encontrada nas duas arquiteturas do VST3 macOS publicado |
| Logo normal no README | `assets/logo.png` publicada e referenciada no README; ambas as imagens preservadas em `assets/` |
| Conteúdo incluído | 44 arquivos de samples/wavetables/patches em `Resources/Cure`, com 1366 presets; incluídos nos três instaladores |
| Arpeggiadores | Todos os testes automatizados aprovados; banco completo renderizado e preset ARP PIANO tocado nos testes de host |
| macOS | AU/VST3/Standalone universais arm64/x86_64, instalador `.pkg` publicado |
| Windows | VST3 x64/Standalone, instalador Inno Setup `.exe` publicado; host compilado sem AU e teste de áudio aprovado |
| Linux | VST3/LV2/Standalone x64, `.tar.gz` com `install.sh` publicado; host compilado sem AU e teste de áudio aprovado |
| README e GitHub | README atualizado, documentação antiga mesclada/removida, commits/push/tag/release concluídos |

## Instaladores publicados

| Arquivo | Bytes | SHA-256 |
| --- | ---: | --- |
| `Guilherme-Bolinha-macOS.pkg` | 69283500 | `011c8ef5495c4b37a747909f13ce21e6aa84bab3aa7d64e84bd9809c727eb801` |
| `Guilherme-Bolinha-Windows.exe` | 52751139 | `b708e5fffd3bea0bf4adcc834f860939a7e6e2f6acdbcbe904a7f8a25c20bd4f` |
| `Guilherme-Bolinha-Linux.tar.gz` | 64246591 | `ee2b08d77e5dfa66f04fcfd67782336c0f2890a7618cfa8503b25d47e408bd4c` |

Os três arquivos foram baixados e seus hashes comparados com os digests publicados pelo GitHub.

## Evidências de validação

- **Importação:** 44 arquivos extraídos de `setup.exe`, byte a byte idênticos à referência local. O importador continua disponível para restauração.
- **Biblioteca incluída:** `installer/verify-content.py` verifica os 44 arquivos contra `Resources/Cure/SHA256SUMS`; passou localmente e nos três jobs do CI.
- **Render completo:** 1366 patches, `silent=0`, `nearFullScale=20`, `invalid=0`. Áudio finito dentro de [-1,1]; picos próximos do máximo passam pelo soft-clip da engine.
- **Arp:** 8 padrões, 18 rates, 1–6 oitavas, gate, latch, key sync, grid e tempo do host: todos os testes passaram, inclusive usando diretamente a biblioteca de `Resources/Cure`.
- **Host VST3:** macOS, Windows e Linux passaram com os patches 0/128/300/1000 e 1269 (ARP PIANO). O teste falha em silêncio ou amostras não finitas. No macOS, também passou ao carregar o VST3 extraído do instalador publicado.
- **Banco de usuário:** round trip com 1366 patches de fábrica + 2 patches salvos separadamente; nomes e parâmetro editado preservados. O banco pessoal é sobreposto à biblioteca de fábrica sem precisar escrever nas pastas de sistema.
- **UI:** `BolinhaUiTest` percorreu os 16 canais e gerou imagens das cinco abas mais edição drum, em dois tamanhos. Verificou que os dois painéis IFX cabem completamente. Imagens inspecionadas, incluindo logo, seletores e valores.
- **macOS publicado:** `.pkg` expandido, hashes dos 44 arquivos conferidos no payload; AU/VST3/Standalone universais, versões 1.0.0 e bundle ids verificados; assinaturas ad hoc dos três bundles aprovadas; destinos não relocáveis. Logo transparente encontrada duas vezes no binário universal do VST3.
- **Linux publicado:** arquivo expandido, hashes da biblioteca conferidos; executáveis ELF x86-64, VST3/LV2, script executável e logo do README presentes. Manifesto LV2 referencia corretamente nomes com espaços.
- **Windows publicado:** Inno Setup 6.7.1 concluiu a compilação e registrou todos os 44 arquivos mais manifesto, VST3 x64, Standalone, README e logo; lista comparada programaticamente com o manifesto. Hash do `.exe` publicado conferido. O extrator local innoextract 1.9 suporta somente até Inno Setup 6.0.5, portanto não foi usado como evidência de extração desse pacote.

## Correções concluídas na validação

- Modelo de bancos inicializado antes de `ListBox::setModel()`, eliminando acesso nulo na criação do editor.
- Seleção de canal convertida corretamente de parâmetro 1–16 para índice 0–15; canal 16 não acessa mais índice inválido.
- Capturas explícitas nas lambdas de LFO/drum para compilar com MSVC.
- Layout Edit em três linhas completas no tamanho mínimo, incluindo ambos os inserts; seletores de base do sequenciador alargados.
- Formatação dos knobs restaurada após anexar parâmetros, evitando valores com casas decimais excessivas.
- Banco `099 User.pth` salvo por usuário e carregado como overlay; migração de banco anterior preservada.
- Busca por `Resources/Cure` acima dos outputs de build, além de diretório configurado, pasta pessoal, instalação de sistema e caminhos legados.
- Assinaturas macOS refeitas após os metadados e destinos do instalador fixados por component plist.

## Biblioteca e instalação

Conforme a instrução atual do usuário, o conteúdo acompanha o repositório privado e os instaladores. Não é necessário importar outro instalador para tocar.

- macOS: `/Library/Application Support/Guilherme Bolinha/Cure`.
- Windows: `%ProgramData%\Guilherme Bolinha\Cure`.
- Linux: `~/.local/share/guilherme-bolinha/Cure`.

`.gitignore` exclui somente instalador de origem, extração local e resultados de build. Notas de formato estão em `FORMAT.md`.

## Limites declarados

A conclusão refere-se à implementação, auditoria de paridade, testes e publicação acima; não significa equivalência sonora completa ao Cure. O README lista as diferenças verificadas, inclusive polifonia de 256 vozes, tratamento de presets multicamada, controles/atalhos ausentes, MIDI learn/NRPN, padrões `.mid`, entrada numérica, envelope gráfico e mapeamento de semitone drum ainda não validado contra o binário original.

A revisão de UI usou renderização isolada do componente real do editor. A tentativa de acesso ao Standalone por computer-use não foi aprovada; interação manual em DAWs/Standalone e instalação interativa dos pacotes não foram realizadas. Os testes de host de áudio nos três sistemas e a inspeção visual isolada estão comprovados acima.

macOS usa assinatura ad hoc dos bundles, sem Developer ID/notarização do instalador. O instalador Windows não tem assinatura digital.

## Comandos úteis

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel 3
./build/BolinhaArpTest_artefacts/Release/BolinhaArpTest "$PWD/Resources/Cure"
./build/BolinhaRender_artefacts/Release/BolinhaRender "$PWD/Resources/Cure" /tmp/bolinha-render 0 128 300 1269
./build/BolinhaUiTest_artefacts/Release/BolinhaUiTest /tmp/bolinha-ui
python3 installer/verify-content.py Resources/Cure
```

Notas técnicas do formato em `FORMAT.md`.

## Atualização Phoenix/Cure — em validação

- Interface, documentação, código e locais de compatibilidade renomeados.
- Cabeçalhos dos 44 arquivos migrados para Phoenix/Cure; todos os bytes após o campo de marca de 19 bytes preservados, sem alterar tamanhos, offsets, samples ou parâmetros.
- Loader reconhece a estrutura do formato; importação e gravação normalizam para os cabeçalhos novos.
- Nenhuma ocorrência dos nomes anteriores nos arquivos versionados, incluindo os dados binários.
- Manifesto SHA-256 regenerado; verificação exige explicitamente o cabeçalho Phoenix/Cure.
- Versão 1.0.1 em build/teste antes de publicar novos instaladores.
