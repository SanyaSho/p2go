SET DEF_WARN=-Wall -Werror -Wundef -Wno-multichar 
SET LINK_FLAGS= -xnone -Xlinker --start-group ../../../lib/binkspu.a ../../../lib/binkspu_spurs.a -Xlinker --end-group
SET LIBS= -ldma
REM Necessary for Bink
SET SPU_ELF_TO_PPU_OBJ_EXTRA_FLAGS= --strip-mode=normal