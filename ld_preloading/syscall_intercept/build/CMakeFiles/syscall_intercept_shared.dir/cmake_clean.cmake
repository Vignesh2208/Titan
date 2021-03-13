file(REMOVE_RECURSE
  "syscall_intercept_scoped.o"
  "syscall_intercept_unscoped.o"
  "libsyscall_intercept.pdb"
  "libsyscall_intercept.so.0.1.0"
  "libsyscall_intercept.so"
  "libsyscall_intercept.so.0"
)

# Per-language clean rules from dependency scanning.
foreach(lang )
  include(CMakeFiles/syscall_intercept_shared.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
