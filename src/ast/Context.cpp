#include "fuzzy/ast/Context.h"

namespace fuzzy::detail {
thread_local ExprContext* curr_ctx{nullptr};
}
