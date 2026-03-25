InlayHints:
  ParameterNames: false     # removes parameter names
  DeducedTypes: false       # removes value_type, auto type hints
  Designators: false        # removes [0]=, [1]= hints
  Enabled: true

CompileFlags:
  Add:
    - -std=c++20
    - -I{{PROJECT_ROOT}}/libs/nlohmann/json/v3.12.0/single_include/nlohmann
