# Link options of the WebAssembly build.
# Memory layout (see NOTES.md 4.3): guest memory occupies host 0x00010000-0x70010000 (rotated mapping,
# CEMU_WASM_GUEST_BASE in util/MemMapper/MemMapper.h), Emscripten's data/stack/heap start at GLOBAL_BASE above it.
set(CEMU_WASM_TARGET "node" CACHE STRING "node or web")
target_link_options(cemu-headless PRIVATE
	-pthread -fwasm-exceptions -sSUPPORT_LONGJMP=wasm
	-sGLOBAL_BASE=1793MB
	-sINITIAL_MEMORY=3072MB -sALLOW_MEMORY_GROWTH=1 -sMAXIMUM_MEMORY=4GB
	-sSTACK_SIZE=8MB -sDEFAULT_PTHREAD_STACK_SIZE=2MB
	-sPTHREAD_POOL_SIZE=64
	-sPROXY_TO_PTHREAD -sEXIT_RUNTIME=1
	-sASSERTIONS=1
	--profiling-funcs
)
if(CEMU_WASM_TARGET STREQUAL "node")
	target_link_options(cemu-headless PRIVATE -sENVIRONMENT=node -sNODERAWFS=1)
else()
	# runs inside a dedicated Worker (web/cemu-worker.js); the user's game is mounted with WORKERFS (read in place, never copied)
	target_link_options(cemu-headless PRIVATE -sENVIRONMENT=web,worker -sFORCE_FILESYSTEM=1 -lworkerfs.js -lidbfs.js
		-sMODULARIZE=1 -sEXPORT_NAME=createCemu -sINVOKE_RUN=0 -sINCOMING_MODULE_JS_API=print,printErr,locateFile,onExit,onAbort,mainScriptUrlOrBlob,preRun,postRun,arguments -sEXPORTED_RUNTIME_METHODS=FS,ENV,callMain,WORKERFS,IDBFS
		"SHELL:--preload-file ${CMAKE_SOURCE_DIR}/bin/gameProfiles@/cemu-data/gameProfiles"
		"SHELL:--preload-file ${CMAKE_SOURCE_DIR}/bin/resources/sharedFonts@/cemu-data/resources/sharedFonts")
	set_target_properties(cemu-headless PROPERTIES OUTPUT_NAME cemu-web)
endif()
