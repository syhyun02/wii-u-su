# wii-u-su

Wii U 에뮬레이터 [Cemu](https://github.com/cemu-project/Cemu)를 WebAssembly + WebGPU로 브라우저에서 실행하려는 실험 프로젝트.

- 진행 상황, 분석, 계획: [NOTES.md](NOTES.md)
- 원본 Cemu: `upstream/Cemu` (git 서브모듈) — `git submodule update --init upstream/Cemu`
- Cemu 변경 사항: [patches/cemu/](patches/cemu/)

게임 이미지, 키(`keys.txt`), 콘솔 덤프(`otp.bin`, `seeprom.bin`, `mlc01`)는 포함하지 않으며 저장소에 넣지 않는다.

## 지금 해볼 수 있는 것 (화면 출력 없음, 로그만)

```bash
scripts/apply-patches.sh
source ~/emsdk/emsdk_env.sh
scripts/build-deps-wasm.sh
WASM_TARGET=web BUILD=build-wasm-web scripts/build-wasm.sh
scripts/package-web.sh
scripts/serve-web.py 8080   # http://localhost:8080/ (Chrome/Edge 권장)
```
