void clear_music_gpio(void *mmio);

void set_music_gpio(void *mmio, uint32_t music_mask);

void send_instruction_2(void *mmio, uint64_t ins);

void send_instruction(void *mmio, uint64_t instr, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint32_t tilemask);

void* initialize_uio(char *dev_path, char *size_path);

void poll_dma(void *DMA, int y);

void write_to_fb();
