/*
 *  ----------------------------------
 * |         libmem - by rdbo         |
 * |      Memory Hacking Library      |
 *  ----------------------------------
 */

/*
 * Copyright (C) 2025    Rdbo
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License version 3
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <libmem/libmem.h>
#include <memory.h>
#include <stdio.h>
#include "arch.h"

lm_arch_t get_architecture() {
  return LM_GetBits() == 64 ? LM_ARCH_X64 : LM_ARCH_X86;
}

lm_size_t get_max_hook_size() {
  return 14;
}

lm_size_t generate_hook_payload(lm_address_t from,
                                lm_address_t to,
                                lm_size_t bits,
                                lm_byte_t** payload_out) {
  lm_byte_t* code = NULL;
  lm_byte_t jump32[] = {0xE9, 0x0, 0x0, 0x0, 0x0}; /* jmp <rel addr> */
  lm_byte_t jump64[] = {
      0xFF,
      0x25,
      0x0,
      0x0,
      0x0,
      0x0, /* jmp [rip] */
      0xFF,
      0xFF,
      0xFF,
      0xFF,
      0xFF,
      0xFF,
      0xFF,
      0xFF /* <abs addr> */
  };
  lm_byte_t* payload;
  lm_size_t size = 0;
  lm_address_t relative_addr;

  relative_addr = to - from - sizeof(jump32);

  if (bits == 64 && ((int64_t)relative_addr < (int64_t)0xFFFFFFFF80000000 ||
                     (int64_t)relative_addr > (int64_t)0x7FFFFFFF)) {
    size = sizeof(jump64);
    payload = (lm_byte_t*)jump64;
    *(uint64_t*)(&jump64[6]) = (uint64_t)to;
  } else {
    size = sizeof(jump32);
    payload = (lm_byte_t*)jump32;
    *(uint32_t*)(&jump32[1]) = (uint32_t)relative_addr;
  }

  code = malloc(size);
  if (!code) {
    return 0;
  }

  memcpy(code, payload, size);
  *payload_out = code;

  return size;
}

lm_size_t generate_no_ops(lm_byte_t* buf, lm_size_t size) {
  memset(buf, 0x90, size);
  return size;
}
