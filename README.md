# wii-u-su

Wii U 에뮬레이터 [Cemu](https://github.com/cemu-project/Cemu)를 WebAssembly + WebGPU로 브라우저에서 실행하려는 실험 프로젝트.

- 진행 상황, 분석, 계획: [NOTES.md](NOTES.md)
- 원본 Cemu: `upstream/Cemu` (git 서브모듈) — `git submodule update --init upstream/Cemu`
- Cemu 변경 사항: [patches/cemu/](patches/cemu/)

게임 이미지, 키(`keys.txt`), 콘솔 덤프(`otp.bin`, `seeprom.bin`, `mlc01`)는 포함하지 않으며 저장소에 넣지 않는다.
