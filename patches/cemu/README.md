# Cemu 패치 시리즈

`upstream/Cemu`(서브모듈, upstream 커밋에 고정)에 적용하는 변경을 번호 순서의 `.patch` 파일로 보관한다.

- 적용: `git -C upstream/Cemu am ../../patches/cemu/*.patch`
- 작성: 서브모듈 안의 로컬 브랜치 `wasm`에서 주제별로 커밋한 뒤
  `git -C upstream/Cemu format-patch -o ../../patches/cemu <기준 커밋>` 으로 다시 생성한다. 패치 파일을 손으로 고치지 않는다.
- 패치 하나 = 주제 하나 (예: CMake 헤드리스 옵션, NullRenderer, MemMapper Emscripten 구현).
- 기준 커밋은 `NOTES.md`의 "현재 상태"에 적는다.

아직 패치 없음 (1단계는 분석만).
