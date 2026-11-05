# The Last of Us Remastered — Android/Bionic

**Status: etapa 2 (CPU FEX ARM64 + ponte de renderização AHB em integração). Sem APK ou gameplay Android validados.**

This port must preserve the TLOU-specific runtime and Vulkan changes already in the repository. We use the other Android projects for portable infrastructure, not as a substitute for TLOU graphics.

## Integration sources

| Existing port | What to reuse | Status |
|---|---|---|
| erickmendes22/gow3_android_daedalus_fork, branch android | ARM64 FEX ABI, Android/Bionic build, native renderer bridge, driver selection, PKG, launcher | Native adaptation pending |
| erickmendes22/uncharted-android | Naughty Dog engine compatibility, worker scheduling, ARM64 graphics and input patterns | Source review pending |
| erickmendes22/iss_android | PS4 imports, controller integration and game-specific frontend patterns | Source review pending |
| This repository | CUSA00552 prepared image, verified patch definitions, TLOU graphics and runtime fixes | Stage 1 implemented |

## Implemented

android/tlou_bionic.py is a Python 3 standard-library game preparation tool. It checks TITLE_ID=CUSA00552 in both base and update, makes a read-only overlay in app-private storage, uses this repository's own prepare.py, link_modules.py and content_profile.py, and writes the boot-linked.bin and content.bin files. It invokes the existing byte-signature-checked G3PATCH2 generator and never applies a patch for an unknown executable version. The optional flashlight patch needs --flashlight.

Example using a legally dumped game:

    python3 android/tlou_bionic.py --source . \
      --game /path/to/CUSA00552 \
      --update /path/to/CUSA00552-patch \
      --out /path/to/private-preparation

The --out directory must support symlinks (app-private Android storage). Input game folders are not modified. If an update has dvdroot_ps4 content, this tool refuses it until native app0 content overlay support is ready, rather than running a mixed game/update.

## Etapa 2 — código integrado na branch experimental

- CPU: `gow3_pc/src/guest_cpu.h`, `runtime_cpu.c`, `cpu/guest_cpu_fex.cpp` e `host_call_arm64.S` portados do GoW3 Bionic; o loader do TLOU agora encaminha inicializadores, callbacks, importações e entrada de jogo via `guest_cpu_call` / `guest_cpu_host_function`.
- Bionic: gerenciamento de thread, GS/TLS e endereços guest baixos; tratamento de SIGSEGV e frame de exceção ARM64; áudio Android sem SDL pelo mixer `runtime_audio_bionic.c` com saída PCM/FIFO.
- GPU do TLOU preservada: selecionador Vulkan nativo (`vk_driver_loader`), janela Android sem SDL, protocolo de apresentação AHardwareBuffer, swapchain AHB, ownership de filas Vulkan e controle de fences no presenter; Vulkan do TLOU não foi substituído pelo renderer de GoW3.
- Compilação nativa: `android/bionic/build_deps.sh` prepara bibliotecas ARM64, `build_fex.sh` compila o FEX embutido, `build_gpu.sh` compila o renderer específico e `build_runtime.sh` liga a camada nativa Bionic. Esses scripts ainda não constituem um APK.
- CI: `android-cpu-source.yml` usa Clang/NDK ARM64 real para validar os módulos sem dependências externas de FFmpeg e a ponte AHB. `runtime_mp3.c` e `runtime_videodec.c` precisam do FFmpeg Android compilado antes da validação nativa final.

A preparação do jogo (`tlou_bionic.py`) está separada dos componentes binários. O loader do GoW3 não pode simplesmente substituir o do TLOU: as opções específicas (`--low-image`, `--guest-path`, `--app0-overlay`) e os patches continuam sendo preservados.

## Comandos de build nativa ARM64 (infraestrutura, não APK)

```sh
export TLOU_NDK=/path/to/android-ndk-r29
bash android/bionic/build_deps.sh
bash android/bionic/build_fex.sh
bash android/bionic/build_gpu.sh
bash android/bionic/build_runtime.sh --link-only
```

A etapa seguinte é validar a compilação/link reais do renderizador completo, solucionar as diferenças de HLE e integrar uma Activity Android dedicada para inicializar o jogo e apresentar o frame. Não declarar boot, renderização nem desempenho antes do teste real.

## Bloqueios restantes para o primeiro APK funcional

1. O loader TLOU agora contém a interface FEX, mas a compilação e ligação completas com o backend C++ do FEX ainda precisam ser verificadas; a transferência das rotinas específicas de red-zone e a compatibilidade de inicialização do jogo em ARM64 não foram validadas em dispositivo.
2. O renderizador Vulkan é o do TLOU, agora com suporte inicial ao protocolo de apresentação AHardwareBuffer. A compilação completa de `libgow3gpu.so`, sincronização de readbacks, shaders, Videodec2 e tratamento de formatos precisam ser confirmados sem regressões.
3. Faltam o launcher Android específico do TLOU, carregamento automático de payload, seleção de diretório e atualização, interação com o áudio PCM e controles, auditoria dos ELF gerados e assinatura do APK.

**Never publish an Android APK that is merely a rebranded GOW3 binary.** The first useful APK milestone needs the actual TLOU ARM64 loader/renderer to compile and pass native-link validation.

## No-game tests

    python3 -m unittest discover -s android/tests -v
    python3 -m compileall -q android

Tests create tiny synthetic SFO files and do not require or include copyrighted game files.
