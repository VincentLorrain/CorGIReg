# NodeTest rules

`NodeTest/NodeRule.hpp` defines a typed `NodeValue` document and
`compileNodeRule(rule)`, which validates and compiles a rule into a reusable
boolean callback. It does not access a global registry. Regexes compile once,
using C++ ECMAScript syntax and full-string matching.

`NodeCondition(key, rule)` applies the same rules to `NodeNN`. The existing
`NodeCondition(key, expressionString)` now evaluates its expression instead of
unconditionally returning true. Both return false for a null node. Register a
condition explicitly with `NodeDictionary::registerFunction` when needed.

```cpp
using V = CorGIReg::NodeValue;
CorGIReg::NodeCondition conv3("conv3", V::Object{
    {"path", V::Array{"attributes", "kernel_shape"}},
    {"eq", V::Array{3, 3}}
});
// conv3.test(node) checks the original NodeNN metadata.
```

The `NodeNN` adapter exposes its info dictionary under `attributes`, its `Type`
as `op`, and its optional `name`, `domain` and `inputs` fields at the root.
Supported std::any values include strings, booleans, standard int/long integer
types, float/double, null, NodeValue, NodeValue objects/arrays, NodeNN::Dictionary,
and vectors of int, int64_t, float, double or string. Unsupported values retain
their presence but cannot satisfy direct value comparisons. No string-to-number
or boolean-to-number conversion occurs.

Rules have exactly one of these forms:

- `{"path": ["attributes", "group"], "gte": 1}`: one comparison at a path.
- `{"all": [rule, ...]}`, `{"any": [rule, ...]}`, `{"not": rule}`: composition.
- `{"keys": ["attributes"], "regex": "debug_.*", "count": {"eq": 0}}`:
  count matching keys in an existing object.

Path segments are literal string keys or nonnegative integer array indices.
An empty path addresses the document itself. Logical arrays must be nonempty.
All branches are validated at compilation, even unreachable ones; evaluation
short-circuits. Nesting is limited to 64 rule levels.

| Operator | Operand and behavior |
| --- | --- |
| exists | Boolean; null, false, zero and empty string are present |
| eq / ne | Typed value comparison; missing and incompatible values return false |
| in | Array of candidate values |
| gt / gte / lt / lte | Finite numeric bound; booleans are not numbers |
| regex | String pattern, applied only to string values |
| contains | Array membership using typed equality |
| length | One numeric comparison on an array/object size or string byte length |

Arrays compare by order; objects compare by key and value. Numeric integer and
floating values compare numerically. Missing paths fail all value comparisons,
including `ne`. Explicit `not` negates its entire child result; combine it with
`exists: true` if absence should be rejected. Key-count rules fail if their
target object is absent or has the wrong type. Attribute defaults are never
injected by this evaluator.
