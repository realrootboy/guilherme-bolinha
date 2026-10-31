# GB VST

![Guilherme Bolinha — logo orbital](assets/logo.png)

Instrumento multitimbral de 16 canais em C++/JUCE, com biblioteca sonora completa de 1366 patches incluída. Biblioteca Phoenix/Cure. Versão do projeto: 1.0.3.

## Instalação

Baixe o pacote do seu sistema na [página de releases](https://github.com/realrootboy/gb-vst/releases).

- **macOS 11+ (Apple Silicon e Intel):** execute o `.pkg`. Instala AU e VST3 em `/Library/Audio/Plug-Ins` e o Standalone em `/Applications`. O pacote não tem assinatura Developer ID nem notarização.
- **Windows x64:** execute o instalador `.exe`. O VST3 vai para `Common Files\VST3`; o Standalone fica em Program Files. O instalador não tem assinatura digital.
- **Linux x64:** extraia o `.tar.gz` e execute `sh install.sh`. Instala VST3 em `~/.vst3`, LV2 em `~/.lv2` e Standalone em `~/.local/bin/guilherme-bolinha`. Compilado em Ubuntu 22.04; requer bibliotecas de áudio e interface compatíveis.

Abra novamente o DAW e faça uma nova busca de plugins após instalar.

## Biblioteca sonora incluída

Os instaladores incluem os 44 arquivos de samples, wavetables e patches em `Resources/Cure`. Não é necessário obter ou importar outro instalador para tocar. O repositório permanece privado.

A biblioteca é instalada automaticamente em:

- **macOS:** `/Library/Application Support/Guilherme Bolinha/Cure`.
- **Windows:** `%ProgramData%\Guilherme Bolinha\Cure`.
- **Linux:** `~/.local/share/guilherme-bolinha/Cure`.

Abra o instrumento e escolha um timbre na aba **PRESET**. O banco **099 User** é salvo na pasta pessoal do aplicativo, separado da biblioteca de fábrica, para funcionar também sem permissão de escrita nas pastas do sistema.

A aba **SETUP** permite trocar a pasta da biblioteca e restaurar o conteúdo de uma pasta existente ou do `setup.exe` 1.1.2. Ao compilar a partir deste repositório, o instrumento também procura `Resources/Cure` acima da pasta de build.

## Recursos e paridade

Esta é uma implementação independente. Presença de um controle não implica equivalência sonora exata com o original; a paridade completa ainda precisa de comparação auditiva e validação em DAWs.

| Recurso | Situação |
| --- | --- |
| Biblioteca original, patches e wavetables | ✅ 44 arquivos incluídos; importação disponível para restauração |
| Polifonia global | ⚠️ Até 256 vozes nesta implementação; original anuncia até 1024 |
| Mixer de 16 canais, links, volume, pan, transpose e limites de vozes | ✅ Implementado |
| Presets com múltiplos canais no original | ⚠️ As camadas são reunidas no canal selecionado; não são distribuídas automaticamente por canais consecutivos |
| Renomear canal por duplo clique | ❌ Nome exibido vem do patch |
| Saídas Main/Sub e mix-down estéreo | ✅ Implementado; roteamento depende do host |
| Edição melody/drum, filtros, envelopes, LFO e IFX | ⚠️ Implementado; equivalência sonora completa não comprovada |
| Semitone de drum | ⚠️ Código usa ±48 semitons; manual 1.1.2 indica ±12. Paridade desse mapeamento não validada |
| Nomes/unidades dos parâmetros de IFX | ⚠️ Controles P1–P3 normalizados, sem rótulos específicos de cada efeito |
| IFX Overdrive, Distortion, Amp Distortion, Crusher e LP Noise | ✅ Mapeamento de parâmetros, curvas e faixas conferidos contra o binário 1.1.2 (Drive/Tone/Level; Rate/Bits/Cutoff; Crackle/Dirt/Year) |
| Arpeggiador: 8 padrões, 18 rates, 1–6 oitavas, gate, latch e sync | ✅ Coberto pelo teste automatizado de arp |
| Sequenciador: 5 lanes, até 64 passos | ⚠️ Editor e engine implementados; validação manual pendente |
| Chorus/reverb, afinação, escalas, curvas de velocity e GM reset | ⚠️ Implementado; paridade completa não comprovada |
| Estado do plugin e banco de patches do usuário | ✅ Implementado |
| MIDI learn/assign e funções Factory/Clear/Report | ❌ Não implementados |
| Automação do host | ⚠️ Parâmetros globais expostos; sem os slots remapeáveis de automação das camadas do original |
| NRPN de parâmetros e MIDI Omni CC124/125 | ❌ Não implementados; RPN 0 e os CCs principais estão disponíveis |
| Importar/exportar padrões, lanes e arquivos `.mid` | ❌ Não implementado |
| Full RAM share mode | ⚠️ Biblioteca sempre compartilhada; sem opção separada |
| Envelope gráfico | ❌ Parâmetros editáveis por controles; sem gráfico do envelope |
| Interface VST2 e suporte a sistemas antigos | ❌ Release usa VST3/AU/LV2 e sistemas atuais |
| Entrada numérica direta nos knobs/faders | ❌ Sem o indicador de valor editável do original |
| Atalhos avançados e edição em bloco no sequenciador | ❌ Sem seleção/cópia/colagem/inserção de blocos; disponíveis edição por mouse, reset e troca de páginas |
| Modo Caps Lock para teclado físico, preset inicial e F1/manual | ❌ Não implementados |
| Opções de ativação de hotkeys, exibição de valores e mapa MIDI | ❌ Não implementadas |

## Compilar e testar

Requer CMake 3.22+, compilador C++17 e dependências de plataforma do JUCE. O CMake usa `~/Developer/JUCE`, aceita `-DJUCE_DIR=/caminho/JUCE` ou baixa JUCE 8.0.4.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel 3
```

No Linux, instale antes: `libasound2-dev libjack-jackd2-dev libfreetype6-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libgl1-mesa-dev libcurl4-openssl-dev`.

Os plugins ficam em `build/GuilhermeBolinha_artefacts/Release/`. Ferramentas de verificação:

```sh
./build/BolinhaArpTest_artefacts/Release/BolinhaArpTest /pasta/conteudo
./build/BolinhaHostTest_artefacts/Release/BolinhaHostTest '/caminho/Guilherme Bolinha.vst3'
./build/BolinhaRender_artefacts/Release/BolinhaRender /pasta/conteudo /tmp/bolinha-render 0 128 300
./build/BolinhaUiTest_artefacts/Release/BolinhaUiTest /tmp/bolinha-ui
```

`BolinhaRender --import setup.exe /pasta/destino` também importa o conteúdo sem abrir a interface. O teste de host usa a biblioteca incluída ou instalada e falha se os programas testados não gerarem áudio finito e não silencioso.

O workflow `Build and release` compila os três sistemas e gera seus instaladores. Execução manual gera artefatos de teste; uma tag `v*` publica a release somente depois de os três builds passarem.

Validação local em macOS arm64: 1366 patches renderizados, nenhum silencioso ou com amostras inválidas; importação de 44 arquivos idênticos à referência; teste de host VST3 e suíte de arp aprovados. O editor é revisado por renderização isolada das cinco abas nos tamanhos padrão e mínimo; a validação interativa do Standalone depende de acesso ao aplicativo.
