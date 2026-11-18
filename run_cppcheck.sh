cppcheck --enable=all --addon=misra.json --language=c --std=c99 --platform=unix32 \
  --checkers-report=${PWD}/cppcheck.log \
  --suppress=missingIncludeSystem \
  --template="{file},{line},{severity},{id},{message}" \
  -I ${PWD}/dataloggerv2mx/stm32/Core/Inc \
  ${PWD}/dataloggerv2mx/stm32/Core/Src 2> raport_misra.csv

