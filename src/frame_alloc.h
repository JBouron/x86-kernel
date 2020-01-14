#pragma once

void init_frame_alloc(void);

void *alloc_frame(void);

void fixup_frame_alloc_to_virt(void);

void frame_alloc_test(void);
