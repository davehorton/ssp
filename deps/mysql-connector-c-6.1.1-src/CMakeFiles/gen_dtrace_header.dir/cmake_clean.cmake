FILE(REMOVE_RECURSE
  "CMakeFiles/gen_dtrace_header"
  "include/probes_mysql_dtrace.h"
  "include/probes_mysql_nodtrace.h"
)

# Per-language clean rules from dependency scanning.
FOREACH(lang)
  INCLUDE(CMakeFiles/gen_dtrace_header.dir/cmake_clean_${lang}.cmake OPTIONAL)
ENDFOREACH(lang)
