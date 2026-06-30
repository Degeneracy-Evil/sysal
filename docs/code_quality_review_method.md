# 代码质量多维度评审方法

## 概述

本文档定义了一套可复用的代码质量评审方法，通过多维度评分 + 并行 agent 评审的方式，
对代码库进行全面、客观、可追溯的质量评估。评审结果记录在独立的质量检测报告中。

## 评审流程

### 1. 确定评分维度与权重

根据项目特点选择评分维度。以下是 sysal 项目使用的维度作为参考模板，
其他项目可按需调整维度集合和权重（权重总和 100%，差别不要太大）：

| 维度 | 权重 | 评什么 |
|------|------|--------|
| 设计文档忠实度 | 15% | 代码是否完全匹配设计文档——struct 字段、类型名、API 签名、枚举值 |
| API 优雅度 | 15% | 公共 API 是否简洁、直觉、符合现代语言习惯；是否有不必要的复杂度 |
| 代码一致性 | 12% | 命名风格、代码组织、include 顺序、注释风格、错误处理模式是否全项目一致 |
| Resolver/核心逻辑正确性 | 12% | 核心业务逻辑是否正确实现；边界条件是否处理；设计文档要求的交叉检查是否有效 |
| 类型安全 | 8% | 强类型（StrongId/NamedString/ScalarUnit 等）是否在所有该用的地方用了 |
| 错误处理健壮性 | 10% | 异常输入处理是否优雅；warning 是否有意义；错误是否在正确的场景抛出 |
| Parser/模块健壮性 | 10% | 各模块是否处理了边界情况（空输入、缺字段、格式异常）；是否有 fallback |
| 职责分离 | 10% | 各层是否干净分离；有没有跨层调用；每层是否只做自己的事 |
| 测试质量 | 8% | 测试是否覆盖有意义的场景；是否有边界测试；是否可维护 |

### 2. 评分规则

- **整数评分**：1-10 分，不允许小数
- **评分标准**：
  - 10 = 无瑕
  - 8 = 有小问题
  - 6 = 有明显缺口
  - 4 = 有严重问题
  - 2 = 勉强能用
- **加权总分**：各维度分数 × 权重之和，最终取整
- **每条发现必须有具体代码引用**：`文件路径:行号 + 问题描述`

### 3. 评审方法

使用 Oracle agent（高推理能力的只读 consultant）进行评审。Oracle 适合做质量评判，
因为它们能判断"设计是否优雅"而不只是"代码是否有 bug"。

#### 分批执行

为避免 LLM 后端过载，分批启动 agent，每批最多 2 个并行：

| 批次 | Agent | 负责维度 |
|------|-------|---------|
| 1 | Oracle A | 维度 1 + 维度 2 |
| 1 | Oracle B | 维度 3 + 维度 4 |
| 2 | Oracle C | 维度 5 + 维度 6 |
| 2 | Oracle D | 维度 7 + 维度 8 |
| 3 | Oracle E | 维度 9 |

每个 Oracle agent 负责评估 1-2 个维度，独立阅读代码文件后给出评分。

#### Agent Prompt 模板

每个 Oracle agent 的 prompt 应包含：

```
## TASK
You are a code quality reviewer. Score N dimensions of the [项目名] codebase.

## SCORING RULES
- Integer scores only (1-10), no decimals
- Every score MUST have specific code evidence: file path + line number + description
- Score 10 = flawless, 8 = minor issues, 6 = notable gaps, 4 = serious problems, 2 = barely functional

## DIMENSION 1: [维度名] (Weight: X%)
**What to evaluate**: [评估什么]
**How to evaluate**: [怎么评估——具体步骤、要读哪些文件、检查什么]
**Files to read**: [需要读的文件列表]

## DIMENSION 2: [维度名] (Weight: X%)
[同上]

## OUTPUT FORMAT
## DIMENSION 1: [维度名]
Score: X/10
### Findings
- [PASS/FAIL/WARN] File: path:line — description
### Summary
2-3 sentence assessment

## DIMENSION 2: [维度名]
[同上]
```

### 4. 汇总报告

收集所有 agent 的评分后，汇总为最终报告：

1. **总分表**：维度、分数、权重、加权分
2. **加权总分**：取整
3. **各维度详评**：亮点 + 扣分项（附代码引用）
4. **关键问题优先级排序**：P0（必须修复）/ P1（应该修复）/ P2（建议修复）

### 5. 评审结果文档

评审结果写入独立的质量检测报告（如 `docs/quality_reports/` 目录下），
与本文档（方法定义）分开存放。报告格式：

```markdown
# [项目名] [版本] 代码质量评审报告

## 评审日期
YYYY-MM-DD

## 评审范围
[评审了哪些文件/组件]

## 总分
[维度表 + 加权总分]

## 各维度详评
[每个维度的分数、亮点、扣分项]

## 关键问题优先级排序
[P0/P1/P2 问题表]
```

## 注意事项

1. **Oracle agent 是只读的**——它们不能修改文件，只能读取和分析
2. **分批执行**——每批最多 2 个并行 agent，避免后端过载
3. **prompt 要具体**——告诉 agent 读哪些文件、检查什么、用什么标准评分
4. **评分必须有证据**——不允许只给分数不给代码引用
5. **方法文档与结果文档分离**——本文档定义方法，评审结果写入独立报告
6. **可复用**——其他项目可按此模板调整维度和权重后复用
