#!/bin/bash
# Builds Cemu's third-party dependencies for wasm32-emscripten (with pthreads + wasm exceptions)
# into $PREFIX (default: build-wasm-deps/prefix). Needs an activated emsdk.
#   usage: scripts/build-deps-wasm.sh [package ...]   (default: all)
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
WORK="${WORK:-$ROOT/build-wasm-deps}"
PREFIX="${PREFIX:-$WORK/prefix}"
SRC="$WORK/src"
JOBS="${JOBS:-$(nproc)}"
mkdir -p "$SRC" "$PREFIX"

export CFLAGS="-O2 -pthread -fwasm-exceptions -sSUPPORT_LONGJMP=wasm"
export CXXFLAGS="$CFLAGS"
export LDFLAGS="-pthread -fwasm-exceptions -sSUPPORT_LONGJMP=wasm"

CMAKE_COMMON=(-G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$PREFIX" -DCMAKE_PREFIX_PATH="$PREFIX"
  -DCMAKE_FIND_ROOT_PATH="$PREFIX" -DBUILD_SHARED_LIBS=OFF -DCMAKE_POLICY_VERSION_MINIMUM=3.5
  "-DCMAKE_C_FLAGS=$CFLAGS" "-DCMAKE_CXX_FLAGS=$CXXFLAGS")

fetch() { # url dir
  local url="$1" dir="$2"
  [ -d "$SRC/$dir" ] && return
  echo "== fetch $dir"
  case "$url" in
    *.git@*) git clone -q --depth 1 -b "${url##*@}" "${url%@*}" "$SRC/$dir" ;;
    *) mkdir -p "$SRC/$dir" && curl -fsSL "$url" | tar -xz -C "$SRC/$dir" --strip-components=1 ;;
  esac
}

cmake_pkg() { # dir [cmake args...]
  local dir="$1"; shift
  echo "== build $dir"
  emcmake cmake -S "$SRC/$dir" -B "$WORK/build/$dir" "${CMAKE_COMMON[@]}" "$@" >"$WORK/$dir.configure.log" 2>&1 \
    || { tail -30 "$WORK/$dir.configure.log"; exit 1; }
  cmake --build "$WORK/build/$dir" -j "$JOBS" >"$WORK/$dir.build.log" 2>&1 || { tail -30 "$WORK/$dir.build.log"; exit 1; }
  cmake --install "$WORK/build/$dir" >/dev/null
}

build_zlib()     { fetch https://github.com/madler/zlib.git@v1.3.1 zlib; cmake_pkg zlib -DZLIB_BUILD_EXAMPLES=OFF; rm -f "$PREFIX"/lib/libz.so*; }
build_libpng()   { fetch https://github.com/pnggroup/libpng.git@v1.6.43 libpng; cmake_pkg libpng -DPNG_SHARED=OFF -DPNG_TESTS=OFF -DPNG_TOOLS=OFF -DPNG_HARDWARE_OPTIMIZATIONS=OFF; }
build_zstd()     { fetch https://github.com/facebook/zstd.git@v1.5.6 zstd
                   echo "== build zstd"
                   emcmake cmake -S "$SRC/zstd/build/cmake" -B "$WORK/build/zstd" "${CMAKE_COMMON[@]}" -DZSTD_BUILD_SHARED=OFF -DZSTD_BUILD_PROGRAMS=OFF -DZSTD_BUILD_TESTS=OFF -DZSTD_MULTITHREAD_SUPPORT=OFF >"$WORK/zstd.log" 2>&1
                   cmake --build "$WORK/build/zstd" -j "$JOBS" >>"$WORK/zstd.log" 2>&1 && cmake --install "$WORK/build/zstd" >/dev/null; }
build_libzip()   { fetch https://github.com/nih-at/libzip.git@v1.10.1 libzip; cmake_pkg libzip -DBUILD_TOOLS=OFF -DBUILD_REGRESS=OFF -DBUILD_EXAMPLES=OFF -DBUILD_DOC=OFF -DENABLE_BZIP2=OFF -DENABLE_LZMA=OFF -DENABLE_ZSTD=OFF -DENABLE_OPENSSL=OFF -DENABLE_GNUTLS=OFF -DENABLE_MBEDTLS=OFF -DENABLE_COMMONCRYPTO=OFF -DENABLE_WINDOWS_CRYPTO=OFF; }
build_fmt()      { fetch https://github.com/fmtlib/fmt.git@12.1.0 fmt; cmake_pkg fmt -DFMT_TEST=OFF -DFMT_DOC=OFF; }
build_pugixml()  { fetch https://github.com/zeux/pugixml.git@v1.14 pugixml; cmake_pkg pugixml -DPUGIXML_BUILD_TESTS=OFF; }
build_rapidjson(){ fetch https://github.com/Tencent/rapidjson.git@master rapidjson; cmake_pkg rapidjson -DRAPIDJSON_BUILD_DOC=OFF -DRAPIDJSON_BUILD_EXAMPLES=OFF -DRAPIDJSON_BUILD_TESTS=OFF; }
build_glm()      { fetch https://github.com/g-truc/glm.git@1.0.1 glm; cmake_pkg glm -DGLM_BUILD_TESTS=OFF -DGLM_BUILD_LIBRARY=OFF; }
build_boost()    { fetch https://github.com/boostorg/boost/releases/download/boost-1.86.0/boost-1.86.0-cmake.tar.gz boost
                   cmake_pkg boost "-DBOOST_INCLUDE_LIBRARIES=program_options;nowide;filesystem;container;random;algorithm;tokenizer;optional;signals2;ptr_container;static_string;predef;iterator;dynamic_bitset;bind;crc;circular_buffer;variant;range" -DBOOST_ENABLE_MPI=OFF -DBOOST_ENABLE_PYTHON=OFF -DBUILD_TESTING=OFF; }
build_openssl()  { fetch https://github.com/openssl/openssl/releases/download/openssl-3.0.15/openssl-3.0.15.tar.gz openssl
                   echo "== build openssl"
                   ( cd "$SRC/openssl" && [ -f Makefile ] || emconfigure ./Configure linux-generic32 no-asm no-shared no-dso no-engine no-tests no-afalgeng no-ui-console no-secure-memory \
                       --prefix="$PREFIX" --libdir=lib CFLAGS="$CFLAGS" >"$WORK/openssl.log" 2>&1
                     sed -i 's/^CROSS_COMPILE=.*/CROSS_COMPILE=/' Makefile
                     emmake make -j "$JOBS" build_libs >>"$WORK/openssl.log" 2>&1 && emmake make install_dev >>"$WORK/openssl.log" 2>&1 ) || { tail -30 "$WORK/openssl.log"; exit 1; }; }
build_curl()     { fetch https://github.com/curl/curl/releases/download/curl-8_10_1/curl-8.10.1.tar.gz curl
                   cmake_pkg curl -DBUILD_CURL_EXE=OFF -DBUILD_TESTING=OFF -DBUILD_LIBCURL_DOCS=OFF -DBUILD_MISC_DOCS=OFF -DENABLE_CURL_MANUAL=OFF -DCURL_USE_OPENSSL=ON -DOPENSSL_ROOT_DIR="$PREFIX" -DOPENSSL_USE_STATIC_LIBS=ON \
                     -DCURL_USE_LIBPSL=OFF -DCURL_USE_LIBSSH2=OFF -DUSE_LIBIDN2=OFF -DUSE_NGHTTP2=OFF -DCURL_ZLIB=ON -DCURL_BROTLI=OFF -DCURL_ZSTD=OFF -DENABLE_THREADED_RESOLVER=OFF -DCURL_DISABLE_LDAP=ON -DCURL_CA_BUNDLE=none -DCURL_CA_PATH=none -DHAVE_POLL_FINE=0; }

ALL="zlib libpng zstd libzip fmt pugixml rapidjson glm boost openssl curl"
for p in ${@:-$ALL}; do "build_$p"; done
echo "deps installed in $PREFIX"
