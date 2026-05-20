DEFINE_MODEL_FOR(Equation,
  (x, y, z),

  CONSTRAINT(cst,
    ITEM(x > 0),
    ITEM(y > 0),
    ITEM(z > 0),
    ITEM(x + y > z),
    ITEM(x + z > y),
    ITEM(y + z > x)
  )
)
