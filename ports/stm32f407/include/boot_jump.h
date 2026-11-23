#ifndef BOOT_JUMP_H
#define BOOT_JUMP_H
/* Call only after the image has passed iap_image_validate(). Never returns. */
void boot_jump_to_app(void) __attribute__((noreturn));
#endif
