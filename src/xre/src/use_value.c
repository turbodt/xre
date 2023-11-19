#include "./use_value.h"
#include "./context.h"
#include "./base.h"


struct XREState {
    struct IComponentRef base;
};


#define REF_TO_STATE(ref, type) ((type *)(ref))
#define GET_REF_VALUE_PTR(state, type) (type *)((state)->base.base.value)


#define XRE_USE_X_FACTORY(type, struct_type, promoted_type, name) \
\
struct struct_type { \
    struct XREState base; \
}; \
 \
 \
static void * name##_alloc(va_list); \
static void name##_destroy(void *); \
\
\
struct struct_type * xre_use_##name(struct IContext * ctx, type initial_value) { \
    struct IComponentRef * ref = context_use_ref_ex( \
        ctx, \
        name##_alloc, \
        name##_destroy, \
        initial_value \
    ); \
    return REF_TO_STATE(ref, struct struct_type); \
}; \
 \
 \
inline type xre_state_get_##name(struct struct_type *state) { \
    return *GET_REF_VALUE_PTR(state, type); \
}; \
 \
 \
inline void xre_state_set_##name(struct struct_type *state, type value) { \
    type * ptr = GET_REF_VALUE_PTR(state, type); \
    *ptr = value; \
}; \
 \
 \
void * name##_alloc(va_list args) { \
    type * value = XRE_ALLOC(type, 1); \
    *value = (type) va_arg(args, promoted_type); \
    return value; \
}; \
 \
void name##_destroy(void *value) { \
    XRE_FREE(value); \
};


XRE_USE_X_FACTORY(char, XREStateChar, int, char)
XRE_USE_X_FACTORY(int, XREStateInt, int, int)
XRE_USE_X_FACTORY(double, XREStateDouble, double, double)
XRE_USE_X_FACTORY(float, XREStateFloat, double, float)
