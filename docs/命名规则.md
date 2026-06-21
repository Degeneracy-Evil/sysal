| 对象 | 推荐规则 | 示例 |
|---|---|---|
| namespace | `snake_case` | `blas_benchmark::cuda` |
| class / struct | `PascalCase` | `BenchmarkRunner`, `BenchmarkConfig` |
| type alias | `PascalCase` | `MatrixView`, `ScalarType` |
| function | `snake_case` | `run_benchmark()` |
| variable | `snake_case` | `matrix_size` |
| member variable | `snake_case_` | `elapsed_time_`, `config_` |
| enum class type | `PascalCase` | `BackendType`, `MatrixLayout` |
| enum value | `PascalCase` | `RowMajor`, `ColumnMajor`, `Cuda`, `OpenBlas` |
| concept | `snake_case` | `matrix_like`, `scalar_like` |
| template parameter | `T` or `PascalCase` | `T`, `Backend`, `Scalar` |
| macro | `PROJECT_PREFIX_UPPER_SNAKE_CASE` | `BLAS_BENCHMARK_ENABLE_CUDA` |
| file name | `snake_case` | `benchmark_runner.hpp` |
