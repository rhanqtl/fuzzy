#include <boost/preprocessor.hpp>

#include "fuzzy/fuzzy.h"

#define DEFINE_MODEL_FOR(class_name, vars, blocks...) \
  template <>                                         \
  struct Model<class_name> : ModelBase {              \
    using self_type = Model<class_name>;              \
    _GENERATE_VAR_DECLS(class_name, vars)             \
    blocks _GENERATE_CTOR(class_name, vars, ##blocks) \
  };

#define CONSTRAINT(name, ...)                                                             \
  ConstraintBlock name;                                                                   \
  static constexpr auto _##name##_file = __FILE__;                                        \
  static constexpr auto _##name##_line = __LINE__;                                        \
  void _##name##_thunk_() {                                                               \
    BOOST_PP_SEQ_FOR_EACH(_GENERATE_BLOCK_ITEM, _, BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__)) \
  }

#define ITEM(expr) ::fuzzy::detail::curr_ctx->add(expr, {__FILE__, __LINE__, #expr});

#define _GENERATE_VAR_DECLS(class_name, vars) \
  BOOST_PP_SEQ_FOR_EACH(_GENERATE_VAR_DECL, class_name, BOOST_PP_TUPLE_TO_SEQ(vars))

#define _GENERATE_VAR_DECL(_r, class_name, name) \
  convert_to_rand_t<decltype(class_name::name)>& name;

#define _GENERATE_BLOCK_ITEM(_r, _data, elem) elem

#define _GENERATE_CTOR(class_name, vars, blocks...) Model(class_name&);
