#ifndef _ENGINE_HAL_H_
#define _ENGINE_HAL_H_

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

#include <stdint.h>

int set_right_motor_speed(int speed);
int set_left_motor_speed(int speed);
int halt_right_motor();
int halt_left_motor();

#ifdef __cplusplus
} // extern "C"
#endif  // __cplusplus

#endif /* _ENGINE_HAL_H_ */
