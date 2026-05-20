#include "Equation.h"

#include "fuzzy/define_model_1.h"
namespace fuzzy {
#include "model.inc"
}  // namespace fuzzy

#include "fuzzy/define_model_2.h"
namespace fuzzy {
#include "model.inc"
}  // namespace fuzzy

#undef DEFINE_MODEL_FOR
#undef CONSTRAINT
#undef ITEM
#undef VARS
#undef _GENERATE_BLOCK_ITEM
#undef _GENERATE_VAR_DECL
#undef _GENERATE_VAR_DECLS
#undef _GENERATE_CTOR
#undef _INIT_VAR
