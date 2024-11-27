#!/bin/bash

input_file="grub/grub-core/lib/gnulib/argp.h"
cp "$input_file" "$input_file.bak"
sed -i '' '597,604d' "$input_file"
sed -i '' '597a\
#endif\
#define ARGP_EI static inline\
' "$input_file"

input_file="grub/grub-core/lib/gnulib/argp-fmtstream.h"
cp "$input_file" "$input_file.bak"
sed -i '' '194,201d' "$input_file"
sed -i '' '194a\
#endif\
#define ARGP_FS_EI static inline\
' "$input_file"

input_file="grub/grub-core/lib/gnulib/ialloc.h"
cp "$input_file" "$input_file.bak"
sed -i '' '31,33d' "$input_file"
sed -i '' '31a\
#define IALLOC_INLINE static inline\
' "$input_file"

input_file="grub/grub-core/lib/gnulib/stat-time.h"
cp "$input_file" "$input_file.bak"
sed -i '' '34,36d' "$input_file"
sed -i '' '34a\
# define _GL_STAT_TIME_INLINE static inline
' "$input_file"

input_file="grub/grub-core/lib/gnulib/xsize.h"
cp "$input_file" "$input_file.bak"
sed -i '' '37,39d' "$input_file"
sed -i '' '37a\
# define XSIZE_INLINE static inline
' "$input_file"

input_file="grub/grub-core/lib/posix_wrap/sys/types.h"
cp "$input_file" "$input_file.bak"
sed -i '' 's|typedef grub_int64_t int64_t;|typedef long long int64_t;|' "$input_file"
sed -i '' 's|typedef grub_addr_t uintptr_t;|typedef unsigned long uintptr_t;|' "$input_file"



