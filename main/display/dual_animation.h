// [NEW AND CORRECTED FILE CONTENT]

#ifndef DUAL_ANIMATION_H_
#define DUAL_ANIMATION_H_

#include <lvgl.h>
#include <string>
#include <list>
#include <vector>

// Define the function pointer type for animation callbacks
typedef uint32_t (*AnimationFunction)(int count, uint16_t duration);

// --- Fix 1: Forward-declare the functions BEFORE the list that uses them ---
uint32_t shark_animation(int count, uint16_t duration);
uint32_t smile_animation(int count, uint16_t duration);

// Define the core data structures
typedef struct {
    const lv_img_dsc_t** images;
    const char* name;
    AnimationFunction callback;
    int count;
    uint16_t duration;
} dual_anim_t;

typedef struct {
    dual_anim_t dual_anim;
    const char* text;
} emotions_anim_t;

// --- Fix 2: Use 'extern' to DECLARE the list, not define it ---
// This tells other files "this list exists somewhere else."
extern const std::vector<emotions_anim_t> emotions_anim;

// The class definition itself
class DualAnimation {
public:
    DualAnimation();
    void ShowAnimation(const char* emotion);
    void ShowAnimationNow(const char* emotion);

private:
    static void animation_task(void *param);
    const dual_anim_t* get_anim_by_name(const char* name);
    std::list<dual_anim_t> anim_queue_;
};

#endif // DUAL_ANIMATION_H_