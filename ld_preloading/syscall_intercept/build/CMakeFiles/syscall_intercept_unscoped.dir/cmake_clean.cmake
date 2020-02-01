file(REMOVE_RECURSE
  "libsyscall_intercept_unscoped.pdb"
  "libsyscall_intercept_unscoped.a"
)

# Per-language clean rules from dependency scanning.
foreach(lang ASM C)
  include(CMakeFiles/syscall_intercept_unscoped.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
