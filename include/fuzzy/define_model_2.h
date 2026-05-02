#include <boost/preprocessor.hpp>

#undef DEFINE_MODEL_FOR
#undef CONSTRAINT

#define DEFINE_MODEL_FOR(class_name, vars, blocks...)                                          \
  Model<class_name>::Model(class_name& dst) :                                                  \
      BOOST_PP_SEQ_ENUM(BOOST_PP_SEQ_TRANSFORM(_INIT_VAR, dst, BOOST_PP_TUPLE_TO_SEQ(vars))) { \
    blocks                                                                                     \
  }

#define CONSTRAINT(block_name, ...)                                        \
  block_name.init(_##block_name##_file, _##block_name##_line, #block_name, \
                  [this] { _##block_name##_thunk_(); });

#define _INIT_VAR(_r, data, elem)                    \
  elem {                                             \
    ::fuzzy::detail::curr_ctx->create_var(data.elem) \
  }
