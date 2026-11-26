# NOTES — Cemu 웹 브라우저 이식 (WebAssembly + WebGPU)

진행 상황과 발견 내용을 시간 순서대로 기록하는 문서다. 최신 상태는 맨 위의 "현재 상태"를 보면 된다.

## 현재 상태

| 항목 | 상태 |
|---|---|
| 1단계: 분석과 계획 | ✅ 완료 (2026-11-26) |
| P1 네이티브 헤드리스 | ✅ 테스트 홈브루 부팅, 결과값이 호스트 기준값과 일치 (`scripts/build-native.sh`) |
| P2 wasm 헤드리스 | ✅ Node와 **브라우저(Chromium)** 에서 테스트 홈브루가 끝까지 실행, 출력이 네이티브와 일치 |
| P3 웹 프런트엔드 | 🚧 `web/index.html`: 폴더/파일 선택(업로드 없음, WORKERFS), Worker에서 실행, IDBFS에 설정·세이브, 로그 표시. 화면 출력 없음 |
| 원본 Cemu | `upstream/Cemu` git 서브모듈, upstream `2f49c079` (2026-11-20)에 고정 |
| Cemu 코드 수정 | `patches/cemu/0001-0006` (적용: `scripts/apply-patches.sh`) |

> **원칙:** 게임 이미지(`.wud/.wux/.wua/.rpx` 덤프 등), `keys.txt`, `otp.bin`, `seeprom.bin`, `mlc01/` 같은
> 콘솔 데이터는 저장소에 절대 넣지 않는다. `.gitignore`로 막아 두었다. 테스트용 홈브루는 **우리가 소스에서 직접
> 빌드한 것**만 쓰고, 소스만 커밋한다.

---

## 0. 저장소 구조와 변경 관리 방식

```
wii-u-su/
├── NOTES.md              ← 이 문서
├── upstream/Cemu/        ← 원본 Cemu (git 서브모듈, upstream 커밋에 고정, 직접 수정하지 않음)
└── patches/cemu/         ← Cemu에 대한 모든 변경 (번호 붙은 .patch 시리즈, 2단계부터)
```

- 서브모듈은 upstream URL을 가리키므로 서브모듈 안에서 만든 커밋은 push할 곳이 없다. 그래서 panda3ds/gaemboy와
  같은 방식으로 **변경은 패치 시리즈로 이 저장소에 커밋**한다.
  - 작업 흐름: 서브모듈 안에 로컬 브랜치 `wasm`을 만들어 주제별로 커밋 → `git format-patch`로
    `patches/cemu/NNNN-*.patch` 재생성 → 이 저장소에 커밋. 적용은 `git am patches/cemu/*.patch`.
  - 이렇게 하면 "upstream 커밋 X + 패치 N개"로 항상 재현 가능하고, upstream을 올릴 때 패치 단위로 rebase할 수 있다.
  - 나중에 Cemu fork 저장소를 만들 수 있게 되면 fork의 브랜치를 source of truth로 옮기고 패치는 생성물로만 둔다
    (panda3ds-webxr의 `docs/forks.md`와 같은 규칙).
- 새로 작성하는 코드(웹 프런트엔드, Emscripten 전용 구현, 빌드 스크립트)는 가능한 한 Cemu 트리 밖, 이 저장소에 둔다.
  Cemu 쪽 패치는 "플랫폼 추상화 지점에 새 구현을 끼울 수 있게 하는" 최소 변경으로 유지한다.

---

## 1. 선례 조사

### 1.1 panda3ds-webxr (`gagarinyury/panda3ds-webxr`, 내부 명칭 panda3ds-wasm, 마지막 커밋 2026-10-18)

Panda3DS(3DS)를 브라우저와 iOS WKWebView에서 돌린다. 핵심은 **런타임 ARM → WebAssembly JIT**.

| 주제 | 그들이 한 일 | Cemu에 주는 교훈 |
|---|---|---|
| 저장소 구조 | 3개 저장소: 웹/도구/문서 저장소 + Panda3DS fork(`wasm` 브랜치) + dynarmic fork(`wasm32`). `patches/*.patch`는 fork에서 **생성만** 하고 손으로 고치지 않음 | 우리도 같은 규칙(§0) |
| CPU | Panda3DS에는 Dynarmic JIT밖에 없음 → Dynarmic에 **wasm 백엔드**를 새로 작성 (`backend/wasm/`, IR → wasm 모듈). 블록마다 wasm 모듈을 만들어 `WebAssembly.Instance`로 올림 | Cemu도 이미 **IML이라는 중간표현 + x64/AArch64 백엔드** 구조라서 같은 방식(IML → wasm 백엔드)이 그대로 맞는다 |
| JIT 성능 | 블록 체이닝(`return_call_indirect` + 공유 함수 테이블), 인라인 메모리 접근(페이지 테이블), 인라인 VFP. 인터프리터식 디스패치 대비 5~10배 | 블록 연결, 메모리 접근 인라인화가 핵심 |
| WebKit 함정 | 살아 있는 `WebAssembly.Instance` 수에 비례하는 프레임당 비용, import마다 생기는 실행 메모리 → 블록 256개를 한 인스턴스로 묶고, 헬퍼는 import 대신 `call_indirect`로 호출 | JIT 설계 초기부터 "인스턴스 묶기 + 테이블 호출" 전제 |
| 검증 | 차등 테스트(같은 C 코드를 게스트/호스트로 컴파일해 결과 비교), 소프트 FP와 실제 FPU 비트 단위 비교 수백만 건 | PPC 인터프리터를 오라클로 삼아 JIT 차등 테스트 |
| 스레드 | 단일 스레드 웹 빌드 (`requestAnimationFrame`에서 코어 구동) | Cemu는 구조상 스레드가 필수라 그대로 따를 수 없음 (§2.5) |
| 메모리 | `-sALLOW_MEMORY_GROWTH -sINITIAL_MEMORY=256MB -sMAXIMUM_MEMORY=2GB`, fastmem 끔 | Wii U는 게스트 메모리만 1.4GB+라 다른 설계가 필요 (§4) |
| GPU | Panda의 OpenGL ES 렌더러를 WebGL2 위에서 사용 | 아래 gaemboy와 같은 교훈 |
| GL 성능 | `glBufferSubData`를 GPU가 읽는 중인 버퍼에 하면 ANGLE/Metal이 버퍼 전체를 복사 → 회당 30~40µs. orphaning(`glBufferData`)으로 15→60fps. **헤드리스 크롬은 이 문제를 숨김: 항상 보이는 창에서 측정** | WebGPU에서도 버퍼 업로드 전략(링 버퍼 + `writeBuffer`/매핑)을 처음부터 측정 |
| 큰 이미지 | 1~2GB 이미지를 메모리에 올리지 않고 1MB 단위 `Range` 요청 | Wii U 이미지(수~25GB)는 반드시 스트리밍 |
| 개발 도구 | Node 헤드리스 실행기, CDP 스크립트, 페이지가 dev 서버로 텔레메트리 전송 | 1단계부터 Node 헤드리스 러너 |

### 1.2 gaemboy-3ds-web (`DomenicoGonnelli/gaemboy-3ds-web`, 마지막 커밋 2026-11-19)

Azahar(Citra 후속)의 **libretro 코어**를 Emscripten으로 빌드. 개발 노트(`docs/DEV-NOTES-it.md`, 이탈리아어)가 매우 상세하다.

| 주제 | 그들이 한 일 | Cemu에 주는 교훈 |
|---|---|---|
| 출발점 | Azahar의 `ENABLE_LIBRETRO` 타깃(프런트엔드 없는 코어) 사용. 첫 부팅까지 Azahar 패치 **11개 파일, +86/−12줄** | Cemu에는 libretro 타깃이 없지만 `gui/interface/WindowSystem.h`라는 UI 추상화가 있고, 코어(`CemuCafe`)는 wx 헤더를 include하지 않는다 → 헤드리스 프런트엔드를 만들기 쉬운 편 |
| CPU | **JIT 없이 인터프리터(FastInterp)**. 수정 없이 wasm으로 컴파일됨. 네이티브 속도의 67~75% 유지, 데스크톱 브라우저에서 3DS 실시간의 130~145% | **인터프리터의 wasm 변환 손실은 약 25~33%** 라는 실측치. Wii U는 요구 연산량이 훨씬 커서 인터프리터만으로는 실시간 불가가 거의 확실 → JIT이 필수 로드맵 항목 |
| 스레드 | `-pthread`, `SharedArrayBuffer` + COOP/COEP 헤더, `PTHREAD_POOL_SIZE=hardwareConcurrency+8`, 정적 호스트용 `coi-sw.js`(헤더 주입 서비스 워커) | Cemu도 같은 구조. pthread 풀은 **미리** 만들어 두어야 함 |
| 데드락 | `std::async`가 요청마다 새 스레드 생성 → 풀이 바닥나면 메인 이벤트 루프가 막혀 있어 새 워커를 못 만듦 → 데드락. 비동기 작업을 인라인 실행으로 바꿔 해결 | Cemu에도 `std::async` 21곳, `std::thread` 128곳. **스레드 생성 지점 전수 조사 + 풀 크기 산정 필수** |
| 미세 동기화 | 스캔라인마다 futex → 수백 번 대기. 작업 단위를 키워 +30% | 브라우저의 futex(`Atomics.wait`) 왕복 비용이 크다. 파이버를 스레드로 바꿀 때 전환 횟수가 성능을 좌우 (§2.5) |
| 예외 | `-fwasm-exceptions -sSUPPORT_LONGJMP=wasm` | Cemu도 `throw` 140곳 → 네이티브 wasm 예외 사용 |
| 메모리 | `-sINITIAL_MEMORY=1024MB -sMAXIMUM_MEMORY=4GB` | wasm32 4GB 상한까지 쓰는 설정이 실제로 동작함을 보여 줌 |
| 파일 | 사용자 파일을 `WORKERFS`로 마운트해 블록 단위로 읽음(3.5GB 파일을 0.7초에 열기), 세이브는 `IDBFS` | Wii U 이미지도 WORKERFS/File System Access/OPFS로 지연 로딩 |
| GPU | Azahar GL 렌더러(GL 4.3/GLES 3.2)를 WebGL2(GLES 3.0)로 낮춤: 텍스처 버퍼 → 2D 텍스처, geometry shader 끔, `glCopyImageSubData` 대체, 셰이더 비동기 컴파일(`KHR_parallel_shader_compile`) + IndexedDB 캐시. **OffscreenCanvas를 워커로 넘겨 렌더링** | Latte → WebGPU에서도 (1) 비동기 파이프라인 생성 + 캐시, (2) 렌더 스레드가 캔버스를 소유하는 구조 |
| 소프트웨어 렌더러 | 프레임 시간의 80~90%를 차지해 3D 게임에는 부족 | Cemu에는 소프트웨어 렌더러가 없다. 하드웨어(WebGPU) 경로뿐 |

### 1.3 두 선례의 공통 결론

1. **"일단 인터프리터로 부팅 → 그 다음 렌더러 → 그 다음 JIT"** 순서가 둘 다 통했다.
2. 브라우저에서는 **기계어를 만들 수 없으므로** "JIT"은 곧 "게스트 코드 → wasm 모듈 생성"이다.
3. 그래픽 성능 문제의 대부분은 드라이버/ANGLE 경로의 버퍼 업로드와 동기 셰이더 컴파일에서 나왔다.
4. 헤드리스(Node) 러너와 측정 도구를 먼저 만든 것이 디버깅 속도를 크게 올렸다.

---

## 2. Cemu 구조 분석 (upstream `2f49c079`)

### 2.1 규모 (C/C++ 소스 줄 수, `wc -l`)

| 디렉터리 | 파일 | 줄 수 | 내용 |
|---|---:|---:|---|
| `src/Cafe` | 588 | 211k | 에뮬레이션 코어: CPU, GPU, OS HLE, IOSU, 파일 시스템 |
| `src/Common` | 40 | 32k | 공용 헤더(`precompiled.h`), 플랫폼별 코드 |
| `src/gui` | 121 | 32k | wxWidgets UI (`gui/wxgui`) + `gui/interface/WindowSystem.h` |
| `src/util` | 80 | 19k | Fiber, MemMapper, 소켓, 암호, 컨테이너 등 |
| `src/Cemu` | 34 | 17k | 온라인 기능(napi, nex), 로깅, Discord 등 |
| `src/input` | 61 | 10k | 컨트롤러 API |
| 기타 | | | `audio`(3k), `config`(3k), `imgui`(3k) |

주요 하위 모듈:

| 경로 | 줄 수 | 설명 |
|---|---:|---|
| `Cafe/HW/Espresso/Interpreter` | 6.6k | PowerPC 750CL(Espresso) 인터프리터 |
| `Cafe/HW/Espresso/Recompiler` | 24.6k | 재컴파일러: PPC → **IML**(중간표현) → x64 / AArch64 |
| `Cafe/HW/Latte/Core` | 19.4k | Latte GPU(R700 계열) 명령 처리, 텍스처/버퍼 캐시 |
| `Cafe/HW/Latte/LegacyShaderDecompiler` | 13.8k | R700 셰이더 → GLSL / MSL |
| `Cafe/HW/Latte/Renderer/{OpenGL,Vulkan,Metal}` | 5.8k / 14.2k / 7.3k | 렌더러 백엔드 |
| `Cafe/OS/libs/*` | — | coreinit, gx2, nn_*, sndcore2 등 Cafe OS 라이브러리의 HLE 구현 |
| `Cafe/IOSU` | — | IOSU(보안 프로세서 OS) HLE |

### 2.2 CPU (Espresso: PowerPC 750CL 3코어 @ 1.24GHz)

- **실행 모드** (`config/CemuConfig.h`의 `CPUMode`): 싱글코어 인터프리터, 싱글코어 재컴파일러, 멀티코어 재컴파일러(코어마다 호스트 스레드 1개). 커맨드라인 `--force-interpreter`, `--force-multicore-interpreter`.
- **인터프리터** (`Interpreter/*`): 일반 C++ switch/테이블 디스패치. 페어드 싱글(PS) 명령 포함. 플랫폼 의존성이 거의 없어 **wasm으로 바로 컴파일될 가능성이 높다**.
- **재컴파일러** (`Recompiler/*`):
  - `PPCRecompilerImlGen*.cpp`: PPC → IML 변환. `IML/`에 최적화기와 **레지스터 할당기**(2.2k줄)가 있다.
  - `BackendX64/`(x86 에미터 직접 구현), `BackendAArch64/`(xbyak_aarch64 사용): IML → 기계어.
  - 함수 단위 컴파일(`PPCFunctionBoundaryTracker`), 별도 스레드에서 비동기 컴파일.
  - 게스트 주소 → 호스트 코드 조회에 `ppcRecompilerDirectJumpTable`(코드 영역 0~0x10000000의 4바이트마다 포인터 1개)을
    쓰고, `MemMapper`로 **4MB 단위 지연 커밋**한다. 64비트 호스트 기준 최대 512MB의 가상 주소 예약.
  - 다른 백엔드를 끼울 자리가 이미 있다(x64/AArch64 분기). **wasm 백엔드는 IML을 입력으로 받는 세 번째 백엔드**로 넣는 것이 자연스럽다.
- **스케줄링** (`PPCScheduler.cpp`, `coreinit_Thread.cpp`): 게스트 스레드(OSThread)마다 호스트 **Fiber**를 하나 만든다.
  `ppcThreadQuantum = 45000` 명령마다 또는 HLE 함수가 블록될 때 `PPCCore_switchToScheduler` → `Fiber::Switch`.
  Fiber 구현은 `util/Fiber/FiberUnix.cpp`(`ucontext`의 `makecontext/swapcontext`, 스택 2MB)와 `FiberWin.cpp`.
- **HLE**: Cafe OS 라이브러리 함수는 C++로 구현되어 있고, 인터프리터/재컴파일러가 특수 명령으로 호출한다
  (`PPCInterpreterHLE.cpp`, `cafeExportRegister`). HLE 코드는 **게스트 메모리를 호스트 포인터로 직접** 읽고 쓴다
  (`MEMPTR<T>` 625곳, `MPTR` 2,046곳, `uint32be` 1,535곳, `memory_getPointerFromVirtualOffset` 217곳).
- **타이머** (`PPCTimer.cpp`): x86에서는 `__rdtsc`, 그 외에는 대체 경로.
- **FPU 모드**: `coreinit_Thread.cpp:30`에서 x86 MXCSR의 FTZ 비트를 켠다. 재컴파일러는 MXCSR 반올림 모드를 바꾼다.

### 2.3 GPU (Latte, AMD R700 계열)

- **명령 처리**: 게스트가 GX2 라이브러리(HLE, `Cafe/OS/libs/gx2`)로 PM4 명령을 링 버퍼에 쓰면, 전용 스레드
  `LatteThread`(`Latte/Core/LatteThread.cpp`, `std::thread`)가 `LatteCommandProcessor.cpp`에서 해석한다.
- **렌더러 추상화**: `Latte/Renderer/Renderer.h`의 `Renderer` 클래스와 전역 `g_renderer`(188곳에서 사용).
  백엔드 3개: OpenGL(GL 4.5, GLSL 450), Vulkan(GLSL → glslang → SPIR-V), Metal(MSL, macOS).
  CMake `ENABLE_OPENGL/ENABLE_VULKAN/ENABLE_METAL`로 끌 수 있고, 코어 쪽 코드도 `#if ENABLE_*`로 이미 분리되어 있다.
- **셰이더**: `LegacyShaderDecompiler`가 R700 셰이더를 분석해 GLSL(`LatteDecompilerEmitGLSL.cpp`) 또는
  MSL(`LatteDecompilerEmitMSL.cpp`)로 출력. **WGSL 출력기는 MSL 출력기를 본떠 추가**하는 것이 자연스럽다.
- **WebGPU 관점에서 중요한 발견**: **Metal 백엔드가 WebGPU의 가장 좋은 본보기**다.
  Metal도 geometry shader와 transform feedback이 없어서 Cemu는 GS를 **mesh shader**로, streamout을 버퍼 쓰기로
  우회한다(`MetalPipelineCompiler.cpp:292-348`, `MetalRenderer.cpp:1014-1055`). WebGPU에는 mesh shader도 없으므로
  GS는 **compute 셰이더 사전 패스**로 에뮬레이션해야 한다. 그 외 구조(명시적 파이프라인, 버퍼 할당기, 샘플러/뎁스 캐시)는
  Metal 백엔드의 파일 구성을 거의 그대로 따라갈 수 있다.
- **OSScreen**: GX2 없이 텍스트를 찍는 홈브루(OSScreen API)도 `LatteThread_HandleOSScreen`에서 **렌더러를 거쳐** 표시된다.
  즉 화면이 하나라도 나오려면 최소한의 렌더러가 필요하다.

### 2.4 UI (wxWidgets)와 진입점

- `src/main.cpp`: 설정 로드 → `CafeSystem::Initialize()` → `WindowSystem::Create()`(wx 메인 루프).
- `gui/interface/WindowSystem.h`가 UI와 코어 사이의 인터페이스이고, `gui/CMakeLists.txt`는 `ENABLE_WXWIDGETS`일 때만
  `wxgui`를 링크한다. 코어(`Cafe`, `Common`, `config`, `util`)에는 `wx/` include가 **하나도 없다**.
  → **헤드리스/웹 프런트엔드는 `WindowSystem` 구현 + 새 `main`만 쓰면 된다.** (단 CMake 옵션 설명에 "Currently required"라고
  되어 있어, 빌드 스크립트 쪽 정리는 필요.)
- 게임 실행 경로: `CafeSystem::PrepareForegroundTitle(titleId)` 또는
  **`CafeSystem::PrepareForegroundTitleFromStandaloneRPX(path)`**(홈브루용, RPX 해시로 임시 TitleId 생성) →
  `CafeSystem::LaunchForegroundTitle()`(새 `std::thread`에서 `OSSchedulerBegin(1 또는 3)`).

### 2.5 스레드 구조

| 스레드 | 생성 위치 | 역할 |
|---|---|---|
| UI/메인 | `main.cpp` / wx | 이벤트 루프 |
| 타이틀 런처 | `CafeSystem::LaunchForegroundTitle` | 스케줄러 시작 |
| PPC 스케줄러 ×1 또는 ×3 | `coreinit_Thread.cpp` `sSchedulerThreads` | 게스트 코어. 내부에서 게스트 스레드마다 Fiber 전환 |
| Latte | `LatteThread.cpp` | GPU 명령 처리와 렌더링 |
| 재컴파일러 | `PPCRecompiler.cpp` | 비동기 함수 컴파일 |
| IOSU 모듈들 | `Cafe/IOSU/*` | 각자 스레드 + 메시지 큐 |
| 오디오, 입력, 타이머, 셰이더 캐시 로더, `std::async` 작업 | 여러 곳 | — |

`std::thread` 사용 128곳(86개 파일), `std::async` 21곳(14개 파일), `thread_local` 7곳.
웹에서는 모두 Web Worker 기반 pthread가 되며, **pthread 풀은 시작 시 미리 채워 두어야 한다**(gaemboy §1.2의 데드락 참고).

### 2.6 파일 시스템

- `Cafe/Filesystem/fsc.cpp`: 가상 파일 시스템(FSC). 장치 구현:
  `fscDeviceHostFS`(호스트 디렉터리), `fscDeviceWud`(디스크 이미지 .wud/.wux), `fscDeviceWua`(ZArchive),
  `fscDeviceWuhb`(홈브루 번들 .wuhb), `fscDeviceRedirect`(그래픽 팩 등).
- `FST/`: Wii U 콘텐츠 볼륨 파서. 암호화된 이미지에는 `KeyCache`(사용자의 `keys.txt`)가 필요 → **홈브루 RPX/WUHB에는 키가 필요 없다**.
- 공유 폰트(`LoadSharedData`)는 없으면 로그만 남기고 계속 진행한다.
- 내부적으로 `std::filesystem`(`fs::`)과 boost를 사용. 웹에서는 Emscripten의 MEMFS/NODERAWFS/WORKERFS/OPFS(WasmFS) 위에서 동작시켜야 한다.

### 2.7 외부 의존성 (`vcpkg.json`, `CMakeLists.txt`)

필수: Threads, CURL, pugixml, RapidJSON, Boost(program_options, nowide, 그 외 헤더 전용 다수), libzip, glslang, zlib, zstd, OpenSSL(Crypto+SSL), glm, fmt ≥12.1, libpng, ZArchive, ih264d(번들).
선택: SDL3, wxWidgets, cubeb, hidapi, libusb, bluez, wayland, gamemode, Discord RPC.
`dependencies/ih264d`에는 x86/ARM SIMD 외에 **generic C 경로**(`ih264d_function_selector_generic.c`)가 있다.

---

## 3. Emscripten 빌드 걸림돌 목록

난이도: ★ 쉬움 / ★★ 보통 / ★★★ 어려움 / ★★★★ 매우 어려움(연구 수준).

### 3.1 JIT (재컴파일러)

| # | 걸림돌 | 위치 | 난이도 | 대응 |
|---|---|---|---|---|
| J1 | x64/AArch64 기계어 생성은 브라우저에서 불가 | `Recompiler/BackendX64`, `BackendAArch64` | — | 1단계에서는 **인터프리터만** 사용 (`CPUMode::SinglecoreInterpreter`이면 `PPCRecompiler_init`이 바로 return) |
| J2 | `ppcRecompilerDirectJumpTable`: 코드 영역 전체 크기의 포인터 테이블(wasm32에서 256MB)을 `MemMapper`로 지연 커밋 | `PPCRecompiler.h:130`, `PPCRecompiler.cpp:518` | ★★ | wasm에는 예약/지연 커밋이 없음 → 2단계 테이블(페이지별 할당) 또는 해시로 교체 |
| J3 | IML → wasm 백엔드 신규 작성 (블록/함수를 wasm 모듈로 생성, `WebAssembly.Module` 컴파일, 함수 테이블 등록, 블록 체이닝, 인라인 메모리 접근) | 신규 | ★★★★ | panda3ds-webxr의 dynarmic wasm 백엔드 설계(인스턴스 묶기, `call_indirect` 헬퍼, `return_call_indirect` 체이닝) 참고 |
| J4 | MXCSR 반올림 모드/FTZ 조작 | `PPCRecompiler.cpp:122`, `coreinit_Thread.cpp:30` | ★★ | wasm에는 반올림 모드 변경이 없음(항상 round-to-nearest, 비정규수 보존). 기본 모드가 아닌 경우는 소프트 FP 또는 보정 코드 |
| J5 | 생성된 wasm 모듈은 **만든 Worker에서만** 바로 쓸 수 있음. 멀티코어 모드에서 코어 스레드(Worker) 3개가 같은 코드를 쓰려면 모듈을 각 Worker에 전달해 인스턴스화해야 함 | 신규 | ★★★ | `WebAssembly.Module`은 `postMessage`로 공유 가능. 코어별 테이블 동기화 프로토콜 필요 |

### 3.2 메모리 매핑

| # | 걸림돌 | 위치 | 난이도 | 대응 |
|---|---|---|---|---|
| M1 | 게스트 4GB 주소 공간 전체를 호스트 가상 메모리에 **예약**하고 필요한 영역만 커밋 (`ReserveMemory(0x100000000)`) | `MMU.cpp:133`, `util/MemMapper/*` | ★★★ | wasm32 선형 메모리는 연속 하나뿐, 예약·보호·매핑이 없음. **회전 매핑**(§4.3)으로 해결 |
| M2 | `memory_base + 게스트주소` 형태의 직접 포인터 연산이 코드 전반에 퍼져 있음 (`memory_base` 92곳 + MEMPTR/betype 계열) | 전역 | ★★ | 회전 매핑은 32비트 래핑 덧셈이면 되므로 대부분 그대로 둔다. 단, 포인터 오버플로는 C++ UB이므로 변환 함수(`memory_getPointerFromVirtualOffset` 등)를 정수 연산으로 바꾸고 `memory_base +` 직접 사용처를 점검 |
| M3 | 매핑되지 않은 게스트 주소 접근은 네이티브에서 segfault(디버깅 신호)였지만 wasm에는 페이지 보호가 없음 | — | ★ | 디버그 빌드에 범위 검사 옵션 |
| M4 | 재컴파일러 인스턴스 데이터 예약 | `PPCRecompiler.cpp:687` | ★★ | J2와 함께 처리 |
| M5 | 호스트 포인터 크기 가정(Cemu는 64비트 전용) | 전역 | ★★ | 간단한 grep에서는 명시적 `sizeof(void*)==8` 가정이 거의 보이지 않았다. 실제로는 컴파일해 봐야 앎 (static_assert 323곳 중 일부) |

### 3.3 스레드와 파이버

| # | 걸림돌 | 위치 | 난이도 | 대응 |
|---|---|---|---|---|
| T1 | **Fiber**: `ucontext`(`makecontext/swapcontext`)가 Emscripten에 없음. 게스트 스레드 전환이 인터프리터/HLE의 깊은 호출 스택 한가운데서 일어남 | `util/Fiber/FiberUnix.cpp`, `coreinit_Thread.cpp` | ★★★ | 후보 A: **게스트 스레드마다 pthread 1개 + 바통 전달(세마포어)** — 계측 없음, 정확, 전환당 `Atomics.wait/notify` 비용. 후보 B: Emscripten fiber(**Asyncify 기반**, 계측 범위가 인터프리터+HLE 전체라 코드 크기·속도 손실 큼). 후보 C: JSPI/wasm stack-switching — fiber 용도로 쓸 수 있는지 미확인. **1단계는 A**로 시작하고 전환 비용을 측정 |
| T2 | `SharedArrayBuffer`에는 COOP/COEP 헤더 필요 | 배포 | ★ | gaemboy의 `coi-sw.js` 방식 |
| T3 | 스레드 수: 게스트 스레드 수(게임마다 수십 개) + 코어 스레드 + Latte + IOSU + 재컴파일러 + 오디오. 풀이 부족하면 데드락 | 전역 | ★★ | `PTHREAD_POOL_SIZE` 넉넉히 + 실행 중 스레드 수 계측 + `std::async` 지점 정리 |
| T4 | 메인 브라우저 스레드에서 블로킹 금지(`Atomics.wait` 불가) | `main` | ★★ | `-sPROXY_TO_PTHREAD`로 `main`을 워커에서 실행, 페이지는 표시와 입력만 |
| T5 | `pthread_setname_np`, 스레드 우선순위/affinity | `util/helpers`, `SetThreadName` | ★ | 스텁 |

### 3.4 그래픽 API

| # | 걸림돌 | 난이도 | 대응 |
|---|---|---|---|
| G1 | OpenGL 4.5 / Vulkan / Metal 모두 브라우저에서 직접 사용 불가 (WebGL2 = GLES 3.0 수준은 GL 4.5 기능 대부분 부족) | ★★★★ | **새 WebGPU 렌더러** (`Renderer` 구현). Metal 백엔드 구조를 본보기로 |
| G2 | 셰이더 언어: WGSL 필요 | ★★★ | `LatteDecompilerEmitMSL.cpp`를 본떠 **WGSL 출력기** 추가. (GLSL→SPIR-V→WGSL을 wasm 안에서 Tint/naga로 하는 방법은 바이너리가 커서 비추천) |
| G3 | Geometry shader, transform feedback(streamout) 없음 | ★★★★ | compute 사전 패스로 GS 에뮬레이션, streamout은 storage buffer 쓰기 |
| G4 | 논리 연산(logic op), 일부 텍스처 포맷, 뎁스 클램프, 폴리곤 모드 등 기능 부재 | ★★★ | 셰이더 에뮬레이션 또는 기능 감지 후 폴백. BC 압축은 데스크톱에서 `texture-compression-bc` 기능으로 대부분 가능 |
| G5 | WebGPU 객체는 만든 JS 컨텍스트(Worker)에 묶임 | ★★ | Latte 스레드(워커)가 디바이스와 `OffscreenCanvas`를 소유하도록 설계 |
| G6 | 파이프라인 생성 지연 | ★★ | `createRenderPipelineAsync` + IndexedDB 셰이더 캐시 (gaemboy의 비동기 셰이더 교훈) |
| G7 | 1단계에서도 GX2/OSScreen 호출 시 `g_renderer`가 필요 | ★ | **NullRenderer**(아무것도 그리지 않는 `Renderer`)로 우선 해결 |

### 3.5 플랫폼 의존 코드

| # | 걸림돌 | 위치 | 난이도 | 대응 |
|---|---|---|---|---|
| P1 | 아키텍처 감지가 x86_64/aarch64만 가정. x86 intrinsic(`_mm_*` 104곳/13파일), `__cpuid`, `__rdtsc` | `Common/precompiled.h`, `cpu_features.cpp`, `PPCTimer.cpp`, `fspinlock.h`, `aes128.cpp`, `LatteIndices.cpp` 등 | ★★ | `__wasm__` 분기 추가. 일부는 Emscripten의 SSE→wasm SIMD 헤더(`-msimd128 -msse4.2`)로 그대로 컴파일 가능, `_mm_setcsr`, `__cpuid`, `__rdtsc`는 스텁/대체 |
| P2 | 크래시 핸들러(`sigaction`, `SIGSEGV`) | `Common/ExceptionHandler` 계열 | ★ | 스텁 |
| P3 | `dlopen`(Vulkan/OpenGL 로더 등) | 2곳 | ★ | 해당 백엔드를 끄면 빠짐 |
| P4 | 네트워크: CURL, 소켓(`nsysnet`, `nlibcurl`, `napi`, `nex`, boss) | 여러 곳 | ★★ | 1단계는 스텁(오류 반환). 장기적으로 WebSocket 프록시가 필요하고 범위 밖으로 둠 |
| P5 | OpenSSL | IOSU crypto, 계정 등 | ★★ | libcrypto만 wasm으로 빌드(asm 없이). libssl은 네트워크와 함께 보류 |
| P6 | Boost 컴파일 라이브러리(program_options, nowide 등), glslang, libzip, zstd 등 | 의존성 | ★★ | vcpkg `wasm32-emscripten` 커뮤니티 트리플릿 또는 Emscripten 포트(zlib, libpng 등). glslang은 렌더러를 끄면 링크에서 빼도록 CMake 정리 |
| P7 | 오디오(cubeb, XAudio, DirectSound), 입력(SDL, hidapi, libusb, bluez, XInput) | `audio/`, `input/` | ★★ | 1단계 비활성. 이후 AudioWorklet + SAB 링, Gamepad API |
| P8 | wx 의존 CMake 구조 ("Currently required") | `CMakeLists.txt`, `src/CMakeLists.txt` | ★ | 헤드리스 실행 파일 타깃 추가 |
| P9 | 경로/설정(`ActiveSettings` 사용자 데이터, mlc 경로, 실행 파일 경로) | `config/` | ★ | 가상 FS의 고정 경로(`/cemu/...`)로 |

---

## 4. 메모리 4GB 한계와 Memory64 검토

### 4.1 Cemu가 실제로 매핑하는 게스트 메모리 (`MMU.cpp:111-127`)

| 영역 | 게스트 주소 | 크기 |
|---|---|---:|
| CODE_LOW0 / TRAMPOLINE / CODECAVE | 0x00010000 / 0x00E00000 / 0x01800000 | ~7MB |
| TEXT (모듈 코드) | 0x02000000–0x0E000000 | 192MB |
| CEMU / IOSU 전용 | 0x0E000000–0x10000000 | 32MB |
| **MEM2 (메인 메모리)** | 0x10000000–0x50000000 | **1GB** |
| OVERLAY (선택, 요청 시만) | 0xA0000000–0xBC000000 | 448MB |
| FGBUCKET / TILINGAPERTURE | 0xE0000000 / 0xE8000000 | 96MB |
| MEM1 / RPLLOADER / SHAREDDATA | 0xF4000000–0xFA000000 | 96MB |
| 코어별 LC, HIGHMEM | 0xFFC00000–, 0xFFFFF000– | 수십 KB |
| **합계** | | **약 1,447MB** (OVERLAY 포함 시 약 1,895MB) |

여기에 호스트 쪽 메모리(텍스처/버퍼 캐시의 CPU 사본, 셰이더 캐시, 파일 버퍼, JIT 메타데이터, 스레드 스택 2MB × 게스트 스레드 수)가 더해진다.

### 4.2 문제: 주소 공간이 넓게 흩어져 있다

게스트가 쓰는 주소는 0x00010000부터 0xFFFFFFFF까지 거의 4GB 범위 전체에 흩어져 있다.
네이티브 Cemu는 4GB를 **예약만** 하고 필요한 곳만 커밋해서 `memory_base + 게스트주소`를 그대로 쓴다.
wasm32 선형 메모리는 0부터 시작하는 **연속 영역 하나**이고 최대 4GB(65536페이지), 예약·보호 기능이 없다.
1:1 배치(`memory_base = 0`)를 하면 호스트의 정적 데이터·스택·힙이 게스트 영역과 겹치고, 최상단 영역(0xFFFFF000) 때문에 4GB를 전부 할당해야 한다.

### 4.3 제안: 회전(모듈러) 매핑 — wasm32에서 해결 가능

wasm32에서는 포인터가 32비트이므로 `호스트 = (게스트 + B) mod 2^32`로 놓으면 변환이 **덧셈 한 번**이고,
`memory_getVirtualOffsetFromPointer`(뺄셈)도 그대로 성립한다. 게스트 주소 공간에서 가장 큰 빈 구간
(0x50000000–0xA0000000, OVERLAY를 안 쓰면 0x50000000–0xE0000000)을 호스트 주소 공간의 **끝 쪽**으로 보내고,
호스트 0번지 근처는 게스트의 빈 곳에 오게 B를 고른다. 예: **B = 0x20010000**

| 영역 | 게스트 | 호스트 (wasm 선형 메모리) |
|---|---|---|
| FGBUCKET … HIGHMEM (상단 영역들) | 0xE0000000–0xFFFFFFFF | 0x00010000–0x2000FFFF |
| LOW0 … MEM2 (하단 영역들) | 0x00010000–0x50000000 | 0x20020000–0x70010000 |
| 호스트 정적 데이터·스택·힙 (`-sGLOBAL_BASE` 등으로 배치) | (게스트 빈 영역) | 약 0x70100000 이후 |
| OVERLAY (선택) | 0xA0000000–0xBC000000 | 0xC0010000–0xDC010000 (이 경우 호스트 힙 상한 약 1.25GB) |

- 호스트 `nullptr`(0)은 게스트 0xDFFF0000(빈 영역)에 해당하므로 "널 포인터 = 유효한 게스트 메모리" 충돌이 없다.
- 필요한 선형 메모리: 호스트 힙 1GB 기준 약 **2.8GB** (OVERLAY 사용 시 최대 약 3.4GB). wasm32 상한 4GB 안에 들어간다.
- 주의할 점(검증 필요):
  1. C++에서 포인터 오버플로는 UB → 변환은 `uintptr_t` 정수 연산으로 구현하고, `memory_base +`를 직접 쓰는 곳을 정리한다.
  2. wasm `load/store`의 상수 오프셋(memarg offset)은 33비트로 더해져 **래핑되지 않는다**. 상단 영역이 호스트 하단에 있으므로
     영역 경계를 넘는 접근만 문제가 되며, 정상적인 게스트 접근은 영역 안에서 끝난다.
  3. Emscripten 힙(sbrk/malloc)이 게스트 영역을 침범하지 않도록 힙 시작 위치와 상한을 고정해야 한다.
     (`-sGLOBAL_BASE`를 크게 잡거나, 게스트 영역을 처음부터 확보한 뒤 sbrk 상한을 둔다. 2단계 초반에 프로토타입으로 확인.)
  4. 선형 메모리 2.8GB를 처음에 확보해야 하므로 **모바일 브라우저에서는 실패할 가능성이 높다.** 데스크톱 우선.
     (Windows는 wasm 메모리 증가 시 커밋 차지가 실제로 잡힐 수 있음 — 측정 필요.)
  5. 게스트가 빈 영역에 잘못 쓰면 segfault 대신 호스트 메모리를 덮어쓸 수 있다 → 디버그 빌드 범위 검사(M3).

### 4.4 Memory64는 필요한가?

| | wasm32 + 회전 매핑 | Memory64 (wasm64) |
|---|---|---|
| 게스트 4GB 공간 | 위 방법으로 수용 | `memory_base`를 4GB 위에 두면 네이티브와 동일한 1:1 배치 |
| 호스트 힙 여유 | 약 1~1.5GB (OVERLAY 사용 시 1.25GB) | 사실상 제한 없음(브라우저 상한 16GB 수준) |
| 포인터 크기 | 32비트 → Cemu의 64비트 가정을 고쳐야 할 수 있음 | 64비트 → Cemu 코드와 그대로 맞음 |
| 성능 | 가드 페이지로 경계 검사 제거 → 빠름 | 명시적 경계 검사 → 느려짐 (SpiderMonkey 팀 분석상 상당한 손실) |
| 브라우저 | 모든 주요 브라우저 | Chrome 133+, Firefox 134+. **Safari 미지원**(WebKit에서 작업 중, 2026년 11월 기준 출시 확인 못 함) |
| JIT 생성 코드 | i32 주소 | i64 주소(명령 크기 증가) |

**결론: 1~3단계는 Memory64 없이 wasm32로 간다.** 게스트 메모리(1.45GB)와 일반적인 호스트 사용량을 합쳐도 4GB 안에 들어가고,
성능과 Safari 호환이 wasm32 쪽이 낫다. Memory64는 다음 경우의 **대안**으로 남겨 둔다:
(a) 호스트 메모리(텍스처 캐시 등)가 실제 게임에서 1GB를 크게 넘는 것이 측정되었을 때,
(b) 32비트 포인터 때문에 고칠 곳이 예상보다 많을 때.
빌드 시스템은 처음부터 `-sMEMORY64` 빌드도 시도해 볼 수 있게 옵션화해 둔다.

---

## 5. 최소 빌드 계획: UI 없이 CPU 인터프리터만으로 홈브루 부팅

### 5.1 목표와 성공 기준

- 실행 파일 `cemu-headless`: wx, 렌더러(NullRenderer만), 오디오, 입력, 네트워크 없음.
- `--force-interpreter`(싱글코어 인터프리터)로 **우리가 직접 빌드한 테스트 RPX**를 부팅한다.
- 테스트 RPX(`tests/homebrew/`, wut 툴체인, 소스만 커밋):
  1. `OSReport`로 문자열 출력 (Cemu 로그에 찍힘),
  2. 정수 연산 루프와 체크섬, 부동소수점/페어드 싱글 연산,
  3. `OSCreateThread`로 스레드 2개 생성/조인 (Fiber 대체 검증),
  4. 파일 읽기(`/vol/content`),
  5. 종료 코드 반환.
- **성공 기준**: (a) 네이티브 헤드리스와 wasm 헤드리스(Node)의 로그·체크섬이 같다, (b) 정상 종료, (c) 브라우저 페이지에서 같은 로그가 나온다.

### 5.2 단계

| 단계 | 내용 | 산출물 |
|---|---|---|
| 1-a | **네이티브(Linux x86_64) 헤드리스 빌드**: 새 `main`(설정 기본값, 인자로 RPX 경로) + `WindowSystem` 스텁 + `NullRenderer` + CMake 옵션 `CEMU_HEADLESS`(wx/GL/Vulkan/cubeb/SDL/hidapi/libusb/Discord/Wayland/Bluez/Gamemode 끔). 플랫폼 문제를 wasm 문제와 분리하려고 먼저 네이티브로 함 | `patches/cemu/0001-…` CMake 옵션, `0002-…` NullRenderer, 헤드리스 프런트엔드 소스(이 저장소) |
| 1-b | 테스트 홈브루 작성·빌드 (devkitPro `devkitppc` + `wut` Docker 이미지) | `tests/homebrew/hello/` 소스와 빌드 스크립트 (바이너리는 커밋하지 않음) |
| 1-c | 네이티브 헤드리스에서 1-b 부팅 → 기준 로그 확보 | `tests/expected/*.log` |
| 2-a | 의존성 wasm 빌드: zlib/libpng(Emscripten 포트), fmt, pugixml, rapidjson, glm, zstd, libzip, ZArchive, Boost(필요 최소), OpenSSL libcrypto(no-asm). curl/glslang은 헤드리스에서 제외 | `scripts/build-deps-wasm.sh` |
| 2-b | Cemu 코어 패치 (각각 별도 패치 파일): `precompiled.h`의 `__wasm__` 분기와 intrinsic 대체, `PPCTimer`의 시계 대체, `MemMapper` Emscripten 구현 + 회전 매핑(§4.3), `FiberEmscripten.cpp`(pthread 바통 방식), 크래시 핸들러/소켓/CURL 스텁 | `patches/cemu/00NN-*.patch` |
| 2-c | Emscripten 링크 옵션: `-pthread -sPROXY_TO_PTHREAD -sPTHREAD_POOL_SIZE=… -fwasm-exceptions -sSUPPORT_LONGJMP=wasm -sINITIAL_MEMORY/-sMAXIMUM_MEMORY(≤4GB) -sSTACK_SIZE` + Node용 `-sNODERAWFS` | `scripts/build-cemu-wasm.sh` |
| 2-d | **Node에서 실행**(헤드리스, 호스트 파일 직접 접근) → 1-c 기준 로그와 비교 | `scripts/run-node.sh`, 비교 스크립트 |
| 2-e | 브라우저 최소 페이지: 파일 선택 → MEMFS/WORKERFS 마운트 → 로그를 `<pre>`에 출력. COOP/COEP 개발 서버 | `web/` |

### 5.3 1단계 범위에서 일부러 하지 않는 것

화면 출력(OSScreen 포함), GX2 그래픽, 오디오, 입력, 멀티코어, JIT, 네트워크, 세이브 영속화, 디스크 이미지(.wud/.wux) 지원.

### 5.4 위험 요소

- 인터프리터 속도: gaemboy 실측으로 보면 wasm 변환 손실은 25~33% 정도다. Cemu 인터프리터 자체가 재컴파일러보다 훨씬 느리므로
  홈브루 부팅에는 충분하지만 상용 게임 실시간 실행은 기대하지 않는다. (Cemu 인터프리터의 네이티브 MIPS는 아직 측정하지 않았다 — 1-c에서 측정.)
- Fiber → pthread 바통 방식의 전환 비용: 45,000 명령 퀀텀 + HLE 블로킹마다 전환한다. 1-c/2-d에서 초당 전환 횟수와 전환당 시간을 계측한다.
- 헤드리스에서도 IOSU/coreinit 초기화가 설정 파일·mlc 디렉터리 구조를 기대할 수 있음 → 가상 FS에 빈 mlc 골격 생성.

---

## 6. 다음에 할 일 (2단계 첫 작업)

1. [ ] 네이티브 Linux 의존성 설치/빌드 환경 확인 (vcpkg 또는 시스템 패키지)
2. [ ] `CEMU_HEADLESS` CMake 옵션 패치 + 헤드리스 `main` + `WindowSystem` 스텁
3. [ ] `NullRenderer`
4. [ ] wut 테스트 홈브루 소스 작성, Docker로 빌드
5. [ ] 네이티브 헤드리스 부팅, 기준 로그와 인터프리터 MIPS 측정
6. [ ] emsdk 설치, 회전 매핑 프로토타입(작은 C 프로그램으로 `GLOBAL_BASE`/sbrk 상한/3GB 선형 메모리 동작 확인)
7. [ ] 이후 §5.2의 2-a부터

---

## 7. 전체 로드맵

| 단계 | 내용 | 난이도 | 비고 |
|---|---|---|---|
| **P0** | 분석, 계획, 선례 조사 | — | ✅ 이 문서 |
| **P1** | 네이티브 헤드리스 빌드 (wx 제거, NullRenderer, 테스트 홈브루) | ★★ | wasm 이전에 이음새를 확인 |
| **P2** | wasm 헤드리스 인터프리터 부팅 (Node → 브라우저 로그 페이지) | ★★★ | 메모리 회전 매핑, Fiber 대체, 의존성 wasm 빌드가 핵심 |
| **P3** | 웹 프런트엔드 기반: 파일 열기(WORKERFS/File System Access), OPFS/IndexedDB 영속화(세이브·mlc), COOP/COEP 배포, 로그·텔레메트리 | ★★ | gaemboy 구조 참고 |
| **P4a** | 최소 화면 출력: WebGPU 디바이스 + 스왑체인, 프레임버퍼/OSScreen 표시 (TV + GamePad 두 화면) | ★★ | Latte 스레드가 `OffscreenCanvas` 소유 |
| **P4b** | WebGPU 렌더러 본체: Metal 백엔드를 본보기로 한 `Renderer` 구현, WGSL 셰이더 출력기, 텍스처/버퍼 캐시, 비동기 파이프라인 + 캐시 | ★★★★ | 프로젝트에서 가장 큰 작업. 2D 홈브루 → 간단한 3D → 상용 게임 순 |
| **P4c** | WebGPU에 없는 기능: GS(compute 에뮬레이션), streamout, logic op, 특수 포맷 | ★★★★ | 게임 호환성 대부분이 여기서 결정 |
| **P5** | PPC → wasm JIT: IML 입력 wasm 백엔드, 블록 체이닝, 인라인 메모리 접근, 인터프리터 대비 차등 테스트 | ★★★★ | 실시간 실행의 전제 조건. panda3ds-webxr 설계 참고 |
| **P6** | 오디오: AudioWorklet + SAB 링 버퍼, 동적 리샘플 | ★★ | panda3ds의 버퍼 고갈 대응 참고 |
| **P7** | 입력: 키보드, Gamepad API, GamePad 터치 화면(포인터 이벤트), 모션은 선택 | ★★ | |
| **P8** | 성능: 멀티코어(코어 3개 = Worker 3개, JIT 모듈 공유), 셰이더 캐시 사전 컴파일, 버퍼 업로드 최적화, 실제 브라우저 창에서 측정 | ★★★ | |
| **P9** | (선택) Memory64 빌드 비교, 모바일 대응, 온라인 기능(WebSocket 프록시) | ★★★ | 범위 밖 후보 |

현실적인 기대치: P2까지는 "기술적으로 가능"을 확인하는 단계이고, 상용 게임이 의미 있는 속도로 돌려면 **P4b + P5가 모두 필요**하다.
Wii U(3코어 1.24GHz + R700급 GPU)는 3DS보다 요구 성능이 훨씬 크므로 처음 목표는 홈브루와 가벼운 2D 타이틀로 잡는다.

---

## 8. 작업 로그

### 2026-11-26 — 1단계 분석

- Cemu를 `upstream/Cemu` 서브모듈로 추가 (upstream `2f49c079`). Cemu의 하위 서브모듈(vcpkg, Vulkan-Headers 등)은 아직 받지 않음.
- 선례 두 개를 받아 읽음: `gagarinyury/panda3ds-webxr`(README, docs/forks.md, engineering-log, build-core.sh, 패치 목록),
  `DomenicoGonnelli/gaemboy-3ds-web`(README, build.sh, DEV-NOTES-it.md, 패치 목록). 저장소에는 넣지 않음.
- Cemu 소스 분석 (§2), 걸림돌 목록 (§3), 메모리 설계 (§4), 최소 빌드 계획 (§5), 로드맵 (§7) 작성.
- 주요 발견:
  - 코어는 wx와 분리되어 있다(`WindowSystem` 인터페이스) → 헤드리스 프런트엔드가 쉽다.
  - 렌더러 백엔드는 이미 CMake/`#if`로 선택 가능하다 → WebGPU 백엔드를 네 번째로 추가할 자리가 있다.
  - **Metal 백엔드가 WebGPU 백엔드의 가장 좋은 본보기**다 (GS/streamout 없는 환경을 이미 우회하고 있음).
  - 재컴파일러는 IML 중간표현 위에 백엔드가 2개 있는 구조 → wasm 백엔드를 세 번째로 넣으면 된다.
  - 가장 까다로운 플랫폼 문제는 **4GB 게스트 주소 공간 예약**과 **ucontext Fiber**. 전자는 wasm32 회전 매핑으로 풀 수 있고
    Memory64는 당장 필요하지 않다고 판단.
- 검증하지 않은 가정 (2단계에서 확인): 회전 매핑에서 Emscripten 힙 배치 제어, Fiber 대체의 전환 비용,
  Cemu 코드의 64비트 포인터 가정의 실제 양, Cemu 인터프리터 속도.

### 2026-11-26 — P1 네이티브 헤드리스, P2 wasm 빌드

**P1 (네이티브, 완료)**
- `frontend/headless/`: wx 없는 `main`, `WindowSystem` 스텁, `NullRenderer`(그리지 않는 `Renderer` 구현, 텍스처/셰이더/FBO/쿼리 더미).
  Latte 스레드는 게임과 관계없이 항상 렌더러를 요구하므로 NullRenderer가 필요했다. 나중에 WebGPU 렌더러가 이 자리에 들어간다.
- Cemu 패치 0001: `CEMU_FRONTEND_DIR`로 외부 프런트엔드를 빌드 (wx일 때만 X11/GTK3 요구).
- Cemu 패치 0002: 그래픽 백엔드가 하나도 없을 때 생기는 문제 3개 (기본 API assert, `VulkanRendererConst`, `robin_hood` include).
- `tests/homebrew/hello`: devkitPro Docker 이미지(devkitPPC+wut)로 빌드하는 테스트 프로그램. CRC32, 정수 연산, 부동소수점, 스레드 2개, printf.
- 결과: `--force-interpreter`로 끝까지 실행, 결정적인 출력 3개(crc32, int, threads)가 호스트에서 컴파일한 같은 C 코드의 결과와 일치.
  (홈브루가 main에서 return한 뒤 프로세스 종료 신호가 오지 않는 문제는 남아 있음 — 시간 제한으로 종료.)

**P2 (wasm, 진행 중)**
- 의존성 11개를 wasm32로 빌드 (`scripts/build-deps-wasm.sh`). Boost는 asio/interprocess를 빼야 boost.context 어셈블리를 피할 수 있다.
- 컴파일 에러는 예상보다 훨씬 적었다: 바이트 스왑/시계 등 플랫폼 분기, x86 intrinsic 4개, ih264d generic 경로, `.S` 리소스 임베딩(→ `#embed`),
  64비트 소수 테이블 하나. 링크 에러는 예상대로 `ucontext`(Fiber)뿐.
- **Fiber (§3.3 T1)**: 후보 A(게스트 스레드마다 pthread + 바통 전달)로 구현 (`FiberEmscripten.cpp`).
  - 발견 1: 에뮬레이션 코어를 따라다녀야 하는 thread_local이 4개 있다 → `CEMU_FIBER_LOCAL`로 바꾸고 전환 때 넘겨준다.
  - 발견 2: 스케줄러 락(재귀 pthread mutex)을 잡은 채로 Fiber를 전환하고 다른 Fiber가 푼다 → "Fiber=스레드"에서는 데드락.
    소유자 없는 락(atomic wait/notify)으로 교체.
- **메모리 (§4.3 회전 매핑)**: 실제로 구현. `CEMU_WASM_GUEST_BASE = 0x20010000`, Emscripten 데이터는 `-sGLOBAL_BASE=1793MB`(0x70100000)부터,
  `-sINITIAL_MEMORY=3072MB`. `MemMapper`는 할 일이 거의 없어졌다. 포인터 범위 assert 3곳을 `MMU_IsInPPCMemorySpace`로 교체.
- **포인터 오버플로 UB (§4.3 주의점 1이 실제로 발생)**: `coreinit_FG.cpp`의 memset이 out-of-bounds. printf를 넣으면 사라지는
  전형적인 최적화 문제였다. LLVM이 `memory_base + guest`가 넘치지 않는다고 가정한다. 해결: Emscripten에서 `memory_base`를
  `GuestMemoryBase` 클래스로 바꿔 `operator+`가 정수 덧셈을 하게 함 (패치 0006). 90여 곳의 사용처는 그대로.
- 디버깅 요령: `EXTRA_FLAGS=-g BUILD=build-wasm-dbg scripts/build-wasm.sh`로 DWARF 빌드 후 `emsymbolizer <wasm> <offset>`으로 줄 번호 확인.
- **결과**: Node에서 `hello.rpx`가 끝까지 실행, 결정적 출력 5줄이 네이티브와 동일, `title exited with status 0`으로 정상 종료.
- 속도(같은 머신, 인터프리터): 정수 루프 네이티브 128ms → wasm 420ms, 부동소수점 4.6s → 15.0s. 약 **3.3배 느림**.
  gaemboy의 FastInterp(손실 25~33%)보다 손실이 크다. Cemu 인터프리터의 디스패치 구조 때문으로 보이며, 어차피 JIT(P5)이 필요하다.

### 2026-11-26 — 브라우저에서 첫 실행

- `WASM_TARGET=web scripts/build-wasm.sh` → `scripts/package-web.sh` → `scripts/serve-web.py` (COOP/COEP 헤더) → http://localhost:8080/
- 구조: 페이지 → `web/cemu-worker.js`(Worker) 안에서 모듈 실행 → pthread 작업자들은 그 Worker가 띄움(중첩 Worker).
  게임은 `WORKERFS`로 마운트(파일을 복사하지 않고 필요할 때 조각 단위로 읽음), `/cemu`는 IDBFS(설정·mlc01·세이브, 10초마다 동기화).
  `gameProfiles`와 오픈 소스 대체 폰트(`resources/sharedFonts`)는 `cemu-web.data`(27MB)로 함께 배포.
- 함정: Worker 안에서 MODULARIZE 모듈을 띄우면 pthread 작업자가 Worker 스크립트 자체를 로드하려 한다 → `mainScriptUrlOrBlob` 지정
  (그리고 `INCOMING_MODULE_JS_API`에 추가해야 함).
- 확인: `tests/web/run-page.js`로 헤드리스 Chromium에서 페이지를 열고 파일 선택 → 실행 → 로그 확인. 출력이 네이티브와 동일.

### 참고 링크

- Cemu: https://github.com/cemu-project/Cemu
- panda3ds-webxr: https://github.com/gagarinyury/panda3ds-webxr
- gaemboy-3ds-web: https://github.com/DomenicoGonnelli/gaemboy-3ds-web
- Memory64 지원 현황: https://caniuse.com/wf-wasm-memory64 , https://spidermonkey.dev/blog/2025/01/15/is-memory64-actually-worth-using.html
- Emscripten fiber(Asyncify 기반): https://emscripten.org/docs/api_reference/fiber.h.html
