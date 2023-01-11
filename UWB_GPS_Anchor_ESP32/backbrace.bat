set addr2line="C:\Users\ma\.platformio\packages\toolchain-xtensa-esp32\bin\xtensa-esp32-elf-addr2line.exe"
set elf=".pio\build\esp32dev\firmware.elf"
for %%a in (%*%) do (
  %addr2line% -pfiaC -e %elf% %%a
)
pause