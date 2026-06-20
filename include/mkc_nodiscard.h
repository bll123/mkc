/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */

#pragma once

#include <stddef.h>

#if __STDC_VERSION__ < 202000
# undef MKC_NODISCARD
# define MKC_NODISCARD __attribute__ ((warn_unused_result))
#endif
#if __STDC_VERSION__ >= 202000
# undef MKC_NODISCARD
# define MKC_NODISCARD [[nodiscard]]
#endif
#if ! defined (MKC_NODISCARD)
# define MKC_NODISCARD
#endif
